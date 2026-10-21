#include "plugin/Processor.h"
#include "ui/WebEditor.h"
#include <cmath>
#include <iterator>
#include <juce_audio_utils/juce_audio_utils.h>
#if JucePlugin_Build_Standalone
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif
namespace {
bool inputAvailable(const juce::AudioProcessor& processor){
#if JucePlugin_Build_Standalone
    if(auto* holder=juce::StandalonePluginHolder::getInstance()){
        auto* device=holder->deviceManager.getCurrentAudioDevice();
        return device&&!holder->muteInput.load()&&device->getActiveInputChannels().countNumberOfSetBits()>0;
    }
#endif
    return processor.getTotalNumInputChannels()>0;
}
}
AionProcessor::AionProcessor(ExportDestination destination)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "AionState", {std::make_unique<juce::AudioParameterFloat>(
          juce::ParameterID{"masterGain", 1}, "Master gain", juce::NormalisableRange<float>{0.0f, 2.0f}, 0.0f),
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"inputMonitor", 1}, "Input monitor", false),
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"autoPlayAfterRecord", 1}, "Auto-play after Rec", true),
          std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"syncMode",1},"Sync mode",juce::StringArray{"Off","Static","Dynamic"},2),
          std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"syncBpm",1},"Tempo",juce::NormalisableRange<float>{10,999},120),
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"syncBars",1},"Bars",1,64,2),
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"syncNum",1},"Beats per bar",1,32,4),
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"syncDen",1},"Time signature denominator",1,32,4),
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"countInVisual",1},"Visual count-in",true),
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"countInAudio",1},"Sound count-in",false),
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"metronomeEnabled",1},"Metronome",false),
          std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"metronomeVolume",1},"Metronome volume",juce::NormalisableRange<float>{0,2},0.5f)}) {
    exportDestination=std::move(destination);
    midiDeviceChanges=juce::MidiDeviceListConnection::make([this]{engine.cancelMidi();});
    metroEnabled=parameters.getRawParameterValue("metronomeEnabled");metroVolume=parameters.getRawParameterValue("metronomeVolume");
    master = parameters.getRawParameterValue("masterGain");
    monitor = parameters.getRawParameterValue("inputMonitor");
    autoPlay = parameters.getRawParameterValue("autoPlayAfterRecord");
    syncMode=parameters.getRawParameterValue("syncMode");syncBpm=parameters.getRawParameterValue("syncBpm");
    syncBars=parameters.getRawParameterValue("syncBars");syncNum=parameters.getRawParameterValue("syncNum");
    syncDen=parameters.getRawParameterValue("syncDen");countIn=parameters.getRawParameterValue("countInVisual");countInAudio=parameters.getRawParameterValue("countInAudio");
}
AionProcessor::~AionProcessor(){midiDeviceChanges.reset();stopTimer();exportLifetime.reset();exportWriter.removeAllJobs(true,-1);exportChooser.reset();}
bool AionProcessor::updateMidi(aion::MidiSettings next){
    std::lock_guard guard(midiSettingsMutex);
    next.revision=midiSettings.revision+1;if(!next.revision)++next.revision;
    if(!engine.configureMidi(next))return false;
    midiSettings=next;midiSettings.modeOverride=-1;startTimer(25);return true;
}
void AionProcessor::cancelMidiLearn(){
    std::lock_guard guard(midiSettingsMutex);
    if(midiSettings.learnTarget>=0){auto next=midiSettings;next.learnTarget=-1;++next.learnToken;
        if(!updateMidi(next)){midiSettings.learnTarget=-1;++midiSettings.learnToken;engine.cancelMidiLearn();}
        midiStatus="MIDI learn cancelled";midiLearnDeadline=0;
    }
}
void AionProcessor::serviceMidi(){
    midiInputReady.store(inputAvailable(*this),std::memory_order_release);
    std::lock_guard guard(midiSettingsMutex);
    if(midiSettings.learnTarget>=static_cast<int>(aion::MidiSettings::volumeBase)&&midiLearnDeadline>0&&juce::Time::getMillisecondCounterHiRes()>=midiLearnDeadline){cancelMidiLearn();midiStatus="MIDI Learn timed out";}
    aion::MidiLearnResult result;
    while(engine.popMidiLearn(result))if(result.token==midiSettings.learnToken&&result.generation==engine.generation()&&result.target==midiSettings.learnTarget){
        if(engine.midiLearnCurrent(result))pendingMidiLearn=result;
        else cancelMidiLearn(); // Panic may invalidate capture before message-thread acknowledgement.
    }
    if(pendingMidiLearn){const auto learned=*pendingMidiLearn;
        if(learned.token!=midiSettings.learnToken||learned.generation!=engine.generation()){pendingMidiLearn.reset();}
        else if(!engine.midiLearnCurrent(learned)){pendingMidiLearn.reset();cancelMidiLearn();}
        else {auto next=midiSettings;next.bindings[static_cast<unsigned>(learned.target)]=learned.binding;next.learnTarget=-1;++next.learnToken;
            const bool valid=aion::MidiController::valid(next);if(!valid)next.bindings=midiSettings.bindings;
            if(updateMidi(next)){midiStatus=valid?"MIDI binding learned":"MIDI binding already assigned; previous mapping kept";pendingMidiLearn.reset();}
        }
    }
    if(midiSettings.learnTarget>=0&&!pendingMidiLearn&&latest.midi.revision==midiSettings.revision&&latest.midi.learnTarget<0){midiSettings.learnTarget=-1;++midiSettings.learnToken;midiStatus="MIDI learn cancelled";}
    if(midiSettings.learnTarget>=0&&midiSettings.learnGeneration!=engine.generation()){
        auto next=midiSettings;next.learnTarget=-1;++next.learnToken;if(updateMidi(next))midiStatus="MIDI learn cancelled after audio reset";
    }
   #if JucePlugin_Build_Standalone
    if(midiSettings.enabled&&juce::Time::getMillisecondCounterHiRes()>=nextMidiDeviceCheck){
        nextMidiDeviceCheck=juce::Time::getMillisecondCounterHiRes()+1000;
        if(auto* holder=juce::StandalonePluginHolder::getInstance()){
            juce::StringArray enabled;
            for(const auto& device:juce::MidiInput::getAvailableDevices())if(holder->deviceManager.isMidiInputDeviceEnabled(device.identifier))enabled.add(device.identifier);
            for(const auto& old:previousMidiDevices)if(!enabled.contains(old)){engine.cancelMidi();break;}
            previousMidiDevices=std::move(enabled);
        }
    }
   #endif
}
void AionProcessor::cancelExport(){
    if(exportBusy&&exportWriteResult->load()==0){
        ++exportSequence;exportAbandoned=true;exportChooser.reset();exportStatus="WAV export cancelled";
        if(!exportEncoding)exportBusy=false;
    }
}
void AionProcessor::timerCallback(){
    serviceMidi();
    const auto written=exportWriteResult->load(std::memory_order_acquire);
    if(written>0){exportWriteResult->store(0);exportBusy=false;exportStatus=written==1?"WAV saved":"WAV export failed: could not write the selected file";}
    if(!exportBusy){std::lock_guard guard(midiSettingsMutex);if(!midiSettings.enabled)stopTimer();}
    if(!exportEncoding)return;
    aion::WavExport output;if(!engine.popExport(output))return;
    exportEncoding=false;
    if(exportAbandoned||engine.generation()!=exportGeneration){exportBusy=false;exportStatus="WAV export cancelled";return;}
    if(output.bytes.empty()){exportBusy=false;exportStatus="WAV export failed: loop changed or encoding ran out of memory";return;}
    exportStatus="Choose a WAV destination";
    const std::weak_ptr<int> lifetime=exportLifetime;
    try {
    auto choose=[this,lifetime,sequence=exportSequence,data=std::make_shared<std::vector<std::uint8_t>>(std::move(output.bytes))](juce::File file){
        if(lifetime.expired()||exportAbandoned||sequence!=exportSequence)return;
        if(file==juce::File{}){exportBusy=false;exportStatus="WAV export cancelled";return;}
        exportStatus="Saving WAV";exportWriteResult->store(-1);
        try {exportWriter.addJob([file,data,state=exportWriteResult]{
            bool ok=false;
            try {
                juce::TemporaryFile temporary(file);
                {juce::FileOutputStream stream(temporary.getFile());
                 ok=stream.openedOk()&&stream.write(data->data(),data->size());stream.flush();ok=ok&&stream.getStatus().wasOk();}
                if(ok)ok=temporary.overwriteTargetFileWithTemporary();
            }catch(...){ok=false;}
            state->store(ok?1:2,std::memory_order_release);
        });}catch(...){exportWriteResult->store(2,std::memory_order_release);}
    };
    if(exportDestination)exportDestination(exportFilename,std::move(choose));
    else {
        exportChooser=std::make_unique<juce::FileChooser>("Export Loop WAV",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(exportFilename),"*.wav");
        exportChooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,
            [choose=std::move(choose)](const juce::FileChooser& chooser){choose(chooser.getResult());});
    }
    }catch(...){exportChooser.reset();exportBusy=false;exportStatus="WAV export failed: could not open the save dialog";}
}
void AionProcessor::prepareToPlay(double rate, int) { midiInputReady.store(inputAvailable(*this),std::memory_order_release);engine.prepare(rate); }
bool AionProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
    return b.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (b.getMainInputChannelSet().isDisabled() || b.getMainInputChannelSet() == juce::AudioChannelSet::mono()
            || b.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
}
void AionProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& incomingMidi) {
    const juce::ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < audio.getNumChannels(); ++c) audio.clear(c, 0, audio.getNumSamples());
    if (getTotalNumInputChannels() == 1 && audio.getNumChannels() >= 2)
        audio.copyFrom(1, 0, audio, 0, 0, audio.getNumSamples());
    engine.setTarget(master->load(std::memory_order_relaxed));
    engine.setMonitoring(monitor->load(std::memory_order_relaxed) >= 0.5f);
    engine.setMetronome(metroEnabled->load()>=0.5f,metroVolume->load());
    engine.setAutoPlay(autoPlay->load(std::memory_order_relaxed) >= 0.5f);
    engine.configureSync({static_cast<aion::SyncMode>(static_cast<unsigned>(syncMode->load())),syncBpm->load(),
        static_cast<unsigned>(syncBars->load()),static_cast<unsigned>(syncNum->load()),static_cast<unsigned>(syncDen->load()),countIn->load()>=0.5f,countInAudio->load()>=0.5f});
    engine.transport = {};
    if (auto* head = getPlayHead()) if (auto position = head->getPosition()) {
        engine.transport.hostValid = true;
        engine.transport.playing = position->getIsPlaying();
        if (auto bpm = position->getBpm(); bpm && std::isfinite(*bpm) && *bpm > 0.0) engine.transport.bpm = *bpm;
        if (auto ppq = position->getPpqPosition(); ppq && std::isfinite(*ppq)) engine.transport.ppq = *ppq;
    }
    std::size_t midiCount=0,inspected=0;bool midiOverflow=false;
    engine.setMidiInputAvailable(getTotalNumInputChannels()>0&&midiInputReady.load(std::memory_order_acquire));
    for(const auto metadata:incomingMidi){
        if(++inspected>256){midiOverflow=true;break;}
        const auto* bytes=metadata.data;const int size=metadata.numBytes;
        if(size<1||size>3||bytes[0]<128||bytes[0]==240)continue;
        if(metadata.samplePosition<0||metadata.samplePosition>=audio.getNumSamples()){midiOverflow=true;break;}
        midiEvents[midiCount++]={static_cast<std::uint32_t>(metadata.samplePosition),bytes[0],static_cast<std::uint8_t>(size>1?bytes[1]:0),static_cast<std::uint8_t>(size>2?bytes[2]:0),static_cast<std::uint8_t>(size)};
    }
    if(midiOverflow){engine.midiOverflow();midiCount=0;}
    engine.process(audio.getArrayOfWritePointers(), audio.getNumChannels(), audio.getNumSamples(),std::span(midiEvents.data(),midiCount));
}
juce::AudioProcessorEditor* AionProcessor::createEditor() { return new AionWebEditor(*this); }
void AionProcessor::getStateInformation(juce::MemoryBlock& data) {
    auto state=parameters.copyState();juce::ValueTree stored("MidiSettings");
    {std::lock_guard guard(midiSettingsMutex);stored.setProperty("enabled",midiSettings.enabled,nullptr);
     stored.setProperty("version",2,nullptr);
     for(unsigned i=0;i<aion::MidiSettings::bindingCount;++i)stored.setProperty("binding"+juce::String(i),static_cast<int>(midiSettings.bindings[i]),nullptr);}
    state.removeChild(state.getChildWithName("MidiSettings"),nullptr);state.appendChild(stored,nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, data);
}
void AionProcessor::setStateInformation(const void* data, int size) {
    if (size <= 0 || size > 1024 * 1024) return;
    if (auto xml = getXmlFromBinary(data, size); xml && xml->hasTagName("AionState")) {
        auto state=juce::ValueTree::fromXml(*xml);aion::MidiSettings next;const auto stored=state.getChildWithName("MidiSettings");
        if(stored.isValid()){
            const auto enabled=stored.getProperty("enabled").toString();if(enabled!="0"&&enabled!="1")return;next.enabled=enabled=="1";
            const auto version=stored.getProperty("version",1).toString();if(version!="1"&&version!="2")return;
            const unsigned count=version=="2"?aion::MidiSettings::bindingCount:33;
            for(unsigned i=0;i<count;++i){const auto value=stored.getProperty("binding"+juce::String(i)).toString();if(value.isEmpty()||!value.containsOnly("0123456789")||value.getLargeIntValue()>65535)return;next.bindings[i]=static_cast<std::uint16_t>(value.getIntValue());}
        }
        std::lock_guard guard(midiSettingsMutex);next.learnToken=midiSettings.learnToken+1;
        if(updateMidi(next))parameters.replaceState(state);
    }
}
juce::var AionProcessor::request(const juce::Array<juce::var>& args) {
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    serviceMidi();
    auto* result = new juce::DynamicObject(); juce::var reply(result);
    result->setProperty("protocol", 1); result->setProperty("ok", false);
    const auto fail = [result, &reply](const char* reason) { result->setProperty("error", reason); return reply; };
    if (args.size() != 1 || args[0].getDynamicObject() == nullptr) return fail("Expected one request object");
    const auto& request = args[0];
    const auto session=request.getProperty("_aionSession",{});
    if(!session.isVoid()){
        if(!session.isString()||session.toString().isEmpty()||session.toString().length()>128)return fail("Invalid bridge session");
        result->setProperty("_aionSession",session); // Echo on success AND failure, message thread only.
    }
    const auto operation = request.getProperty("operation", {}).toString();
    // Consume acknowledged clears before reading/editing labels. Only fixed-size
    // epochs cross from audio; string allocation and destruction stay here.
    aion::EngineView view{};
    while (engine.popView(view)) latest = view;
    aion::WaveformEvent waveform{};
    while(engine.popWaveform(waveform))waveformCache[waveform.track]=waveform;
    aion::LiveInputEvent live{};while(engine.popLiveInput(live))latestLiveInput=live;
    if (latest.generation != nameGeneration) {
        if (latest.audioReset) trackNames.fill({});
        nameGeneration = latest.generation;
    }
    for (std::size_t i=0; i<trackNames.size(); ++i) {
        if (nameClearEpochs[i] != latest.tracks[i].clearEpoch) trackNames[i].clear();
        nameClearEpochs[i] = latest.tracks[i].clearEpoch;
    }
    if (operation == "hello" || operation == "snapshot") {
        result->setProperty("ok", true); result->setProperty("stage", "native-loop-preview");
        result->setProperty("legacyAudioEnabled", false);
        auto* midiView=new juce::DynamicObject();juce::var midiReply(midiView);juce::Array<juce::var> bindings;
        for(const auto binding:latest.midi.bindings)bindings.add(static_cast<int>(binding));
        juce::Array<juce::var> recentMidi;
        const auto observed=latest.midi.observed;
        for(unsigned n=0,count=std::min(observed,8u);n<count;++n){const auto& item=latest.midi.recent[(observed-count+n)%8];auto* event=new juce::DynamicObject();
            event->setProperty("frame",static_cast<juce::int64>(item.frame));event->setProperty("status",item.event.status);event->setProperty("data1",item.event.data1);event->setProperty("data2",item.event.data2);event->setProperty("size",item.event.size);recentMidi.add(juce::var(event));}
        midiView->setProperty("recent",recentMidi);
        midiView->setProperty("bindings",bindings);midiView->setProperty("enabled",latest.midi.enabled);midiView->setProperty("revision",static_cast<int>(latest.midi.revision));
        midiView->setProperty("mode",latest.midi.mode);midiView->setProperty("modeRevision",static_cast<int>(latest.midi.modeRevision));midiView->setProperty("sustain",latest.midi.sustain);
        {std::lock_guard guard(midiSettingsMutex);midiView->setProperty("learnTarget",midiSettings.learnTarget);}
        midiView->setProperty("messages",static_cast<int>(latest.midi.messages));midiView->setProperty("overflows",static_cast<int>(latest.midi.overflows));midiView->setProperty("rejected",static_cast<int>(latest.midi.rejected));midiView->setProperty("actions",static_cast<int>(latest.midi.actions));
        midiView->setProperty("status",midiStatus);result->setProperty("midi",midiReply);
        const float masterValue = master->load();
        result->setProperty("masterGain", std::isfinite(masterValue) ? juce::jlimit(0.0f, 2.0f, masterValue) : 0.0f);
        result->setProperty("inputAvailable",inputAvailable(*this));
        const bool metroOn=metroEnabled->load()>=0.5f;
        const float metroLevel=metroVolume->load();
        result->setProperty("metronomeEnabled",metroOn);
        result->setProperty("metronomeVolume",std::isfinite(metroLevel)?juce::jlimit(0.0f,2.0f,metroLevel):0.0f);
        result->setProperty("metronomeFlash",metroOn&&latest.metronomeFlash);
        result->setProperty("inputMonitor", monitor->load() >= 0.5f);
        result->setProperty("autoPlayAfterRecord", autoPlay->load() >= 0.5f);
        result->setProperty("generation", static_cast<int>(latest.generation));
        result->setProperty("audioReady", latest.generation != 0 && latest.generation == engine.generation());
        result->setProperty("frame", static_cast<juce::int64>(latest.frame));
        result->setProperty("sampleRate", latest.sampleRate);
        result->setProperty("freeFrames", static_cast<int>(latest.freeFrames));
        result->setProperty("inputPeak", latest.inputPeak); result->setProperty("outputPeak", latest.outputPeak);result->setProperty("masterPeak",latest.masterPeak);
        result->setProperty("lastRequestId", static_cast<juce::int64>(latest.lastRequestId));
        result->setProperty("rejected", latest.rejected); result->setProperty("poolFull", latest.poolFull);
        result->setProperty("audioReset", latest.audioReset);
        result->setProperty("exportBusy",exportBusy);result->setProperty("exportStatus",exportStatus);result->setProperty("exportFilename",exportFilename);
        result->setProperty("nextUndo",latest.nextUndo);result->setProperty("nextRedo",latest.nextRedo);
        const char* names[] = {"empty", "recording", "playing", "stopped", "overdubbing", "substituting", "finalizing", "multiplying", "armed", "queued", "stopping"};
        const auto& timing=latest.sync;
        result->setProperty("syncMode",static_cast<int>(timing.settings.mode));result->setProperty("syncBpm",timing.settings.bpm);
        result->setProperty("syncBars",static_cast<int>(timing.settings.bars));result->setProperty("syncNum",static_cast<int>(timing.settings.numerator));
        result->setProperty("syncDen",static_cast<int>(timing.settings.denominator));result->setProperty("countInVisual",timing.settings.countIn);
        result->setProperty("countInAudio",timing.settings.countInAudio);result->setProperty("countInFlash",static_cast<int>(latest.countInFlash));
        result->setProperty("gridLength",timing.loopSeconds);result->setProperty("barLength",timing.barSeconds);
        result->setProperty("gridPosition",timing.position);result->setProperty("syncFrame",static_cast<juce::int64>(timing.frame));
        const auto& live=latestLiveInput;
        if(live.generation==latest.generation&&live.track<aion::LoopBank::trackCount){
            const auto& track=latest.tracks[live.track];
            if(track.state==aion::LoopState::recording&&track.dynamicRecorded&&track.captureStartFrame==live.startFrame){
                auto* input=new juce::DynamicObject();input->setProperty("track",static_cast<int>(live.track));input->setProperty("startFrame",static_cast<juce::int64>(live.startFrame));input->setProperty("frame",static_cast<juce::int64>(live.frame));
                juce::Array<juce::var> samples;samples.ensureStorageAllocated(512);for(const auto sample:live.samples)samples.add(sample);input->setProperty("samples",samples);result->setProperty("liveInput",juce::var(input));
            }
        }
        const auto known=request.getProperty("knownWaveforms",{});
        const bool sameGeneration=static_cast<int>(request.getProperty("knownGeneration",0))==static_cast<int>(latest.generation);
        juce::Array<juce::var> tracks;
        for (const auto& t : latest.tracks) {
            auto* object = new juce::DynamicObject();
            object->setProperty("state", names[static_cast<unsigned>(t.state)]);
            object->setProperty("name", trackNames[static_cast<std::size_t>(&t-latest.tracks.data())]);
            object->setProperty("length", static_cast<int>(t.length)); object->setProperty("position", static_cast<int>(t.position));
            object->setProperty("undoCount",static_cast<int>(t.undoCount));object->setProperty("redoCount",static_cast<int>(t.redoCount));
            object->setProperty("reverse", t.reverse);
            object->setProperty("captureStartFrame",static_cast<juce::int64>(t.captureStartFrame));
            object->setProperty("waveformId",static_cast<juce::int64>(t.waveformId));
            object->setProperty("mergeSerial",static_cast<juce::int64>(t.mergeSerial));
            object->setProperty("mergeSource",static_cast<int>(t.mergeSource));
            object->setProperty("mergeSourceWaveform",static_cast<juce::int64>(t.mergeSourceWaveform));
            object->setProperty("mergeSourceClearEpoch",static_cast<juce::int64>(t.mergeSourceClearEpoch));
            const auto trackIndex=static_cast<int>(&t-latest.tracks.data());
            const bool alreadyKnown=sameGeneration&&known.isArray()&&known.size()==static_cast<int>(aion::LoopBank::trackCount)&&static_cast<juce::int64>(known[trackIndex])==static_cast<juce::int64>(t.waveformId);
            const auto& cached=waveformCache[static_cast<unsigned>(trackIndex)];
            if(!alreadyKnown){
                if(!t.waveformId)object->setProperty("wavePeaks",juce::var());
                else if(cached.generation==latest.generation&&cached.id==t.waveformId){
                    juce::Array<juce::var> peaks;peaks.ensureStorageAllocated(aion::LoopBank::waveformWidth);
                    for(const auto peak:cached.peaks)peaks.add(peak);
                    object->setProperty("wavePeaks",peaks);
                }
            }
            object->setProperty("clearEpoch",static_cast<juce::int64>(t.clearEpoch));object->setProperty("stuttering",t.stuttering);object->setProperty("stutterClock",t.stutterClock);object->setProperty("clockProgress",t.clockProgress);object->setProperty("stutterStart",t.stutterStart);object->setProperty("stutterEnd",t.stutterEnd);
            object->setProperty("startDelay",t.startDelay);object->setProperty("phaseApplyFrame",static_cast<juce::int64>(t.phaseApplyFrame));
            object->setProperty("scheduledFrame",static_cast<juce::int64>(t.scheduledFrame));object->setProperty("stopPending",t.stopPending);
            object->setProperty("effectiveRate",t.effectiveRate);object->setProperty("dynamicRecorded",t.dynamicRecorded);
            object->setProperty("pan", t.pan); object->setProperty("playbackRate", t.playbackRate);
            object->setProperty("volume", t.volume); object->setProperty("feedback", t.feedback);
            object->setProperty("finalizationFailed", t.finalizationFailed); object->setProperty("solo", t.solo); object->setProperty("muted", t.muted); object->setProperty("peak", t.peak);
            tracks.add(juce::var(object));
        }
        result->setProperty("tracks", juce::var(tracks));
    } else if (operation == "cancelExport") {
        cancelExport();result->setProperty("ok",true);
    } else if (operation == "exportLoop") {
        const auto id=request.getProperty("track",{}),generation=request.getProperty("generation",{}),waveform=request.getProperty("waveformId",{});
        const auto filename=request.getProperty("filename",{});
        if(!id.isInt()||static_cast<int>(id)<0||static_cast<int>(id)>=static_cast<int>(aion::LoopBank::trackCount))return fail("Invalid export track");
        if(!generation.isInt()||static_cast<int>(generation)!=static_cast<int>(engine.generation()))return fail("Audio generation changed");
        if(!(waveform.isInt()||waveform.isInt64()||waveform.isDouble())||!std::isfinite(static_cast<double>(waveform))||static_cast<double>(waveform)<1||static_cast<double>(waveform)>9007199254740991.0||std::floor(static_cast<double>(waveform))!=static_cast<double>(waveform))return fail("Invalid clip identity");
        const auto name=filename.toString();
        if(!filename.isString()||name.length()>4096||!name.endsWith(".wav")||!name.dropLastCharacters(4).containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")||name.length()<=4)return fail("Invalid WAV filename");
        if(exportBusy){result->setProperty("exportBusy",true);return fail("A WAV export is already in progress");}
        if(!engine.enqueueExport({static_cast<unsigned>(static_cast<int>(id)),static_cast<unsigned>(static_cast<int>(generation)),static_cast<std::uint64_t>(static_cast<double>(waveform))}))return fail("WAV export unavailable");
        ++exportSequence;exportFilename=name;exportGeneration=engine.generation();exportAbandoned=false;exportBusy=exportEncoding=true;exportStatus="Preparing WAV";startTimer(25);
        result->setProperty("ok",true);
    } else if (operation == "renameLoop") {
        const auto id = request.getProperty("track", {});
        const auto name = request.getProperty("name", {});
        if (!id.isInt() || static_cast<int>(id)<0 || static_cast<int>(id)>=static_cast<int>(trackNames.size())) return fail("Invalid track");
        if (!name.isString() || name.toString().getNumBytesAsUTF8()>4096) return fail("Expected a loop name of at most 4096 UTF-8 bytes");
        trackNames[static_cast<std::size_t>(static_cast<int>(id))] = name.toString();
        result->setProperty("ok", true);
    } else if (operation == "toggleMetronome") {
        // Serialize rapid button/key toggles against the native value, not a stale UI snapshot.
        const bool next=metroEnabled->load()<0.5f;
        auto* parameter=parameters.getParameter("metronomeEnabled");
        parameter->beginChangeGesture();parameter->setValueNotifyingHost(next?1.0f:0.0f);parameter->endChangeGesture();
        result->setProperty("ok",true);result->setProperty("enabled",next);
    } else if (operation == "setParameter") {
        const auto id = request.getProperty("id", {}).toString();
        const bool boolean=id=="inputMonitor"||id=="autoPlayAfterRecord"||id=="countInVisual"||id=="countInAudio"||id=="metronomeEnabled";
        const bool integral=id=="syncMode"||id=="syncBars"||id=="syncNum"||id=="syncDen";
        if(!boolean&&!integral&&id!="masterGain"&&id!="syncBpm"&&id!="metronomeVolume")return fail("Unknown parameter");
        const auto value = request.getProperty("value", {});
        if (!(value.isDouble() || value.isInt())) return fail("Expected numeric parameter value");
        const double number = static_cast<double>(value);
        const double maximum=id=="syncBpm"?999:(id=="syncBars"?64:(id=="syncNum"||id=="syncDen"?32:(id=="masterGain"||id=="syncMode"||id=="metronomeVolume"?2:1)));
        const double minimum=id=="syncBpm"?10:(id=="syncBars"||id=="syncNum"||id=="syncDen"?1:0);
        if(!std::isfinite(number)||number<minimum||number>maximum||(boolean&&number!=0&&number!=1)||(integral&&number!=std::floor(number)))return fail("Parameter outside range");
        auto* parameter = parameters.getParameter(id);
        parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(number))); parameter->endChangeGesture();
        result->setProperty("ok", true);
    } else if (operation == "releaseStutter") {
        const auto id=request.getProperty("track",{}),generation=request.getProperty("generation",{});
        if(!id.isInt()||static_cast<int>(id)<0||static_cast<int>(id)>static_cast<int>(aion::LoopBank::trackCount))return fail("Invalid stutter track");
        if(!generation.isInt()||static_cast<int>(generation)!=static_cast<int>(engine.generation()))return fail("Audio generation changed");
        engine.cancelStutter(static_cast<unsigned>(static_cast<int>(id)));result->setProperty("ok",true);
    } else if (operation == "loop") {
        const auto expiration = request.getProperty("expiresAt", {});
        if (!(expiration.isInt() || expiration.isInt64() || expiration.isDouble())) return fail("Missing command expiration");
        const double expiresAt = static_cast<double>(expiration);
        const auto now = static_cast<double>(juce::Time::currentTimeMillis());
        if (!std::isfinite(expiresAt) || expiresAt < now || expiresAt > now + 5000.0) return fail("Command expired; refresh state before retrying");
        const auto id = request.getProperty("track", {});
        const auto generation = request.getProperty("generation", {});
        if (!id.isInt() || static_cast<int>(id) < 0 || static_cast<int>(id) >= static_cast<int>(aion::LoopBank::trackCount)
            || !generation.isInt() || static_cast<int>(generation) <= 0) return fail("Invalid track or audio generation");
        const auto actionName = request.getProperty("action", {}).toString();
        const char* names[] = {"record", "finish", "play", "stop", "overdub", "substitute", "clear", "stopAll", "volume", "feedback", "mute", "clearAll", "solo", "retrigger", "pan", "speed", "normalize", "reverse", "undo", "redo", "undoGlobal", "redoGlobal", "multiply", "stopImmediate", "startDelay", "applyStart", "quantizeStart", "nudgeStart", "stutterStart", "stutterStop", "merge", "clearMergedSource"};
        unsigned action = 0;
        for (; action < std::size(names) && actionName != names[action]; ++action) {}
        if (action == std::size(names)) return fail("Unknown looper action");
        const auto value = request.getProperty("value", 0);
        if (!(value.isDouble() || value.isInt())) return fail("Expected numeric action value");
        const double number = static_cast<double>(value);
        if (!std::isfinite(number) || number < (actionName == "nudgeStart" ? -1.0 : actionName == "speed" ? 0.25 : 0.0) || number > (actionName=="clearMergedSource"?9007199254740991.0:actionName=="merge"?19:actionName == "pan" ? 10 : (actionName == "volume" || actionName == "speed" ? 2 : 1))
            || ((actionName=="merge"||actionName=="clearMergedSource")&&number!=std::floor(number))
            || (actionName == "nudgeStart" && number != -1 && number != 1)
            || ((actionName == "mute" || actionName == "reverse") && number != 0 && number != 1)) return fail("Action value outside range");
        if(actionName=="record"&&!inputAvailable(*this))return fail("No native input: enable an input device and Unmute Input in Options, or route audio from the host.");
        const auto requestId = nextRequest + 1;
        if (!engine.enqueueLoop({static_cast<aion::LoopAction>(action), static_cast<std::uint32_t>(static_cast<int>(id)), requestId,
                                static_cast<std::uint32_t>(static_cast<int>(generation)), 0, actionName=="multiply"?(inputAvailable(*this)?1.0f:0.0f):(actionName=="startDelay"||actionName=="clearMergedSource")?number:static_cast<double>(static_cast<float>(number))}))
            return fail("Audio generation changed or command queue is full");
        nextRequest = requestId;
        result->setProperty("ok", true); result->setProperty("queued", true);
        result->setProperty("requestId", static_cast<juce::int64>(requestId));
    } else if(operation=="midiConfig"){
        std::lock_guard guard(midiSettingsMutex);auto next=midiSettings;
        const auto action=request.getProperty("action",{}).toString();
        if(action=="enable"||action=="toggle"){
            if(action=="toggle")next.enabled=!midiSettings.enabled;
            else {const auto value=request.getProperty("enabled",{});if(!value.isBool())return fail("Expected MIDI enable flag");next.enabled=static_cast<bool>(value);}
            next.learnTarget=-1;++next.learnToken;
        }else if(action=="learn"||action=="clear"){
            const auto target=request.getProperty("target",{});if(!target.isInt())return fail("Expected MIDI target");const int id=target;
            if(id<0||id>=static_cast<int>(aion::MidiSettings::bindingCount)||(id>=20&&id<30))return fail("MIDI target unavailable");
            ++next.learnToken;next.learnGeneration=engine.generation();next.learnTarget=action=="learn"?id:-1;
            if(action=="learn")next.enabled=true;else next.bindings[static_cast<unsigned>(id)]=0;
        }else if(action=="cancelLearn"){
            const auto target=request.getProperty("target",{});
            if(!target.isVoid()&&(!target.isInt()||static_cast<int>(target)<0||static_cast<int>(target)>=static_cast<int>(aion::MidiSettings::bindingCount)||(static_cast<int>(target)>=20&&static_cast<int>(target)<30)))return fail("Invalid MIDI cancellation target");
            // A delayed blur of one field must not cancel another field/slider.
            if(target.isVoid()||static_cast<int>(target)==midiSettings.learnTarget)cancelMidiLearn();
            result->setProperty("ok",true);result->setProperty("revision",static_cast<int>(midiSettings.revision));return reply;
        }
        else if(action=="mode"){
            const auto value=request.getProperty("mode",{});if(!value.isInt()||static_cast<int>(value)<0||static_cast<int>(value)>2)return fail("Invalid performance mode");next.modeOverride=static_cast<int>(value);
        }else if(action=="save"){
           #if JucePlugin_Build_Standalone
            if(auto* holder=juce::StandalonePluginHolder::getInstance()){holder->savePluginState();holder->saveAudioDeviceState();}
           #endif
            midiStatus="Mappings saved with native app/plugin state";result->setProperty("ok",true);return reply;
        }else return fail("Unknown MIDI configuration action");
        const bool enableDevices=!midiSettings.enabled&&next.enabled;
        if(!updateMidi(next))return fail("MIDI configuration is invalid or queue is full");
        if(action=="learn")midiLearnDeadline=next.learnTarget>=static_cast<int>(aion::MidiSettings::volumeBase)?juce::Time::getMillisecondCounterHiRes()+10000:0;
        else if(action=="clear"||action=="enable"||action=="toggle")midiLearnDeadline=0;
        midiStatus=action=="learn"?"Waiting for MIDI Note, CC or Program Change...":"MIDI configuration queued";
       #if JucePlugin_Build_Standalone
        if(enableDevices)if(auto* holder=juce::StandalonePluginHolder::getInstance())
            for(const auto& device:juce::MidiInput::getAvailableDevices())holder->deviceManager.setMidiInputDeviceEnabled(device.identifier,true);
       #else
        juce::ignoreUnused(enableDevices);
       #endif
        result->setProperty("ok",true);result->setProperty("revision",static_cast<int>(midiSettings.revision));
    }else if(operation=="midiDevices"){
        juce::Array<juce::var> devices;bool standalone=false;
       #if JucePlugin_Build_Standalone
        if(auto* holder=juce::StandalonePluginHolder::getInstance()){
            standalone=true;for(const auto& info:juce::MidiInput::getAvailableDevices()){
                auto* device=new juce::DynamicObject();device->setProperty("id",info.identifier);device->setProperty("name",info.name);device->setProperty("enabled",holder->deviceManager.isMidiInputDeviceEnabled(info.identifier));devices.add(juce::var(device));
            }
        }
       #endif
        result->setProperty("ok",true);result->setProperty("standalone",standalone);result->setProperty("devices",devices);
    }else if(operation=="setMidiDevice"){
       #if JucePlugin_Build_Standalone
        if(auto* holder=juce::StandalonePluginHolder::getInstance()){
            const auto id=request.getProperty("id",{}),enabled=request.getProperty("enabled",{});if(!id.isString()||!enabled.isBool())return fail("Invalid MIDI device selection");
            bool found=false;for(const auto& device:juce::MidiInput::getAvailableDevices())if(device.identifier==id.toString())found=true;if(!found)return fail("MIDI input disconnected");
            holder->deviceManager.setMidiInputDeviceEnabled(id.toString(),static_cast<bool>(enabled));engine.cancelMidi();result->setProperty("ok",true);return reply;
        }
       #endif
        return fail("Route MIDI inputs through the plugin host");
    }else return fail("Unknown operation");
    return reply;
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AionProcessor(); }
