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
        return reset(engine.generation());
    }
    // A processor service pins the completed prepare it observed. Never adopt
    // a newer Engine generation that may still be inside another prepare.
    bool reset(std::uint32_t next) noexcept {
        if(next!=engine.generation()||next==generation)return false;
        generation=next;ticket=0;lastClearSerial=lastBufferSerial=lastPlaySerial=lastStopSerial=lastQueueSerial=bufferId=queuedId=queueHighWater=0;bufferNoticeSinceStart=true;graph=LoopMeterGraph{};receipt={};cleanup={};
        // Keep timer serials across prepare; old delivered callbacks cannot alias.
        return true;
    }
    bool setBuffer(bool available) noexcept {
        if(!current())return false;
        // A boolean notification cannot prove that the committed buffer is the
        // one an older raw-window clear removed. Require fresh graph ownership
        // or an explicitly ordered clear after any intervening buffer notice.
        graph.setBuffer(available);bufferId=0;bufferNoticeSinceStart=true;return true;
    }
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
        graph=candidate;++ticket;queuedId=0;bufferNoticeSinceStart=false;
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
        graph=candidate;queuedId=0;cleanup=delay>=0?MeterCleanupRequest{++timerSerial,generation,delay}:MeterCleanupRequest{};
        if(!graph.inspect().analyser)receipt={};
        return true;
    }
    bool cleanupCurrent(MeterCleanupRequest delivered) const noexcept {
        return current()&&cleanup.serial&&delivered==cleanup;
    }
    bool deliverCleanup(MeterCleanupRequest delivered) noexcept {
        if(!cleanupCurrent(delivered))return false;
        return teardown();
    }
    bool teardown() noexcept {
        if(!current())return false;
        graph.teardown();receipt={};cleanup={};return true;
    }
    bool clear() noexcept {
        if(!current())return false;
        graph.clear();bufferId=queuedId=0;receipt={};cleanup={};return true; // Ticket high-water survives clear.
    }
    // Sole stream consumer supplies the actual installed immutable clip receipt.
    // A buffer change does not create/reset/detach an analyser or stop timer.
    bool consumeBuffer(const MeterGraphEvent& event) noexcept {
        const auto matches=[&]() noexcept {
            return observable()&&event.kind==MeterGraphEventKind::bufferInstalled&&event.generation==generation
                &&event.track==id&&event.serial>lastSerial()&&event.bufferId
                &&!event.ticket&&!event.retiredTicket;
        };
        if(!commitLoopMeterObservation(graph,matches,[](LoopMeterGraph& candidate) noexcept {candidate.setBuffer(true);return true;}))return false;
        bufferId=event.bufferId;lastBufferSerial=event.serial;bufferNoticeSinceStart=true;return true;
    }
    // Ordered proof of successful immediate play, separate from raw starts.
    // Consume historical receipts in order even if a newer audio window exists;
    // observe() independently rejects PCM until the current binding matches.
    bool consumeImmediatePlay(const MeterGraphEvent& event) noexcept {
        const auto matches=[&]() noexcept {
            return observable()&&event.kind==MeterGraphEventKind::immediatePlay&&event.generation==generation
                &&event.track==id&&event.serial>lastSerial()&&event.bufferId&&event.bufferId==bufferId
                &&event.ticket>ticket&&!event.retiredTicket;
        };
        if(!commitLoopMeterObservation(graph,matches,[](LoopMeterGraph& candidate) noexcept {
            // Immediate play from stopped/stopping replaces any previous graph
            // and cancels its cleanup, retaining the original meter history.
            candidate.setState(MeterLoopState::stopping);return candidate.startGraph();
        }))return false;
        ticket=event.ticket;lastPlaySerial=event.serial;queuedId=0;bufferNoticeSinceStart=false;cleanup={};
        const auto identity=graph.inspect();receipt={generation,id,ticket,identity.analyser,identity.data};return true;
    }
    bool consumeImmediateStop(const MeterGraphEvent& event) noexcept {
        return event.stopLeadSeconds==0&&consumeStop(event,MeterGraphEventKind::immediateStop,0);
    }
    bool consumeScheduledStop(const MeterGraphEvent& event) noexcept {
        return std::isfinite(event.stopLeadSeconds)&&event.stopLeadSeconds>0
            &&std::isfinite(event.stopLeadSeconds+.05)&&consumeStop(event,MeterGraphEventKind::scheduledStop,event.stopLeadSeconds);
    }
    bool consumeStopAll(const MeterGraphEvent& event) noexcept {
        // LoopManager.stopAll calls Loop.stop(now + .015) for active graphs.
        // Repeated panic while message state is stopping replaces its timer;
        // native audio may already be stopped. Detached owners remain no-ops.
        return graph.inspect().state!=MeterLoopState::queued&&event.stopLeadSeconds==.015
            &&consumeStop(event,MeterGraphEventKind::stopAll,.015);
    }
    bool consumeQueuedPlay(const MeterGraphEvent& event) noexcept {
        const auto matches=[&]() noexcept {return queueMatches(event,MeterGraphEventKind::queuedPlay)&&event.queueId>queueHighWater;};
        if(!commitLoopMeterObservation(graph,matches,[](LoopMeterGraph& candidate) noexcept {candidate.setState(MeterLoopState::queued);return true;}))return false;
        queuedId=queueHighWater=event.queueId;lastQueueSerial=event.serial;return true;
    }
    bool consumeQueuedCancel(const MeterGraphEvent& event) noexcept {
        const auto matches=[&]() noexcept {return queueMatches(event,MeterGraphEventKind::queuedCancel)&&event.queueId==queuedId&&graph.inspect().state==MeterLoopState::queued;};
        if(!commitLoopMeterObservation(graph,matches,[](LoopMeterGraph& candidate) noexcept {candidate.setState(MeterLoopState::stopped);return true;}))return false;
        queuedId=0;lastQueueSerial=event.serial;return true; // Retain old cleanup, analyser/data and history.
    }
    bool consumeQueuedStop(const MeterGraphEvent& event) noexcept {
        const auto matches=[&]() noexcept {return queueMatches(event,MeterGraphEventKind::queuedStop)&&event.queueId==queuedId&&graph.inspect().state==MeterLoopState::queued;};
        bool arm=false;
        if(!commitLoopMeterObservation(graph,matches,[&](LoopMeterGraph& candidate) noexcept {
            if(!candidate.stop(0))return false;
            arm=candidate.inspect().teardownDelay>=0;
            return !arm||timerSerial!=std::numeric_limits<std::uint64_t>::max();
        }))return false;
        queuedId=0;lastQueueSerial=event.serial;
        cleanup=arm?MeterCleanupRequest{++timerSerial,generation,graph.inspect().teardownDelay}:MeterCleanupRequest{};
        if(!graph.inspect().analyser)receipt={};return true;
    }
