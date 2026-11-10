// SPDX-License-Identifier: AGPL-3.0-only
#include "LoopBank.h"
#include <limits>
namespace aion {
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
  const auto& p=pendingArpSettings[id];
  if(p.active&&(p.clearEpoch!=tracks[id].clearEpoch||p.revision!=tracks[id].info.arpSettingsRevision))cancelArpStorage(id);
 }
 const auto completion=arpStorage->poll(frame);
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
                      std::uint64_t epoch,std::uint64_t revision) noexcept {
 if(id>=trackCount||!validArpRenderSettings(settings)||(mask&~arpAllParameters)||epoch!=tracks[id].clearEpoch)return false;
 auto& info=tracks[id].info;
 if(revision!=info.arpSettingsRevision||revision==UINT64_MAX||settings.seed!=info.arp.seed)return false;
 for(unsigned p=0;p<12;++p)if(!(mask&(1u<<p))&&settings.values[p]!=info.arp.values[p])return false;
 // An enable is asynchronous and MUST use a matching prepared transaction.
 if(settings.enabled&&!info.arp.enabled)return false;
 if(settings.enabled){
  auto* graph=arpStorage->graph(id);if(!graph)return false;
  // Validate before touching any lane. Each selected bit remains a gesture,
  // including an equal-value retarget; never restart unselected automation.
  if(graph->position()==UINT64_MAX)return false;
  for(unsigned p=0;p<12;++p)if(mask&(1u<<p))(void)graph->setTarget(p,settings.values[p]);
 }else{
  arpStorage->clear(id);pendingArpSettings[id]={}; // Worker reclaims; never delete here.
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
 for(unsigned id=0;id<trackCount;++id){
  const auto& settings=tracks[id].info.arp;
  if(settings.enabled){
   cancelArpStorage(id);
   if(auto* graph=arpStorage->graph(id))(void)graph->reset(settings.seed,firstFrame);
  }else {arpStorage->clear(id);pendingArpSettings[id]={};}
 }
}
}
