#pragma once
#include <atomic>
#include <cstdint>
namespace aion {
class DeviceCallbackBinding final {
    std::atomic<std::uint32_t> value{0};
    inline static constinit std::atomic<std::uint32_t> configuration{0};
public:
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
    static std::uint32_t configurationEpoch() noexcept {return configuration.load(std::memory_order_acquire);}
    std::uint32_t epoch() const noexcept {return value.load(std::memory_order_acquire);}
    std::uint32_t advance() noexcept {
        // Conservatively invalidate enclosing observations before any prepare
        // work. Unrelated device preparation may suppress timing metadata only.
        configuration.fetch_add(1,std::memory_order_acq_rel);
        auto next=value.fetch_add(1,std::memory_order_acq_rel)+1;
        if(!next)next=value.fetch_add(1,std::memory_order_acq_rel)+1;
        return next;
    }
};
}
