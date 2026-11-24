// SPDX-License-Identifier: AGPL-3.0-only
// FFT arithmetic: HiFi-LoFi AudioFFT / Takuya Ooura. Pinned sources, MIT license
// and Ooura attribution: third_party/audiofft/README.md. Aion owns this history
// and transactional preparation layer; no upstream convolver source is copied.
#include "PartitionedConvolver.h"
#include "AudioFFT.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace aion {
namespace {
constexpr auto p = PartitionedConvolver::partitionFrames;
constexpr auto bins = p + 1;
using Spectrum = std::array<float, bins>;
bool cancelled(const std::atomic<bool>* flag) noexcept {
    return flag && flag->load(std::memory_order_relaxed);
}
void multiplyAdd(Spectrum& re, Spectrum& im, const Spectrum& xr,
                 const Spectrum& xi, const Spectrum& hr, const Spectrum& hi) noexcept {
    for (std::size_t b = 0; b < bins; ++b) {
        re[b] += xr[b] * hr[b] - xi[b] * hi[b];
        im[b] += xr[b] * hi[b] + xi[b] * hr[b];
    }
}
}
struct PartitionedConvolver::State {
    audiofft::AudioFFT fft;
    std::vector<Spectrum> hr, hi, xr, xi;
    std::array<float, 2 * p> input{}, result{};
    std::array<float, p> overlap{};
    Spectrum pastRe{}, pastIm{}, sumRe{}, sumIm{};
    std::size_t frames = 0, current = 0, fill = 0, valid = 0;

    void reset() noexcept {
        // Never read a history slot until it has been written since reset.
        // valid saturates, avoiding epoch wrap and an IR-sized memset.
        current = fill = valid = 0;
        input.fill(0);
        overlap.fill(0);
    }
    void process(std::span<const float> in, std::span<float> out) noexcept {
        std::size_t offset = 0;
        const auto partitions = hr.size();
        while (offset < in.size()) {
            if (fill == 0) {
                pastRe.fill(0); pastIm.fill(0);
                for (std::size_t j = 1; j <= valid; ++j) {
                    const auto old = (current + partitions - j) % partitions;
                    multiplyAdd(pastRe, pastIm, xr[old], xi[old], hr[j], hi[j]);
                }
            }
            const auto n = std::min(p - fill, in.size() - offset);
            for (std::size_t i = 0; i < n; ++i) {
                const auto v = in[offset + i];
                input[fill + i] = std::isfinite(v) ? std::clamp(v, -1.e6f, 1.e6f) : 0.f;
            }
            // Recompute only the partial current partition for zero latency.
            // The preceding partition sum is cached across short callbacks.
            fft.fft(input.data(), xr[current].data(), xi[current].data());
            sumRe = pastRe; sumIm = pastIm;
            multiplyAdd(sumRe, sumIm, xr[current], xi[current], hr[0], hi[0]);
            fft.ifft(result.data(), sumRe.data(), sumIm.data());
            for (std::size_t i = 0; i < n; ++i)
                out[offset + i] = result[fill + i] + overlap[fill + i];
            fill += n; offset += n;
            if (fill == p) {
                std::copy_n(result.data() + p, p, overlap.data());
                input.fill(0);
                fill = 0;
                current = (current + 1) % partitions;
                valid = std::min(valid + 1, partitions - 1);
            }
        }
    }
};
PartitionedConvolver::PartitionedConvolver() noexcept = default;
PartitionedConvolver::~PartitionedConvolver() = default;
std::size_t PartitionedConvolver::preparationBytes(std::size_t frames) noexcept {
    if (!frames || frames > maximumImpulseFrames) return 0;
    // Ooura: double transform[256], twiddles[128], int work[2+sqrt(256)].
    // 256 extra bytes conservatively cover its implementation object. Caller
    // State already includes the AudioFFT handle. Reaudit on backend changes.
    constexpr std::size_t fftPayload = 256 * 12 + 18 * sizeof(int) + 256;
    // MSVC's large-vector allocations add a 39-byte alignment prefix in this
    // toolchain. Reserve 64 for each of four spectra + three FFT vectors on
    // both platforms; general heap/OS bookkeeping is still not an RSS estimate.
    return sizeof(State) + 4 * ((frames + p - 1) / p) * sizeof(Spectrum) + fftPayload + 7 * 64;
}
bool PartitionedConvolver::prepare(std::span<const float> ir,
                                    const std::atomic<bool>* flag) noexcept {
    if (ir.empty() || ir.size() > maximumImpulseFrames || cancelled(flag)) return false;
    for (std::size_t i = 0; i < ir.size(); ++i) {
        if ((i % p == 0 && cancelled(flag)) || !std::isfinite(ir[i]) || std::abs(ir[i]) > 1.e6f)
            return false;
    }
    try {
        auto next = std::make_unique<State>();
        const auto count = (ir.size() + p - 1) / p;
        next->fft.init(2 * p);
        next->hr.resize(count); next->hi.resize(count);
        next->xr.resize(count); next->xi.resize(count);
        std::array<float, 2 * p> work{};
        for (std::size_t j = 0; j < count; ++j) {
            if (cancelled(flag)) return false;
            work.fill(0);
            std::copy_n(ir.data() + j * p, std::min(p, ir.size() - j * p), work.data());
            next->fft.fft(work.data(), next->hr[j].data(), next->hi[j].data());
        }
        next->frames = ir.size();
        if (cancelled(flag)) return false;
        state.swap(next); // old history/FFT retired here, on the worker only
        return true;
    } catch (...) { return false; }
}
bool PartitionedConvolver::process(std::span<const float> in, std::span<float> out) noexcept {
    if (in.size() != out.size() || in.size() > maximumBlockFrames || !state) return false;
    state->process(in, out);
    return true;
}
void PartitionedConvolver::reset() noexcept { if (state) state->reset(); }
std::size_t PartitionedConvolver::impulseFrames() const noexcept { return state ? state->frames : 0; }
}
