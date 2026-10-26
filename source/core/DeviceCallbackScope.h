#pragma once
#include "core/CallbackClock.h"
#include <atomic>

namespace aion {
class DeviceCallbackBinding final {
    std::atomic<std::uint32_t> value{0};
public:
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
    std::uint32_t epoch() const noexcept {return value.load(std::memory_order_acquire);}
    std::uint32_t advance() noexcept {
        auto next=value.fetch_add(1,std::memory_order_acq_rel)+1;
        if(!next)next=value.fetch_add(1,std::memory_order_acq_rel)+1;
        return next;
    }
};
// One audio thread's current Standalone IO dispatch. No owner pointer escapes in
// telemetry. Nested callbacks restore the previous owner's consumption state.
class DeviceCallbackScope final {
    struct Context {
        const DeviceCallbackBinding* owner;
        DeviceCallbackObservation observation;
        bool consumed;
        std::uint32_t epoch;
    };
    inline static thread_local constinit Context current{nullptr,{},false,0};
    inline static thread_local constinit std::uint64_t serial=0;
    Context previous;
public:
    // Production uses braced initialization: snapshot epoch BEFORE reading the
    // counter, so prepare during either operation invalidates the observation.
    DeviceCallbackScope(const DeviceCallbackBinding* owner,int frames,std::uint32_t epoch,double seconds) noexcept : previous(current) {
        if(++serial==0)++serial;
        const DeviceCallbackObservation candidate{seconds,serial,frames>0?static_cast<std::uint32_t>(frames):0,true};
        current={owner,validatedDeviceCallback(candidate,frames),false,epoch};
    }
    ~DeviceCallbackScope() noexcept {current=previous;}
    DeviceCallbackScope(const DeviceCallbackScope&)=delete;
    DeviceCallbackScope& operator=(const DeviceCallbackScope&)=delete;
    static DeviceCallbackObservation take(const DeviceCallbackBinding* owner,int frames,std::uint32_t epoch) noexcept {
        if(!owner||current.owner!=owner||current.consumed||!epoch||current.epoch!=epoch||owner->epoch()!=epoch)return {};
        current.consumed=true;
        return validatedDeviceCallback(current.observation,frames);
    }
};
}
