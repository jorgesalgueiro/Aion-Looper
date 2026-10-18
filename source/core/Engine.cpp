#include "core/Engine.h"
#include <algorithm>
#include <cmath>
namespace aion {
void Engine::prepare(double sampleRate, std::uint32_t poolFrames) {
    double rate = sampleRate;
    if (!std::isfinite(rate) || rate < 8000.0 || rate > 384000.0) rate = 48000.0;
    epoch.fetch_add(1, std::memory_order_acq_rel);
    audioReset = preparedRate != 0 && preparedRate != rate;
    if (preparedRate != rate || loops.capacity() != std::clamp(poolFrames / LoopBank::blockFrames, 1u, LoopBank::maxBlocks) * LoopBank::blockFrames)
        loops.prepare(poolFrames, static_cast<std::uint32_t>(rate * 0.005), rate);
    else (void) loops.apply(LoopAction::stopAll, 0);
    loops.resetTimeline();preparedRate = rate;
    LoopCommand oldCommand{}; while (loopCommands.pop(oldCommand)) {};
    hasPendingLoop = false; lastRequestId = 0; rejected = false;
    frame = 0; gain = target = increment = 0.0f; remaining = 0;
    rampLength = static_cast<std::uint32_t>(std::clamp(rate * 0.02, 1.0, 38400.0));
    GainCommand discard{}; while (commands.pop(discard)) {};
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
bool Engine::enqueueLoop(LoopCommand command) noexcept {
    if (command.generation == 0 || command.generation != generation() || command.track >= LoopBank::trackCount
        || !std::isfinite(command.value)) return false;
    return loopCommands.push(command);
}
void Engine::process(float* const* channels, int count, int frames) noexcept {
    if (count < 0 || frames < 0 || (count > 0 && channels == nullptr)) return;
    float peak = 0.0f, inputPeak = 0.0f;
    loops.beginBlock();
    int loopApplied = 0;
    int applied = 0; // Bound work even when many past-due commands arrive together.
    for (int i = 0; i < frames; ++i, ++frame) {
        if (!hasPending) hasPending = commands.pop(pending);
        while (hasPending && pending.frame <= frame && applied < 128) {
            setTarget(pending.gain); ++applied; hasPending = commands.pop(pending);
        }
        if (!hasPendingLoop) hasPendingLoop = loopCommands.pop(pendingLoop);
        while (hasPendingLoop && pendingLoop.frame <= frame && loopApplied < 128) {
            lastRequestId = pendingLoop.requestId;
            rejected = pendingLoop.generation != generation() || !loops.apply(pendingLoop.action, pendingLoop.track, pendingLoop.value);
            ++loopApplied; hasPendingLoop = loopCommands.pop(pendingLoop);
        }
        const float left = count > 0 && channels[0] && std::isfinite(channels[0][i]) ? channels[0][i] : 0.0f;
        const float right = count > 1 && channels[1] && std::isfinite(channels[1][i]) ? channels[1][i] : left;
        inputPeak = std::max(inputPeak, std::max(std::abs(left), std::abs(right)));
        const auto loopOutput = loops.tick({left, right});
        if (remaining > 0) { gain += increment; if (--remaining == 0) gain = target; }
        for (int c = 0; c < count; ++c) {
            if (channels[c] == nullptr) continue;
            const float value = std::isfinite(channels[c][i]) ? channels[c][i] : 0.0f;
            const float loopValue = c == 0 ? loopOutput.left : (c == 1 ? loopOutput.right : 0.0f);
            const double scaled = (static_cast<double>(monitoring ? value : 0.0f) + loopValue) * gain;
            channels[c][i] = static_cast<float>(std::clamp(scaled, -1.0, 1.0));
            peak = std::max(peak, std::abs(channels[c][i]));
        }
    }
    // Telemetry is lossy by design; never wait when the editor is closed/slow.
    (void) meters.push({frame, peak});
    (void) views.push({loops.view(), frame, preparedRate, generation(), lastRequestId, loops.available(), inputPeak, peak, rejected, loops.exhausted(), audioReset, loops.nextUndo(), loops.nextRedo(), loops.syncView()});
}
}
