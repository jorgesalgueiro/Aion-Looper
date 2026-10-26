#pragma once
#include "core/LoopMeterObservation.h"
#include <limits>

namespace aion {
struct MeterCleanupRequest {
    std::uint64_t serial=0;
    std::uint32_t generation=0;
    double delay=-1;
    bool operator==(const MeterCleanupRequest&) const noexcept = default;
};
// One message owner per track, sharing Engine's existing single command producer.
// Keep this owner for the Engine lifetime; do not recreate it per editor.
// Explicit notifications only: no source/view polling, timers or clock queries.
// Successful enqueue is not audio acceptance; observe requires its window receipt.
class LoopMeterLifecycle {
public:
    LoopMeterLifecycle(Engine& audio,std::uint32_t track) noexcept :engine(audio),id(track){reset();}
    LoopMeterLifecycle(const LoopMeterLifecycle&)=delete;
    LoopMeterLifecycle& operator=(const LoopMeterLifecycle&)=delete;
    // Call only after the Engine prepare barrier; same-generation reset is a no-op.
    bool reset() noexcept {
        const auto next=engine.generation();if(next==generation)return false;
        generation=next;ticket=0;graph=LoopMeterGraph{};receipt={};cleanup={};
        // Keep timer serials across prepare; old delivered callbacks cannot alias.
        return true;
    }
    bool setBuffer(bool available) noexcept {if(!current())return false;graph.setBuffer(available);return true;}
    bool setState(MeterLoopState state) noexcept {
        if(!current()||static_cast<unsigned>(state)>static_cast<unsigned>(MeterLoopState::multiplying))return false;
        graph.setState(state);return true;
    }
    bool start(std::uint64_t frame) noexcept {
        if(!observable()||ticket==std::numeric_limits<std::uint32_t>::max())return false;
        auto candidate=graph;
        if(!candidate.startGraph())return false;
        LoopCommand command{LoopAction::meterGraphStart,id,0,generation,frame,0};
        command.meterGraphTicket=ticket+1;
        if(!engine.enqueueLoop(command))return false; // No graph/timer/follower mutation on saturation.
        graph=candidate;++ticket;
        const auto identity=graph.inspect();receipt={generation,id,ticket,identity.analyser,identity.data};
        if(identity.teardownDelay<0)cleanup={};
        // Original queued->play retains the old stop timer; stopping->play cancels it.
        return true;
    }
    bool stop(double now,double scheduled=0,bool hasVolume=true) noexcept {
        if(!current())return false;
        auto candidate=graph;
        if(!candidate.stop(now,scheduled,hasVolume))return false;
        const auto delay=candidate.inspect().teardownDelay;
        if(!std::isfinite(delay)||(delay>=0&&timerSerial==std::numeric_limits<std::uint64_t>::max()))return false;
        graph=candidate;cleanup=delay>=0?MeterCleanupRequest{++timerSerial,generation,delay}:MeterCleanupRequest{};
        if(!graph.inspect().analyser)receipt={};
        return true;
    }
    bool deliverCleanup(MeterCleanupRequest delivered) noexcept {
        if(!current()||!cleanup.serial||delivered!=cleanup)return false;
        return teardown();
    }
    bool teardown() noexcept {
        if(!current())return false;
        graph.teardown();receipt={};cleanup={};return true;
    }
    bool clear() noexcept {
        if(!current())return false;
        graph.clear();receipt={};cleanup={};return true; // Ticket high-water survives clear.
    }
    bool destroyGraph() noexcept {if(!current())return false;graph.destroyGraph();return true;}
    bool request() noexcept {
        const auto identity=graph.inspect();
        return observable()&&receipt.ticket&&identity.buffer&&identity.present&&!identity.destroyed
            &&LoopMeterGraph::activeState(identity.state)&&identity.analyser==receipt.analyser&&identity.data==receipt.data
            &&engine.requestLoopMeterWindow(id,generation,receipt.ticket);
    }
    bool observe(const MeterWindowEvent& packet,const EngineView& view,const CallbackClockStamp& clock,CallbackAudibleTime time) noexcept {
        return commitLoopMeterObservation(graph,
            [&]() noexcept {return observable()&&engine.loopMeterGraphCurrent(generation,id,receipt.ticket);},
            [&](LoopMeterGraph& candidate) noexcept {return observeBoundLoopMeterWindow(candidate,receipt,packet,view,clock,time);});
    }
    // Explicit message-owner observation when the original main-loop gate would
    // not read an analyser. Supplied context/audible clocks only; never infer a
    // stopped device from missing PCM or use this to conceal a stale packet.
    bool observeUnavailable(double now,double playbackTime,std::uint64_t context,bool running) noexcept {
        if(!std::isfinite(now)||!std::isfinite(playbackTime))return false;
        const auto identity=graph.inspect();
        const bool needsWindow=running&&context&&LoopMeterGraph::activeState(identity.state)
            &&identity.present&&!identity.destroyed&&identity.analyser&&identity.data;
        if(needsWindow)return false; // A live attached graph must take the bound PCM path.
        return commitLoopMeterObservation(graph,
            [&]() noexcept {return observable()&&(!receipt.ticket||engine.loopMeterGraphCurrent(generation,id,receipt.ticket));},
            [&](LoopMeterGraph& candidate) noexcept {return candidate.observe({},0,now,playbackTime,context,running);});
    }
    LoopMeterGraphView inspect() const noexcept {return graph.inspect();}
    const MeterReading& value() const noexcept {return graph.value();}
    LoopMeterBinding binding() const noexcept {return receipt;}
    MeterCleanupRequest cleanupRequest() const noexcept {return cleanup;}
private:
    bool current() const noexcept {return id<LoopBank::trackCount&&generation&&generation==engine.generation();}
    // Event loss invalidates retained graph/window receipts for this generation.
    // Recheck generation around the stream query; cleanup metadata still uses
    // current() so telemetry loss cannot strand an explicit stop/clear/timer.
    bool observable() const noexcept {return current()&&engine.meterGraphEventsCurrent()&&current();}
    Engine& engine;
    std::uint32_t id=0,generation=0,ticket=0;
    std::uint64_t timerSerial=0;
    LoopMeterGraph graph;
    LoopMeterBinding receipt{};
    MeterCleanupRequest cleanup{};
};
}
