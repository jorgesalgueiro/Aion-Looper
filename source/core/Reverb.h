// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "NonUniformConvolver.h"
#include "ReverbSettings.h"

namespace aion {
// Prepared stereo B component, not yet connected to product graph ownership.
// Controls/routing: Jorge Salgueiro's Ascii-Looper _createReverb. FFT backend:
// HiFi-LoFi AudioFFT / Takuya Ooura; see CREDITS.md and full retained notices.
// prepare/destruction are worker-only, never concurrent with process/set/reset.
// set/process/reset require one exclusive audio owner. No internal queue/locks.
class Reverb {
public:
    static constexpr auto maximumBlockFrames = NonUniformConvolver::maximumBlockFrames;
    Reverb() noexcept;
    ~Reverb();
    Reverb(const Reverb&) = delete;
    Reverb& operator=(const Reverb&) = delete;
    // IRs must have floor(sampleRate * duration) frames at the supplied rate.
    // Coefficients are copied into FFT storage; no borrowed pointers survive.
    // Failure/cancellation preserves the entire old stereo processor/history.
    bool prepare(double sampleRate, const ReverbSettings&, std::span<const float> leftIr,
                 std::span<const float> rightIr, const std::atomic<bool>* cancelled = nullptr) noexcept;
    // Live-only controls/metadata. Shape edits (duration/decay) require prepare.
    // Invalid edits reject atomically; no IR/FFT preparation on this path.
    bool set(const ReverbSettings&) noexcept;
    bool process(std::span<const float> left, std::span<const float> right,
                 std::span<float> outLeft, std::span<float> outRight) noexcept;
    // Destructive reset: constant-sized convolution/filter clearing and O(1)
    // predelay invalidation. Ordinary playback restarts must not call reset.
    void reset() noexcept;
    ReverbSettings settings() const noexcept;
    double sampleRate() const noexcept;
    std::size_t predelayStorageFrames() const noexcept;
    // New prepared stereo state's requested allocation payload; excludes input
    // IR vectors, independently held old state and general heap/OS bookkeeping.
    // Standard-library vector alignment slack is included.
    static std::size_t preparationBytes(double sampleRate, const ReverbSettings&) noexcept;
private:
    struct State;
    std::unique_ptr<State> state;
};
}
