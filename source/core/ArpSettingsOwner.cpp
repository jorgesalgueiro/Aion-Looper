// SPDX-License-Identifier: AGPL-3.0-only
#include "ArpSettingsOwner.h"
namespace aion {
bool ArpSettingsOwner::bind(const EngineView& view,const Bank& known) noexcept {
 if(attached||bank.active||!view.generation||view.generation!=engine.generation())return false;
 for(unsigned id=0;id<records.size();++id){ArpRenderSettings render;
  if(records[id].pending||!compileArpSettings(known[id].controls,known[id].bpm,known[id].seed,render)||render!=view.tracks[id].arp)return false;
 }
 for(unsigned id=0;id<records.size();++id)records[id]={known[id],view.tracks[id].clearEpoch,view.tracks[id].arpSettingsRevision,0,ArpPreparationStatus::ready,false};
 generation=view.generation;attached=true;return true;
}
bool ArpSettingsOwner::submit(unsigned id,const ArpOwnedSettings& candidate,std::uint32_t mask,bool preset,bool prepare,std::uint32_t request,std::uint64_t frame) noexcept {
 if(!bound()||id>=records.size()||records[id].pending||!request)return false;
 ArpRenderSettings render;if(!compileArpSettings(candidate.controls,candidate.bpm,candidate.seed,render))return false;
 const auto& old=records[id];std::uint64_t transaction=0;
 if(prepare){
  ArpPreparationRequest command;command.track=id;command.requestId=request;command.generation=generation;command.values=render.values;command.seed=render.seed;
  command.activate=true;command.clearEpoch=old.clearEpoch;command.settingsRevision=old.revision;
  if(frame||!engine.enqueueArpPreparation(command))return false; // Preparation adopts at its ready callback, never a fabricated future deadline.
 }else{
  LoopCommand command{LoopAction::arpDelay,id,request,generation,frame};command.arp=render;command.arpMask=mask;command.arpPreset=preset;
  command.arpClearEpoch=old.clearEpoch;command.arpSettingsRevision=old.revision;
  if(!engine.enqueueArpEdit(command,transaction))return false;
 }
 pending[id]={candidate,transaction,old.clearEpoch,old.revision,generation,request,false};records[id].pending=true;records[id].requestId=request;return true;
}
bool ArpSettingsOwner::set(unsigned id,unsigned parameter,double value,double bpm,std::uint32_t request,std::uint64_t frame) noexcept {
 if(id>=records.size())return false;
 auto next=records[id].committed;std::uint32_t mask=0;
 if(!setArpControl(next.controls,parameter,value)||!arpRenderMask(1u<<parameter,mask))return false;
 next.bpm=bpm;return submit(id,next,mask,false,false,request,frame);
}
bool ArpSettingsOwner::preset(unsigned id,unsigned preset,double bpm,std::uint32_t request,std::uint64_t frame) noexcept {
 if(id>=records.size())return false;
 auto next=records[id].committed;if(!applyArpPreset(next.controls,preset))return false;
 next.bpm=bpm;return submit(id,next,arpAllParameters,true,false,request,frame);
}
bool ArpSettingsOwner::enable(unsigned id,bool enabled,double bpm,std::uint32_t request,std::uint64_t frame) noexcept {
 if(id>=records.size())return false;
 auto next=records[id].committed;const bool prepare=enabled&&!next.controls.enabled;
 next.controls.enabled=enabled;if(prepare)next.bpm=bpm;
 return submit(id,next,0,false,prepare,request,frame);
}
void ArpSettingsOwner::cancel(unsigned id) noexcept {engine.cancelArpPreparation(id);}
void ArpSettingsOwner::service(const EngineView& view) noexcept {
 const auto current=engine.generation();
 if(attached&&generation!=current){attached=false;engine.cancelArpPreparation();}
 // Clear wins over an older unread ready result. Older snapshots never move
 // epochs/revisions backwards or overwrite a just-confirmed original value.
 if(attached&&view.generation==generation)for(unsigned id=0;id<records.size();++id){auto& r=records[id];
  if(view.tracks[id].clearEpoch>r.clearEpoch){
   // DEL resets native Arp revision to zero. This snapshot may already include
   // fresh MIDI after DEL; replay those exact receipts from the reset baseline,
   // rather than skipping straight to the snapshot's final revision.
   r.committed={};r.clearEpoch=view.tracks[id].clearEpoch;r.revision=0;
   if(r.pending){pending[id].invalidated=true;engine.cancelArpPreparation(id);}
  }
 }
 serviceBank();
 ArpPreparationResult result;
 for(unsigned n=0;n<records.size()&&engine.popArpPreparation(result);++n){
  if(result.track>=records.size()){attached=false;continue;}
  auto& r=records[result.track];const auto p=pending[result.track];
  if(!r.pending||p.request!=result.requestId||p.generation!=result.generation||p.transaction!=result.transaction){attached=false;continue;}
  // A ready receipt remains a fact across prepare. Retain its original values
  // even while unbound; only an explicit known-state bind can make them current
  // in the new generation. Never reconstruct that old precision from floats.
  const bool commit=result.status==ArpPreparationStatus::ready&&!p.invalidated
      &&result.clearEpoch==p.epoch&&result.settingsRevision==p.revision+1;
  if(commit){r.committed=p.candidate;r.clearEpoch=result.clearEpoch;r.revision=result.settingsRevision;}
  r.status=commit?ArpPreparationStatus::ready:result.status==ArpPreparationStatus::ready?ArpPreparationStatus::cancelled:result.status;
  r.pending=false;pending[result.track]={};
 }
 ArpMidiReceipt midi;
 for(unsigned n=0;n<511&&engine.popArpMidi(midi);++n){
  if(midi.generation!=generation||midi.track>=records.size())continue;
  auto& r=records[midi.track];if(midi.epoch<r.clearEpoch)continue; // A later clear wins even with saturated telemetry.
  if(midi.epoch!=r.clearEpoch||midi.before!=r.revision||midi.revision!=midi.before+1){attached=false;continue;}
  if(!setArpControl(r.committed.controls,midi.parameter,midi.value)){attached=false;continue;}
  r.committed.bpm=midi.bpm;r.revision=midi.revision;
 }
 if(attached&&view.generation==generation)for(unsigned id=0;id<records.size();++id){const auto& r=records[id];const auto& audio=view.tracks[id];
  if(r.pending||audio.clearEpoch!=r.clearEpoch||audio.arpSettingsRevision<r.revision)continue;
  ArpRenderSettings expected;
  if(audio.arpSettingsRevision!=r.revision||!compileArpSettings(r.committed.controls,r.committed.bpm,r.committed.seed,expected)||audio.arp!=expected){attached=false;break;}
 }
}
}
