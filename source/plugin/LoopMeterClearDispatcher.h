#pragma once
#include "plugin/LoopMeterCleanupTimer.h"

namespace aion {
struct MeterClearDispatchResult {
    unsigned examined=0,started=0,cleared=0,applied=0,rejected=0;
    bool current=false;
};
// Sole message consumer of Engine's shared raw-window stream. All twenty
// non-owning routes must exist, match their Engine/track and outlive this object.
// Only clear is applied here; start facts are counted, never inferred as births.
class LoopMeterClearDispatcher {
public:
    using Routes=std::array<LoopMeterCleanupTimer*,LoopBank::trackCount>;
    LoopMeterClearDispatcher(Engine& audio,Routes destinations) noexcept :engine(audio),routes(destinations){reset();}
    LoopMeterClearDispatcher(const LoopMeterClearDispatcher&)=delete;
    LoopMeterClearDispatcher& operator=(const LoopMeterClearDispatcher&)=delete;
    // After the Engine prepare barrier and all owner resets, not per editor.
    bool reset() noexcept {
        assertMessageThread();const auto next=engine.generation();
        if(!next||next==generation||!validRoutes()||!engine.meterGraphEventsCurrent())return false;
        generation=next;return true;
    }
    MeterClearDispatchResult drain(unsigned limit=32) noexcept {
        assertMessageThread();MeterClearDispatchResult result;
        if(!generation||generation!=engine.generation()||!validRoutes())return result;
        unsigned budget=std::min(limit,MeterGraphEvents::queueSize-1),initial=budget;
        MeterGraphEvent event;
        while(engine.popMeterGraphEvent(event,generation,budget)){
            if(generation!=engine.generation())break;
            if(event.kind==MeterGraphEventKind::started)++result.started;
            else if(event.kind==MeterGraphEventKind::cleared){
                ++result.cleared;
                if(event.track<routes.size()&&routes[event.track]->consumeClear(event))++result.applied;
                else ++result.rejected;
            }
        }
        result.examined=initial-budget;
        result.current=generation==engine.generation()&&engine.meterGraphEventsCurrent()&&validRoutes();
        return result;
    }
private:
    static void assertMessageThread() noexcept {jassert(juce::MessageManager::existsAndIsCurrentThread());}
    bool validRoutes() const noexcept {
        for(unsigned id=0;id<routes.size();++id)if(!routes[id]||!routes[id]->routesTo(engine,id))return false;
        return true;
    }
    Engine& engine;
    const Routes routes;
    std::uint32_t generation=0;
};
}
