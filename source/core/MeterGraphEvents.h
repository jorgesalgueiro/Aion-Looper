#pragma once
#include "core/SpscQueue.h"
#include <limits>
namespace aion {
// Ordered audio-owner facts. Only immediatePlay explicitly proves a new graph;
// a raw started event or a buffer installation never implies analyser creation.
enum class MeterGraphEventKind : std::uint8_t { started, cleared, bufferInstalled, immediatePlay, immediateStop, scheduledStop, stopAll, queuedPlay, queuedCancel, queuedStop, queuedWindowRejected, queuedWindowAccepted };
// Component scopes retain raw-window/buffer-only contracts. Production adds
// accepted immediate playback once at construction; never switch at runtime.
enum class MeterEventScope : std::uint8_t { rawWindows, committedBuffers, acceptedPlayback };
struct MeterGraphEvent {
    std::uint64_t serial=0,frame=0,clearEpoch=0;
    std::uint32_t generation=0,track=0,ticket=0;
    MeterGraphEventKind kind=MeterGraphEventKind::cleared;
    std::uint32_t retiredTicket=0; // Actual window removed by clear; never a JS graph inference.
    std::uint64_t bufferId=0; // Installed immutable clip, or the clip removed by clear. Never an analyser ID.
    double stopLeadSeconds=0; // Scheduled stop delta at audio acceptance, before sample deadline rounding. Not wall time.
    std::uint64_t queueId=0; // Accepted queued intent, independent of analyser/raw-window tickets.
    double queueDelaySeconds=0; // max(0, unrounded grid delta - 100ms), only on queuedPlay. Not wall time.
};
// Heap-owned SPSC stream. Any loss invalidates the generation until prepare.
// Never silently resynchronise from later events/snapshots or reset queue indices.
class MeterGraphEvents {
public:
    static constexpr unsigned queueSize=256,trackCount=20;
    void reset(std::uint32_t generation) noexcept {
        active.store(0,std::memory_order_release);
        serial=0;lost.store(0,std::memory_order_release);
        for(auto& word:windows)word.store(0,std::memory_order_release);
        active.store(generation,std::memory_order_release);
    }
    void publish(MeterGraphEventKind kind,std::uint32_t track,std::uint32_t ticket,
        std::uint64_t frame,std::uint64_t clearEpoch,std::uint64_t bufferId=0,double stopLeadSeconds=0,std::uint64_t queueId=0,double queueDelaySeconds=0) noexcept {
        const auto generation=active.load(std::memory_order_acquire);
        if(!generation||lost.load(std::memory_order_acquire))return;
        if(track>=trackCount){lost.store(generation,std::memory_order_release);return;}
        const auto previous=windows[track].load(std::memory_order_relaxed); // Same audio publisher.
        const auto retired=kind==MeterGraphEventKind::cleared&&(previous>>32)==generation?
            static_cast<std::uint32_t>(previous):0u;
        const auto currentTicket=kind==MeterGraphEventKind::started||kind==MeterGraphEventKind::immediatePlay?ticket:0;
        if(kind==MeterGraphEventKind::started||kind==MeterGraphEventKind::immediatePlay||kind==MeterGraphEventKind::cleared)
            windows[track].store((static_cast<std::uint64_t>(generation)<<32)|currentTicket,std::memory_order_release);
        if(serial==std::numeric_limits<std::uint64_t>::max()||
            !events.push({++serial,frame,clearEpoch,generation,track,ticket,kind,retired,bufferId,stopLeadSeconds,queueId,queueDelaySeconds}))
            lost.store(generation,std::memory_order_release);
    }
    bool current(std::uint32_t generation) const noexcept {
        return generation&&active.load(std::memory_order_acquire)==generation&&
            lost.load(std::memory_order_acquire)==0;
    }
    void invalidate() noexcept {const auto generation=active.load(std::memory_order_acquire);if(generation)lost.store(generation,std::memory_order_release);}
    // Message owner: freshness of this explicit raw-window identity, independent
    // of event consumption. This does not infer a source/analyser lifecycle.
    bool owns(std::uint32_t generation,unsigned track,std::uint32_t ticket) const noexcept {
        if(track>=trackCount||!ticket||!current(generation))return false;
        const auto expected=(static_cast<std::uint64_t>(generation)<<32)|ticket;
        return windows[track].load(std::memory_order_acquire)==expected&&current(generation);
    }
    bool pop(MeterGraphEvent& output,std::uint32_t generation) noexcept {
        unsigned budget=queueSize-1;return pop(output,generation,budget);
    }
    // Budget counts raw dequeues, including stale/lost receipts. A stale caller
    // must leave future-generation entries queued for the correct owner.
    bool pop(MeterGraphEvent& output,std::uint32_t generation,unsigned& budget) noexcept {
        MeterGraphEvent event;
        while(budget&&events.popIf(event,[&]() noexcept {return generation&&active.load(std::memory_order_acquire)==generation;})){
            --budget;
            if(event.generation==generation&&current(generation)){
                output=event;return true;
            }
        }
        return false;
    }
private:
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,"Window freshness tokens must not lock");
    std::array<std::atomic<std::uint64_t>,trackCount> windows{};
    std::uint64_t serial=0; // Audio owner only, reset at prepare barrier.
    std::atomic<std::uint32_t> active{0},lost{0};
    SpscQueue<MeterGraphEvent,queueSize> events;
};
}
