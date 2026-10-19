#pragma once
#include "core/SpscQueue.h"
#include "core/LoopBank.h"
#include "core/Metronome.h"
#include "core/CountIn.h"
#include <cstdint>
namespace aion {
struct GainCommand { std::uint64_t frame; float gain; };
struct Transport { double bpm = 120.0; double ppq = 0.0; bool playing = false; bool hostValid = false; };
struct Meter { std::uint64_t frame; float peak; };
struct LoopCommand {
    LoopAction action{};
    std::uint32_t track = 0, requestId = 0, generation = 0;
    std::uint64_t frame = 0;
    float value = 0;
};
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
};
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
    bool finalizationPending() const noexcept { return loops.finalizationPending(); }
    bool popView(EngineView& view) noexcept { return views.pop(view); }
    std::uint32_t generation() const noexcept { return epoch.load(std::memory_order_acquire); }
    void setTarget(float value) noexcept;
    bool enqueue(GainCommand command) noexcept;
    void process(float* const* channels, int channelCount, int frames) noexcept;
    bool popMeter(Meter& meter) noexcept { return meters.pop(meter); }
    std::uint64_t clock() const noexcept { return frame; }
    Transport transport{}; // Audio-owned; never read directly from the editor.
private:
    LoopBank loops;
    Metronome metronome;
    CountIn countIn;
    std::uint32_t countInTrack=0;
    SpscQueue<LoopCommand, 256> loopCommands;
    SpscQueue<EngineView, 8> views;
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
    std::uint32_t rampLength = 960, remaining = 0;
};
}
