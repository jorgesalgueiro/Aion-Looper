// SPDX-License-Identifier: AGPL-3.0-only
#include "ReverbSettingsOwner.h"
namespace aion {
bool ReverbSettingsOwner::bind(const EngineView& view,const Bank& known) noexcept {
 if(attached||bank.active||!view.generation||view.generation!=engine.generation())return false;
 for(unsigned id=0;id<records.size();++id)
  if(records[id].pending||!validReverbSettings(known[id])||known[id]!=view.tracks[id].reverb)return false;
 for(unsigned id=0;id<records.size();++id)records[id]={known[id],view.tracks[id].clearEpoch,view.tracks[id].reverbSettingsRevision,0,ReverbEditStatus::ready,false};
 generation=view.generation;attached=true;return true;
}
bool ReverbSettingsOwner::submit(unsigned id,const ReverbSettings& candidate,std::uint32_t request,std::uint64_t frame) noexcept {
 if(!bound()||bank.active||id>=records.size()||records[id].pending||!request||!validReverbSettings(candidate))return false;
 const auto& old=records[id];std::uint64_t transaction=0;
 LoopCommand command{LoopAction::reverb,id,request,generation,frame};command.reverb=candidate;
 command.reverbClearEpoch=old.clearEpoch;command.reverbSettingsRevision=old.revision;
 if(!engine.enqueueReverbEdit(command,transaction))return false;
 pending[id]={candidate,transaction,old.clearEpoch,old.revision,generation,request};
 records[id].pending=true;records[id].requestId=request;return true;
}
bool ReverbSettingsOwner::set(unsigned id,unsigned p,double value,std::uint32_t request,std::uint64_t frame) noexcept {
 if(id>=records.size())return false;
 auto candidate=records[id].committed;return setReverbControl(candidate,p,value)&&submit(id,candidate,request,frame);
}
bool ReverbSettingsOwner::room(unsigned id,ReverbRoom room,std::uint32_t request,std::uint64_t frame) noexcept {
 if(id>=records.size())return false;
 auto candidate=records[id].committed;return setReverbRoom(candidate,room)&&submit(id,candidate,request,frame);
}
bool ReverbSettingsOwner::preset(unsigned id,unsigned preset,std::uint32_t request,std::uint64_t frame) noexcept {
 if(id>=records.size())return false;
 auto candidate=records[id].committed;return applyReverbPreset(candidate,preset)&&submit(id,candidate,request,frame);
}
bool ReverbSettingsOwner::enable(unsigned id,bool enabled,std::uint32_t request,std::uint64_t frame) noexcept {
 if(id>=records.size())return false;
 auto candidate=records[id].committed;candidate.enabled=enabled;return submit(id,candidate,request,frame);
}
bool ReverbSettingsOwner::reset(unsigned id,std::uint32_t request,std::uint64_t frame) noexcept {
 if(id>=records.size())return false;
 ReverbSettings candidate;candidate.enabled=records[id].committed.enabled;return submit(id,candidate,request,frame);
}
bool ReverbSettingsOwner::restore(const Bank& candidate,std::uint32_t request,std::uint64_t frame) noexcept {
 if(!bound()||bank.active||!request)return false;
 ReverbBankRequest command;command.settings=candidate;command.requestId=request;command.generation=generation;command.frame=frame;
 for(unsigned id=0;id<records.size();++id){
  if(records[id].pending||!validReverbSettings(candidate[id]))return false;
  command.clearEpochs[id]=records[id].clearEpoch;command.settingsRevisions[id]=records[id].revision;
 }
 std::uint64_t transaction=0;if(!engine.enqueueReverbBank(command,transaction))return false;
 bank={candidate,command.clearEpochs,command.settingsRevisions,transaction,generation,request,true};
 for(auto& record:records){record.pending=true;record.requestId=request;}
 return true;
}
void ReverbSettingsOwner::cancel(unsigned id) noexcept {engine.cancelReverbEdit(id);}
void ReverbSettingsOwner::serviceBank() noexcept {
 ReverbBankResult result;if(!engine.popReverbBank(result))return;
 if(!bank.active||result.requestId!=bank.request||result.generation!=bank.generation||result.transaction!=bank.transaction){attached=false;return;}
 bankReceipt=result;bool committed=result.status==ReverbEditStatus::ready;
 if(committed)for(unsigned id=0;id<records.size();++id)
  if(result.clearEpochs[id]!=bank.epochs[id]||result.settingsRevisions[id]!=bank.revisions[id]+1){committed=false;attached=false;break;}
 for(unsigned id=0;id<records.size();++id){auto& record=records[id];const bool cleared=record.clearEpoch>bank.epochs[id];
  if(committed&&!cleared){record.committed=bank.candidate[id];record.clearEpoch=result.clearEpochs[id];record.revision=result.settingsRevisions[id];}
  record.status=cleared?ReverbEditStatus::cancelled:committed?ReverbEditStatus::ready:result.status==ReverbEditStatus::ready?ReverbEditStatus::stale:result.status;
  record.pending=false;
 }
 bank={};
}
void ReverbSettingsOwner::service(const EngineView& view) noexcept {
 if(attached&&generation!=engine.generation()){attached=false;cancel();}
 if(attached&&view.generation==generation)for(unsigned id=0;id<records.size();++id){auto& record=records[id];
  if(view.tracks[id].clearEpoch>record.clearEpoch){
   record.committed={};record.clearEpoch=view.tracks[id].clearEpoch;record.revision=0;
   if(record.pending)cancel(id);
  }
 }
 serviceBank();
 ReverbEditResult result;
 for(unsigned n=0;n<records.size()&&engine.popReverbEdit(result);++n){
  if(result.track>=records.size()){attached=false;continue;}
  auto& record=records[result.track];const auto& p=pending[result.track];
  if(!record.pending||p.request!=result.requestId||p.generation!=result.generation||p.transaction!=result.transaction){attached=false;continue;}
  const bool committed=result.status==ReverbEditStatus::ready&&record.clearEpoch<=p.epoch
   &&result.clearEpoch==p.epoch&&result.settingsRevision==p.revision+1;
  if(committed){record.committed=p.candidate;record.clearEpoch=result.clearEpoch;record.revision=result.settingsRevision;}
  record.status=committed?ReverbEditStatus::ready:result.status==ReverbEditStatus::ready?ReverbEditStatus::cancelled:result.status;
  record.pending=false;pending[result.track]={};
 }
 if(attached&&view.generation==generation)for(unsigned id=0;id<records.size();++id){const auto& record=records[id];const auto& audio=view.tracks[id];
  if(record.pending||audio.clearEpoch!=record.clearEpoch||audio.reverbSettingsRevision<record.revision)continue;
  if(audio.reverbSettingsRevision!=record.revision||audio.reverb!=record.committed){attached=false;break;}
 }
}
}
