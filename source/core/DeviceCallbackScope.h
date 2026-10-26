#pragma once
#include "core/CallbackClock.h"
#include "core/DeviceDeliveryScope.h"

namespace aion {
// One audio thread's current Standalone IO dispatch. No owner pointer escapes in
// telemetry. Nested callbacks restore the previous owner's consumption state.
class DeviceCallbackScope final {
    struct Context {
        const DeviceCallbackBinding* owner;
        DeviceCallbackObservation observation;
        bool consumed;
        std::uint32_t epoch;
        const void* deviceOwner;
    };
    inline static thread_local constinit Context current{nullptr,{},false,0,nullptr};
    inline static thread_local constinit std::uint64_t serial=0;
    Context previous;
public:
    // Production uses braced initialization: snapshot epoch BEFORE reading the
    // counter, so prepare during either operation invalidates the observation.
    DeviceCallbackScope(const DeviceCallbackBinding* owner,int frames,std::uint32_t epoch,double seconds,const void* deviceOwner=nullptr) noexcept : previous(current) {
        if(++serial==0)++serial;
        const DeviceCallbackObservation candidate{seconds,serial,frames>0?static_cast<std::uint32_t>(frames):0,true,DeviceDeliveryScope::read(deviceOwner,frames)};
        current={owner,validatedDeviceCallback(candidate,frames),false,epoch,deviceOwner};
    }
    ~DeviceCallbackScope() noexcept {current=previous;}
    DeviceCallbackScope(const DeviceCallbackScope&)=delete;
    DeviceCallbackScope& operator=(const DeviceCallbackScope&)=delete;
    static DeviceCallbackObservation take(const DeviceCallbackBinding* owner,int frames,std::uint32_t epoch) noexcept {
        if(!owner||current.owner!=owner||current.consumed||!epoch||current.epoch!=epoch||owner->epoch()!=epoch)return {};
        current.consumed=true;
        auto result=validatedDeviceCallback(current.observation,frames);
        if(result.delivery!=DeviceDeliveryScope::read(current.deviceOwner,frames))result.delivery={};
        return result;
    }
};
}
