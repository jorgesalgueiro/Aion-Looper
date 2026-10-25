#pragma once
#include "core/SpscQueue.h"
#include "core/Transport.h"
#include "core/LoopBank.h"
#include "core/Metronome.h"
#include "core/CountIn.h"
#include "core/LiveInput.h"
#include "core/MeterTransfer.h"
#include "core/MidiController.h"
#include <cstdint>
namespace aion {
struct GainCommand { std::uint64_t frame; float gain; };
struct Meter { std::uint64_t frame; float peak; };
struct LoopCommand {
    LoopAction action{};
    std::uint32_t track = 0, requestId = 0, generation = 0;
    std::uint64_t frame = 0;
    double value = 0;
    std::uint32_t stutterRevision = 0;
};
struct WaveformEvent {std::uint32_t generation=0,track=0;std::uint64_t id=0;std::array<float,LoopBank::waveformWidth> peaks{};};
struct ExportRequest {std::uint32_t track=0,generation=0;std::uint64_t waveformId=0;};
struct AudioInspection {LoopView track{};std::uint64_t frame=0;float masterPeak=0;unsigned countInFlash=0;};
struct EngineView {
    std::array<LoopView, LoopBank::trackCount> tracks{};
    std::uint64_t frame = 0;
    double sampleRate = 48000;
    std::uint32_t generation = 0, lastRequestId = 0, freeFrames = 0;
    float inputPeak = 0, outputPeak = 0;
    bool rejected = false, poolFull = false, audioReset = false;
    int nextUndo=-1,nextRedo=-1;
    SyncView sync{};
    bool metronomeFlash=false;
    float masterPeak=0;
    unsigned countInFlash=0;
    MidiView midi{};
    Transport hostTransport{};
};
// Identity validation only: not an analyser reset decision or audible-time gate.
inline bool sameLoopMeterSource(const MeterWindowEvent& event,const EngineView& view) noexcept {
    if(!event.generation||event.generation!=view.generation||event.track>=LoopBank::trackCount)return false;
    const auto& track=view.tracks[event.track];
    return event.clearEpoch==track.clearEpoch&&event.waveformId==track.waveformId&&event.sourceRevision==track.sourceRevision;
}
// Audio owner only for prepare/process/target/transport. The message thread is
// the sole producer of commands; the message thread alone consumes meters.
class Engine {
public:
    void prepare(double sampleRate, std::uint32_t poolFrames = LoopBank::defaultFrames);
    void configureSync(SyncSettings settings) noexcept {loops.configureSync(settings);}
    void setMonitoring(bool enabled) noexcept { monitoring = enabled; }
    void setMetronome(bool enabled,float volume) noexcept {metronome.setEnabled(enabled);metronome.setVolume(volume);}
    void setAutoPlay(bool enabled) noexcept { loops.setAutoPlay(enabled); }
    bool enqueueLoop(LoopCommand command) noexcept;
    bool enqueueExport(ExportRequest request) noexcept; // Message owner, separate from performance FIFO.
    bool popExport(WavExport& output) noexcept;
    void cancelStutter(std::uint32_t track = LoopBank::trackCount) noexcept {
        if(track<LoopBank::trackCount)stutterRevisions[track].fetch_add(1,std::memory_order_release);
        else for(auto& revision:stutterRevisions)revision.fetch_add(1,std::memory_order_release);
    }
    bool finalizationPending() const noexcept { return loops.finalizationPending(); }
    bool popLiveInput(LiveInputEvent& event) noexcept {return liveInput->pop(event);}
    // Message owner only. On-request raw telemetry, not enabled visible metering.
    bool requestLoopMeterWindow(std::uint32_t track) noexcept {return meterTransfer->request(track);}
    bool popLoopMeterWindow(MeterWindowEvent& event) noexcept {return meterTransfer->pop(event,generation());}
    bool popWaveform(WaveformEvent& event) noexcept {return waveforms->pop(event);}
    bool popView(EngineView& view) noexcept { return views->pop(view); }
    std::uint32_t generation() const noexcept { return epoch.load(std::memory_order_acquire); }
    void setTarget(float value) noexcept;
    bool enqueue(GainCommand command) noexcept;
    void process(float* const* channels, int channelCount, int frames,std::span<const MidiEvent> midiEvents={}, bool nonRealtime = false) noexcept;
    bool configureMidi(const MidiSettings& settings) noexcept {return midi->configure(settings);}
    bool popMidiLearn(MidiLearnResult& result) noexcept {return midi->popLearn(result);}
    bool midiLearnCurrent(const MidiLearnResult& result) const noexcept {return midi->learnCurrent(result);}
    void cancelMidi() noexcept {midiCancellation.fetch_add(1,std::memory_order_release);}
    void cancelMidiLearn() noexcept {midiLearnCancellation.fetch_add(1,std::memory_order_release);}
    void midiOverflow() noexcept {midiOverflowRevision.fetch_add(1,std::memory_order_release);}
    void setMidiInputAvailable(bool available) noexcept {midiInputAvailable=available;}
    bool popMeter(Meter& meter) noexcept { return meters.pop(meter); }
    std::uint64_t clock() const noexcept { return frame; }
    // Audio owner only, after process. Passive diagnostics; never call from UI.
    AudioInspection inspectAudioThread(std::uint32_t track=0) const noexcept {
        return {track<LoopBank::trackCount?loops.view()[track]:LoopView{},frame,renderedMasterPeak,countIn.flash(frame)};
    }
    // Raw post-fader/pan loop PCM; audio owner only, no UI/IPC publication yet.
    bool copyLoopMeterWindow(std::uint32_t track,std::span<float,MeterWindow::size> output) const noexcept {return loops.copyMeterWindow(track,output);}
    Transport transport{}; // Audio-owned; never read directly from the editor.
private:
    std::unique_ptr<MidiController> midi=std::make_unique<MidiController>();
    std::atomic<std::uint32_t> midiCancellation{0};
    std::uint32_t seenMidiCancellation=0;
    std::atomic<std::uint32_t> midiLearnCancellation{0};
    std::uint32_t seenMidiLearnCancellation=0;
    std::atomic<std::uint32_t> midiOverflowRevision{0};
    std::uint32_t seenMidiOverflow=0;
    bool midiInputAvailable=true;
    bool applyLoop(LoopAction,std::uint32_t,double) noexcept;
    bool applyMidi(LoopAction,std::uint32_t,double) noexcept;
    MidiController::Dispatch midiDispatch() noexcept {return {loops,frame,preparedRate,generation(),this,[](void* context,LoopAction action,std::uint32_t track,double value){return static_cast<Engine*>(context)->applyMidi(action,track,value);}};}
    LoopBank loops;
    SpscQueue<ExportRequest,2> exportRequests;
    bool exportOutstanding=false; // Message thread only, retained across prepare.
    std::array<std::atomic<std::uint32_t>,LoopBank::trackCount> stutterRevisions{};
    std::array<std::uint32_t,LoopBank::trackCount> seenStutterRevisions{};
    std::unique_ptr<LiveInput> liveInput=std::make_unique<LiveInput>();
    static_assert(LoopBank::trackCount==MeterTransfer::trackCount);
    std::unique_ptr<MeterTransfer> meterTransfer=std::make_unique<MeterTransfer>();
    Metronome metronome;
    CountIn countIn;
    std::uint32_t countInTrack=0;
    SpscQueue<LoopCommand, 256> loopCommands;
    std::unique_ptr<SpscQueue<EngineView,8>> views=std::make_unique<SpscQueue<EngineView,8>>(); // Construction-time storage, never allocate snapshots on audio.
    std::unique_ptr<SpscQueue<WaveformEvent,8>> waveforms=std::make_unique<SpscQueue<WaveformEvent,8>>(); // Allocate off callback, avoid large engine stack frames.
    std::array<std::uint64_t,LoopBank::trackCount> sentWaveforms{};
    unsigned waveformCursor=0;
    std::atomic<std::uint32_t> epoch{0};
    LoopCommand pendingLoop{};
    bool hasPendingLoop = false, monitoring = true, rejected = false, audioReset = false;
    double preparedRate = 0;
    std::uint32_t lastRequestId = 0;
    SpscQueue<GainCommand, 1024> commands;
    SpscQueue<Meter, 8> meters;
    GainCommand pending{};
    bool hasPending = false;
    std::uint64_t frame = 0;
    float gain = 0.0f, target = 0.0f, increment = 0.0f;
    float renderedMasterPeak=0;
    std::uint32_t rampLength = 960, remaining = 0;
};
}
