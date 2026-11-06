#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <vector>
#include <atomic>
#include <memory>
#include <thread>
#include "core/SpscQueue.h"
#include "core/SyncClock.h"
#include "core/Wav.h"
#include "core/MeterWindow.h"
#include "core/MeterGraphEvents.h"

namespace aion {
// Application queues wait for explicit source creation; direct DSP clients may schedule at the grid.
enum class QueuedStartMode { sampleGrid, messageCallback };
enum class LoopState : std::uint8_t { empty, recording, playing, stopped, overdubbing, substituting, finalizing, multiplying, armed, queued, stopping };
enum class LoopAction : std::uint8_t { record, finish, play, stop, overdub, substitute, clear, stopAll, volume, feedback, mute, clearAll, solo, retrigger, pan, speed, normalize, reverse, undo, redo, undoGlobal, redoGlobal, multiply, stopImmediate, startDelay, applyStart, quantizeStart, nudgeStart, stutterStart, stutterStop, merge, clearMergedSource, sourceRestart, meterGraphStart, meterQueuedWindowStart, meterQueuedWindowRequest, meterQueuedGraphRequest }; // Source/reset primitives are core-only, never exposed by IPC.
constexpr bool isMeterCommand(LoopAction action) noexcept {return action==LoopAction::meterGraphStart||action==LoopAction::meterQueuedWindowStart||action==LoopAction::meterQueuedWindowRequest||action==LoopAction::meterQueuedGraphRequest;}
struct Stereo { float left = 0, right = 0; };
struct LoopView {
    LoopState state = LoopState::empty;
    std::uint32_t length = 0, position = 0, undoCount = 0, redoCount = 0;
    std::uint64_t scheduledFrame = 0, clearEpoch = 0;
    std::uint64_t waveformId = 0, captureStartFrame = 0;
    std::uint64_t sourceRevision = 0; // Native source transitions, not analyser identity.
    std::uint64_t mergeSerial = 0, mergeSourceWaveform = 0, mergeSourceClearEpoch = 0;
    std::uint32_t mergeSource = 20;
    double originalBpm = 0, startDelay = 0;
    bool stuttering = false, stutterClock = false;
    double clockProgress = 0;
    double stutterStart = 0, stutterEnd = 0; // Source region in frames.
    std::uint64_t phaseApplyFrame = 0;
    float effectiveRate = 1;
    bool dynamicRecorded = false, stopPending = false;
    bool queuedSourceReady = false; // Transient audio-owned readiness, never persisted.
    float volume = 1, feedback = 0.8f, peak = 0, pan = 5, playbackRate = 1;
    bool muted = false, solo = false, finalizationFailed = false, reverse = false;
};
// Audio owns capture blocks/state; the worker borrows pinned takes and owns immutable clips.
// Capture storage is allocated in prepare; clip allocation/deletion stays off the callback.
class LoopBank {
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
public:
    static constexpr std::uint32_t trackCount = 20, blockFrames = 4096, maxBlocks = 2048;
    static_assert(trackCount==MeterGraphEvents::trackCount);
    static constexpr std::uint32_t defaultFrames = blockFrames * maxBlocks; // 64 MiB stereo
    explicit LoopBank(MeterEventScope scope=MeterEventScope::rawWindows,QueuedStartMode startMode=QueuedStartMode::sampleGrid) :meterScope(scope),queuedStartMode(startMode){}
    ~LoopBank();
    LoopBank(const LoopBank&) = delete;
    LoopBank& operator=(const LoopBank&) = delete;
    void prepare(std::uint32_t frames = defaultFrames, std::uint32_t fadeFrames = 240, double sampleRate = 48000);
    bool finalizationPending() const noexcept { return jobState.load(std::memory_order_acquire) != 0; }
    bool apply(LoopAction, std::uint32_t track, float value = 0) noexcept;
    bool setStartDelay(std::uint32_t track,double value) noexcept;
    bool clearMergedSource(std::uint32_t track,double serial) noexcept;
    Stereo tick(Stereo input, bool nonRealtime = false) noexcept;
    // Offline render owner only: never call on a live audio callback.
    void completeOfflineEdits() noexcept;
    static constexpr unsigned waveformWidth=720;
    bool copyWaveform(std::uint32_t,std::uint64_t,std::array<float,waveformWidth>&) const noexcept;
    // Audio pins one immutable clip; worker encodes it; message thread takes bytes.
    // One outstanding request, including failures, until popExport consumes it.
    bool requestExport(std::uint32_t track,std::uint64_t waveformId) noexcept;
    bool popExport(WavExport&) noexcept;
    void beginBlock() noexcept;
    void setAutoPlay(bool enabled) noexcept { autoPlay = enabled; }
    std::array<LoopView, trackCount> view() const noexcept;
    // Audio owner only, at a callback boundary. Never read this storage from IPC.
    bool copyMeterWindow(std::uint32_t id,std::span<float,MeterWindow::size> output) const noexcept {
        if(id>=trackCount)return false;
        (*meterWindows)[id].copy(output);return true;
    }
    // Explicit audio-owner notification. Increasing tickets prevent repeats
    // from erasing healthy history; source revisions never choose this identity.
    bool startMeterGraph(std::uint32_t id,std::uint32_t ticket) noexcept {
        if(id>=trackCount||!ticket||ticket<=meterGraphHighWater[id]||!tracks[id].clip)return false;
        meterGraphHighWater[id]=meterGraphTickets[id]=ticket;meterGraphFrames[id]=frame;
        meterImmediateTickets[id]=0; // A raw component reset does not prove a playback graph.
        meterPanicKnown[id]=false;
        meterQueuedIds[id]=meterQueuedBufferIds[id]=0;
        (*meterWindows)[id].reset();
        meterGraphEvents->publish(MeterGraphEventKind::started,id,ticket,frame,tracks[id].clearEpoch);return true;
    }
    // Audio-owned validation for a queued message callback. This only resets a
    // raw window; it neither creates a message analyser nor starts audio early.
    // A successfully enqueued request is not an acceptance receipt.
    bool startQueuedMeterWindow(std::uint32_t id,std::uint32_t ticket,std::uint64_t queueId,std::uint64_t bufferId) noexcept {
        if(meterScope!=MeterEventScope::acceptedPlayback||id>=trackCount||!ticket||!queueId||!bufferId
            ||ticket<=meterGraphHighWater[id]||queueId!=meterQueuedIds[id]||queueId==meterQueuedWindowIds[id]
            ||!tracks[id].clip||meterQueuedBufferIds[id]!=bufferId
            ||(!waitingQueuedSource(tracks[id])&&tracks[id].info.state!=LoopState::playing
                &&!(tracks[id].info.state==LoopState::finalizing&&job.normalize&&job.track==id&&tracks[id].finalPlay)))return false;
        meterQueuedWindowIds[id]=queueId;
        meterGraphHighWater[id]=meterGraphTickets[id]=ticket;meterGraphFrames[id]=frame;
        meterImmediateTickets[id]=0;meterPanicKnown[id]=false;
        (*meterWindows)[id].reset();
        meterGraphEvents->publish(MeterGraphEventKind::started,id,ticket,frame,tracks[id].clearEpoch,bufferId,0,queueId);
        return true;
    }
    void reportQueuedMeterWindow(bool accepted,std::uint32_t id,std::uint32_t ticket,std::uint64_t queueId,std::uint64_t bufferId) noexcept {
        if(id<trackCount)meterGraphEvents->publish(accepted?MeterGraphEventKind::queuedWindowAccepted:MeterGraphEventKind::queuedWindowRejected,id,ticket,frame,tracks[id].clearEpoch,bufferId,0,queueId);
    }
    // A pre-birth MULT worker owns one bounded deferred graph request. The
    // request stays pending until PCM commits; cancellation is revalidated then.
    void submitQueuedMeterGraph(std::uint32_t id,std::uint32_t ticket,std::uint64_t queueId,std::uint64_t bufferId,std::uint32_t generation) noexcept;
    bool startQueuedMeterGraph(std::uint32_t id,std::uint32_t ticket,std::uint64_t queueId,std::uint64_t bufferId) noexcept {
        if(id<trackCount&&tracks[id].info.state==LoopState::finalizing&&tracks[id].multiplyQueued)return false;
        if(!startQueuedMeterWindow(id,ticket,queueId,bufferId))return false;
        // This explicit callback origin, unlike a raw reset, owns later stops.
        meterImmediateTickets[id]=ticket;meterPanicKnown[id]=true;meterQueuedIds[id]=meterQueuedBufferIds[id]=0;
        tracks[id].queuedSourceTicket=ticket;
        if(queuedStartMode==QueuedStartMode::messageCallback&&ticket<=retiredSourceTickets[id])cutQueuedSource(tracks[id]);
        else admitQueuedSource(tracks[id]);
        return true;
    }
    void retireQueuedSourceThrough(unsigned id,std::uint32_t ticket) noexcept {
        if(queuedStartMode!=QueuedStartMode::messageCallback||id>=trackCount||!ticket)return;
        retiredSourceTickets[id]=std::max(retiredSourceTickets[id],ticket);
        auto& track=tracks[id];
        if(track.queuedSourceTicket&&track.queuedSourceTicket<=retiredSourceTickets[id])cutQueuedSource(track);
    }
    void reportQueuedMeterGraph(bool accepted,std::uint32_t id,std::uint32_t ticket,std::uint64_t queueId,std::uint64_t bufferId) noexcept {
        if(id<trackCount)meterGraphEvents->publish(accepted?MeterGraphEventKind::queuedGraphAccepted:MeterGraphEventKind::queuedGraphRejected,id,ticket,frame,tracks[id].clearEpoch,bufferId,0,queueId);
    }
    // Prepare barrier/audio producer; message owner consumes the same stream.
    void resetMeterGraphEvents(std::uint32_t generation) noexcept {
        meterGraphEvents->reset(generation);
        for(unsigned id=0;id<trackCount;++id)publishMeterBuffer(id); // Explicit retained-clip prepare handoff.
    }
    bool popMeterGraphEvent(MeterGraphEvent& event,std::uint32_t generation) noexcept {return meterGraphEvents->pop(event,generation);}
    bool popMeterGraphEvent(MeterGraphEvent& event,std::uint32_t generation,unsigned& budget) noexcept {return meterGraphEvents->pop(event,generation,budget);}
    bool meterGraphEventsCurrent(std::uint32_t generation) const noexcept {return meterGraphEvents->current(generation);}
    bool meterGraphCurrent(std::uint32_t generation,unsigned id,std::uint32_t ticket) const noexcept {return meterGraphEvents->owns(generation,id,ticket);}
    std::uint32_t meterGraphTicket(unsigned id) const noexcept {return id<trackCount?meterGraphTickets[id]:0;}
    std::uint64_t meterGraphFrame(unsigned id) const noexcept {return id<trackCount?meterGraphFrames[id]:0;}
    LoopView trackView(std::uint32_t id) const noexcept {if(id>=trackCount)return {};auto v=tracks[id].info;v.clearEpoch=tracks[id].clearEpoch;v.sourceRevision=tracks[id].sourceRevision;v.waveformId=tracks[id].clip?tracks[id].clip->waveformId:0;return v;}
    std::uint32_t capacity() const noexcept { return totalBlocks * blockFrames; }
    std::uint32_t available() const noexcept;
    int nextUndo() const noexcept { return undoSize?undoOrder[undoSize-1]:-1; }
    int nextRedo() const noexcept { return redoSize?redoOrder[redoSize-1]:-1; }
    bool exhausted() const noexcept { return poolFull; }
    void configureSync(SyncSettings settings) noexcept;
    void resetTimeline() noexcept { deferredMultiplyGraph={};meterCallbackClock={};retiredSourceTickets.fill(0);for(auto& window:*meterWindows)window.reset();meterGraphTickets.fill(0);meterGraphHighWater.fill(0);meterImmediateTickets.fill(0);meterPanicKnown.fill(false);meterQueuedIds.fill(0);meterQueuedBufferIds.fill(0);meterQueuedWindowIds.fill(0);meterQueueHighWater=0;meterGraphFrames.fill(0);frame=0;sync.reset(rate);while(phaseRead!=phaseWrite){retire(phaseEvents[phaseRead].reverseClip);retire(phaseEvents[phaseRead].forwardClip);phaseRead=(phaseRead+1)%phaseCapacity;}phaseRead=phaseWrite=0;for(auto& track:tracks){clearRetained(track);track.queuedSourceTicket=0;track.queuedGainAt=0;++track.phaseEpoch;++track.sourceRevision;track.info.phaseApplyFrame=0;} }
    SyncView syncView() const noexcept {return sync.view(frame);}
    void setMeterCallbackClock(const CallbackClockStamp& callback) noexcept {meterCallbackClock=callback;}
    double armedStartSeconds(std::uint32_t id) const noexcept {return id<trackCount&&tracks[id].info.state==LoopState::armed?tracks[id].recordStartSeconds:-1;}
private:
    const MeterEventScope meterScope;
    const QueuedStartMode queuedStartMode;
    CallbackClockStamp meterCallbackClock{}; // Audio owner, supplied once before dispatch in this callback.
    void publishMeterBuffer(unsigned id,bool preserveQueue=false) noexcept {
        if(meterScope!=MeterEventScope::rawWindows&&tracks[id].clip)
            meterGraphEvents->publish(MeterGraphEventKind::bufferInstalled,id,0,frame,tracks[id].clearEpoch,tracks[id].clip->waveformId,0,preserveQueue?meterQueuedIds[id]:0);
    }
    // Only successful immediate Loop.play origins call this. A raw-window reset,
    // buffer install, healthy retrigger or queued grid transition is not a birth.
    void publishImmediateMeterPlay(unsigned id) noexcept {
        tracks[id].queuedSourceTicket=0;
        if(meterScope!=MeterEventScope::acceptedPlayback)return;
        if(meterGraphHighWater[id]==std::numeric_limits<std::uint32_t>::max()){meterGraphEvents->invalidate();return;}
        const auto ticket=++meterGraphHighWater[id];
        meterImmediateTickets[id]=ticket;
        meterPanicKnown[id]=true;
        meterQueuedIds[id]=meterQueuedBufferIds[id]=0;
        meterGraphTickets[id]=ticket;meterGraphFrames[id]=frame;(*meterWindows)[id].reset();
        meterGraphEvents->publish(MeterGraphEventKind::immediatePlay,id,ticket,frame,tracks[id].clearEpoch,tracks[id].clip->waveformId);
    }
    // Explicit immediate stop requests only. Retain the originating ticket after
    // audio stops so repeated taps can replace a still-pending message timer.
    // A detached message owner accepts later taps as no-ops, never new teardown.
    void publishImmediateMeterStop(unsigned id) noexcept {
        publishMeterStop(id,MeterGraphEventKind::immediateStop,0);
    }
    void publishMeterStop(unsigned id,MeterGraphEventKind kind,double lead) noexcept {
        if(meterScope!=MeterEventScope::acceptedPlayback||!tracks[id].clip||!meterImmediateTickets[id]
            ||meterImmediateTickets[id]!=meterGraphTickets[id])return;
        // Native worker/finalizing state cannot decide whether the JS-style
        // graph remains attached. Its explicit origin and message owner do.
        meterGraphEvents->publish(kind,id,meterImmediateTickets[id],frame,tracks[id].clearEpoch,tracks[id].clip->waveformId,lead,0,0,meterCallbackClock);
    }
    void publishMeterPanic(unsigned id) noexcept {
        // A queued PLAY introduces an unmapped message-time graph transition.
        // Until its 100ms lead is mapped, never infer panic policy from the
        // audible queued/playing snapshot or revive its old immediate origin.
        if(meterPanicKnown[id])publishMeterStop(id,MeterGraphEventKind::stopAll,.015);
    }
    void publishMeterQueue(unsigned id,double delay) noexcept {
        if(meterScope!=MeterEventScope::acceptedPlayback)return;
        if(meterQueueHighWater==std::numeric_limits<std::uint64_t>::max()){meterGraphEvents->invalidate();return;}
        meterQueuedIds[id]=++meterQueueHighWater;
        meterQueuedBufferIds[id]=tracks[id].clip?tracks[id].clip->waveformId:0;
        publishMeterQueueFact(id,MeterGraphEventKind::queuedPlay,delay);
    }
    void publishMeterQueueEnd(unsigned id,MeterGraphEventKind kind) noexcept {
        if(meterScope!=MeterEventScope::acceptedPlayback||!meterQueuedIds[id])return;
        publishMeterQueueFact(id,kind);meterQueuedIds[id]=meterQueuedBufferIds[id]=0;
    }
    void publishMeterQueueFact(unsigned id,MeterGraphEventKind kind,double delay=0) noexcept {
        const auto ticket=meterImmediateTickets[id]==meterGraphTickets[id]?meterImmediateTickets[id]:0u;
        meterGraphEvents->publish(kind,id,ticket,frame,tracks[id].clearEpoch,tracks[id].clip?tracks[id].clip->waveformId:0,0,meterQueuedIds[id],delay,
            kind==MeterGraphEventKind::queuedPlay||kind==MeterGraphEventKind::queuedStop?meterCallbackClock:CallbackClockStamp{});
    }
    struct Clip {
        std::atomic<std::uint32_t> leases{1};
        std::array<std::vector<float>,2> channels;
        std::uint32_t length = 0;
        std::uint64_t waveformId=0;
        std::array<float,waveformWidth> peaks{};
    };
    std::uint64_t nextWaveformId=1; // Worker-only, survives prepare after join.
    std::uint64_t nextMergeSerial=1; // Audio-only, survives prepare.
    struct SourceMetadata {double startDelay=0;bool reverse=false;double rate=1,origin=0;bool master=false;};
    struct Track {
        LoopView info{};
        Clip* clip = nullptr; // Immutable samples; one audio lease plus any in-flight job lease.
        std::array<Clip*,5> undo{},redo{}; // Immutable history leases, matching the reference depth.
        Clip* reverseHold = nullptr; // Independent reversed source snapshot.
        Clip* forwardHold = nullptr; // Forward source buffer retained across a stored-buffer history swap.
        std::uint64_t storageVersion=1,forwardVersion=0; // Logical buffer identity; history clones can reuse immutable PCM.
        bool activeReverse = false;
        bool queuedSourcePending = false; // Audio-owned source admission, independent of raw meter resets.
        double queuedSourcePhase=0,queuedStartFraction=0; // Fractional source time retained across inaudible rate edits.
        bool queuedSourceReverse=false; // Source controls captured at early graph birth, not at its grid.
        std::uint32_t queuedSourceTicket=0; // Explicit graph callback identity; raw window resets never replace it.
        std::uint64_t revision = 0, clearEpoch = 0, sourceRevision = 0;
        std::uint32_t captureLength = 0, captureOffset = 0;
        bool finalPlay = false, playWhileFinalizing = false;
        bool normalizeQueued = false; // NORM preserves a constructed source's future grid while the worker runs.
        bool multiplyQueued = false; // Pre-birth doubling preserves the original PLAY receipt/grid until commit.
        bool multiplySource = false; // No-input doubling retains the constructed source until its 5ms replacement.
        SyncMode recordMode=SyncMode::off;
        double recordBarFrames=0,recordOrigin=0,recordStartSeconds=0;
        std::uint32_t plannedFrames=0,recordPrefix=0;
        std::uint64_t startAt=0,endAt=0;
        std::uint64_t queuedGainAt=0; // Retained graph volume start while a replacement source runs early.
        std::array<std::uint16_t, maxBlocks> blocks{};
        std::uint32_t blockCount = 0, blendRemaining = 0, tailRemaining = 0;
        Stereo last{}, blendFrom{}, tail{};
        float gain = 1;
        double phase = 0, phaseRate = 1, sourceStartFrame = 0, sourceOffset = 0;
        std::uint64_t phaseEpoch = 0;
        float rate = 1, targetRate = 1, rateIncrement = 0;
        std::uint32_t rateRemaining = 0, panRemaining = 0;
        Stereo panGain{0.7071067811865475f, 0.7071067811865475f};
        Stereo panTarget = panGain, panIncrement{};
    };
    struct Job {
        std::array<std::uint16_t,maxBlocks> blocks{};
        std::uint32_t blockCount = 0, length = 0, offset = 0, track = 0, baseLength = 0;
        std::uint64_t revision = 0;
        Clip* base = nullptr;
        double feedback = 0.8;
        SyncMode recordMode=SyncMode::off;
        std::uint32_t outputFrames=0;
        bool edit = false, normalize = false, multiply = false, doubling = false;
        SourceMetadata multiplyMetadata{}; // Audio-owned command snapshot, immutable while the worker runs.
        bool merge = false, mergeNotify = false, sourceDynamic = false;
        Clip* mergeSource = nullptr;
        std::uint32_t sourceTrack = 20;
        std::uint64_t sourceClearEpoch = 0, sourceWaveform = 0;
        double sourceBpm = 0;
    };
    // A reversed source snapshots the raw PCM at construction; its event owns
    // one lease until activation/retention transfer or cancellation. History
    // freezes forward event buffer identity; NORM redirects only matching buffers.
    struct PhaseEvent {std::uint64_t frame=0,epoch=0;double phase=0;std::uint32_t track=0;bool reverse=false,retrigger=false;double sourceStartSeconds=0,sourceOffset=0;float rate=1;unsigned retainedSlot=128;Clip* reverseClip=nullptr;Clip* forwardClip=nullptr;std::uint64_t bufferVersion=0;};
    static constexpr unsigned phaseCapacity=128;
    std::array<PhaseEvent,phaseCapacity> phaseEvents{};
    unsigned phaseRead=0,phaseWrite=0;
    // Fixed overlap leases, reserved before accepting an overtaking command.
    // A reservation has no clip until its event starts; exhaustion rejects it.
    struct RetainedSource {
        Clip* clip=nullptr;
        bool used=false,reverse=false,stuttering=false;
        unsigned track=0;
        std::uint64_t epoch=0,startAt=0,stopAt=0,bufferVersion=0;
        double phase=0,stutterStart=0,stutterEnd=0;
        float rate=1,targetRate=1,rateIncrement=0;
        unsigned rateRemaining=0,blendRemaining=0;
        Stereo last{},blendFrom{};
    };
    std::array<RetainedSource,phaseCapacity> retainedSources{};
    std::array<unsigned,trackCount> retainedCounts{};
    void releaseRetained(unsigned slot) noexcept;
    void clearRetained(const Track&) noexcept;
    Stereo tickRetained(const Track&) noexcept;

