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
AionProcessor::AionProcessor()
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
    metroEnabled=parameters.getRawParameterValue("metronomeEnabled");metroVolume=parameters.getRawParameterValue("metronomeVolume");
    master = parameters.getRawParameterValue("masterGain");
    monitor = parameters.getRawParameterValue("inputMonitor");
    autoPlay = parameters.getRawParameterValue("autoPlayAfterRecord");
    syncMode=parameters.getRawParameterValue("syncMode");syncBpm=parameters.getRawParameterValue("syncBpm");
    syncBars=parameters.getRawParameterValue("syncBars");syncNum=parameters.getRawParameterValue("syncNum");
    syncDen=parameters.getRawParameterValue("syncDen");countIn=parameters.getRawParameterValue("countInVisual");countInAudio=parameters.getRawParameterValue("countInAudio");
}
void AionProcessor::prepareToPlay(double rate, int) { engine.prepare(rate); }
bool AionProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
    return b.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (b.getMainInputChannelSet().isDisabled() || b.getMainInputChannelSet() == juce::AudioChannelSet::mono()
            || b.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
}
void AionProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) {
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
    engine.process(audio.getArrayOfWritePointers(), audio.getNumChannels(), audio.getNumSamples());
}
juce::AudioProcessorEditor* AionProcessor::createEditor() { return new AionWebEditor(*this); }
void AionProcessor::getStateInformation(juce::MemoryBlock& data) {
    if (auto xml = parameters.copyState().createXml()) copyXmlToBinary(*xml, data);
}
void AionProcessor::setStateInformation(const void* data, int size) {
    if (size <= 0 || size > 1024 * 1024) return;
    if (auto xml = getXmlFromBinary(data, size); xml && xml->hasTagName("AionState")) parameters.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::var AionProcessor::request(const juce::Array<juce::var>& args) {
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    auto* result = new juce::DynamicObject(); juce::var reply(result);
    result->setProperty("protocol", 1); result->setProperty("ok", false);
    const auto fail = [result, &reply](const char* reason) { result->setProperty("error", reason); return reply; };
    if (args.size() != 1 || args[0].getDynamicObject() == nullptr) return fail("Expected one request object");
    const auto& request = args[0];
    const auto operation = request.getProperty("operation", {}).toString();
    // Consume acknowledged clears before reading/editing labels. Only fixed-size
    // epochs cross from audio; string allocation and destruction stay here.
    aion::EngineView view{};
    while (engine.popView(view)) latest = view;
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
        result->setProperty("nextUndo",latest.nextUndo);result->setProperty("nextRedo",latest.nextRedo);
        const char* names[] = {"empty", "recording", "playing", "stopped", "overdubbing", "substituting", "finalizing", "multiplying", "armed", "queued", "stopping"};
        const auto& timing=latest.sync;
        result->setProperty("syncMode",static_cast<int>(timing.settings.mode));result->setProperty("syncBpm",timing.settings.bpm);
        result->setProperty("syncBars",static_cast<int>(timing.settings.bars));result->setProperty("syncNum",static_cast<int>(timing.settings.numerator));
        result->setProperty("syncDen",static_cast<int>(timing.settings.denominator));result->setProperty("countInVisual",timing.settings.countIn);
        result->setProperty("countInAudio",timing.settings.countInAudio);result->setProperty("countInFlash",static_cast<int>(latest.countInFlash));
        result->setProperty("gridLength",timing.loopSeconds);result->setProperty("barLength",timing.barSeconds);
        result->setProperty("gridPosition",timing.position);result->setProperty("syncFrame",static_cast<juce::int64>(timing.frame));
        juce::Array<juce::var> tracks;
        for (const auto& t : latest.tracks) {
            auto* object = new juce::DynamicObject();
            object->setProperty("state", names[static_cast<unsigned>(t.state)]);
            object->setProperty("name", trackNames[static_cast<std::size_t>(&t-latest.tracks.data())]);
            object->setProperty("length", static_cast<int>(t.length)); object->setProperty("position", static_cast<int>(t.position));
            object->setProperty("undoCount",static_cast<int>(t.undoCount));object->setProperty("redoCount",static_cast<int>(t.redoCount));
            object->setProperty("reverse", t.reverse);
            object->setProperty("startDelay",t.startDelay);object->setProperty("phaseApplyFrame",static_cast<juce::int64>(t.phaseApplyFrame));
            object->setProperty("scheduledFrame",static_cast<juce::int64>(t.scheduledFrame));object->setProperty("stopPending",t.stopPending);
            object->setProperty("effectiveRate",t.effectiveRate);object->setProperty("dynamicRecorded",t.dynamicRecorded);
            object->setProperty("pan", t.pan); object->setProperty("playbackRate", t.playbackRate);
            object->setProperty("volume", t.volume); object->setProperty("feedback", t.feedback);
            object->setProperty("finalizationFailed", t.finalizationFailed); object->setProperty("solo", t.solo); object->setProperty("muted", t.muted); object->setProperty("peak", t.peak);
            tracks.add(juce::var(object));
        }
        result->setProperty("tracks", juce::var(tracks));
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
        const char* names[] = {"record", "finish", "play", "stop", "overdub", "substitute", "clear", "stopAll", "volume", "feedback", "mute", "clearAll", "solo", "retrigger", "pan", "speed", "normalize", "reverse", "undo", "redo", "undoGlobal", "redoGlobal", "multiply", "stopImmediate", "startDelay", "applyStart"};
        unsigned action = 0;
        for (; action < std::size(names) && actionName != names[action]; ++action) {}
        if (action == std::size(names)) return fail("Unknown looper action");
        const auto value = request.getProperty("value", 0);
        if (!(value.isDouble() || value.isInt())) return fail("Expected numeric action value");
        const double number = static_cast<double>(value);
        if (!std::isfinite(number) || number < (actionName == "speed" ? 0.25 : 0.0) || number > (actionName == "pan" ? 10 : (actionName == "volume" || actionName == "speed" ? 2 : 1))
            || ((actionName == "mute" || actionName == "reverse") && number != 0 && number != 1)) return fail("Action value outside range");
        if(actionName=="record"&&!inputAvailable(*this))return fail("No native input: enable an input device and Unmute Input in Options, or route audio from the host.");
        const auto requestId = nextRequest + 1;
        if (!engine.enqueueLoop({static_cast<aion::LoopAction>(action), static_cast<std::uint32_t>(static_cast<int>(id)), requestId,
                                static_cast<std::uint32_t>(static_cast<int>(generation)), 0, actionName=="multiply"?(inputAvailable(*this)?1.0f:0.0f):actionName=="startDelay"?number:static_cast<double>(static_cast<float>(number))}))
            return fail("Audio generation changed or command queue is full");
        nextRequest = requestId;
        result->setProperty("ok", true); result->setProperty("queued", true);
        result->setProperty("requestId", static_cast<juce::int64>(requestId));
    } else return fail("Unknown operation");
    return reply;
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AionProcessor(); }
