// SPDX-License-Identifier: AGPL-3.0-only
// Original control order: Jorge Salgueiro / Ascii-Looper _createReverb.
// FFT: unmodified HiFi-LoFi AudioFFT / Takuya Ooura (CREDITS.md, retained notices).
// TPT damping, smoothing and constant-sum delay edits follow Aion's Delay.cpp;
// the Q=.5 coefficients below are bilinear-transform equations, not copied
// WebAudio filter code. No additional third-party code or coefficient table.
#include "Reverb.h"
#include "ReverbImpulse.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace aion {
namespace {
constexpr std::size_t chunkFrames = 128;
double clean(double x) noexcept { return std::isfinite(x) ? std::clamp(x, -1.e6, 1.e6) : 0.; }
void smooth(double& x, double target, double k) noexcept {
    x += k * (target - x);
    if (std::abs(target - x) < 1.e-12) x = target;
}
bool cancelled(const std::atomic<bool>* flag) noexcept {
    return flag && flag->load(std::memory_order_relaxed);
}
}
struct Reverb::State {
    std::array<NonUniformConvolver, 2> convolution;
    std::vector<std::array<float, 2>> predelay;
    std::array<std::array<float, chunkFrames>, 2> dry{}, wet{};
    std::array<double, chunkFrames> frameMix{}, frameGain{}, frameEnable{}, frameG{}, frameA{};
    std::array<double, 2> ic1{}, ic2{};
    ReverbSettings target{};
    double rate = 0, smoothing = 0, mix = 0, gain = 1, enable = 0;
    double g = 0, gTarget = 0, dampA = 0, wanted = 0, delayA = 0, delayB = 0;
    std::size_t write = 0, history = 0;
    unsigned fade = 0, fadeLength = 0;
    bool asleep = true;

