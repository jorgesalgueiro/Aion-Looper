#pragma once
#include "core/CallbackClock.h"
#include "core/DeviceCallbackBinding.h"
namespace aion {
// Enclosing AudioIODevice callback entry and its current splitter sub-block.
// Owner pointers and lifecycle tokens are scoped, never placed in telemetry.
class DeviceDeliveryScope final {
    struct Context {
        const void* owner;
        const DeviceCallbackBinding* binding;
        DeviceDeliveryObservation observation;
        std::uint32_t epoch,configuration,blockFrames;
    };
    inline static thread_local constinit Context current{nullptr,nullptr,{},0,0,0};
    inline static thread_local constinit std::uint64_t serial=0;
    Context previous;
public:
    // Braces sequence owner epochs before the clock read, as for holder entry.
    DeviceDeliveryScope(const void* owner,const DeviceCallbackBinding* binding,int frames,
                        std::uint32_t epoch,std::uint32_t configuration,double seconds) noexcept : previous(current){
        if(++serial==0)++serial;
        const DeviceDeliveryObservation value{seconds,serial,frames>0?static_cast<std::uint32_t>(frames):0,0,true};
        current={owner,binding,validDeviceDelivery(value)?value:DeviceDeliveryObservation{},epoch,configuration,0};
    }
    ~DeviceDeliveryScope() noexcept {current=previous;}
    DeviceDeliveryScope(const DeviceDeliveryScope&)=delete;
    DeviceDeliveryScope& operator=(const DeviceDeliveryScope&)=delete;
    static DeviceDeliveryObservation read(const void* owner,int frames) noexcept {
        if(!owner||current.owner!=owner||!current.binding||!current.epoch
            ||current.binding->epoch()!=current.epoch
            ||DeviceCallbackBinding::configurationEpoch()!=current.configuration
            ||frames<=0||current.blockFrames!=static_cast<std::uint32_t>(frames)
            ||!validDeviceDelivery(current.observation)
            ||!current.observation.available)return {};
        return current.observation;
    }
    class Block final {
        DeviceDeliveryObservation previous;
        std::uint32_t previousFrames;
    public:
        Block(int offset,int frames) noexcept : previous(current.observation),previousFrames(current.blockFrames){
            const auto total=current.observation.frames;
            if(offset<0||frames<=0||static_cast<std::uint32_t>(offset)>=total
                ||static_cast<std::uint32_t>(frames)>total-static_cast<std::uint32_t>(offset)){
                current.observation={};current.blockFrames=0;
            }else{
                current.observation.frameOffset=static_cast<std::uint32_t>(offset);
                current.blockFrames=static_cast<std::uint32_t>(frames);
            }
        }
        ~Block() noexcept {current.observation=previous;current.blockFrames=previousFrames;}
        Block(const Block&)=delete;
        Block& operator=(const Block&)=delete;
    };
};
}
