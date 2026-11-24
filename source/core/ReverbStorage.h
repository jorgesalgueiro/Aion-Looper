// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Reverb.h"
#include <optional>

namespace aion {
// Internal convolution ownership layer, following Aion's DelayStorage pattern.
// Audio owner: request/cancel/poll/controls/process. Sole worker: serviceWorker.
// Construction, destruction, prepareStopped/releaseStopped require BOTH owners
// quiescent. No product LoopBank/GUI/CLI binding or Machine engine is connected.
// FFT credits: HiFi-LoFi AudioFFT / Takuya Ooura, retained in CREDITS.md.
class ReverbStorage {
public:
    static constexpr unsigned trackCount = 20, capacity = 40;
    static constexpr std::size_t defaultBudget = 256u * 1024u * 1024u;
    struct View {
        bool active = false, pending = false, failed = false, transitioning = false;
        std::uint64_t installation = 0;
        std::uint32_t seed = 0;
    };
    struct Completion { std::uint32_t installed = 0, failed = 0; };
    explicit ReverbStorage(std::size_t byteBudget = defaultBudget);
    ReverbStorage(const ReverbStorage&) = delete;
    ReverbStorage& operator=(const ReverbStorage&) = delete;
    bool prepareStopped(double sampleRate) noexcept;
    void releaseStopped() noexcept;
    // Includes temporary stereo IR and full new stereo graph. A reservation is
    // held through retirement/reclamation, not released at audio cancellation.
    static std::size_t reservationBytes(double sampleRate, const ReverbSettings&) noexcept;
    bool request(unsigned track, const ReverbSettings&, std::uint32_t seed) noexcept;
    void cancel(unsigned track) noexcept;
    void clear(unsigned track) noexcept;
    void clearAll() noexcept;
    void reset(unsigned track) noexcept; // retain current graph, discard all history/pending work
    Completion poll() noexcept;
    // Continuous controls 0/1/4/5 only; shape controls 2/3 need a new request.
    // Per-control revision fences replay only edits accepted AFTER a request.
    bool setControl(unsigned track, unsigned control, double value) noexcept;
    bool setEnabled(unsigned track, bool enabled) noexcept;
    bool process(unsigned track, std::span<const float> left, std::span<const float> right,
                 std::span<float> outLeft, std::span<float> outRight) noexcept;
    View view(unsigned track) const noexcept;
    const Reverb* graph(unsigned track) const noexcept; // audio-only borrow, never mutate/retain across lifecycle calls
    std::size_t reservedBytes() const noexcept; // audio-only conservative reservation, not RSS
    void serviceWorker() noexcept;
private:
    enum class Phase : unsigned { free, queued, preparing, ready, failed, active, retired };
    static_assert(std::atomic<Phase>::is_always_lock_free);
    static_assert(std::atomic<bool>::is_always_lock_free);
    static constexpr std::array<unsigned, 4> liveControls{0, 1, 4, 5};
    struct Intent {
        std::array<std::uint64_t, 5> revisions{};
        std::array<double, 4> values{};
        bool enabled = false;
    };
    struct Slot {
        std::atomic<Phase> phase{Phase::free};
        std::atomic<bool> cancelled{false};
        unsigned track = 0;
        std::size_t bytes = 0;
        std::uint32_t seed = 0;
        std::uint64_t request = 0;
        ReverbSettings settings{};
        std::array<std::uint64_t, 5> revisions{};
        std::optional<Reverb> graph; // worker constructs/retires, audio exclusively owns ready/active
    };
    std::unique_ptr<std::array<Slot, capacity>> slots;
    std::array<Slot*, trackCount> active{}, pending{}, previous{};
    std::array<unsigned, trackCount> remaining{};
    std::array<bool, trackCount> failed{};
    std::array<std::uint64_t, trackCount> installations{};
    std::array<Intent, trackCount> intents{};
    std::array<std::array<float, 128>, 4> scratch{};
    const std::size_t budget;
    double rate = 0;
    unsigned cursor = 0, fadeFrames = 0;
    std::uint64_t serial = 0;
};
}
