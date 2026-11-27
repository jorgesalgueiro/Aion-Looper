// SPDX-License-Identifier: AGPL-3.0-only
#include "Engine.h"
#include <algorithm>
namespace aion {
bool Engine::enqueueReverbBank(ReverbBankRequest request,std::uint64_t& transaction) noexcept {
 if(!request.requestId||!request.generation||request.generation!=generation()||request.transaction
  ||coordinatedReverbBank.load(std::memory_order_acquire)
  ||outstandingReverbBank||nextReverbTransaction==UINT64_MAX)return false;
 for(unsigned id=0;id<LoopBank::trackCount;++id)
  if(outstandingReverb[id]||!validReverbSettings(request.settings[id])||request.settingsRevisions[id]==UINT64_MAX)return false;
 if(!reverbBankRequests->canPush()||!loopCommands->canPush())return false;
 request.transaction=nextReverbTransaction+1;request.cancellation=reverbBankCancellation.load(std::memory_order_acquire);
 LoopCommand command{LoopAction::reverbBank,0,request.requestId,request.generation,request.frame};
 command.reverbTransaction=request.transaction;
 // Sole producer of both queues. Audio can only free room after preflight.
 (void)reverbBankRequests->push(request);(void)loopCommands->push(command);
 nextReverbTransaction=request.transaction;outstandingReverbBank=true;transaction=request.transaction;return true;
}
bool Engine::popReverbBank(ReverbBankResult& result) noexcept {
 if(!reverbBankResults->pop(result))return false;
 outstandingReverbBank=false;return true;
}
void Engine::cancelReverbBank() noexcept {reverbBankCancellation.fetch_add(1,std::memory_order_release);if(coordinatedReverbBank.load(std::memory_order_acquire))cancelArpBank();}
void Engine::finishReverbBank(ReverbEditStatus status) noexcept {
 auto& p=*pendingReverbBank;
 if(!p.finished){
  p.completion.requestId=p.request.requestId;p.completion.generation=p.request.generation;
  p.completion.transaction=p.request.transaction;p.completion.status=status;p.completion.frame=frame;
  for(unsigned id=0;id<LoopBank::trackCount;++id){const auto& info=loops.trackView(id);
   p.completion.clearEpochs[id]=info.clearEpoch;p.completion.settingsRevisions[id]=info.reverbSettingsRevision;
  }
  p.finished=true;
 }
 if(reverbBankResults->push(p.completion))p.active=p.started=p.finished=false;
}
void Engine::cancelStaleReverbBank() noexcept {
 auto& p=*pendingReverbBank;if(!p.active)return;
 if(p.finished){finishReverbBank(p.completion.status);return;}
 const bool cancelled=p.request.generation!=generation()||p.request.cancellation!=reverbBankCancellation.load(std::memory_order_acquire);
 bool stale=false;
 for(unsigned id=0;id<LoopBank::trackCount;++id){const auto& info=loops.trackView(id);
  stale|=info.clearEpoch!=p.request.clearEpochs[id]||info.reverbSettingsRevision!=p.request.settingsRevisions[id];
 }
 if(cancelled||stale){if(p.started)loops.cancelReverbBank();finishReverbBank(cancelled?ReverbEditStatus::cancelled:ReverbEditStatus::stale);}
}
void Engine::serviceReverbBank() noexcept {
 auto& p=*pendingReverbBank;
 if(p.active&&!p.finished&&p.started){const auto result=loops.reverbBankResult();
  if(result.request==p.request.requestId&&result.transaction==p.request.transaction){
   if(result.status==LoopBank::ReverbBankStatus::ready){
    bool owns=true;
    for(unsigned id=0;id<LoopBank::trackCount;++id){const auto& info=loops.trackView(id);
     owns&=info.reverbRequest==p.request.requestId&&info.clearEpoch==p.request.clearEpochs[id]
      &&info.reverbSettingsRevision==p.request.settingsRevisions[id]+1;
    }
    finishReverbBank(owns?ReverbEditStatus::ready:ReverbEditStatus::stale);
   }
   else if(result.status==LoopBank::ReverbBankStatus::failed)finishReverbBank(ReverbEditStatus::failed);
   else if(result.status==LoopBank::ReverbBankStatus::cancelled)finishReverbBank(ReverbEditStatus::cancelled);
  }
 }
 // The sole message owner cannot enqueue another bank until this receipt is
 // consumed. Pop directly into heap-owned pending payload, not an audio stack
 // copy of twenty settings. Registration alone never applies any target.
 if(!p.active&&reverbBankRequests->pop(p.request)){
  p.active=true;p.started=p.finished=false;seenReverbTransaction=std::max(seenReverbTransaction,p.request.transaction);
  cancelStaleReverbBank();
 }
}
bool Engine::currentReverbCommand(const LoopCommand& command) const noexcept {
 if(command.action!=LoopAction::reverbBank)return currentReverbEdit(command);
 const auto& p=*pendingReverbBank;
 return p.active&&!p.finished&&p.request.transaction==command.reverbTransaction&&p.request.requestId==command.requestId
  &&p.request.generation==command.generation&&command.generation==generation()
  &&p.request.cancellation==reverbBankCancellation.load(std::memory_order_acquire);
}
void Engine::applyReverbBank(const LoopCommand& command) noexcept {
 auto& p=*pendingReverbBank;
 if(!currentReverbCommand(command)){
  if(p.active&&!p.finished&&p.request.transaction==command.reverbTransaction)finishReverbBank(ReverbEditStatus::cancelled);
  return;
 }
 cancelStaleReverbBank();if(!p.active||p.finished)return;
 if(!loops.requestReverbBank(p.request.settings,p.request.clearEpochs,p.request.settingsRevisions,p.request.requestId,p.request.transaction)){
  finishReverbBank(ReverbEditStatus::busy);return;
 }
 p.started=true;
}
void Engine::cancelQueuedReverbBank() noexcept {
 auto& p=*pendingReverbBank;
 if(!p.active&&reverbBankRequests->pop(p.request)){
  p.active=true;p.started=p.finished=false;seenReverbTransaction=std::max(seenReverbTransaction,p.request.transaction);
  finishReverbBank(ReverbEditStatus::cancelled);
 }
}
}
