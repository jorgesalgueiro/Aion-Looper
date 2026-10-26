#pragma once
#include "core/SpscQueue.h"
#include <limits>
namespace aion {
// Facts emitted at the actual raw-window mutation, not inferred graph births.
enum class MeterGraphEventKind : std::uint8_t { started, cleared };
struct MeterGraphEvent {
    std::uint64_t serial=0,frame=0,clearEpoch=0;
    std::uint32_t generation=0,track=0,ticket=0;
    MeterGraphEventKind kind=MeterGraphEventKind::cleared;
};
// Heap-owned SPSC stream. Any loss invalidates the generation until prepare.
// Never silently resynchronise from later events/snapshots or reset queue indices.
class MeterGraphEvents {
public:
    static constexpr unsigned queueSize=256;
    void reset(std::uint32_t generation) noexcept {
        active.store(0,std::memory_order_release);
        serial=0;lost.store(0,std::memory_order_release);
        active.store(generation,std::memory_order_release);
    }
    void publish(MeterGraphEventKind kind,std::uint32_t track,std::uint32_t ticket,
        std::uint64_t frame,std::uint64_t clearEpoch) noexcept {
        const auto generation=active.load(std::memory_order_acquire);
        if(!generation||lost.load(std::memory_order_acquire))return;
        if(serial==std::numeric_limits<std::uint64_t>::max()||
            !events.push({++serial,frame,clearEpoch,generation,track,ticket,kind}))
            lost.store(generation,std::memory_order_release);
    }
    bool current(std::uint32_t generation) const noexcept {
        return generation&&active.load(std::memory_order_acquire)==generation&&
            lost.load(std::memory_order_acquire)==0;
    }
    bool pop(MeterGraphEvent& output,std::uint32_t generation) noexcept {
        MeterGraphEvent event;
        for(unsigned n=0;n<queueSize-1&&events.pop(event);++n){
            if(event.generation==generation&&current(generation)){
                output=event;return true;
            }
        }
        return false;
    }
private:
    std::uint64_t serial=0; // Audio owner only, reset at prepare barrier.
    std::atomic<std::uint32_t> active{0},lost{0};
    SpscQueue<MeterGraphEvent,queueSize> events;
};
}
