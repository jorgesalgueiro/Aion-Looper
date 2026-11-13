// SPDX-License-Identifier: AGPL-3.0-only
#include "LoopBank.h"
#include "ArpSettings.h"
#include <limits>
namespace aion {
bool LoopBank::validEffectsBank(const EffectsBank& bank) noexcept {
 for(unsigned id=0;id<trackCount;++id)if(!validEqSettings(bank.equalizers[id])||!validCompressorSettings(bank.compressors[id])||!validSignalChain(bank.chains[id]))return false;
 return true;
}
bool LoopBank::requestArpBank(const ArpBank& settings,const ArpEpochs& epochs,const ArpEpochs& revisions,std::uint32_t request,const EffectsBank* effects) noexcept {
 if(!request||pendingArpBank.active)return false;
 if(effects&&!validEffectsBank(*effects))return false;
 for(unsigned id=0;id<trackCount;++id)if(pendingArpSettings[id].active||epochs[id]!=tracks[id].clearEpoch||
    revisions[id]!=tracks[id].info.arpSettingsRevision||revisions[id]==UINT64_MAX)return false;
 if(!arpStorage->requestBank(settings))return false;
 pendingArpBank={settings,epochs,revisions,request,true};
 if(effects){pendingArpBank.effects=*effects;pendingArpBank.restoreEffects=true;}
 bankArpResult={request,ArpBankStatus::pending};
 bool allOff=true;for(const auto& setting:settings)allOff&=!setting.enabled;
 if(allOff)pollArps(); // No allocations/worker wait; preserve one-callback inactive state loads.
 return true;
}
void LoopBank::cancelArpBank() noexcept {
 if(!pendingArpBank.active)return;
 arpStorage->cancelBank();bankArpResult={pendingArpBank.request,ArpBankStatus::cancelled};pendingArpBank={};
}
bool LoopBank::requestArpStorage(unsigned id,const ArpModValues& values,std::uint32_t seed,std::uint64_t epoch,
                                bool activate,std::uint64_t revision,std::uint32_t request) noexcept {
 if(id>=trackCount||epoch!=tracks[id].clearEpoch)return false;
 const auto& info=tracks[id].info;
 if(activate){
  if(!request||revision!=info.arpSettingsRevision||revision==UINT64_MAX)return false;
 }else if(info.arp.enabled)return false; // Raw diagnostics cannot replace a live owner.
 if(!arpStorage->request(id,values,seed))return false;
 pendingArpSettings[id]=activate?PendingArpSettings{{values,seed,true},revision,epoch,request,true}:PendingArpSettings{};
 return true;
}
void LoopBank::pollArps() noexcept {
 // Invalidate before storage can retire the old graph. This also protects direct
 // LoopBank clients; Engine cancellation adds its independent message revision.
 for(unsigned id=0;id<trackCount;++id){
  if(pendingArpBank.active&&(pendingArpBank.epochs[id]!=tracks[id].clearEpoch||pendingArpBank.revisions[id]!=tracks[id].info.arpSettingsRevision))cancelArpBank();
  const auto& p=pendingArpSettings[id];
  if(p.active&&(p.clearEpoch!=tracks[id].clearEpoch||p.revision!=tracks[id].info.arpSettingsRevision))cancelArpStorage(id);
 }
 const auto completion=arpStorage->poll(frame);
 if(pendingArpBank.active&&(completion.bankInstalled||completion.bankFailed)){
  if(completion.bankInstalled)for(unsigned id=0;id<trackCount;++id){
   auto& info=tracks[id].info;info.arp=pendingArpBank.settings[id];info.arpRequest=pendingArpBank.request;++info.arpSettingsRevision;
  }
  if(completion.bankInstalled&&pendingArpBank.restoreEffects){
   // Every setting was validated before graph reservation. All masks are full,
   // epochs are checked above, and these bounded setters allocate no storage.
   // Disable only the external-edit cancellation guard during this own commit.
   pendingArpBank.restoreEffects=false;
   for(unsigned id=0;id<trackCount;++id){const auto& fx=pendingArpBank.effects;
    (void)setEqualizer(id,fx.equalizers[id],eqAllParameters,pendingArpBank.request,tracks[id].clearEpoch);
    (void)setCompressor(id,fx.compressors[id],compressorAllParameters,pendingArpBank.request,tracks[id].clearEpoch);
    (void)setSignalChain(id,fx.chains[id],pendingArpBank.request,tracks[id].clearEpoch);
   }
  }
  bankArpResult={pendingArpBank.request,completion.bankInstalled?ArpBankStatus::ready:ArpBankStatus::failed};pendingArpBank={};
 }
 for(unsigned id=0;id<trackCount;++id){
  auto& p=pendingArpSettings[id];
  if((completion.installed&(1u<<id))&&p.active){
   // The validated immutable descriptor belongs to this exact pending slot.
   // No later settings command is needed: commit before render or snapshots.
   auto& info=tracks[id].info;info.arp=p.settings;info.arpRequest=p.request;++info.arpSettingsRevision;
  }
  if((completion.installed|completion.failed)&(1u<<id))p={};
 }
}
bool LoopBank::setArp(unsigned id,const ArpRenderSettings& settings,std::uint32_t mask,std::uint32_t request,
                      std::uint64_t epoch,std::uint64_t revision,bool preset) noexcept {
 if(id>=trackCount||!validArpRenderSettings(settings)||(mask&~arpAllParameters)||epoch!=tracks[id].clearEpoch)return false;
 auto& info=tracks[id].info;
 if(preset&&(mask!=arpAllParameters||settings.enabled!=info.arp.enabled))return false;
 if(revision!=info.arpSettingsRevision||revision==UINT64_MAX||settings.seed!=info.arp.seed)return false;
 for(unsigned p=0;p<12;++p)if(!(mask&(1u<<p))&&settings.values[p]!=info.arp.values[p])return false;
 // An enable is asynchronous and MUST use a matching prepared transaction.
 if(settings.enabled&&!info.arp.enabled)return false;
 if(settings.enabled){
  auto* graph=arpStorage->graph(id);if(!graph)return false;
  // Validate before touching any lane. Each selected bit remains a gesture,
  // including an equal-value retarget; never restart unselected automation.
  if(graph->position()==UINT64_MAX)return false;
  if(preset){for(const auto& write:arpPresetWrites(settings))(void)graph->setTarget(write.parameter,write.value);}
  else for(unsigned p=0;p<12;++p)if(mask&(1u<<p))(void)graph->setTarget(p,settings.values[p]);
 }else{
  cancelArpBank();arpStorage->clear(id);pendingArpSettings[id]={}; // Worker reclaims; never delete here.
 }
 info.arp=settings;info.arpRequest=request;++info.arpSettingsRevision;return true;
}
Stereo LoopBank::tickArp(unsigned id,Stereo input,bool sourceActive) noexcept {
 if(!tracks[id].info.arp.enabled)return input;
 auto* graph=arpStorage->graph(id);
 if(!graph||!graph->process(&input.left,&input.right,2,&input.left,&input.right,1))return {};
 // Persistent delay/STAY/LFO run through zero input after STOP. The existing
 // source ownership gate still bounds audible tails and prevents revival.
 return sourceActive?input:Stereo{};
}
void LoopBank::resetArps(std::uint64_t firstFrame) noexcept {
 cancelArpBank();
 for(unsigned id=0;id<trackCount;++id){
  const auto& settings=tracks[id].info.arp;
  if(settings.enabled){
   cancelArpStorage(id);
   if(auto* graph=arpStorage->graph(id))(void)graph->reset(settings.seed,firstFrame);
  }else {arpStorage->clear(id);pendingArpSettings[id]={};}
 }
}
}
