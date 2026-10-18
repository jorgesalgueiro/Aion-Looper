#include "core/Engine.h"
#include <algorithm>
#include <cmath>
namespace aion {
void Engine::prepare(double rate) noexcept {
    if (!std::isfinite(rate) || rate < 8000.0 || rate > 384000.0) rate = 48000.0;
    frame = 0; gain = target = increment = 0.0f; remaining = 0;
    rampLength = static_cast<std::uint32_t>(std::clamp(rate * 0.02, 1.0, 38400.0));
    GainCommand discard{}; while (commands.pop(discard)) {};
    Meter meter{}; while (meters.pop(meter)) {};
    hasPending = false;
}
void Engine::setTarget(float value) noexcept {
    value = std::isfinite(value) ? std::clamp(value, 0.0f, 2.0f) : 0.0f;
    if (value == target) return;
    target = value; remaining = rampLength; increment = (target - gain) / static_cast<float>(remaining);
}
bool Engine::enqueue(GainCommand command) noexcept {
    return std::isfinite(command.gain) && command.gain >= 0.0f && command.gain <= 2.0f && commands.push(command);
}
void Engine::process(float* const* channels, int count, int frames) noexcept {
    if (count < 0 || frames < 0 || (count > 0 && channels == nullptr)) return;
    float peak = 0.0f;
    int applied = 0; // Bound work even when many past-due commands arrive together.
    for (int i = 0; i < frames; ++i, ++frame) {
        if (!hasPending) hasPending = commands.pop(pending);
        while (hasPending && pending.frame <= frame && applied < 128) {
            setTarget(pending.gain); ++applied; hasPending = commands.pop(pending);
        }
        if (remaining > 0) { gain += increment; if (--remaining == 0) gain = target; }
        for (int c = 0; c < count; ++c) {
            if (channels[c] == nullptr) continue;
            const float value = std::isfinite(channels[c][i]) ? channels[c][i] : 0.0f;
            const double scaled = static_cast<double>(value) * gain;
            channels[c][i] = static_cast<float>(std::clamp(scaled, -1.0, 1.0));
            peak = std::max(peak, std::abs(channels[c][i]));
        }
    }
    // Telemetry is lossy by design; never wait when the editor is closed/slow.
    (void) meters.push({frame, peak});
}
}
