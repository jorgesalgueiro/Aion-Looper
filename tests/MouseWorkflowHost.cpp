// Hardware-free host for real WebView mouse integration tests. Production
// processor, APVTS, IPC, editor and DSP are linked without modification.
#include "plugin/Processor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <cmath>
#include <thread>
#include <cstdlib>

class MouseWorkflowHost final : public juce::JUCEApplication, private juce::Timer {
public:
    const juce::String getApplicationName() override { return "Aion mouse test host"; }
    const juce::String getApplicationVersion() override { return "1"; }
    void initialise(const juce::String& args) override {
        const bool nativePicker=args.trim().isEmpty()||args.contains("--native-picker");
       #if JUCE_WINDOWS
        // This EXCLUDE_FROM_ALL fixture also supports an ordinary interactive
        // launch for OS-dialog tests. Production binaries never set this port.
        if(nativePicker)_putenv_s("WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS","--remote-debugging-address=127.0.0.1 --remote-debugging-port=9235 --disable-background-timer-throttling --disable-renderer-backgrounding");
       #endif
        blockSize = args.contains("128") ? 128 : 64;
        // Deterministic picker for the hardware-free integration fixture only.
        // All encoding, ownership, file writing and IPC remain production code.
        const auto exportDirectory=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("wav-export-"+juce::String(blockSize));
        (void)exportDirectory.createDirectory();pcmDirectory=exportDirectory;
        (void)pcmDirectory.getChildFile("midi-input.json").deleteFile();(void)pcmDirectory.getChildFile("midi-ack").deleteFile();
        (void)pcmDirectory.getChildFile("navigation-probe-start").deleteFile();(void)pcmDirectory.getChildFile("navigation-probe-stop").deleteFile();(void)pcmDirectory.getChildFile("navigation-evidence.json").deleteFile();
        (void)pcmDirectory.getChildFile("pcm-reset").deleteFile();(void)pcmDirectory.getChildFile("pcm-stop").deleteFile();writePcmEvidence();
        AionProcessor::ExportDestination picker=[this,exportDirectory](const juce::String& filename,std::function<void(juce::File)> done){
            choices.push_back({exportDirectory,filename,std::move(done)});
        };
        processor = std::make_unique<AionProcessor>(nativePicker?AionProcessor::ExportDestination{}:std::move(picker));
        processor->setPlayConfigDetails(2, 2, 48000, blockSize);
        processor->prepareToPlay(48000, blockSize);
        window = std::make_unique<Window>();
        window->setContentOwned(processor->createEditor(), true);
        window->centreWithSize(1200, 800);
        window->setVisible(true);
        // Consume Windows' initial SW_HIDE from a background launcher before
        // showing this interactive fixture. A hidden WebView throttles replies.
        window->setVisible(false);window->setVisible(true);
        running.store(true);
        audio = std::thread([this] {
            juce::AudioBuffer<float> buffer(2, blockSize);
            juce::MidiBuffer midi;
            midi.ensureSize(4096);
            std::uint64_t frame = 0;
            std::uint32_t seenRevision=0;bool heard=false;
            auto deadline = std::chrono::steady_clock::now();
            while (running.load()) {
                for (int i = 0; i < blockSize; ++i, ++frame) {
                    buffer.setSample(0, i, 0.25f * std::sin(float(frame % 48000) * 0.057595865f));
                    buffer.setSample(1, i, 0.17f * std::sin(float(frame % 48000) * 0.079848813f));
                }
                midi.clear();MidiPacket packet{};std::uint32_t midiSequence=0;
                for(unsigned n=0;n<256&&midiPackets.pop(packet);++n){midi.addEvent(packet.bytes.data(),packet.size,packet.offset);midiSequence=packet.sequence;}
                processor->processBlock(buffer, midi);
                if(midiSequence)midiAcknowledged.store(midiSequence,std::memory_order_release);
                pcmClock.store(frame,std::memory_order_release);
                if(navigationActive.load(std::memory_order_acquire)){
                    const auto inspected=processor->inspectAudioThread();float peak=0;
                    for(int c=0;c<2;++c)for(int i=0;i<blockSize;++i)peak=std::max(peak,std::abs(buffer.getSample(c,i)));
                    (void)navigationEvidence.push({inspected.frame,inspected.track.length,inspected.track.waveformId,inspected.track.state,inspected.track.stuttering,peak});
                }
                if(pcmActive.load(std::memory_order_acquire)){
                const auto revision=pcmRevision.load(std::memory_order_acquire);
                if(revision!=seenRevision){seenRevision=revision;heard=false;pcmLastAudible.store(0,std::memory_order_release);(void)pcmEvidence.push({revision});}
                float peak=0;for(int c=0;c<2;++c)for(int i=0;i<blockSize;++i)peak=std::max(peak,std::abs(buffer.getSample(c,i)));
                if(peak>0.01f){
                    pcmLastAudible.store(frame,std::memory_order_release);
                    if(!heard){const auto inspected=processor->inspectAudioThread();
                        if(inspected.track.state==aion::LoopState::armed){heard=true;(void)pcmEvidence.push({revision,true,peak,inspected.masterPeak,inspected.countInFlash,inspected.frame,inspected.track.scheduledFrame});}
                    }
                }
                }
                deadline += std::chrono::nanoseconds(1000000000LL * blockSize / 48000);
                std::this_thread::sleep_until(deadline);
            }
        });
        startTimer(20);
    }
    void shutdown() override {
        stopTimer();choices.clear();
        running.store(false);
        if (audio.joinable()) audio.join();
        window.reset();
        processor->releaseResources();
        processor.reset();
    }
    void systemRequestedQuit() override { quit(); }
private:
    struct MidiPacket {std::array<juce::uint8,3> bytes{};int size=0,offset=0;std::uint32_t sequence=0;};
    aion::SpscQueue<MidiPacket,512> midiPackets;
    std::atomic<std::uint32_t> midiAcknowledged{0};
    std::uint32_t reportedMidi=0;
    // Test-only, fixed-size audio evidence; JSON/file work stays on the message thread.
    struct NavigationEvidence {std::uint64_t frame=0,length=0,waveformId=0;aion::LoopState state{};bool stuttering=false;float peak=0;};
    aion::SpscQueue<NavigationEvidence,8> navigationEvidence;
    std::atomic<bool> navigationActive{false};
    struct PcmEvidence {std::uint32_t epoch=0;bool heard=false;float peak=0,masterPeak=0;unsigned flash=0;std::uint64_t frame=0,deadline=0;};
    aion::SpscQueue<PcmEvidence,8> pcmEvidence; // Audio -> message; reset and first real cue only.
    PcmEvidence latestPcm{};
    std::atomic<std::uint32_t> pcmRevision{0};
    std::atomic<bool> pcmActive{false};
    std::atomic<std::uint64_t> pcmClock{0},pcmLastAudible{0};
    std::uint64_t reportedLastAudible=0;
    juce::File pcmDirectory;
    void writePcmEvidence(){
        auto* data=new juce::DynamicObject();juce::var report(data);
        data->setProperty("epoch",static_cast<juce::int64>(latestPcm.epoch));data->setProperty("heard",latestPcm.heard);
        data->setProperty("outputPeak",latestPcm.peak);data->setProperty("masterPeak",latestPcm.masterPeak);data->setProperty("countInFlash",static_cast<int>(latestPcm.flash));
        data->setProperty("frame",static_cast<juce::int64>(latestPcm.frame));data->setProperty("scheduledFrame",static_cast<juce::int64>(latestPcm.deadline));
        data->setProperty("currentFrame",static_cast<juce::int64>(pcmClock.load(std::memory_order_acquire)));data->setProperty("lastAudibleFrame",static_cast<juce::int64>(pcmLastAudible.load(std::memory_order_acquire)));
        (void)pcmDirectory.getChildFile("pcm-evidence.json").replaceWithText(juce::JSON::toString(report));
    }
    struct Choice {juce::File directory;juce::String filename;std::function<void(juce::File)> done;};
    std::vector<Choice> choices;
    void timerCallback() override {
        const auto startNavigation=pcmDirectory.getChildFile("navigation-probe-start");if(startNavigation.existsAsFile()){(void)startNavigation.deleteFile();navigationActive.store(true,std::memory_order_release);}
        const auto stopNavigation=pcmDirectory.getChildFile("navigation-probe-stop");if(stopNavigation.existsAsFile()){(void)stopNavigation.deleteFile();navigationActive.store(false,std::memory_order_release);}
        NavigationEvidence navigation{},nextNavigation{};bool navigationChanged=false;
        while(navigationEvidence.pop(nextNavigation)){navigation=nextNavigation;navigationChanged=true;}
        if(navigationChanged){auto* data=new juce::DynamicObject();juce::var report(data);
            data->setProperty("frame",static_cast<juce::int64>(navigation.frame));data->setProperty("length",static_cast<juce::int64>(navigation.length));data->setProperty("waveformId",static_cast<juce::int64>(navigation.waveformId));
            data->setProperty("playing",navigation.state==aion::LoopState::playing);data->setProperty("stuttering",navigation.stuttering);data->setProperty("outputPeak",navigation.peak);
            (void)pcmDirectory.getChildFile("navigation-evidence.json").replaceWithText(juce::JSON::toString(report));
        }
        const auto injection=pcmDirectory.getChildFile("midi-input.json");
        if(injection.existsAsFile()){
            const auto data=juce::JSON::parse(injection);(void)injection.deleteFile();
            const auto sequence=static_cast<std::uint32_t>(static_cast<int>(data.getProperty("id",0)));
            const auto messages=data.getProperty("messages",{});
            if(const auto* array=messages.getArray();array&&array->size()<=256)for(const auto& value:*array){
                const auto* message=value.getArray();if(!message||message->size()!=5)continue;
                MidiPacket packet{};packet.sequence=sequence;packet.size=static_cast<int>((*message)[3]);packet.offset=static_cast<int>((*message)[4]);
                if(packet.size<1||packet.size>3||packet.offset<0||packet.offset>=blockSize)continue;
                for(int i=0;i<3;++i)packet.bytes[static_cast<unsigned>(i)]=static_cast<juce::uint8>(static_cast<int>((*message)[i]));
                (void)midiPackets.push(packet);
            }
        }
        const auto acknowledged=midiAcknowledged.load(std::memory_order_acquire);
        if(acknowledged!=reportedMidi){reportedMidi=acknowledged;(void)pcmDirectory.getChildFile("midi-ack").replaceWithText(juce::String(acknowledged));}
        const auto reset=pcmDirectory.getChildFile("pcm-reset");if(reset.existsAsFile()){(void)reset.deleteFile();pcmRevision.fetch_add(1,std::memory_order_release);pcmActive.store(true,std::memory_order_release);}
        const auto stop=pcmDirectory.getChildFile("pcm-stop");if(stop.existsAsFile()){(void)stop.deleteFile();pcmActive.store(false,std::memory_order_release);}
        bool changed=false;PcmEvidence evidence;while(pcmEvidence.pop(evidence)){latestPcm=evidence;changed=true;}
        const auto audible=pcmLastAudible.load(std::memory_order_acquire);if(audible!=reportedLastAudible){reportedLastAudible=audible;changed=true;}if(changed)writePcmEvidence();
        for(std::size_t i=0;i<choices.size();){
            auto& choice=choices[i];const auto marker=choice.directory.getChildFile(choice.filename+".choose");
            if(!marker.existsAsFile()){++i;continue;}
            (void)marker.deleteFile();auto completed=std::move(choice);choices.erase(choices.begin()+static_cast<std::ptrdiff_t>(i));
            completed.done(completed.filename.startsWith("Cancel_")?juce::File{}:(completed.filename.startsWith("Fail_")?completed.directory.getChildFile("missing-directory"):completed.directory).getChildFile(completed.filename));
        }
    }
    struct Window : juce::DocumentWindow {
        Window() : DocumentWindow("Aion mouse test (synthetic audio, no device)", juce::Colours::black, allButtons) { setUsingNativeTitleBar(true); }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    int blockSize = 64;
    std::atomic<bool> running{false};
    std::thread audio;
    std::unique_ptr<AionProcessor> processor;
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(MouseWorkflowHost)
JUCE_MAIN_FUNCTION_DEFINITION
