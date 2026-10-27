#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "core/Engine.h"
#include "core/CallbackAudibleTime.h"
#include "plugin/Vst3ProgramRouting.h"
#include "plugin/LoopMeterClearDispatcher.h"
#include "plugin/ProcessorMessageLifetime.h"
#include <mutex>
static_assert(std::atomic<float>::is_always_lock_free, "Audio parameter loads must be lock-free");
static_assert(std::atomic<bool>::is_always_lock_free, "MIDI input readiness must be lock-free");
namespace aion {class DeviceCallbackBinding;}
struct DevicePlaybackObservation {
    aion::CallbackClockStamp callback{};
    aion::CallbackAudibleTime time{};
    bool operator==(const DevicePlaybackObservation&) const noexcept = default;
};
// Native diagnostics, never an IPC meter level or proof of audible playback.
struct MeterServiceStatus {
    std::uint32_t generation=0;
    std::uint64_t passes=0,examined=0,started=0,cleared=0,applied=0,rejected=0;
    unsigned cleanupRoutes=0,pendingCleanups=0,attachedGraphs=0; // Native ownership diagnostics only.
    std::uint64_t buffers=0,bufferApplied=0;
    std::uint64_t plays=0,playApplied=0;
    bool current=false,timerRunning=false;
};
class AionProcessor final : private aion::ProcessorMessageLifetime, public juce::AudioProcessor, public aion::vst3::OverflowSink, private juce::Timer {
public:
    using ExportDestination = std::function<void(const juce::String&, std::function<void(juce::File)>)>;
    using OutputLatencyReader = std::function<aion::DeviceOutputLatency()>; // Optional controlled prepare-time IO.
    using ProcessorClockReader = double (*)() noexcept; // Immutable native test seam, never IPC.
    explicit AionProcessor(ExportDestination = {},OutputLatencyReader = {},ProcessorClockReader = nullptr,aion::DeviceCallbackBinding* controlledDeviceClockOwner = nullptr);
    ~AionProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Aion-Looper"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    void releaseStutter() noexcept {engine.cancelStutter();}
    void cancelExport(); // Editor close cancels an unchosen destination.
    void cancelMidiLearn(); // MIDI performance continues without an editor.
    void programInputOverflow() noexcept override {vst3ProgramOverflow.store(true,std::memory_order_release);}
    aion::AudioInspection inspectAudioThread(std::uint32_t track=0) const noexcept {return engine.inspectAudioThread(track);}
    aion::CallbackClockStamp inspectCallbackClockAudioThread() const noexcept {return engine.inspectCallbackClockAudioThread();}
    // Message owner only. Reads this processor's coherent view, never a foreign
    // supplied stamp. Reported-latency estimate; not xrun/DAC calibration or IPC.
    bool readDevicePlayback(DevicePlaybackObservation&) noexcept;
    MeterServiceStatus inspectMeterServiceMessageThread() const noexcept;
    juce::var request(const juce::Array<juce::var>&); // Message thread only.
    juce::AudioProcessorValueTreeState parameters;
private:
    OutputLatencyReader outputLatencyReader; // Never invoked in processBlock or IPC.
    ProcessorClockReader processorClockReader=nullptr;
    aion::DeviceCallbackBinding* controlledDeviceClockOwner=nullptr; // Immutable controlled IO seam; never IPC.
    aion::DeviceCallbackBinding* deviceClockOwner=nullptr; // Replaced at prepare; audio lifecycle only.
    std::uint32_t deviceClockEpoch=0;
    std::atomic<std::uint32_t> activeDeviceClockEpoch{0}; // Private lifecycle guard, never telemetry.
    // A completed host prepare, independent of Standalone/device-clock ownership.
    // Zero during prepare and after release; never mutate message-owned meters here.
    std::atomic<std::uint32_t> preparedGeneration{0};
    void invalidateDeviceClock() noexcept;
    std::recursive_mutex midiSettingsMutex; // Non-audio configuration/state serialization only.
    aion::MidiSettings midiSettings;
    std::atomic<bool> vst3ProgramOverflow{false}; // Bounded wrapper input failed; reject its whole next callback.
    std::atomic<bool> midiInputReady{false}; // Device queries stay off processBlock.
    std::array<aion::MidiEvent,256> midiEvents{};
    std::optional<aion::MidiLearnResult> pendingMidiLearn;
    juce::String midiStatus;
    juce::StringArray previousMidiDevices;
    double nextMidiDeviceCheck=0,midiLearnDeadline=0;
    juce::MidiDeviceListConnection midiDeviceChanges;
    bool midiDeviceWatchRequested=false; // Message-thread routing diagnostics, not hardware availability.
    bool updateMidi(aion::MidiSettings);
    void serviceMidi();
    void timerCallback() override;
    ExportDestination exportDestination; // Optional host-supplied file picker, never exposed through IPC.
    std::shared_ptr<int> exportLifetime=std::make_shared<int>(0);
    std::unique_ptr<juce::FileChooser> exportChooser;
    juce::ThreadPool exportWriter{1};
    std::shared_ptr<std::atomic<int>> exportWriteResult=std::make_shared<std::atomic<int>>(0);
    bool exportBusy=false,exportEncoding=false,exportAbandoned=false;
    std::uint32_t exportGeneration=0;
    std::uint64_t exportSequence=0;
    juce::String exportFilename,exportStatus;
    aion::Engine engine{aion::MeterEventScope::acceptedPlayback};
    using MeterOwners=std::array<std::optional<aion::LoopMeterLifecycle>,aion::LoopBank::trackCount>;
    std::unique_ptr<MeterOwners> meterOwners; // Construction heap; owners outlive timers and dispatcher.
    using MeterTimers=std::array<std::optional<aion::LoopMeterCleanupTimer>,aion::LoopBank::trackCount>;
    std::unique_ptr<MeterTimers> meterTimers; // Construction heap; timer objects emplaced by message service.
    std::optional<aion::LoopMeterClearDispatcher> meterDispatcher; // Destroyed before its timer routes.
    MeterServiceStatus meterServiceStatus{};
    bool meterPrepared() const noexcept;
    void serviceMeters() noexcept;
    std::atomic<float>* master = nullptr;
    std::atomic<float>* monitor = nullptr;
    std::atomic<float>* autoPlay = nullptr;
    std::atomic<float> *syncMode=nullptr,*syncBpm=nullptr,*syncBars=nullptr,*syncNum=nullptr,*syncDen=nullptr,*countIn=nullptr,*countInAudio=nullptr;
    std::atomic<float> *metroEnabled=nullptr,*metroVolume=nullptr;
    std::array<aion::WaveformEvent,aion::LoopBank::trackCount> waveformCache{}; // Message thread only.
    aion::LiveInputEvent latestLiveInput{};
    aion::EngineView latest{}; // Message-thread cached, coherent audio snapshot.
    // Session labels belong to the message thread, never the audio callback.
    std::array<juce::String, aion::LoopBank::trackCount> trackNames{};
    std::array<std::uint64_t, aion::LoopBank::trackCount> nameClearEpochs{};
    std::uint32_t nameGeneration = 0;
    std::uint32_t nextRequest = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AionProcessor)
};
