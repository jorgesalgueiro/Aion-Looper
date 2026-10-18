#pragma once
#include "core/SpscQueue.h"
#include <cstdint>
namespace aion {
struct GainCommand { std::uint64_t frame; float gain; };
struct Transport { double bpm = 120.0; double ppq = 0.0; bool playing = false; bool hostValid = false; };
struct Meter { std::uint64_t frame; float peak; };
// Audio owner only for prepare/process/target/transport. The message thread is
// the sole producer of commands; the message thread alone consumes meters.
class Engine {
public:
    void prepare(double sampleRate) noexcept;
    void setTarget(float value) noexcept;
    bool enqueue(GainCommand command) noexcept;
    void process(float* const* channels, int channelCount, int frames) noexcept;
    bool popMeter(Meter& meter) noexcept { return meters.pop(meter); }
    std::uint64_t clock() const noexcept { return frame; }
    Transport transport{}; // Audio-owned; never read directly from the editor.
private:
    SpscQueue<GainCommand, 1024> commands;
    SpscQueue<Meter, 8> meters;
    GainCommand pending{};
    bool hasPending = false;
    std::uint64_t frame = 0;
    float gain = 0.0f, target = 0.0f, increment = 0.0f;
    std::uint32_t rampLength = 960, remaining = 0;
};
}
