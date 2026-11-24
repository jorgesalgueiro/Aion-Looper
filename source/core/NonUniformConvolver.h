// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "PartitionedConvolver.h"

namespace aion {
// Zero-latency 128-tap direct head + delayed 128/1024-frame FFT partitions.
// Scalar processing does not recompute partial FFTs; Engine can retain its
// sample-indexed event ordering and arbitrary per-track effect chains.
// FFT arithmetic is HiFi-LoFi AudioFFT / Takuya Ooura; full notices and pinned
// revision: third_party/audiofft/README.md, third_party/notices/AudioFFT-Ooura.txt.
// Aion owns the scheduling/history layer; no upstream convolver source copied.
// Worker-only prepare/destruction, exclusive audio-owner process/reset. The
// future graph owner must provide publication/retirement/cancellation fences.
class NonUniformConvolver {
public:
    static constexpr auto maximumImpulseFrames = PartitionedConvolver::maximumImpulseFrames;
    static constexpr auto maximumBlockFrames = PartitionedConvolver::maximumBlockFrames;
    static constexpr std::size_t tailPartitionFrames = 1024;
    static constexpr std::size_t directHeadFrames = 128;
    struct TailWork {
        std::size_t partitionProducts = 0, transforms = 0;
    };
    NonUniformConvolver() noexcept;
    ~NonUniformConvolver();
    NonUniformConvolver(const NonUniformConvolver&) = delete;
    NonUniformConvolver& operator=(const NonUniformConvolver&) = delete;
    // Same validation, atomic replacement and input protection as the uniform
    // primitive. Exact in-place or disjoint spans; invalid calls change nothing.
    bool prepare(std::span<const float>, const std::atomic<bool>* cancelled = nullptr) noexcept;
    bool process(std::span<const float>, std::span<float>) noexcept;
    bool tick(float input, float& output) noexcept;
    void reset() noexcept;
    std::size_t impulseFrames() const noexcept;
    static constexpr unsigned latencyFrames() noexcept { return 0; }
    static std::size_t preparationBytes(std::size_t impulseFrames) noexcept;
    // Audio-owner diagnostic only, not thread-safe telemetry. Counts tail work
    // during the last accepted call (not head work or a CPU deadline estimate).
    TailWork lastTailWork() const noexcept;
    TailWork lastNearWork() const noexcept;
    std::size_t lastDirectProducts() const noexcept;
private:
    struct State;
    std::unique_ptr<State> state;
};
}
