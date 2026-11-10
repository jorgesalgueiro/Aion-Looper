// SPDX-License-Identifier: AGPL-3.0-only
#include "Engine.h"

namespace aion {
bool Engine::enqueueArpPreparation(ArpPreparationRequest request) noexcept {
    if(request.track>=LoopBank::trackCount||!request.requestId||!request.generation
       ||request.generation!=generation()||outstandingArp[request.track]||!validArpModValues(request.values))return false;
    request.cancellation=arpCancellations[request.track].load(std::memory_order_acquire);
    if(!arpRequests->push(request))return false;
    outstandingArp[request.track]=true;return true;
}
bool Engine::popArpPreparation(ArpPreparationResult& result) noexcept {
    if(!arpResults->pop(result))return false;
    outstandingArp[result.track]=false;return true;
}
void Engine::cancelArpPreparation(unsigned track) noexcept {
    if(track<LoopBank::trackCount)arpCancellations[track].fetch_add(1,std::memory_order_release);
    else if(track==LoopBank::trackCount)for(auto& revision:arpCancellations)revision.fetch_add(1,std::memory_order_release);
}
void Engine::finishArpPreparation(unsigned id,ArpPreparationStatus status) noexcept {
    auto& pending=pendingArp[id];pending.finished=true;pending.status=status;
    const auto& request=pending.request;
    // At most twenty accepted, unread transactions exist, including queued,
    // preparing and completed work. The 31-result capacity exceeds that bound.
    // Retain a terminal receipt on unexpected capacity failure; never drop/spin.
    if(arpResults->push({id,request.requestId,request.generation,status}))pending={};
}
void Engine::cancelStaleArpPreparations() noexcept {
    for(unsigned id=0;id<LoopBank::trackCount;++id){
        auto& pending=pendingArp[id];if(!pending.active)continue;
        if(pending.finished){finishArpPreparation(id,pending.status);continue;}
        const auto& request=pending.request;
        if(request.generation!=generation()||request.cancellation!=arpCancellations[id].load(std::memory_order_acquire)
           ||request.clearEpoch!=loops.trackView(id).clearEpoch){
            loops.cancelArpStorage(id);finishArpPreparation(id,ArpPreparationStatus::cancelled);
        }
    }
}
void Engine::serviceArpPreparations() noexcept {
    // LoopBank has just adopted ready owners at this callback's sample clock.
    // Success is terminal at that adoption. Later cancellation does not undo it.
    for(unsigned id=0;id<LoopBank::trackCount;++id){
        const auto& pending=pendingArp[id];if(!pending.active||pending.finished||!pending.started)continue;
        const auto view=loops.arpStorageView(id);if(view.pending)continue;
        finishArpPreparation(id,view.failed?ArpPreparationStatus::failed:
            loops.preparedArp(id)?ArpPreparationStatus::ready:ArpPreparationStatus::cancelled);
    }
    ArpPreparationRequest request;
    for(unsigned n=0;n<LoopBank::trackCount&&arpRequests->pop(request);++n){
        // The single message owner cannot reuse a track before consuming its
        // terminal result. That result is published only after its old pending
        // record completes, so a new request never overwrites a live record.
        auto& pending=pendingArp[request.track];pending={request,true,false,false};
        if(request.generation!=generation()||request.cancellation!=arpCancellations[request.track].load(std::memory_order_acquire))
            finishArpPreparation(request.track,ArpPreparationStatus::cancelled);
        else if(request.clearEpoch!=loops.trackView(request.track).clearEpoch)
            finishArpPreparation(request.track,ArpPreparationStatus::stale);
        else if(!loops.requestArpStorage(request.track,request.values,request.seed,request.clearEpoch))
            finishArpPreparation(request.track,ArpPreparationStatus::busy);
        else pending.started=true;
    }
}
void Engine::cancelAllArpPreparations() noexcept {
    // Stopped prepare takes over the sole audio consumer/producer roles. Keep
    // unread old-generation results and the message owner's outstanding flags.
    cancelArpPreparation();cancelStaleArpPreparations();
    ArpPreparationRequest request;
    for(unsigned n=0;n<LoopBank::trackCount&&arpRequests->pop(request);++n){
        pendingArp[request.track]={request,true,false,false};
        finishArpPreparation(request.track,ArpPreparationStatus::cancelled);
    }
}
PreparedArpInspection Engine::inspectPreparedArpAudioThread(unsigned track) noexcept {
    if(auto* graph=loops.preparedArp(track))return {true,graph->targets(),graph->state(),graph->position()};
    return {};
}
}
