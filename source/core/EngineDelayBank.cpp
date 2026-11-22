// SPDX-License-Identifier: AGPL-3.0-only
#include "Engine.h"
#include <algorithm>
namespace aion {
bool Engine::enqueueDelayBank(DelayBankRequest request,std::uint64_t& transaction) noexcept {
 if(!request.requestId||!request.generation||request.generation!=generation()||request.transaction
  ||outstandingDelayBank||nextDelayTransaction==UINT64_MAX)return false;
 for(unsigned id=0;id<LoopBank::trackCount;++id)
  if(outstandingDelay[id]||!validDelaySettings(request.settings[id])||request.settingsRevisions[id]==UINT64_MAX)return false;
 if(!delayBankRequests->canPush()||!loopCommands->canPush())return false;
 request.transaction=nextDelayTransaction+1;request.cancellation=delayBankCancellation.load(std::memory_order_acquire);
 LoopCommand command{LoopAction::delayBank,0,request.requestId,request.generation,request.frame};command.delayTransaction=request.transaction;
 (void)delayBankRequests->push(request);(void)loopCommands->push(command);
 nextDelayTransaction=request.transaction;outstandingDelayBank=true;transaction=request.transaction;return true;
}
bool Engine::popDelayBank(DelayBankResult& result) noexcept {
 if(!delayBankResults->pop(result))return false;
 outstandingDelayBank=false;return true;
}
void Engine::cancelDelayBank() noexcept {delayBankCancellation.fetch_add(1,std::memory_order_release);}
void Engine::finishDelayBank(DelayEditStatus status) noexcept {
 auto& p=*pendingDelayBank;
 if(!p.finished){
  p.completion.requestId=p.request.requestId;p.completion.generation=p.request.generation;
  p.completion.transaction=p.request.transaction;p.completion.status=status;p.completion.frame=frame;
  for(unsigned id=0;id<LoopBank::trackCount;++id){const auto& info=loops.trackView(id);
   p.completion.clearEpochs[id]=info.clearEpoch;p.completion.settingsRevisions[id]=info.delaySettingsRevision;
  }
  p.finished=true;
 }
 if(delayBankResults->push(p.completion))p.active=p.started=p.finished=false;
}
void Engine::cancelStaleDelayBank() noexcept {
 auto& p=*pendingDelayBank;if(!p.active)return;
 if(p.finished){finishDelayBank(p.completion.status);return;}
 const bool cancelled=p.request.generation!=generation()||p.request.cancellation!=delayBankCancellation.load(std::memory_order_acquire);
 bool stale=false;
 for(unsigned id=0;id<LoopBank::trackCount;++id){const auto& info=loops.trackView(id);
  stale|=info.clearEpoch!=p.request.clearEpochs[id]||info.delaySettingsRevision!=p.request.settingsRevisions[id];
 }
 if(cancelled||stale){if(p.started)loops.cancelDelayBank();finishDelayBank(cancelled?DelayEditStatus::cancelled:DelayEditStatus::stale);}
}
void Engine::serviceDelayBank() noexcept {
 auto& p=*pendingDelayBank;
 if(p.active&&!p.finished&&p.started){const auto result=loops.delayBankResult();
  if(result.request==p.request.requestId&&result.transaction==p.request.transaction){
   if(result.status==LoopBank::DelayBankStatus::ready){
    bool owns=true;
    for(unsigned id=0;id<LoopBank::trackCount;++id){const auto& info=loops.trackView(id);
     owns&=info.delayRequest==p.request.requestId&&info.clearEpoch==p.request.clearEpochs[id]
      &&info.delaySettingsRevision==p.request.settingsRevisions[id]+1;
    }
    finishDelayBank(owns?DelayEditStatus::ready:DelayEditStatus::stale);
   }
   else if(result.status==LoopBank::DelayBankStatus::failed)finishDelayBank(DelayEditStatus::failed);
   else if(result.status==LoopBank::DelayBankStatus::cancelled)finishDelayBank(DelayEditStatus::cancelled);
  }
 }
 // The sole message owner cannot enqueue another bank until this receipt is
 // consumed. Pop directly into heap-owned pending payload, not an audio stack
 // copy of twenty settings. Registration alone never applies any target.
 if(!p.active&&delayBankRequests->pop(p.request)){
  p.active=true;p.started=p.finished=false;seenDelayTransaction=std::max(seenDelayTransaction,p.request.transaction);
  cancelStaleDelayBank();
 }
}
bool Engine::currentDelayCommand(const LoopCommand& command) const noexcept {
 if(command.action!=LoopAction::delayBank)return currentDelayEdit(command);
 const auto& p=*pendingDelayBank;
 return p.active&&!p.finished&&p.request.transaction==command.delayTransaction&&p.request.requestId==command.requestId
  &&p.request.generation==command.generation&&command.generation==generation()
  &&p.request.cancellation==delayBankCancellation.load(std::memory_order_acquire);
}
void Engine::applyDelayBank(const LoopCommand& command) noexcept {
 auto& p=*pendingDelayBank;
 if(!currentDelayCommand(command)){
  if(p.active&&!p.finished&&p.request.transaction==command.delayTransaction)finishDelayBank(DelayEditStatus::cancelled);
  return;
 }
 cancelStaleDelayBank();if(!p.active||p.finished)return;
 if(!loops.requestDelayBank(p.request.settings,p.request.clearEpochs,p.request.settingsRevisions,p.request.requestId,p.request.transaction)){
  finishDelayBank(DelayEditStatus::busy);return;
 }
 p.started=true;
}
void Engine::cancelQueuedDelayBank() noexcept {
 auto& p=*pendingDelayBank;
 if(!p.active&&delayBankRequests->pop(p.request)){
  p.active=true;p.started=p.finished=false;seenDelayTransaction=std::max(seenDelayTransaction,p.request.transaction);
  finishDelayBank(DelayEditStatus::cancelled);
 }
}
}
