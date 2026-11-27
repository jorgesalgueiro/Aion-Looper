#pragma once
#include "core/SpscQueue.h"
#include "core/Transport.h"
#include "core/OutputLatency.h"
#include "core/LoopBank.h"
#include "core/ArpSettings.h"
#include "core/Metronome.h"
#include "core/CountIn.h"
#include "core/LiveInput.h"
#include "core/MeterTransfer.h"
#include "core/MeterGraphStarts.h"
#include "core/MidiController.h"
#include <cstdint>
namespace aion {
struct GainCommand { std::uint64_t frame; float gain; };
struct ArpMidiReceipt {
    unsigned track=0,parameter=0;double value=0,bpm=120;
    std::uint32_t generation=0;std::uint64_t epoch=0,before=0,revision=0;
};
struct DelayMidiReceipt {unsigned track=0,parameter=0;double value=0;std::uint32_t generation=0;std::uint64_t epoch=0,before=0,revision=0;};
struct ReverbMidiReceipt {unsigned track=0,parameter=0;double value=0;std::uint32_t generation=0;std::uint64_t epoch=0,before=0,revision=0;};
// Shape controls (duration/decay) need off-audio IR preparation; the audio
// callback only publishes the latest intent for the message owner.
struct ReverbMidiShapeRequest {unsigned track=0,parameter=0;double value=0;std::uint32_t generation=0;std::uint64_t epoch=0,revision=0;};
struct Meter { std::uint64_t frame; float peak; };
struct LoopCommand {
    LoopAction action{};
    std::uint32_t track = 0, requestId = 0, generation = 0;
    std::uint64_t frame = 0;
    double value = 0;
    std::uint32_t stutterRevision = 0;
    std::uint32_t meterGraphTicket = 0; // Internal only; never rounded through float value.
    std::uint64_t meterQueueId = 0, meterBufferId = 0; // Conditional raw reset only, never JS numbers/float values.
    EqSettings equalizer{};
    std::uint32_t eqMask=eqAllParameters;
    std::uint64_t eqClearEpoch=0;
    bool eqCheckEpoch=true;
    CompressorSettings compressor{};
    std::uint32_t compressorMask=compressorAllParameters;
    std::uint64_t compressorClearEpoch=0;
    bool compressorCheckEpoch=true;
    DriveBank drives=defaultDriveBank();
    // Six mask bits per family: five original controls, then enable. Partial
    // message-owner edits merge with current audio-owned MIDI controls.
    std::uint32_t driveMask=allDriveFields;
    std::uint64_t driveClearEpoch=0;
    bool driveCheckEpoch=true;
    SignalChain signalChain{};
    std::uint64_t chainClearEpoch=0;
    bool chainCheckEpoch=true;
    ArpRenderSettings arp{};
    std::uint32_t arpMask=arpAllParameters;
    std::uint64_t arpClearEpoch=0,arpSettingsRevision=0;
    bool arpPreset=false; // Internal ordered 22-write gesture; not a product preset ID.
    std::uint64_t arpTransaction=0; // Internal tracked edit identity, never a product ID.
    DelaySettings delay{};
    std::uint64_t delayClearEpoch=0,delaySettingsRevision=0,delayTransaction=0;
    ReverbSettings reverb{};
    std::uint64_t reverbClearEpoch=0,reverbSettingsRevision=0,reverbTransaction=0;
};
struct WaveformEvent {std::uint32_t generation=0,track=0;std::uint64_t id=0;std::array<float,LoopBank::waveformWidth> peaks{};};
struct ExportRequest {std::uint32_t track=0,generation=0;std::uint64_t waveformId=0;};
enum class DelayEditStatus : std::uint8_t {ready,cancelled,stale,busy,failed};
struct DelayEditRequest {
    std::uint32_t track=0,requestId=0,generation=0;
    std::uint64_t clearEpoch=0,settingsRevision=0,transaction=0,cancellation=0;
};
struct DelayEditResult {
    std::uint32_t track=0,requestId=0,generation=0;
    DelayEditStatus status=DelayEditStatus::failed;
    std::uint64_t transaction=0,settingsRevision=0,clearEpoch=0,frame=0;
};
// Internal per-loop B transaction. Live same-shape/disabled edits commit in the
// ordered callback; an enable that needs a new impulse/graph defers its receipt
// until the worker installs it. Product bridge/CLI/GUI/state/MIDI remain gated.
enum class ReverbEditStatus : std::uint8_t {ready,cancelled,stale,busy,failed};
struct ReverbEditRequest {
    std::uint32_t track=0,requestId=0,generation=0;
    std::uint64_t clearEpoch=0,settingsRevision=0,transaction=0,cancellation=0;
};
struct ReverbEditResult {
    std::uint32_t track=0,requestId=0,generation=0;
    ReverbEditStatus status=ReverbEditStatus::failed;
    std::uint64_t transaction=0,settingsRevision=0,clearEpoch=0,frame=0;
};
// Internal coordinated B-only restore bank. One outstanding (including unread
// terminal result), mutually exclusive with per-track B edits. Payload is kept
// out of LoopCommand so ordinary performance FIFO entries do not grow.
struct ReverbBankRequest {
    LoopBank::ReverbBank settings{};LoopBank::ReverbEpochs clearEpochs{},settingsRevisions{};
    std::uint32_t requestId=0,generation=0;
    std::uint64_t frame=0,transaction=0,cancellation=0;
};
struct ReverbBankResult {
    std::uint32_t requestId=0,generation=0;ReverbEditStatus status=ReverbEditStatus::failed;
    std::uint64_t transaction=0,frame=0;
    LoopBank::ReverbEpochs clearEpochs{},settingsRevisions{};
};
// Internal D-only restore bank. One outstanding (including unread terminal
// result), mutually exclusive with per-track D edits. Payload is kept out of
// LoopCommand so ordinary performance FIFO entries do not grow by 20 tracks.
struct DelayBankRequest {
    LoopBank::DelayBank settings{};LoopBank::DelayEpochs clearEpochs{},settingsRevisions{};
    std::uint32_t requestId=0,generation=0;
    std::uint64_t frame=0,transaction=0,cancellation=0;
};
struct DelayBankResult {
    std::uint32_t requestId=0,generation=0;DelayEditStatus status=DelayEditStatus::failed;
    std::uint64_t transaction=0,frame=0;
    LoopBank::DelayEpochs clearEpochs{},settingsRevisions{};
};
// Internal prepared-storage transaction, not a product A toggle/preset/state
// command. Values/seed are explicit DSP inputs; product voicing is still pending.
struct ArpPreparationRequest {
    std::uint32_t track=0,requestId=0,generation=0,seed=0;
    std::uint64_t clearEpoch=0,cancellation=0; // cancellation captured by enqueue
    ArpModValues values=arpModDefaults();
    bool activate=false; // Internal connected-stage install; product UI still disabled.
    std::uint64_t settingsRevision=0;
    bool edit=false,enabled=false,preset=false;
    std::uint32_t mask=arpAllParameters;
    std::uint64_t transaction=0;
    ArpVoicing voicing=ArpVoicing::original;
};
enum class ArpPreparationStatus : std::uint8_t {ready,cancelled,stale,busy,failed};
struct ArpPreparationResult {
    std::uint32_t track=0,requestId=0,generation=0;
    ArpPreparationStatus status=ArpPreparationStatus::failed;
    std::uint64_t transaction=0,settingsRevision=0,clearEpoch=0;
};
// Internal atomic Arp-only bank. One outstanding including unread receipt;
// mutually exclusive with per-track preparations. Not a full session restore.
struct ArpBankRequest {
    LoopBank::ArpBank settings{};
    LoopBank::ArpEpochs clearEpochs{},settingsRevisions{};
    std::uint32_t requestId=0,generation=0;
    std::uint64_t cancellation=0;
    LoopBank::EffectsBank effects{};
    bool restoreEffects=false; // Internal coordinated Q/C/chain/A restore; default preserves Arp-only callers.
    bool tempoOnly=false; // Immediate existing-graph BPM gesture; no allocation or graph replacement.
};
struct ArpBankResult {
    std::uint32_t requestId=0,generation=0;
    ArpPreparationStatus status=ArpPreparationStatus::failed;
    LoopBank::ArpEpochs clearEpochs{},settingsRevisions{}; // Immutable terminal audio ownership.
    std::uint64_t callbackSerial=0; // Zero for stopped prepare cancellation, otherwise exact producing callback.
    LoopBank::ArpEpochs delaySettingsRevisions{};
    LoopBank::ReverbEpochs reverbSettingsRevisions{};
    bool includesDelays=false,includesReverbs=false;
};
struct PreparedArpInspection {bool active=false;ArpModValues targets{};ArpState state{};std::uint64_t frame=0;};
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
    CallbackClockStamp callbackClock{};
};
// Identity validation only: not an analyser reset decision or audible-time gate.
inline bool sameLoopMeterSource(const MeterWindowEvent& event,const EngineView& view) noexcept {
    if(!event.generation||event.generation!=view.generation||event.track>=LoopBank::trackCount)return false;
    const auto& track=view.tracks[event.track];
    return event.clearEpoch==track.clearEpoch&&event.waveformId==track.waveformId&&event.sourceRevision==track.sourceRevision;
}
// Exact callback pairing only; source/graph availability and audible-time
// selection remain separate checks. Offline stamps may match without being a
// live output clock. Never combine a packet with a newer callback's timestamp.
inline bool sameLoopMeterCallback(const MeterWindowEvent& event,const EngineView& view) noexcept {
    const auto& clock=view.callbackClock;
    return validCallbackClock(clock)&&event.generation==view.generation&&clock.generation==view.generation
        &&event.track<LoopBank::trackCount&&event.callbackClock==clock
        &&event.endFrame==clock.endFrame&&view.frame==clock.endFrame
        &&event.sampleRate==clock.sampleRate&&view.sampleRate==clock.sampleRate;
}
// Audio owner only for prepare/process/target/transport. The message thread is
// the sole producer of commands; the message thread alone consumes meters.
class Engine {
public:
    explicit Engine(MeterEventScope scope=MeterEventScope::rawWindows,QueuedStartMode queuedStart=QueuedStartMode::sampleGrid) :loops(scope,queuedStart){}
    void prepare(double sampleRate, std::uint32_t poolFrames = LoopBank::defaultFrames,DeviceOutputLatency outputLatency = {});
    void configureSync(SyncSettings settings) noexcept {loops.configureSync(settings);}
    void setMonitoring(bool enabled) noexcept { monitoring = enabled; }
    void setMetronome(bool enabled,float volume) noexcept {metronome.setEnabled(enabled);metronome.setVolume(volume);}
    void setAutoPlay(bool enabled) noexcept { loops.setAutoPlay(enabled); }
    bool enqueueLoop(LoopCommand command) noexcept;
    // Sole message owner. Compile original controls; one FIFO entry admits the
    // complete preset and consumes one dispatch budget slot, never a prefix.
    // Caller supplies arpDelay identity/epoch/revision/frame and explicit seed.
    bool enqueueArpPreset(LoopCommand,const ArpSettings&,double bpm) noexcept;
    // Sole message producer; shares the preparation receipt/admission bound.
    // Publish an exact receipt descriptor plus one ordered performance command.
    // Failure leaves transaction/output and both queues unchanged.
    bool enqueueArpEdit(LoopCommand,std::uint64_t& transaction) noexcept;
    // Same sole message owner as the performance FIFO. Descriptor + ordered
    // command admission is atomic; one outstanding per track includes unread
    // terminal receipts. Live edits and worker preparations share this result.
    bool enqueueDelayEdit(LoopCommand,std::uint64_t& transaction) noexcept;
    bool popDelayEdit(DelayEditResult&) noexcept;
    void cancelDelayEdit(unsigned track=LoopBank::trackCount) noexcept;
    // Same sole message owner and receipt bound for B. One outstanding edit per
    // track (including unread terminal receipts); never a bare LoopCommand path.
    bool enqueueReverbEdit(LoopCommand,std::uint64_t& transaction) noexcept;
    bool popReverbEdit(ReverbEditResult&) noexcept;
    void cancelReverbEdit(unsigned track=LoopBank::trackCount) noexcept;
    // Same sole message owner for the coordinated B restore bank.
    bool enqueueReverbBank(ReverbBankRequest,std::uint64_t& transaction) noexcept;
    bool popReverbBank(ReverbBankResult&) noexcept;
    void cancelReverbBank() noexcept;
    bool enqueueDelayBank(DelayBankRequest,std::uint64_t& transaction) noexcept;
    bool popDelayBank(DelayBankResult&) noexcept;
    void cancelDelayBank() noexcept;
    bool enqueueEqBank(const std::array<EqSettings,LoopBank::trackCount>&,std::uint32_t firstRequest) noexcept;
    bool enqueueEffectsBank(const std::array<EqSettings,LoopBank::trackCount>&,
                            const std::array<CompressorSettings,LoopBank::trackCount>&,std::uint32_t firstRequest) noexcept;
    bool enqueueEffectsBank(const std::array<EqSettings,LoopBank::trackCount>&,
                            const std::array<CompressorSettings,LoopBank::trackCount>&,
                            const std::array<SignalChain,LoopBank::trackCount>&,std::uint32_t firstRequest) noexcept;
    void prepareChainBank(const std::array<SignalChain,LoopBank::trackCount>& settings) noexcept {
        for(unsigned i=0;i<settings.size();++i)(void)loops.setSignalChain(i,settings[i],0,0,false);
    }
    void prepareEqBank(const std::array<EqSettings,LoopBank::trackCount>& settings) noexcept {
        for(unsigned i=0;i<settings.size();++i)(void)loops.setEqualizer(i,settings[i],eqAllParameters,0,0,false);
    } // Stopped prepare owner only; never a message-thread shortcut into audio.
    void prepareCompressorBank(const std::array<CompressorSettings,LoopBank::trackCount>& settings) noexcept {
        for(unsigned i=0;i<settings.size();++i)(void)loops.setCompressor(i,settings[i],compressorAllParameters,0,0,false);
    }
    void prepareDriveBank(const LoopBank::DriveBanks& settings)noexcept{for(unsigned i=0;i<settings.size();++i)(void)loops.setDrives(i,settings[i],0,0,false);}
    // Single message owner publishes a cumulative source-retirement fence.
    // Independent of the performance FIFO, so cleanup cannot be lost on overflow.
    bool retireQueuedSourceThrough(std::uint32_t requestedGeneration,unsigned track,std::uint32_t ticket) noexcept {
        if(!requestedGeneration||!ticket||track>=LoopBank::trackCount||requestedGeneration!=generation())return false;
        const auto previous=sourceRetirements[track].load(std::memory_order_relaxed);
        if(static_cast<std::uint32_t>(previous>>32)!=requestedGeneration||static_cast<std::uint32_t>(previous)<ticket)
            sourceRetirements[track].store((static_cast<std::uint64_t>(requestedGeneration)<<32)|ticket,std::memory_order_release);
        return requestedGeneration==generation();
    }
    bool enqueueExport(ExportRequest request) noexcept; // Message owner, separate from performance FIFO.
    // Single message producer/consumer. One outstanding transaction per track,
    // INCLUDING unread terminal receipts. No ordinary loop acknowledgement is
    // changed; shared GUI/CLI product integration must retain gesture ordering.
    bool enqueueArpPreparation(ArpPreparationRequest) noexcept;
    bool popArpPreparation(ArpPreparationResult&) noexcept;
    void cancelArpPreparation(unsigned track=LoopBank::trackCount) noexcept;
    // Same sole message producer/consumer as the per-track protocol. An unread
    // receipt excludes either kind of preparation. Commit occurs at beginBlock,
    // before ordinary FIFO commands, not as an ordered product restore command.
    bool enqueueArpBank(ArpBankRequest) noexcept;
    bool popArpBank(ArpBankResult&) noexcept;
    void cancelArpBank() noexcept;
    PreparedArpInspection inspectPreparedArpAudioThread(unsigned track) noexcept;
    bool popExport(WavExport& output) noexcept;
    void cancelStutter(std::uint32_t track = LoopBank::trackCount) noexcept {
        if(track<LoopBank::trackCount)stutterRevisions[track].fetch_add(1,std::memory_order_release);
        else for(auto& revision:stutterRevisions)revision.fetch_add(1,std::memory_order_release);
    }
    bool finalizationPending() const noexcept { return loops.finalizationPending(); }
    bool popLiveInput(LiveInputEvent& event) noexcept {return liveInput->pop(event);}
    // Message owner only. On-request raw telemetry, not enabled visible metering.
    bool requestLoopMeterWindow(std::uint32_t track) noexcept {return meterTransfer->request(track);}
    bool requestLoopMeterWindow(std::uint32_t track,std::uint32_t requestedGeneration,std::uint32_t graphTicket) noexcept {
        return requestedGeneration==generation()&&meterTransfer->request(track,requestedGeneration,graphTicket);
    }
    bool popLoopMeterWindow(MeterWindowEvent& event) noexcept {return meterTransfer->pop(event,generation());}
    // Window and subscribed committed-buffer receipts, not full graph lifecycle.
    // Recheck stream validity before using retained receipts.
    bool popMeterGraphEvent(MeterGraphEvent& event) noexcept {return loops.popMeterGraphEvent(event,generation());}
    bool popMeterGraphEvent(MeterGraphEvent& event,std::uint32_t requestedGeneration,unsigned& budget) noexcept {return loops.popMeterGraphEvent(event,requestedGeneration,budget);}
    bool meterGraphEventsCurrent() const noexcept {return loops.meterGraphEventsCurrent(generation());}
    bool loopMeterGraphCurrent(std::uint32_t requestedGeneration,unsigned track,std::uint32_t ticket) const noexcept {
        return requestedGeneration==generation()&&loops.meterGraphCurrent(requestedGeneration,track,ticket)&&requestedGeneration==generation();
    }
    bool popWaveform(WaveformEvent& event) noexcept {return waveforms->pop(event);}
    bool popView(EngineView& view) noexcept { return views->pop(view); }
    std::uint32_t generation() const noexcept { return epoch.load(std::memory_order_acquire); }
    void setTarget(float value) noexcept;
    bool enqueue(GainCommand command) noexcept;
    void process(float* const* channels, int channelCount, int frames,std::span<const MidiEvent> midiEvents={}, bool nonRealtime = false,ProcessorClockObservation processorEntry = {},DeviceCallbackObservation deviceEntry = {}) noexcept;
    bool configureMidi(const MidiSettings& settings) noexcept {return midi->configure(settings);}
    bool popMidiLearn(MidiLearnResult& result) noexcept {return midi->popLearn(result);}
    bool popArpMidi(ArpMidiReceipt& result) noexcept {return arpMidiResults->pop(result);}
    bool popDelayMidi(DelayMidiReceipt& result) noexcept {return delayMidiResults->pop(result);}
    void delayMidiOwner(std::uint32_t generation) noexcept {delayMidiGeneration.store(generation,std::memory_order_release);}
    bool popReverbMidi(ReverbMidiReceipt& result) noexcept {return reverbMidiResults->pop(result);}
    bool popReverbMidiShape(ReverbMidiShapeRequest& result) noexcept {return reverbMidiShapeRequests->pop(result);}
    void reverbMidiOwner(std::uint32_t generation) noexcept {reverbMidiGeneration.store(generation,std::memory_order_release);}
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
    CallbackClockStamp inspectCallbackClockAudioThread() const noexcept {return callbackClock;}
private:
    CallbackClockStamp callbackClock{}; // Last completed callback; audio owner only.
    std::uint64_t activeCallbackSerial=0; // Current processing scope, zero outside process.
    DeviceOutputLatency outputLatency{}; // Validated and replaced at prepare only.
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
    MidiController::Dispatch midiDispatch() noexcept {return {loops,frame,preparedRate,generation(),this,[](void* context,LoopAction action,std::uint32_t track,double value){return static_cast<Engine*>(context)->applyMidi(action,track,value);},[](void* context,unsigned track,unsigned p,unsigned cc){return static_cast<Engine*>(context)->applyArpMidi(track,p,cc);},[](void* context,unsigned track,unsigned p,unsigned cc){return static_cast<Engine*>(context)->applyDelayMidi(track,p,cc);},[](void* context,unsigned track,unsigned p,unsigned cc){return static_cast<Engine*>(context)->applyReverbMidi(track,p,cc);}};}
    bool applyDelayMidi(unsigned,unsigned,unsigned) noexcept;
    std::atomic<std::uint32_t> delayMidiGeneration{0};
    std::unique_ptr<SpscQueue<DelayMidiReceipt,512>> delayMidiResults=std::make_unique<SpscQueue<DelayMidiReceipt,512>>();
    bool applyReverbMidi(unsigned,unsigned,unsigned) noexcept;
    std::atomic<std::uint32_t> reverbMidiGeneration{0};
    std::unique_ptr<SpscQueue<ReverbMidiReceipt,512>> reverbMidiResults=std::make_unique<SpscQueue<ReverbMidiReceipt,512>>();
    std::unique_ptr<SpscQueue<ReverbMidiShapeRequest,64>> reverbMidiShapeRequests=std::make_unique<SpscQueue<ReverbMidiShapeRequest,64>>();
    bool applyArpMidi(unsigned,unsigned,unsigned) noexcept;
    std::unique_ptr<SpscQueue<ArpMidiReceipt,512>> arpMidiResults=std::make_unique<SpscQueue<ArpMidiReceipt,512>>();
    LoopBank loops;
    struct PendingDelayEdit {
        DelayEditRequest request{};bool active=false,preparing=false,finished=false;
        std::uint64_t installation=0;DelayEditResult completion{};
    };
    std::unique_ptr<std::array<PendingDelayEdit,LoopBank::trackCount>> pendingDelay=std::make_unique<std::array<PendingDelayEdit,LoopBank::trackCount>>();
    std::array<bool,LoopBank::trackCount> outstandingDelay{}; // sole message owner
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    std::array<std::atomic<std::uint64_t>,LoopBank::trackCount> delayCancellations{};
    std::uint64_t nextDelayTransaction=0,seenDelayTransaction=0; // message / audio owners, never reset
    std::unique_ptr<SpscQueue<DelayEditRequest,32>> delayRequests=std::make_unique<SpscQueue<DelayEditRequest,32>>();
    std::unique_ptr<SpscQueue<DelayEditResult,32>> delayResults=std::make_unique<SpscQueue<DelayEditResult,32>>();
    void finishDelayEdit(unsigned,DelayEditStatus) noexcept;
    void cancelStaleDelayEdits() noexcept;
    void serviceDelayEdits() noexcept;
    void cancelAllDelayEdits() noexcept;
    bool currentDelayEdit(const LoopCommand&) const noexcept;
    void applyDelayEdit(const LoopCommand&) noexcept;
    struct PendingDelayBank {
        DelayBankRequest request{};DelayBankResult completion{};
        bool active=false,started=false,finished=false;
    };
    std::unique_ptr<PendingDelayBank> pendingDelayBank=std::make_unique<PendingDelayBank>();
    std::unique_ptr<SpscQueue<DelayBankRequest,2>> delayBankRequests=std::make_unique<SpscQueue<DelayBankRequest,2>>();
    std::unique_ptr<SpscQueue<DelayBankResult,2>> delayBankResults=std::make_unique<SpscQueue<DelayBankResult,2>>();
    bool outstandingDelayBank=false; // message owner; retained across prepare
    std::atomic<std::uint64_t> delayBankCancellation{0};
    void finishDelayBank(DelayEditStatus) noexcept;
    void cancelStaleDelayBank() noexcept;
    void serviceDelayBank() noexcept;
    void cancelQueuedDelayBank() noexcept;
    bool currentDelayCommand(const LoopCommand&) const noexcept;
    void applyDelayBank(const LoopCommand&) noexcept;
    struct PendingReverbEdit {
        ReverbEditRequest request{};bool active=false,preparing=false,finished=false;
        std::uint64_t installation=0;ReverbEditResult completion{};
    };
    std::unique_ptr<std::array<PendingReverbEdit,LoopBank::trackCount>> pendingReverb=std::make_unique<std::array<PendingReverbEdit,LoopBank::trackCount>>();
    std::array<bool,LoopBank::trackCount> outstandingReverb{}; // sole message owner
    std::array<std::atomic<std::uint64_t>,LoopBank::trackCount> reverbCancellations{};
    std::uint64_t nextReverbTransaction=0,seenReverbTransaction=0; // message / audio owners, never reset
    std::unique_ptr<SpscQueue<ReverbEditRequest,32>> reverbRequests=std::make_unique<SpscQueue<ReverbEditRequest,32>>();
    std::unique_ptr<SpscQueue<ReverbEditResult,32>> reverbResults=std::make_unique<SpscQueue<ReverbEditResult,32>>();
    void finishReverbEdit(unsigned,ReverbEditStatus) noexcept;
    void cancelStaleReverbEdits() noexcept;
    void serviceReverbEdits() noexcept;
    void cancelAllReverbEdits() noexcept;
    bool currentReverbEdit(const LoopCommand&) const noexcept;
    void applyReverbEdit(const LoopCommand&) noexcept;
    struct PendingReverbBank {
        ReverbBankRequest request{};ReverbBankResult completion{};
        bool active=false,started=false,finished=false;
    };
    std::unique_ptr<PendingReverbBank> pendingReverbBank=std::make_unique<PendingReverbBank>();
    std::unique_ptr<SpscQueue<ReverbBankRequest,2>> reverbBankRequests=std::make_unique<SpscQueue<ReverbBankRequest,2>>();
    std::unique_ptr<SpscQueue<ReverbBankResult,2>> reverbBankResults=std::make_unique<SpscQueue<ReverbBankResult,2>>();
    bool outstandingReverbBank=false; // message owner; retained across prepare
    std::atomic<std::uint64_t> reverbBankCancellation{0};
    void finishReverbBank(ReverbEditStatus) noexcept;
    void cancelStaleReverbBank() noexcept;
    void serviceReverbBank() noexcept;
    void cancelQueuedReverbBank() noexcept;
    bool currentReverbCommand(const LoopCommand&) const noexcept;
    void applyReverbBank(const LoopCommand&) noexcept;
    struct PendingArpPreparation {
        ArpPreparationRequest request{};
        bool active=false,started=false,finished=false;
        ArpPreparationStatus status=ArpPreparationStatus::failed;
        std::uint64_t installation=0;
        ArpPreparationResult completion{}; // immutable once terminal, even if publication must retry
    };
    void cancelStaleArpPreparations() noexcept;
    void serviceArpPreparations() noexcept;
    void finishArpPreparation(unsigned,ArpPreparationStatus) noexcept;
    void cancelAllArpPreparations() noexcept; // stopped prepare owner
    bool currentArpEdit(const LoopCommand&) const noexcept;
    bool applyArpEdit(const LoopCommand&) noexcept;
    std::array<PendingArpPreparation,LoopBank::trackCount> pendingArp{}; // audio only
    std::array<bool,LoopBank::trackCount> outstandingArp{}; // message only
    std::uint64_t nextArpTransaction=0; // message only, never reset/reused at prepare
    std::uint64_t seenArpTransaction=0; // audio only; distinguishes unregistered vs obsolete FIFO entries
    std::array<std::atomic<std::uint64_t>,LoopBank::trackCount> arpCancellations{};
    std::unique_ptr<SpscQueue<ArpPreparationRequest,32>> arpRequests=std::make_unique<SpscQueue<ArpPreparationRequest,32>>();
    std::unique_ptr<SpscQueue<ArpPreparationResult,32>> arpResults=std::make_unique<SpscQueue<ArpPreparationResult,32>>();
    struct PendingArpBank {
        ArpBankRequest request{};bool active=false,started=false,finished=false;
        ArpPreparationStatus status=ArpPreparationStatus::failed;
        ArpBankResult completion{};
    };
    PendingArpBank pendingArpBank{}; // audio only
    bool outstandingArpBank=false; // sole message owner, retained across prepare
    static_assert(std::atomic<bool>::is_always_lock_free);
    std::atomic<bool> coordinatedDelayBank{false}; // Includes unread coordinated receipts; cancellation also runs on audio.
    std::atomic<bool> coordinatedReverbBank{false}; // Same lifetime rule for the opt-in joint B stage.
    std::atomic<std::uint64_t> arpBankCancellation{0};
    std::unique_ptr<SpscQueue<ArpBankRequest,2>> arpBankRequests=std::make_unique<SpscQueue<ArpBankRequest,2>>();
    std::unique_ptr<SpscQueue<ArpBankResult,2>> arpBankResults=std::make_unique<SpscQueue<ArpBankResult,2>>();
    void finishArpBank(ArpPreparationStatus) noexcept;
    void cancelStaleArpBank() noexcept;
    void serviceArpBank() noexcept;
    void cancelQueuedArpBank() noexcept; // stopped owner
    SpscQueue<ExportRequest,2> exportRequests;
    bool exportOutstanding=false; // Message thread only, retained across prepare.
    std::array<std::atomic<std::uint32_t>,LoopBank::trackCount> stutterRevisions{};
    std::array<std::uint32_t,LoopBank::trackCount> seenStutterRevisions{};
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    std::array<std::atomic<std::uint64_t>,LoopBank::trackCount> sourceRetirements{};
    std::unique_ptr<LiveInput> liveInput=std::make_unique<LiveInput>();
    static_assert(LoopBank::trackCount==MeterTransfer::trackCount);
    std::unique_ptr<MeterTransfer> meterTransfer=std::make_unique<MeterTransfer>();
    std::unique_ptr<MeterGraphStarts> meterGraphStarts=std::make_unique<MeterGraphStarts>();
    Metronome metronome;
    CountIn countIn;
    std::uint32_t countInTrack=0;
    std::unique_ptr<SpscQueue<LoopCommand,256>> loopCommands=std::make_unique<SpscQueue<LoopCommand,256>>();
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
