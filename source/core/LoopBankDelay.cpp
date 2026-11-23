// SPDX-License-Identifier: AGPL-3.0-only
#include "LoopBank.h"
namespace aion {
bool LoopBank::requestDelay(unsigned id,const DelaySettings& settings,std::uint64_t epoch,std::uint64_t revision,std::uint32_t request) noexcept {
 if(id>=trackCount||!request||!settings.enabled||!validDelaySettings(settings)||epoch!=tracks[id].clearEpoch
  ||revision!=tracks[id].info.delaySettingsRevision||revision==UINT64_MAX)return false;
 if(!delayStorage->request(id,settings,sync.settings.bpm))return false;
 pendingDelays[id]={settings,epoch,revision,request,true};return true;
}
void LoopBank::cancelDelay(unsigned id) noexcept {
 if(id>=trackCount)return;
 if(pendingArpBank.active&&pendingArpBank.restoreEffects&&pendingArpBank.effects.restoreDelays)cancelArpBank();
 cancelDelayBank();
 delayStorage->cancel(id);pendingDelays[id]={};
}
bool LoopBank::setDelay(unsigned id,const DelaySettings& settings,std::uint64_t epoch,std::uint64_t revision,std::uint32_t request) noexcept {
 if(id>=trackCount||!request||!validDelaySettings(settings)||epoch!=tracks[id].clearEpoch
  ||revision!=tracks[id].info.delaySettingsRevision||revision==UINT64_MAX)return false;
 auto* graph=delayStorage->graph(id);
 if(graph){
  // Disabled metadata may select a new mode/count without allocating a new
  // ring. Fade the existing runtime to bypass; an eventual enable prepares
  // the requested mode/capacity when it cannot reuse that old runtime.
  auto runtime=settings;
  if(!settings.enabled){runtime=graph->settings();runtime.enabled=false;}
  if(!graph->set(runtime,sync.settings.bpm))return false;
 }
 else if(settings.enabled)return false;
 // An accepted live edit supersedes any not-yet-adopted worker request. Do not
 // let that request overwrite a newer value when its allocation completes.
 cancelDelay(id);auto& info=tracks[id].info;info.delay=settings;info.delayRequest=request;++info.delaySettingsRevision;return true;
}
void LoopBank::pollDelays() noexcept {
 if(pendingDelayBank->active)for(unsigned id=0;id<trackCount;++id)
  if(pendingDelayBank->epochs[id]!=tracks[id].clearEpoch||pendingDelayBank->revisions[id]!=tracks[id].info.delaySettingsRevision){cancelDelayBank();break;}
 for(unsigned id=0;id<trackCount;++id){const auto& p=pendingDelays[id];
  if(p.active&&(p.clearEpoch!=tracks[id].clearEpoch||p.revision!=tracks[id].info.delaySettingsRevision))cancelDelay(id);
 }
 const bool coordinated=pendingArpBank.active&&pendingArpBank.restoreEffects&&pendingArpBank.effects.restoreDelays;
 const auto completion=delayStorage->poll(sync.settings.bpm,coordinated);
 if(pendingDelayBank->active&&(completion.bankInstalled||completion.bankFailed)){
  const auto& bank=*pendingDelayBank;
  if(completion.bankInstalled)for(unsigned id=0;id<trackCount;++id){
   auto& info=tracks[id].info;info.delay=bank.settings[id];info.delayRequest=bank.request;++info.delaySettingsRevision;
  }
  bankDelayResult={bank.request,bank.transaction,completion.bankInstalled?DelayBankStatus::ready:DelayBankStatus::failed};
  pendingDelayBank->active=false;
 }
 for(unsigned id=0;id<trackCount;++id){auto& p=pendingDelays[id];
  if((completion.installed&(1u<<id))&&p.active){
   auto* graph=delayStorage->graph(id);
   // A tempo may change while allocation is in flight. Initialize the freshly
   // prepared head at the current clock before its first rendered sample.
   (void)graph->set(p.settings,sync.settings.bpm);graph->reset();
   auto& info=tracks[id].info;info.delay=p.settings;info.delayRequest=p.request;++info.delaySettingsRevision;
  }
  if((completion.installed|completion.failed)&(1u<<id))p={};
 }
}
void LoopBank::resetDelay(unsigned id,bool fadeIn) noexcept {
 cancelDelay(id);delayStorage->reset(id,fadeIn);
}
void LoopBank::resetDelays() noexcept {
 for(unsigned id=0;id<trackCount;++id){
  (void)delayStorage->setTempo(id,sync.settings.bpm);resetDelay(id);
 }
}
Stereo LoopBank::tickDelay(unsigned id,Stereo input,bool sourceActive) noexcept {
 if(!delayStorage->hasOwner(id))return input;
 delayStorage->tick(id,input.left,input.right,input.left,input.right);
 // Continue persistent memory, but obey the existing source STOP/clear gate.
 return sourceActive?input:Stereo{};
}
}