private:
    bool queueMatches(const MeterGraphEvent& event,MeterGraphEventKind kind) const noexcept {
        return observable()&&event.kind==kind&&event.generation==generation&&event.track==id
            &&event.serial>lastSerial()&&event.bufferId&&event.bufferId==bufferId
            &&(event.ticket?event.ticket==ticket:!graph.inspect().present&&!receipt.ticket)
            &&!event.retiredTicket&&event.stopLeadSeconds==0&&event.queueId;
    }
    bool consumeStop(const MeterGraphEvent& event,MeterGraphEventKind kind,double lead) noexcept {
        const auto matches=[&]() noexcept {
            return observable()&&event.kind==kind&&event.generation==generation
                &&event.track==id&&event.serial>lastSerial()&&event.bufferId&&event.bufferId==bufferId
                &&event.ticket&&event.ticket==ticket&&!event.retiredTicket;
        };
        bool arm=false;
        if(!commitLoopMeterObservation(graph,matches,[&](LoopMeterGraph& candidate) noexcept {
            if(!candidate.stop(0,lead))return !candidate.inspect().present&&candidate.inspect().state==MeterLoopState::stopped;
            arm=candidate.inspect().teardownDelay>=0;
            return !arm||timerSerial!=std::numeric_limits<std::uint64_t>::max();
        }))return false;
        lastStopSerial=event.serial;queuedId=0;
        cleanup=arm?MeterCleanupRequest{++timerSerial,generation,graph.inspect().teardownDelay}:MeterCleanupRequest{};
        if(!graph.inspect().analyser)receipt={};
        return true;
    }
public:
    // Sole message consumer supplies an actual Engine-pop receipt. Never route
    // a raw clear to an unaccepted/pending or replacement graph by track alone.
    bool consumeClear(const MeterGraphEvent& event) noexcept {
        const auto matches=[&]() noexcept {
            return observable()&&event.kind==MeterGraphEventKind::cleared&&event.generation==generation
                &&event.track==id&&event.serial>lastSerial()&&event.ticket==0
                &&(bufferId?event.bufferId==bufferId:
                    event.retiredTicket&&event.retiredTicket==ticket&&!bufferNoticeSinceStart);
        };
        if(!commitLoopMeterObservation(graph,matches,[](LoopMeterGraph& candidate) noexcept {candidate.clear();return true;}))return false;
        lastClearSerial=event.serial;bufferId=queuedId=0;receipt={};cleanup={};return true;
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
    std::uint64_t committedBuffer() const noexcept {return bufferId;}
    std::uint64_t queuedIntentId() const noexcept {return queuedId;}
    bool inGeneration(std::uint32_t completed) const noexcept {return completed&&completed==generation&&current();}
    bool routesTo(const Engine& source,unsigned track) const noexcept {return &engine==&source&&id==track&&current();}
private:
    std::uint64_t lastSerial() const noexcept {return std::max({lastClearSerial,lastBufferSerial,lastPlaySerial,lastStopSerial,lastQueueSerial});}
    bool current() const noexcept {return id<LoopBank::trackCount&&generation&&generation==engine.generation();}
    // Event loss invalidates retained graph/window receipts for this generation.
    // Recheck generation around the stream query; cleanup metadata still uses
    // current() so telemetry loss cannot strand an explicit stop/clear/timer.
    bool observable() const noexcept {return current()&&engine.meterGraphEventsCurrent()&&current();}
    Engine& engine;
    std::uint32_t id=0,generation=0,ticket=0;
    std::uint64_t timerSerial=0,lastClearSerial=0,lastBufferSerial=0,lastPlaySerial=0,lastStopSerial=0,bufferId=0;
    std::uint64_t lastQueueSerial=0,queuedId=0,queueHighWater=0;
    bool bufferNoticeSinceStart=true;
    LoopMeterGraph graph;
    LoopMeterBinding receipt{};
    MeterCleanupRequest cleanup{};
};
}