    void invalidatePhase(Track&) noexcept;
    void applyPhaseEvents() noexcept;
    bool schedulePhase(Track&,std::uint32_t,double,bool,bool replacePending=false,Clip* sourceBuffer=nullptr,const SourceMetadata* metadata=nullptr) noexcept;
    float& sample(Track&, std::uint32_t frame, std::uint32_t channel) noexcept;
    bool finish(Track&) noexcept;
    void acceptResult() noexcept;
    void workerLoop() noexcept;
    void shutdown();
    void retire(Clip*) noexcept;
    void releaseOnWorker(Clip*) noexcept;
    Job job{};
    // Audio publishes immutable jobs (1); worker owns them (2), then publishes result (3).
    std::atomic<unsigned> jobState{0};
    std::atomic<bool> stopping{false};
    std::thread worker;
    Clip* result = nullptr;
    bool resultChanged = true; // Worker publishes with result through jobState.
    SpscQueue<Clip*,512> retired; // Current/held/history/job leases plus at most 128 overlap /127 scheduled reverse leases.
    std::vector<std::unique_ptr<Clip>> owned; // Worker only, except stopped prepare/destruction.
    std::atomic<unsigned> exportState{0}; // 0 idle/message released; 1 worker; 2 message result.
    Clip* exportClip=nullptr;
    double exportRate=48000;
    WavExport exported;
    double rate = 48000;
    std::uint64_t frame=0;
    SyncClock sync;
    Stereo previousInput{},olderInput{};
    double effectiveRateFor(const Track&,std::uint32_t length) const noexcept;
    static Clip* playbackClip(const Track& t) noexcept { return t.reverseHold?t.reverseHold:t.forwardHold?t.forwardHold:t.clip; }
    void freezeHistorySources(Track&,std::uint32_t) noexcept;
    void updateRate(Track&,bool directSpeedEdit=false) noexcept;
    void schedules(Track&) noexcept;
    std::uint32_t residentBlocks = 0;
    static bool hasPlaybackSource(const Track& t) noexcept {
        const auto s=t.info.state;return t.clip&&!t.queuedSourcePending&&(s==LoopState::playing||s==LoopState::stopping||s==LoopState::overdubbing||s==LoopState::substituting||s==LoopState::multiplying||(s==LoopState::finalizing&&t.playWhileFinalizing));
    }
    static bool waitingQueuedSource(const Track& t) noexcept {
        return t.info.state==LoopState::queued||(t.info.state==LoopState::finalizing&&(t.normalizeQueued||t.multiplyQueued||t.multiplySource)&&t.finalPlay&&!t.playWhileFinalizing);
    }
    bool hasPreparedQueuedSource(const Track& t,bool includeStopping=false) const noexcept {
        return queuedStartMode==QueuedStartMode::messageCallback&&waitingQueuedSource(t)
            &&!t.queuedSourcePending&&t.queuedSourceTicket&&(includeStopping||!t.info.stopPending);
    }
    void stop(Track&) noexcept;
    void clear(Track&) noexcept;
    void blend(Track&) noexcept;
    void restartSource(Track&,bool preparedQueue=false) noexcept;
    void admitQueuedSource(Track&) noexcept;
    void cutQueuedSource(Track&) noexcept;
    void saveUndo(Track&,std::uint32_t,bool global = true) noexcept;
    bool restore(Track&,std::uint32_t,bool redo) noexcept;
    std::array<std::uint8_t,50> undoOrder{},redoOrder{};
    std::uint32_t undoSize=0,redoSize=0;
    bool hasCapture() const noexcept;
    std::uint32_t growthRoom(const Track&) const noexcept;
    struct DeferredMultiplyGraph {std::uint32_t track=0,ticket=0,generation=0;std::uint64_t queueId=0,bufferId=0;};
    DeferredMultiplyGraph deferredMultiplyGraph{}; // One edit worker, one pending callback; no FIFO stall.
    // Construction-time heap storage; never grow the Windows audio stack.
    std::array<std::uint32_t,trackCount> meterGraphTickets{},meterGraphHighWater{},meterImmediateTickets{};
    std::array<std::uint32_t,trackCount> retiredSourceTickets{};
    std::array<bool,trackCount> meterPanicKnown{};
    std::array<std::uint64_t,trackCount> meterQueuedIds{};
    std::array<std::uint64_t,trackCount> meterQueuedBufferIds{}; // Immutable play receipt; pre-birth history/NORM change PCM, not this identity.
    std::array<std::uint64_t,trackCount> meterQueuedWindowIds{};
    std::uint64_t meterQueueHighWater=0;
    std::array<std::uint64_t,trackCount> meterGraphFrames{};
    std::unique_ptr<MeterGraphEvents> meterGraphEvents=std::make_unique<MeterGraphEvents>();
    std::unique_ptr<std::array<MeterWindow,trackCount>> meterWindows=std::make_unique<std::array<MeterWindow,trackCount>>();
    std::array<Track, trackCount> tracks{};
    std::array<std::uint16_t, maxBlocks> freeBlocks{};
    std::vector<float> audio;
    std::uint32_t totalBlocks = 0, freeCount = 0, fadeLength = 240;
    bool poolFull = false, autoPlay = true;
    int soloTrack = -1;
};
}
