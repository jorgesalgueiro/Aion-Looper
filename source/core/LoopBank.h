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
enum class LoopState : std::uint8_t { empty, recording, playing, stopped, overdubbing, substituting, finalizing, multiplying, armed, queued, stopping };
enum class LoopAction : std::uint8_t { record, finish, play, stop, overdub, substitute, clear, stopAll, volume, feedback, mute, clearAll, solo, retrigger, pan, speed, normalize, reverse, undo, redo, undoGlobal, redoGlobal, multiply, stopImmediate, startDelay, applyStart, quantizeStart, nudgeStart, stutterStart, stutterStop, merge, clearMergedSource, sourceRestart, meterGraphStart }; // sourceRestart and meterGraphStart are core-only primitives, never exposed by IPC.
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
    explicit LoopBank(MeterEventScope scope=MeterEventScope::rawWindows) :meterScope(scope){}
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
        (*meterWindows)[id].reset();
        meterGraphEvents->publish(MeterGraphEventKind::started,id,ticket,frame,tracks[id].clearEpoch);return true;
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
    void resetTimeline() noexcept { for(auto& window:*meterWindows)window.reset();meterGraphTickets.fill(0);meterGraphHighWater.fill(0);meterGraphFrames.fill(0);frame=0;sync.reset(rate);phaseRead=phaseWrite=0;for(auto& track:tracks){++track.phaseEpoch;++track.sourceRevision;track.info.phaseApplyFrame=0;} }
    SyncView syncView() const noexcept {return sync.view(frame);}
    double armedStartSeconds(std::uint32_t id) const noexcept {return id<trackCount&&tracks[id].info.state==LoopState::armed?tracks[id].recordStartSeconds:-1;}
private:
    const MeterEventScope meterScope;
    void publishMeterBuffer(unsigned id) noexcept {
        if(meterScope!=MeterEventScope::rawWindows&&tracks[id].clip)
            meterGraphEvents->publish(MeterGraphEventKind::bufferInstalled,id,0,frame,tracks[id].clearEpoch,tracks[id].clip->waveformId);
    }
    // Only successful immediate Loop.play origins call this. A raw-window reset,
    // buffer install, healthy retrigger or queued grid transition is not a birth.
    void publishImmediateMeterPlay(unsigned id) noexcept {
        if(meterScope!=MeterEventScope::acceptedPlayback)return;
        if(meterGraphHighWater[id]==std::numeric_limits<std::uint32_t>::max()){meterGraphEvents->invalidate();return;}
        const auto ticket=++meterGraphHighWater[id];
        meterGraphTickets[id]=ticket;meterGraphFrames[id]=frame;(*meterWindows)[id].reset();
        meterGraphEvents->publish(MeterGraphEventKind::immediatePlay,id,ticket,frame,tracks[id].clearEpoch,tracks[id].clip->waveformId);
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
    struct Track {
        LoopView info{};
        Clip* clip = nullptr; // Immutable samples; one audio lease plus any in-flight job lease.
        std::array<Clip*,5> undo{},redo{}; // Immutable history leases, matching the reference depth.
        Clip* reverseHold = nullptr; // Old raw clip backing a playing reversed source after NORM.
        bool activeReverse = false;
        std::uint64_t revision = 0, clearEpoch = 0, sourceRevision = 0;
        std::uint32_t captureLength = 0, captureOffset = 0;
        bool finalPlay = false, playWhileFinalizing = false;
        SyncMode recordMode=SyncMode::off;
        double recordBarFrames=0,recordOrigin=0,recordStartSeconds=0;
        std::uint32_t plannedFrames=0,recordPrefix=0;
        std::uint64_t startAt=0,endAt=0;
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
        bool merge = false, mergeNotify = false, sourceDynamic = false;
        Clip* mergeSource = nullptr;
        std::uint32_t sourceTrack = 20;
        std::uint64_t sourceClearEpoch = 0, sourceWaveform = 0;
        double sourceBpm = 0;
    };
    struct PhaseEvent {std::uint64_t frame=0,epoch=0;double phase=0;std::uint32_t track=0;bool reverse=false,retrigger=false;double sourceStartFrame=0,sourceOffset=0;};
    static constexpr unsigned phaseCapacity=128;
    std::array<PhaseEvent,phaseCapacity> phaseEvents{};
    unsigned phaseRead=0,phaseWrite=0;
    void applyPhaseEvents() noexcept;
    bool schedulePhase(Track&, std::uint32_t, double targetSeconds, bool masterPhase, bool replacePending = false) noexcept;
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
    SpscQueue<Clip*,256> retired; // 20 current + 20 held sources + at most 100 history + one job.
    std::vector<std::unique_ptr<Clip>> owned; // Worker only, except stopped prepare/destruction.
    std::atomic<unsigned> exportState{0}; // 0 idle/message released; 1 worker; 2 message result.
    Clip* exportClip=nullptr;
    double exportRate=48000;
    WavExport exported;
    double rate = 48000;
    std::uint64_t frame=0;
    SyncClock sync;
    Stereo previousInput{},olderInput{};
    void updateRate(Track&) noexcept;
    void schedules(Track&) noexcept;
    std::uint32_t residentBlocks = 0;
    static bool hasPlaybackSource(const Track& t) noexcept {
        const auto s=t.info.state;return t.clip&&(s==LoopState::playing||s==LoopState::stopping||s==LoopState::overdubbing||s==LoopState::substituting||s==LoopState::multiplying||(s==LoopState::finalizing&&t.playWhileFinalizing));
    }
    void stop(Track&) noexcept;
    void clear(Track&) noexcept;
    void blend(Track&) noexcept;
    void restartSource(Track&) noexcept;
    void saveUndo(Track&,std::uint32_t,bool global = true) noexcept;
    bool restore(Track&,std::uint32_t,bool redo) noexcept;
    std::array<std::uint8_t,50> undoOrder{},redoOrder{};
    std::uint32_t undoSize=0,redoSize=0;
    bool hasCapture() const noexcept;
    std::uint32_t growthRoom(const Track&) const noexcept;
    // Construction-time heap storage; never grow the Windows audio stack.
    std::array<std::uint32_t,trackCount> meterGraphTickets{},meterGraphHighWater{};
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