    void derive() noexcept {
        wanted = target.values[5] * rate;
        gTarget = std::tan(std::numbers::pi * std::min(target.values[4], .45 * rate) / rate);
    }
    void reset() noexcept {
        for (auto& c : convolution) c.reset();
        write = history = 0; fade = 0; delayA = delayB = wanted;
        ic1 = {}; ic2 = {};
        mix = target.values[1]; gain = target.values[0];
        enable = target.enabled ? 1 : 0; g = gTarget; dampA = 1. / (1. + g * (g + 2.));
        asleep = !target.enabled;
    }
    double read(unsigned channel, double frames) const noexcept {
        const auto age = static_cast<std::size_t>(frames);
        const auto at = [&](std::size_t back) noexcept -> double {
            if (back >= history) return 0;
            return predelay[(write + predelay.size() - back) % predelay.size()][channel];
        };
        const double fraction = frames - static_cast<double>(age), first = at(age);
        return fraction == 0 ? first : first + fraction * (at(age + 1) - first);
    }
    void process(std::span<const float> left, std::span<const float> right,
                 std::span<float> outLeft, std::span<float> outRight) noexcept {
        for (std::size_t offset = 0; offset < left.size();) {
            const auto n = std::min(chunkFrames, left.size() - offset);
            // Save BOTH channels before writing either output: exact aligned
            // in-place operation (including swapped channels) remains safe.
            for (std::size_t i = 0; i < n; ++i) {
                dry[0][i] = static_cast<float>(clean(left[offset + i]));
                dry[1][i] = static_cast<float>(clean(right[offset + i]));
            }
            if (asleep) {
                std::copy_n(dry[0].data(), n, outLeft.data() + offset);
                std::copy_n(dry[1].data(), n, outRight.data() + offset);
                offset += n; continue;
            }
            for (std::size_t i = 0; i < n; ++i) {
                smooth(mix, target.values[1], smoothing); smooth(gain, target.values[0], smoothing);
                smooth(enable, target.enabled ? 1. : 0., smoothing);
                if (g != gTarget) { smooth(g, gTarget, smoothing); dampA = 1. / (1. + g * (g + 2.)); }
                frameMix[i] = mix; frameGain[i] = gain; frameEnable[i] = enable; frameG[i] = g; frameA[i] = dampA;
                if (!fade && wanted != delayA) { delayB = wanted; fade = fadeLength; }
                predelay[write] = {dry[0][i], dry[1][i]};
                history = std::min(history + 1, predelay.size());
                const double blend = fade ? 1. - static_cast<double>(fade) / fadeLength : 0.;
                for (unsigned c = 0; c < 2; ++c) {
                    double value = read(c, delayA);
                    if (fade) value += blend * (read(c, delayB) - value);
                    wet[c][i] = static_cast<float>(value);
                }
                if (fade && !--fade) delayA = delayB;
                if (++write == predelay.size()) write = 0;
            }
            for (auto c = 0u; c < 2; ++c) {
                auto samples = std::span<float>(wet[c]).first(n);
                convolution[c].process(samples, samples);
            }
            for (std::size_t i = 0; i < n; ++i) {
                const double coefficient = frameG[i], a1 = frameA[i];
                for (unsigned c = 0; c < 2; ++c) {
                    const double v1 = a1 * (ic1[c] + coefficient * (wet[c][i] - ic2[c]));
                    const double v2 = ic2[c] + coefficient * v1;
                    ic1[c] = clean(2. * v1 - ic1[c]); ic2[c] = clean(2. * v2 - ic2[c]);
                    if (std::abs(ic1[c]) < 1.e-24) ic1[c] = 0;
                    if (std::abs(ic2[c]) < 1.e-24) ic2[c] = 0;
                    const double input = dry[c][i];
                    const double effected = frameGain[i] * ((1. - frameMix[i]) * input + frameMix[i] * v2);
                    const auto output = static_cast<float>(clean(input + frameEnable[i] * (effected - input)));
                    (c == 0 ? outLeft : outRight)[offset + i] = output;
                }
            }
            // Once the ramp reaches exact dry, discard history before returning
            // or starting another chunk. Extra wet computation in the rest of
            // this chunk is inaudible; no command can interleave this call.
            offset += n;
            if (!target.enabled && enable == 0) reset();
        }
    }
};
Reverb::Reverb() noexcept = default;
Reverb::~Reverb() = default;
std::size_t Reverb::preparationBytes(double rate, const ReverbSettings& settings) noexcept {
    const auto frames = ReverbImpulse::requiredFrames(rate, settings);
    if (!frames) return 0;
    return sizeof(State) + 2 * NonUniformConvolver::preparationBytes(frames)
        + (static_cast<std::size_t>(std::ceil(rate * .25)) + 2) * sizeof(std::array<float, 2>) + 64;
}
bool Reverb::prepare(double rate, const ReverbSettings& settings, std::span<const float> leftIr,
                     std::span<const float> rightIr, const std::atomic<bool>* flag) noexcept {
    const auto frames = ReverbImpulse::requiredFrames(rate, settings);
    if (!frames || leftIr.size() != frames || rightIr.size() != frames || cancelled(flag)) return false;
    try {
        auto next = std::make_unique<State>();
        if (!next->convolution[0].prepare(leftIr, flag) || !next->convolution[1].prepare(rightIr, flag)) return false;
        next->predelay.resize(static_cast<std::size_t>(std::ceil(rate * .25)) + 2);
        next->rate = rate; next->target = settings;
        next->smoothing = 1. - std::exp(-1. / (.01 * rate));
        next->fadeLength = static_cast<unsigned>(std::ceil(.02 * rate));
        next->derive(); next->reset();
        if (cancelled(flag)) return false;
        state.swap(next); // only the worker retires the previous complete stereo state
        return true;
    } catch (...) { return false; }
}
bool Reverb::set(const ReverbSettings& settings) noexcept {
    if (!state || !validReverbSettings(settings) || settings.values[2] != state->target.values[2]
        || settings.values[3] != state->target.values[3]) return false;
    const bool dampChanged = settings.values[4] != state->target.values[4];
    state->target = settings;
    state->wanted = settings.values[5] * state->rate;
    if (dampChanged) state->derive();
    if (state->asleep && settings.enabled) {
        state->reset(); state->enable = 0; state->asleep = false;
    }
    return true;
}
bool Reverb::process(std::span<const float> left, std::span<const float> right,
                     std::span<float> outLeft, std::span<float> outRight) noexcept {
    if (!state || left.size() != right.size() || left.size() != outLeft.size()
        || left.size() != outRight.size() || left.size() > maximumBlockFrames) return false;
    state->process(left, right, outLeft, outRight); return true;
}
void Reverb::reset() noexcept { if (state) state->reset(); }
bool Reverb::tick(float left, float right, float& outLeft, float& outRight) noexcept {
    return process(std::span<const float>(&left, 1), std::span<const float>(&right, 1),
                   std::span<float>(&outLeft, 1), std::span<float>(&outRight, 1));
}
ReverbSettings Reverb::settings() const noexcept { return state ? state->target : ReverbSettings{}; }
double Reverb::sampleRate() const noexcept { return state ? state->rate : 0.; }
std::size_t Reverb::predelayStorageFrames() const noexcept { return state ? state->predelay.size() : 0; }
}
