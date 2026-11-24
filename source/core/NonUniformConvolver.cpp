// SPDX-License-Identifier: AGPL-3.0-only
// Uses unmodified HiFi-LoFi AudioFFT (MIT), with Takuya Ooura's FFT arithmetic.
// Pinned sources and permissions: third_party/audiofft/README.md. The two-size
// partition concept was also studied in HiFi-LoFi's TwoStageFFTConvolver; its
// implementation is not copied. Scheduling/transactional ownership below is Aion.
#include "NonUniformConvolver.h"
#include "AudioFFT.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace aion {
namespace {
constexpr auto headFrames = NonUniformConvolver::directHeadFrames;
constexpr auto tailFrames = NonUniformConvolver::tailPartitionFrames;
bool cancelled(const std::atomic<bool>* flag) noexcept {
    return flag && flag->load(std::memory_order_relaxed);
}
template<std::size_t p> struct Tail {
    static constexpr auto bins = p + 1;
    using Spectrum = std::array<float, bins>;
    audiofft::AudioFFT fft;
    std::vector<Spectrum> hr, hi, xr, xi;
    std::array<float, 2 * p> input{}, result{};
    std::array<float, p> overlap{}, ready{};
    Spectrum re{}, im{};
    std::size_t current = 0, fill = 0, valid = 0, multiplied = 0;

    bool prepare(std::span<const float> ir, const std::atomic<bool>* flag) {
        const auto count = (ir.size() + p - 1) / p;
        fft.init(2 * p);
        hr.resize(count); hi.resize(count); xr.resize(count); xi.resize(count);
        // This candidate is private to the worker until the whole graph succeeds.
        for (std::size_t j = 0; j < count; ++j) {
            if (cancelled(flag)) return false;
            input.fill(0);
            std::copy_n(ir.data() + j * p, std::min(p, ir.size() - j * p), input.data());
            fft.fft(input.data(), hr[j].data(), hi[j].data());
        }
        reset();
        return !cancelled(flag);
    }
    void reset() noexcept {
        current = fill = valid = multiplied = 0;
        input.fill(0); overlap.fill(0); ready.fill(0);
        // re/im and IR-sized history are overwritten/invalidated, never cleared.
    }
    void multiply(std::size_t slot, std::size_t ir) noexcept {
        for (std::size_t b = 0; b < bins; ++b) {
            re[b] += xr[slot][b] * hr[ir][b] - xi[slot][b] * hi[ir][b];
            im[b] += xr[slot][b] * hi[ir][b] + xi[slot][b] * hr[ir][b];
        }
    }
    void add(std::span<const float> in, std::span<float> out,
             NonUniformConvolver::TailWork& work) noexcept {
        std::size_t offset = 0;
        const auto count = hr.size();
        while (offset < in.size()) {
            if (fill == 0) { re.fill(0); im.fill(0); multiplied = 0; }
            const auto n = std::min(p - fill, in.size() - offset);
            for (std::size_t i = 0; i < n; ++i) {
                const auto v = in[offset + i];
                input[fill + i] = std::isfinite(v) ? std::clamp(v, -1.e6f, 1.e6f) : 0.f;
                out[offset + i] += ready[fill + i];
            }
            // Old spectra are immutable throughout this input partition. Spread
            // their products across consumed samples instead of one tail burst.
            const auto target = (valid * (fill + n) + p - 1) / p;
            while (multiplied < target) {
                ++multiplied;
                multiply((current + count - multiplied) % count, multiplied);
                ++work.partitionProducts;
            }
            fill += n; offset += n;
            if (fill == p) {
                fft.fft(input.data(), xr[current].data(), xi[current].data());
                multiply(current, 0); ++work.partitionProducts;
                fft.ifft(result.data(), re.data(), im.data()); work.transforms += 2;
                for (std::size_t i = 0; i < p; ++i) ready[i] = result[i] + overlap[i];
                std::copy_n(result.data() + p, p, overlap.data());
                input.fill(0); fill = 0;
                current = (current + 1) % count;
                valid = std::min(valid + 1, count - 1);
            }
        }
    }
};
template<std::size_t p> std::size_t tailPreparationBytes(std::size_t frames) noexcept {
    static_assert(p == 128 || p == 1024);
    using Stage = Tail<p>;
    // Pinned Ooura: double transform/twiddles and 2+floor(sqrt(2*p)) ints.
    // Include implementation-object allowance and all seven vector prefixes.
    constexpr std::size_t ints = p == 128 ? 18 : 47;
    constexpr std::size_t fftPayload = 2 * p * 12 + ints * sizeof(int) + 256;
    return sizeof(Stage) + 4 * ((frames + p - 1) / p) * sizeof(typename Stage::Spectrum) + fftPayload + 7 * 64;
}
}
struct NonUniformConvolver::State {
    std::array<float, headFrames> coefficients{}, history{}, scratch{};
    std::unique_ptr<Tail<headFrames>> near;
    std::unique_ptr<Tail<tailFrames>> tail;
    std::size_t frames = 0, directCount = 0, write = 0, valid = 0;
    TailWork work{}, nearWork{};
    std::size_t directProducts = 0;
    float direct(float value) noexcept {
        history[write] = std::isfinite(value) ? std::clamp(value, -1.e6f, 1.e6f) : 0.f;
        valid = std::min(valid + 1, directCount);
        double sum = 0;
        auto at = write;
        const auto* taps = coefficients.data(); const auto* samples = history.data();
        for (std::size_t i = 0; i < valid; ++i) {
            sum += static_cast<double>(taps[i]) * samples[at];
            at = at == 0 ? headFrames - 1 : at - 1;
        }
        directProducts += valid;
        if (++write == headFrames) write = 0;
        return static_cast<float>(sum);
    }
};
NonUniformConvolver::NonUniformConvolver() noexcept = default;
NonUniformConvolver::~NonUniformConvolver() = default;
std::size_t NonUniformConvolver::preparationBytes(std::size_t frames) noexcept {
    if (!frames || frames > maximumImpulseFrames) return 0;
    auto bytes = sizeof(State);
    if (frames > headFrames) bytes += tailPreparationBytes<headFrames>(std::min(frames, tailFrames) - headFrames);
    if (frames > tailFrames) bytes += tailPreparationBytes<tailFrames>(frames - tailFrames);
    return bytes;
}
bool NonUniformConvolver::prepare(std::span<const float> ir, const std::atomic<bool>* flag) noexcept {
    if (ir.empty() || ir.size() > maximumImpulseFrames || cancelled(flag)) return false;
    for (std::size_t i = 0; i < ir.size(); ++i) {
        if ((i % 128 == 0 && cancelled(flag)) || !std::isfinite(ir[i]) || std::abs(ir[i]) > 1.e6f)
            return false;
    }
    try {
        auto next = std::make_unique<State>();
        next->directCount = std::min(headFrames, ir.size());
        std::copy_n(ir.data(), next->directCount, next->coefficients.data());
        if (ir.size() > headFrames) {
            next->near = std::make_unique<Tail<headFrames>>();
            if (!next->near->prepare(ir.subspan(headFrames, std::min(ir.size(), tailFrames) - headFrames), flag)) return false;
        }
        if (ir.size() > tailFrames) {
            next->tail = std::make_unique<Tail<tailFrames>>();
            if (!next->tail->prepare(ir.subspan(tailFrames), flag)) return false;
        }
        next->frames = ir.size();
        if (cancelled(flag)) return false;
        state.swap(next); // Old graph retires here, exclusively on the worker.
        return true;
    } catch (...) { return false; }
}
bool NonUniformConvolver::process(std::span<const float> in, std::span<float> out) noexcept {
    if (!state || in.size() != out.size() || in.size() > maximumBlockFrames) return false;
    state->work = {}; state->nearWork = {}; state->directProducts = 0;
    for (std::size_t offset = 0; offset < in.size();) {
        const auto n = std::min(state->scratch.size(), in.size() - offset);
        // Preserve caller input before head output in exact in-place mode.
        std::copy_n(in.data() + offset, n, state->scratch.data());
        const auto original = std::span<const float>(state->scratch).first(n);
        const auto destination = out.subspan(offset, n);
        for (std::size_t i = 0; i < n; ++i) destination[i] = state->direct(original[i]);
        if (state->near) state->near->add(original, destination, state->nearWork);
        if (state->tail) state->tail->add(original, destination, state->work);
        offset += n;
    }
    return true;
}
bool NonUniformConvolver::tick(float input, float& output) noexcept {
    return process(std::span<const float>(&input, 1), std::span<float>(&output, 1));
}
void NonUniformConvolver::reset() noexcept {
    if (state) {
        state->write = state->valid = state->directProducts = 0;
        if (state->near) state->near->reset();
        if (state->tail) state->tail->reset();
        state->work = {}; state->nearWork = {};
    }
}
std::size_t NonUniformConvolver::impulseFrames() const noexcept { return state ? state->frames : 0; }
NonUniformConvolver::TailWork NonUniformConvolver::lastTailWork() const noexcept {
    return state ? state->work : TailWork{};
}
NonUniformConvolver::TailWork NonUniformConvolver::lastNearWork() const noexcept {
    return state ? state->nearWork : TailWork{};
}
std::size_t NonUniformConvolver::lastDirectProducts() const noexcept { return state ? state->directProducts : 0; }
}
