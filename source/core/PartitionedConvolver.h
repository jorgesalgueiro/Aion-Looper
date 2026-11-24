// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <atomic>
#include <cstddef>
#include <memory>
#include <span>

namespace aion {
// Internal, mono, zero-latency convolution primitive. Independent instances
// provide stereo; there is no hidden normalisation or IR trimming.
// prepare/destruction are worker-only and must not overlap process/reset.
// process/reset belong to one audio owner; no shared ownership or worker calls.
class PartitionedConvolver {
public:
    static constexpr std::size_t partitionFrames = 128;
    static constexpr std::size_t maximumImpulseFrames = 1920000;
    static constexpr std::size_t maximumBlockFrames = 4096;
    PartitionedConvolver() noexcept;
    ~PartitionedConvolver();
    PartitionedConvolver(const PartitionedConvolver&) = delete;
    PartitionedConvolver& operator=(const PartitionedConvolver&) = delete;

    // Failure/cancellation retains the old kernel AND its running history.
    // IR must be nonempty, finite and bounded to +/-1e6. Cancellation atomic
    // must outlive this synchronous call; publication still needs an owner fence.
    bool prepare(std::span<const float> impulse,
                 const std::atomic<bool>* cancelled = nullptr) noexcept;
    // Exact in-place or disjoint input/output only. Invalid lengths/unprepared
    // calls reject without changing history/output. Empty calls are no-ops.
    // Nonfinite input is silence; excessive finite input is limited to +/-1e6.
    bool process(std::span<const float> input, std::span<float> output) noexcept;
    // Constant-sized clearing (128-frame input/overlap), never IR-sized work.
    void reset() noexcept;
    std::size_t impulseFrames() const noexcept;
    static constexpr unsigned latencyFrames() noexcept { return 0; }
private:
    struct State;
    std::unique_ptr<State> state;
};
}
