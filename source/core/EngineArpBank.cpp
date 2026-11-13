// SPDX-License-Identifier: AGPL-3.0-only
#include "Engine.h"
namespace aion {
bool Engine::enqueueArpBank(ArpBankRequest request) noexcept {
 if(outstandingArpBank||!request.requestId||!request.generation||request.generation!=generation())return false;
 for(unsigned id=0;id<LoopBank::trackCount;++id)if(outstandingArp[id]||!validArpRenderSettings(request.settings[id]))return false;
 request.cancellation=arpBankCancellation.load(std::memory_order_acquire);
 if(!arpBankRequests->push(request))return false;
 outstandingArpBank=true;return true;
}
bool Engine::popArpBank(ArpBankResult& result) noexcept {
 if(!arpBankResults->pop(result))return false;
 outstandingArpBank=false;return true;
}
void Engine::cancelArpBank() noexcept {arpBankCancellation.fetch_add(1,std::memory_order_release);}
void Engine::finishArpBank(ArpPreparationStatus status) noexcept {
 // Exactly one admitted transaction owns the sole result slot until read.
 // No new bank or per-track preparation can be admitted in that interval.
 // Retain the terminal result if that invariant is ever violated; never spin.
 if(!pendingArpBank.finished){
  const auto& r=pendingArpBank.request;
  auto& completion=pendingArpBank.completion;completion.requestId=r.requestId;completion.generation=r.generation;completion.status=status;
  for(unsigned id=0;id<LoopBank::trackCount;++id){const auto view=loops.trackView(id);completion.clearEpochs[id]=view.clearEpoch;completion.settingsRevisions[id]=view.arpSettingsRevision;}
  pendingArpBank.finished=true;pendingArpBank.status=status;
 }
 if(arpBankResults->push(pendingArpBank.completion))pendingArpBank={};
}
void Engine::cancelStaleArpBank() noexcept {
 if(!pendingArpBank.active)return;
 if(pendingArpBank.finished){finishArpBank(pendingArpBank.status);return;}
 const auto& r=pendingArpBank.request;
 bool stale=r.generation!=generation()||r.cancellation!=arpBankCancellation.load(std::memory_order_acquire);
 for(unsigned id=0;id<LoopBank::trackCount;++id){const auto info=loops.trackView(id);stale|=r.clearEpochs[id]!=info.clearEpoch||r.settingsRevisions[id]!=info.arpSettingsRevision;}
 if(stale){loops.cancelArpBank();finishArpBank(ArpPreparationStatus::cancelled);}
}
void Engine::serviceArpBank() noexcept {
 if(pendingArpBank.active&&!pendingArpBank.finished&&pendingArpBank.started){
  const auto result=loops.arpBankResult();
  if(result.request==pendingArpBank.request.requestId&&result.status!=LoopBank::ArpBankStatus::pending){
   finishArpBank(result.status==LoopBank::ArpBankStatus::ready?ArpPreparationStatus::ready:
                 result.status==LoopBank::ArpBankStatus::failed?ArpPreparationStatus::failed:ArpPreparationStatus::cancelled);
  }
 }
 ArpBankRequest r;
 if(!arpBankRequests->pop(r))return;
 pendingArpBank={r,true,false};
 if(r.generation!=generation()||r.cancellation!=arpBankCancellation.load(std::memory_order_acquire)){finishArpBank(ArpPreparationStatus::cancelled);return;}
 for(unsigned id=0;id<LoopBank::trackCount;++id){const auto info=loops.trackView(id);
  if(r.clearEpochs[id]!=info.clearEpoch||r.settingsRevisions[id]!=info.arpSettingsRevision||r.settingsRevisions[id]==UINT64_MAX){finishArpBank(ArpPreparationStatus::stale);return;}
 }
 if(!loops.requestArpBank(r.settings,r.clearEpochs,r.settingsRevisions,r.requestId)){finishArpBank(ArpPreparationStatus::busy);return;}
 pendingArpBank.started=true;
}
void Engine::cancelQueuedArpBank() noexcept {
 ArpBankRequest request;
 if(arpBankRequests->pop(request)){pendingArpBank={request,true,false};finishArpBank(ArpPreparationStatus::cancelled);}
}
}
