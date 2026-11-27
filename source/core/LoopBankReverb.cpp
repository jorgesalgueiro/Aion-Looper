// SPDX-License-Identifier: AGPL-3.0-only
#include "LoopBank.h"
namespace aion {
bool LoopBank::requestReverb(unsigned id,const ReverbSettings& settings,std::uint64_t epoch,std::uint64_t revision,std::uint32_t request,std::uint32_t seed) noexcept {
 if(id>=trackCount||!request||!settings.enabled||!validReverbSettings(settings)||epoch!=tracks[id].clearEpoch
  ||revision!=tracks[id].info.reverbSettingsRevision||revision==UINT64_MAX)return false;
 if(!reverbStorage->request(id,settings,seed))return false;
 pendingReverbs[id]={settings,epoch,revision,request,true};return true;
}
void LoopBank::cancelReverb(unsigned id) noexcept {
 if(id>=trackCount)return;
 cancelReverbBank();
 reverbStorage->cancel(id);pendingReverbs[id]={};
}
bool LoopBank::setReverb(unsigned id,const ReverbSettings& settings,std::uint64_t epoch,std::uint64_t revision,std::uint32_t request,bool midi) noexcept {
 // MIDI retains the last command identity, including zero on a fresh track.
 if(id>=trackCount||(!request&&!midi)||!validReverbSettings(settings)||epoch!=tracks[id].clearEpoch
  ||revision!=tracks[id].info.reverbSettingsRevision||revision==UINT64_MAX)return false;
 auto* graph=reverbStorage->graph(id);
 if(graph){
  // Disabled metadata may select a new shape without allocating a new graph.
  // Fade the existing runtime to bypass; an eventual enable with that new shape
  // requires a prepared request. A same-shape edit applies every live control.
  auto runtime=settings;
  if(!settings.enabled){runtime=graph->settings();runtime.enabled=false;}
  if(!reverbStorage->set(id,runtime))return false;
 }
 else if(settings.enabled)return false;
 // An accepted live edit supersedes any not-yet-adopted worker request. Do not
 // let that request overwrite a newer value when its allocation completes.
 cancelReverb(id);auto& info=tracks[id].info;info.reverb=settings;info.reverbRequest=request;++info.reverbSettingsRevision;return true;
}
void LoopBank::pollReverbs() noexcept {
 // The joint Arp/Delay/Reverb owner validates, prepares and commits its staged
 // B bank; this function only advances per-track work while it is active.
 const bool coordinated=pendingArpBank.active&&pendingArpBank.restoreEffects&&pendingArpBank.effects.restoreReverbs;
 if(!coordinated&&pendingReverbBank->active)for(unsigned id=0;id<trackCount;++id)
  if(pendingReverbBank->epochs[id]!=tracks[id].clearEpoch||pendingReverbBank->revisions[id]!=tracks[id].info.reverbSettingsRevision){cancelReverbBank();break;}
 for(unsigned id=0;id<trackCount;++id){const auto& p=pendingReverbs[id];
  if(p.active&&(p.clearEpoch!=tracks[id].clearEpoch||p.revision!=tracks[id].info.reverbSettingsRevision))cancelReverb(id);
 }
 const auto completion=reverbStorage->poll(coordinated);
 if(!coordinated&&pendingReverbBank->active&&(completion.bankInstalled||completion.bankFailed)){
  const auto& bank=*pendingReverbBank;
  if(completion.bankInstalled)for(unsigned id=0;id<trackCount;++id){
   auto& info=tracks[id].info;info.reverb=bank.settings[id];info.reverbRequest=bank.request;++info.reverbSettingsRevision;
  }
  bankReverbResult={bank.request,bank.transaction,completion.bankInstalled?ReverbBankStatus::ready:ReverbBankStatus::failed};
  pendingReverbBank->active=false;
 }
 for(unsigned id=0;id<trackCount;++id){auto& p=pendingReverbs[id];
  if((completion.installed&(1u<<id))&&p.active){
   auto& info=tracks[id].info;info.reverb=p.settings;info.reverbRequest=p.request;++info.reverbSettingsRevision;
  }
  if((completion.installed|completion.failed)&(1u<<id))p={};
 }
}
void LoopBank::resetReverb(unsigned id) noexcept {
 cancelReverb(id);reverbStorage->reset(id);
}
void LoopBank::resetReverbs() noexcept {
 for(unsigned id=0;id<trackCount;++id)resetReverb(id);
}
Stereo LoopBank::tickReverb(unsigned id,Stereo input,bool sourceActive) noexcept {
 if(!reverbStorage->hasOwner(id))return input;
 reverbStorage->tick(id,input.left,input.right,input.left,input.right);
 // Continue persistent convolution memory, but obey the existing source STOP/clear gate.
 return sourceActive?input:Stereo{};
}
}
