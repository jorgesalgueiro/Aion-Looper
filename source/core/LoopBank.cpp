#include "core/LoopBank.h"
#include <algorithm>
#include <cmath>

namespace aion {
namespace {
float safe(float v) noexcept { return std::isfinite(v) ? std::clamp(v, -8.0f, 8.0f) : 0.0f; }
bool audible(LoopState s) noexcept {
    return s == LoopState::playing || s == LoopState::overdubbing || s == LoopState::substituting;
}
}
void LoopBank::prepare(std::uint32_t frames, std::uint32_t fadeFrames) {
    totalBlocks = std::clamp(frames / blockFrames, 1u, maxBlocks);
    audio.assign(static_cast<std::size_t>(totalBlocks) * blockFrames * 2, 0.0f);
    for (auto& t : tracks) t = Track{};
    freeCount = totalBlocks;
    for (std::uint32_t b = 0; b < totalBlocks; ++b) freeBlocks[b] = static_cast<std::uint16_t>(b);
    fadeLength = std::min(fadeFrames, 1920u);
    poolFull = false; soloTrack = -1;
}
float& LoopBank::sample(Track& t, std::uint32_t frame, std::uint32_t channel) noexcept {
    const auto block = t.blocks[frame / blockFrames];
    return audio[(static_cast<std::size_t>(block) * blockFrames + frame % blockFrames) * 2 + channel];
}
void LoopBank::blend(Track& t) noexcept {
    t.blendFrom = t.last;
    t.blendRemaining = fadeLength;
}
void LoopBank::stop(Track& t) noexcept {
    if (t.info.state == LoopState::empty || t.info.state == LoopState::stopped) return;
    t.tail = t.last; t.tailRemaining = fadeLength;
    t.info.state = t.info.length ? LoopState::stopped : LoopState::empty;
    t.info.position = 0; t.phase = 0;
}
void LoopBank::clear(Track& t) noexcept {
    stop(t);
    for (std::uint32_t i = 0; i < t.blockCount; ++i) freeBlocks[freeCount++] = t.blocks[i];
    if (soloTrack == static_cast<int>(&t - tracks.data())) soloTrack = -1;
    t.blockCount = 0;
    (void) apply(LoopAction::pan, static_cast<std::uint32_t>(&t-tracks.data()), 5);
    const bool muted = t.info.muted; // Reference clear() keeps the explicit mute flag.
    t.info = LoopView{}; t.info.muted = muted; t.phase = 0; t.rate = 1; t.rateRemaining = 0;
    if (freeCount) poolFull = false;
}
bool LoopBank::apply(LoopAction action, std::uint32_t id, float value) noexcept {
    if (action == LoopAction::stopAll) { for (auto& t : tracks) stop(t); return true; }
    if (action == LoopAction::clearAll) { for (auto& t : tracks) clear(t); return true; }
    if (id >= trackCount) return false;
    auto& t = tracks[id]; auto& s = t.info.state;
    switch (action) {
        case LoopAction::record:
            if (s != LoopState::empty || freeCount == 0 || hasCapture()) return false;
            t.info.playbackRate = t.rate = 1; t.rateRemaining = 0;
            s = LoopState::recording; t.info.length = t.info.position = 0; t.phase = 0; poolFull = false; return true;
        case LoopAction::finish:
            if (s == LoopState::recording && !t.info.length) { clear(t); return true; }
            if (s == LoopState::recording && t.info.length) {
                s = autoPlay ? LoopState::playing : LoopState::stopped; t.info.position = 0; t.phase = 0; blend(t); return true;
            }
            if (s == LoopState::overdubbing || s == LoopState::substituting) { s = LoopState::playing; blend(t); return true; }
            return false;
        case LoopAction::play:
            if (s != LoopState::stopped || !t.info.length) return false;
            s = LoopState::playing; t.info.position = 0; t.phase = 0; t.tailRemaining = 0; blend(t); return true;
        case LoopAction::stop: stop(t); return true;
        case LoopAction::clear: clear(t); return true;
        case LoopAction::overdub:
        case LoopAction::substitute:
            if (s != LoopState::playing || hasCapture() || t.info.playbackRate != 1 || t.rateRemaining) return false;
            t.phase = t.info.position; blend(t);
            s = action == LoopAction::overdub ? LoopState::overdubbing : LoopState::substituting;
            return true;
        case LoopAction::volume:
            if (!std::isfinite(value) || value < 0 || value > 2) return false;
            t.info.volume = value; return true;
        case LoopAction::feedback:
            if (!std::isfinite(value) || value < 0 || value > 1) return false;
            t.info.feedback = value; return true;
        case LoopAction::mute:
            if (value != 0 && value != 1) return false;
            t.info.muted = value != 0; return true;
        case LoopAction::solo:
            soloTrack = soloTrack == static_cast<int>(id) ? -1 : static_cast<int>(id); return true;
        case LoopAction::retrigger:
            if (!t.info.length || s == LoopState::recording) return false;
            s = LoopState::playing; t.info.position = 0; t.phase = 0; t.tailRemaining = 0; blend(t); return true;
        case LoopAction::pan: {
            if (!std::isfinite(value) || value < 0 || value > 10) return false;
            value = std::trunc(value); // UIManager.setLoopPan uses parseInt.
            if (value == t.info.pan) return true;
            t.info.pan = value;
            // Reference GainNodes feed mono ChannelMerger inputs, at angle pan*pi/20.
            const float angle = value * 0.15707963267948966f;
            t.panTarget = {value == 10 ? 0.0f : std::cos(angle), value == 0 ? 0.0f : std::sin(angle)};
            t.panRemaining = fadeLength * 4;
            if (t.panRemaining) t.panIncrement = {(t.panTarget.left-t.panGain.left)/t.panRemaining,(t.panTarget.right-t.panGain.right)/t.panRemaining};
            else t.panGain = t.panTarget;
            return true;
        }
        case LoopAction::speed:
            if (!std::isfinite(value) || value < 0.25f || value > 2 || s == LoopState::recording
                || s == LoopState::overdubbing || s == LoopState::substituting) return false;
            if (value == t.info.playbackRate) return true;
            t.info.playbackRate = value; t.rateRemaining = fadeLength * 4;
            if (t.rateRemaining) t.rateIncrement = (value-t.rate)/t.rateRemaining;
            else t.rate = value;
            return true;
        case LoopAction::clearAll:
        case LoopAction::stopAll: break;
    }
    return false;
}
bool LoopBank::hasCapture() const noexcept {
    for (const auto& t : tracks)
        if (t.info.state == LoopState::recording || t.info.state == LoopState::overdubbing || t.info.state == LoopState::substituting) return true;
    return false;
}
void LoopBank::beginBlock() noexcept { for (auto& t : tracks) t.info.peak = 0; }
Stereo LoopBank::tick(Stereo input) noexcept {
    input = {safe(input.left), safe(input.right)};
    Stereo mix{};
    for (auto& t : tracks) {
        auto& v = t.info;
        if (v.state == LoopState::recording) {
            if (v.length % blockFrames == 0) {
                if (freeCount) t.blocks[t.blockCount++] = freeBlocks[--freeCount];
                else { poolFull = true; v.state = v.length ? (autoPlay ? LoopState::playing : LoopState::stopped) : LoopState::empty; v.position = 0; t.phase = 0; blend(t); }
            }
            if (v.state == LoopState::recording) {
                sample(t, v.length, 0) = input.left; sample(t, v.length, 1) = input.right; ++v.length;
            }
        }
        if (t.rateRemaining && !audible(v.state)) {t.rate += t.rateIncrement;if (--t.rateRemaining == 0)t.rate = v.playbackRate;}
        if (t.panRemaining) {
            t.panGain.left += t.panIncrement.left; t.panGain.right += t.panIncrement.right;
            if (--t.panRemaining == 0) t.panGain = t.panTarget;
        }
        Stereo out{};
        if (audible(v.state) && v.length) {
            v.position = static_cast<std::uint32_t>(t.phase);
            const auto next = v.position+1 == v.length ? 0 : v.position+1;
            const float fraction = static_cast<float>(t.phase-v.position);
            const float left = sample(t, v.position, 0), right = sample(t, v.position, 1);
            out = {left+(sample(t,next,0)-left)*fraction, right+(sample(t,next,1)-right)*fraction};
            if (v.state == LoopState::overdubbing || v.state == LoopState::substituting) {
                const float feedback = v.state == LoopState::substituting ? 0.0f : v.feedback;
                sample(t, v.position, 0) = safe(out.left * feedback + input.left);
                sample(t, v.position, 1) = safe(out.right * feedback + input.right);
            }
            if (t.blendRemaining) {
                const float progress = 1.0f - static_cast<float>(--t.blendRemaining) / static_cast<float>(fadeLength);
                out.left = t.blendFrom.left + (out.left - t.blendFrom.left) * progress;
                out.right = t.blendFrom.right + (out.right - t.blendFrom.right) * progress;
            }
            t.last = out;
            if (t.rateRemaining) {t.rate += t.rateIncrement;if (--t.rateRemaining == 0)t.rate = v.playbackRate;}
            t.phase += t.rate;
            if (t.phase >= v.length) { t.phase = std::fmod(t.phase, static_cast<double>(v.length)); blend(t); }
            v.position = static_cast<std::uint32_t>(t.phase);
        } else if (t.tailRemaining) {
            const float progress = static_cast<float>(--t.tailRemaining) / static_cast<float>(fadeLength);
            out = {t.tail.left * progress, t.tail.right * progress};
            t.last = out;
        } else { t.last = {}; }
        const bool excluded = soloTrack >= 0 && soloTrack != static_cast<int>(&t - tracks.data());
        const float desired = v.muted || excluded ? 0.0f : v.volume;
        const float step = fadeLength ? 1.0f / static_cast<float>(fadeLength * 4) : 2.0f;
        t.gain += std::clamp(desired - t.gain, -step, step);
        const float mono = (out.left+out.right)*0.5f;
        out = {mono*t.panGain.left*t.gain, mono*t.panGain.right*t.gain};
        v.peak = std::max(v.peak, std::max(std::abs(out.left), std::abs(out.right)));
        mix.left += out.left; mix.right += out.right;
    }
    return mix;
}
std::array<LoopView, LoopBank::trackCount> LoopBank::view() const noexcept {
    std::array<LoopView, trackCount> result{};
    for (std::uint32_t i = 0; i < trackCount; ++i) { result[i] = tracks[i].info; result[i].solo = soloTrack == static_cast<int>(i); }
    return result;
}
}
