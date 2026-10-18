#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <type_traits>
namespace aion {
template<class T, std::uint32_t N> class SpscQueue {
    static_assert(N >= 2 && (N & (N - 1)) == 0);
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
public:
    bool push(const T& value) noexcept {
        const auto w = write.load(std::memory_order_relaxed);
        const auto next = (w + 1) & (N - 1);
        if (next == read.load(std::memory_order_acquire)) return false;
        values[w] = value; write.store(next, std::memory_order_release); return true;
    }
    bool pop(T& value) noexcept {
        const auto r = read.load(std::memory_order_relaxed);
        if (r == write.load(std::memory_order_acquire)) return false;
        value = values[r]; read.store((r + 1) & (N - 1), std::memory_order_release); return true;
    }
private:
    std::array<T, N> values{};
    alignas(128) std::atomic<std::uint32_t> write{0};
    alignas(128) std::atomic<std::uint32_t> read{0};
};
}
