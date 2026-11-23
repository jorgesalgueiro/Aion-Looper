// SPDX-License-Identifier: AGPL-3.0-only
#include "Engine.h"
#include <algorithm>
#include <cmath>
namespace aion {
bool Engine::applyDelayMidi(unsigned track,unsigned slider,unsigned cc) noexcept {
 if(track>=LoopBank::trackCount||slider>=delayMidiControls.size()||cc>127||!generation()||delayMidiGeneration.load(std::memory_order_acquire)!=generation()
  ||coordinatedDelayBank.load(std::memory_order_acquire)||pendingArpBank.active||pendingDelayBank->active||(*pendingDelay)[track].active||!delayMidiResults->canPush())return false;
 const auto old=loops.trackView(track);auto settings=old.delay;const auto parameter=delayMidiControls[slider];const double value=delayMidiValue(slider,cc);
 if(!setDelayControl(settings,parameter,value)||!loops.setDelay(track,settings,old.clearEpoch,old.delaySettingsRevision,old.delayRequest,true))return false;
 (void)delayMidiResults->push({track,parameter,value,generation(),old.clearEpoch,old.delaySettingsRevision,old.delaySettingsRevision+1});return true;
}
bool Engine::enqueueDelayEdit(LoopCommand command,std::uint64_t& transaction) noexcept {
 if(command.action!=LoopAction::delay||command.track>=LoopBank::trackCount||!command.requestId
  ||!command.generation||command.generation!=generation()||command.arpTransaction||command.delayTransaction
  ||!std::isfinite(command.value)||!validDelaySettings(command.delay)||command.delaySettingsRevision==UINT64_MAX
  ||coordinatedDelayBank.load(std::memory_order_acquire)||outstandingDelayBank||outstandingDelay[command.track]||nextDelayTransaction==UINT64_MAX)return false;
 if(!delayRequests->canPush()||!loopCommands->canPush())return false;
 command.delayTransaction=nextDelayTransaction+1;
 DelayEditRequest request{command.track,command.requestId,command.generation,command.delayClearEpoch,
  command.delaySettingsRevision,command.delayTransaction,delayCancellations[command.track].load(std::memory_order_acquire)};
 // Sole producer of both queues. Audio can only free room after preflight.
 // Register the never-reused identity before the FIFO command is published.
 (void)delayRequests->push(request);(void)loopCommands->push(command);
 nextDelayTransaction=command.delayTransaction;outstandingDelay[command.track]=true;
 transaction=command.delayTransaction;return true;
}
bool Engine::popDelayEdit(DelayEditResult& result) noexcept {
 if(!delayResults->pop(result))return false;
 outstandingDelay[result.track]=false;return true;
}
void Engine::cancelDelayEdit(unsigned track) noexcept {
 if(track<=LoopBank::trackCount)cancelDelayBank();
 if(track<LoopBank::trackCount)delayCancellations[track].fetch_add(1,std::memory_order_release);
 else if(track==LoopBank::trackCount)for(auto& serial:delayCancellations)serial.fetch_add(1,std::memory_order_release);
}
void Engine::finishDelayEdit(unsigned id,DelayEditStatus status) noexcept {
 auto& p=(*pendingDelay)[id];
 if(!p.finished){const auto& r=p.request;const auto info=loops.trackView(id);
  p.completion={id,r.requestId,r.generation,status,r.transaction,info.delaySettingsRevision,info.clearEpoch,frame};p.finished=true;
 }
 // At most twenty accepted/unread edits exist; the 31-result queue exceeds
 // that bound. Retain the immutable receipt on unexpected capacity failure.
 if(delayResults->push(p.completion))p={};
}
void Engine::cancelStaleDelayEdits() noexcept {
 cancelStaleDelayBank();
 for(unsigned id=0;id<LoopBank::trackCount;++id){auto& p=(*pendingDelay)[id];if(!p.active)continue;
  if(p.finished){finishDelayEdit(id,p.completion.status);continue;}
  const auto& r=p.request;const auto info=loops.trackView(id);
  const bool cancelled=r.generation!=generation()||r.cancellation!=delayCancellations[id].load(std::memory_order_acquire);
  const bool stale=r.clearEpoch!=info.clearEpoch||r.settingsRevision!=info.delaySettingsRevision;
  if(cancelled||stale){if(p.preparing)loops.cancelDelay(id);finishDelayEdit(id,cancelled?DelayEditStatus::cancelled:DelayEditStatus::stale);}
 }
}
void Engine::serviceDelayEdits() noexcept {
 serviceDelayBank();
 // Called after LoopBank adoption, before ordinary FIFO commands. An already
 // committed success is terminal even if a later cancellation arrives now.
 for(unsigned id=0;id<LoopBank::trackCount;++id){auto& p=(*pendingDelay)[id];
  if(!p.active||p.finished||!p.preparing)continue;
  const auto view=loops.delayStorageView(id);if(view.pending)continue;
  const auto info=loops.trackView(id);
  const bool own=view.active&&view.installation!=p.installation&&info.delayRequest==p.request.requestId
   &&info.clearEpoch==p.request.clearEpoch&&info.delaySettingsRevision==p.request.settingsRevision+1;
  finishDelayEdit(id,own?DelayEditStatus::ready:view.failed?DelayEditStatus::failed:DelayEditStatus::cancelled);
 }
 DelayEditRequest request;
 for(unsigned n=0;n<LoopBank::trackCount&&delayRequests->pop(request);++n){
  seenDelayTransaction=std::max(seenDelayTransaction,request.transaction);
  auto& p=(*pendingDelay)[request.track];p={};p.request=request;p.active=true;
  const auto info=loops.trackView(request.track);
  if(request.generation!=generation()||request.cancellation!=delayCancellations[request.track].load(std::memory_order_acquire))
   finishDelayEdit(request.track,DelayEditStatus::cancelled);
  else if(request.clearEpoch!=info.clearEpoch||request.settingsRevision!=info.delaySettingsRevision)
   finishDelayEdit(request.track,DelayEditStatus::stale);
 }
}
bool Engine::currentDelayEdit(const LoopCommand& command) const noexcept {
 if(command.track>=LoopBank::trackCount)return false;
 const auto& p=(*pendingDelay)[command.track];
 return p.active&&!p.finished&&p.request.transaction==command.delayTransaction&&p.request.requestId==command.requestId
  &&p.request.generation==command.generation&&command.generation==generation()
  &&p.request.cancellation==delayCancellations[command.track].load(std::memory_order_acquire);
}
void Engine::applyDelayEdit(const LoopCommand& command) noexcept {
 auto& p=(*pendingDelay)[command.track];
 if(!currentDelayEdit(command)){
  if(p.active&&!p.finished&&p.request.transaction==command.delayTransaction)finishDelayEdit(command.track,DelayEditStatus::cancelled);
  return;
 }
 const auto info=loops.trackView(command.track);
 if(info.clearEpoch!=command.delayClearEpoch||info.delaySettingsRevision!=command.delaySettingsRevision){finishDelayEdit(command.track,DelayEditStatus::stale);return;}
 if(loops.setDelay(command.track,command.delay,command.delayClearEpoch,command.delaySettingsRevision,command.requestId)){
  finishDelayEdit(command.track,DelayEditStatus::ready);return;
 }
 if(!command.delay.enabled){finishDelayEdit(command.track,DelayEditStatus::failed);return;}
 p.installation=loops.delayStorageView(command.track).installation;
 if(!loops.requestDelay(command.track,command.delay,command.delayClearEpoch,command.delaySettingsRevision,command.requestId)){
  finishDelayEdit(command.track,DelayEditStatus::busy);return;
 }
 p.preparing=true;
}
void Engine::cancelAllDelayEdits() noexcept {
 // Stopped prepare temporarily owns the audio endpoints. Keep unread old-
 // generation receipts and message outstanding flags until actually consumed.
 cancelDelayEdit();cancelStaleDelayEdits();DelayEditRequest request;
 cancelQueuedDelayBank();
 for(unsigned n=0;n<LoopBank::trackCount&&delayRequests->pop(request);++n){
  seenDelayTransaction=std::max(seenDelayTransaction,request.transaction);
  auto& p=(*pendingDelay)[request.track];p={};p.request=request;p.active=true;
  finishDelayEdit(request.track,DelayEditStatus::cancelled);
 }
}
}
