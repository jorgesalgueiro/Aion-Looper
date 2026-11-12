// SPDX-License-Identifier: AGPL-3.0-only
#include "Engine.h"
#include <cmath>
namespace aion {
bool Engine::enqueueArpEdit(LoopCommand command,std::uint64_t& transaction) noexcept {
 if(command.action!=LoopAction::arpDelay||command.track>=LoopBank::trackCount||!command.requestId||!command.generation||command.generation!=generation()
    ||outstandingArpBank||outstandingArp[command.track]||nextArpTransaction==UINT64_MAX||!std::isfinite(command.value)
    ||!validArpRenderSettings(command.arp)||(command.arpMask&~arpAllParameters)||(command.arpPreset&&command.arpMask!=arpAllParameters))return false;
 if(!arpRequests->canPush()||!loopCommands->canPush())return false;
 command.arpTransaction=nextArpTransaction+1;
 ArpPreparationRequest request;request.track=command.track;request.requestId=command.requestId;request.generation=command.generation;
 request.seed=command.arp.seed;request.values=command.arp.values;request.clearEpoch=command.arpClearEpoch;request.settingsRevision=command.arpSettingsRevision;
 request.activate=true;request.edit=true;request.enabled=command.arp.enabled;request.preset=command.arpPreset;request.mask=command.arpMask;
 request.transaction=command.arpTransaction;request.cancellation=arpCancellations[command.track].load(std::memory_order_acquire);
 // The message owner is the only producer of BOTH queues. After both capacity
 // checks, audio can only free room. Descriptor first: dispatch can never precede
 // registration. Cancellation may finish registration before dispatch; the
 // non-reused transaction then makes that old FIFO entry inert.
 (void)arpRequests->push(request);(void)loopCommands->push(command);
 nextArpTransaction=command.arpTransaction;outstandingArp[command.track]=true;transaction=command.arpTransaction;return true;
}
bool Engine::currentArpEdit(const LoopCommand& command) const noexcept {
 if(command.track>=LoopBank::trackCount)return false;
 const auto& p=pendingArp[command.track];
 return p.active&&!p.finished&&p.started&&p.request.edit&&p.request.transaction==command.arpTransaction
     &&p.request.requestId==command.requestId&&p.request.generation==command.generation
     &&p.request.cancellation==arpCancellations[command.track].load(std::memory_order_acquire);
}
bool Engine::applyArpEdit(const LoopCommand& command) noexcept {
 if(!currentArpEdit(command)){
  // A cancellation arriving during this callback must suppress the queued
  // edit too. Older terminal identities must never complete a newer request.
  auto& p=pendingArp[command.track];
  if(p.active&&!p.finished&&p.request.edit&&p.request.transaction==command.arpTransaction)finishArpPreparation(command.track,ArpPreparationStatus::cancelled);
  return false;
 }
 const bool ok=loops.setArp(command.track,command.arp,command.arpMask,command.requestId,command.arpClearEpoch,command.arpSettingsRevision,command.arpPreset);
 finishArpPreparation(command.track,ok?ArpPreparationStatus::ready:ArpPreparationStatus::failed);return ok;
}
}
