// SPDX-License-Identifier: AGPL-3.0-only
#include "DelaySettingsOwner.h"
namespace aion {
bool DelaySettingsOwner::bind(const EngineView& view,const Bank& known) noexcept {
 if(attached||bank.active||!view.generation||view.generation!=engine.generation())return false;
 for(unsigned id=0;id<records.size();++id)
  if(records[id].pending||!validDelaySettings(known[id])||known[id]!=view.tracks[id].delay)return false;
 for(unsigned id=0;id<records.size();++id)records[id]={known[id],view.tracks[id].clearEpoch,view.tracks[id].delaySettingsRevision,0,DelayEditStatus::ready,false};
 generation=view.generation;attached=true;return true;
}
bool DelaySettingsOwner::submit(unsigned id,const DelaySettings& candidate,std::uint32_t request,std::uint64_t frame) noexcept {
 if(!bound()||bank.active||id>=records.size()||records[id].pending||!request||!validDelaySettings(candidate))return false;
 const auto& old=records[id];std::uint64_t transaction=0;
 LoopCommand command{LoopAction::delay,id,request,generation,frame};command.delay=candidate;
 command.delayClearEpoch=old.clearEpoch;command.delaySettingsRevision=old.revision;
 if(!engine.enqueueDelayEdit(command,transaction))return false;
 pending[id]={candidate,transaction,old.clearEpoch,old.revision,generation,request};
 records[id].pending=true;records[id].requestId=request;return true;
}
bool DelaySettingsOwner::set(unsigned id,unsigned p,double value,std::uint32_t request,std::uint64_t frame) noexcept {
 if(id>=records.size())return false;
 auto candidate=records[id].committed;return setDelayControl(candidate,p,value)&&submit(id,candidate,request,frame);
}
bool DelaySettingsOwner::preset(unsigned id,unsigned preset,std::uint32_t request,std::uint64_t frame) noexcept {
 if(id>=records.size())return false;
 auto candidate=records[id].committed;return applyDelayPreset(candidate,preset)&&submit(id,candidate,request,frame);
}
bool DelaySettingsOwner::enable(unsigned id,bool enabled,std::uint32_t request,std::uint64_t frame) noexcept {
 if(id>=records.size())return false;
 auto candidate=records[id].committed;candidate.enabled=enabled;return submit(id,candidate,request,frame);
}
bool DelaySettingsOwner::repeats(unsigned id,DelayRepeatMode mode,unsigned count,std::uint32_t request,std::uint64_t frame) noexcept {
 if(id>=records.size())return false;
 auto candidate=records[id].committed;return setDelayRepeats(candidate,mode,count)&&submit(id,candidate,request,frame);
}
bool DelaySettingsOwner::reset(unsigned id,std::uint32_t request,std::uint64_t frame) noexcept {
 if(id>=records.size())return false;
 DelaySettings candidate;candidate.enabled=records[id].committed.enabled;return submit(id,candidate,request,frame);
}
bool DelaySettingsOwner::restore(const Bank& candidate,std::uint32_t request,std::uint64_t frame) noexcept {
 if(!bound()||bank.active||!request)return false;
 DelayBankRequest command;command.settings=candidate;command.requestId=request;command.generation=generation;command.frame=frame;
 for(unsigned id=0;id<records.size();++id){
  if(records[id].pending||!validDelaySettings(candidate[id]))return false;
  command.clearEpochs[id]=records[id].clearEpoch;command.settingsRevisions[id]=records[id].revision;
 }
 std::uint64_t transaction=0;if(!engine.enqueueDelayBank(command,transaction))return false;
 bank={candidate,command.clearEpochs,command.settingsRevisions,transaction,generation,request,true};
 for(auto& record:records){record.pending=true;record.requestId=request;}
 return true;
}
void DelaySettingsOwner::cancel(unsigned id) noexcept {engine.cancelDelayEdit(id);}
bool DelaySettingsOwner::acceptEffectsRestore(const EngineView& view,const Bank& candidate,const ArpBankResult& receipt,
 const LoopBank::DelayEpochs& epochs,const LoopBank::DelayEpochs& revisions) noexcept {
 if(bank.active||!receipt.requestId||!receipt.includesDelays||receipt.status!=ArpPreparationStatus::ready
  ||receipt.generation!=engine.generation()||view.generation!=receipt.generation
  ||view.callbackClock.generation!=receipt.generation||!receipt.callbackSerial||view.callbackClock.serial<receipt.callbackSerial)return false;
 for(unsigned id=0;id<records.size();++id){const auto& audio=view.tracks[id];const auto& record=records[id];
  if(record.pending||!validDelaySettings(candidate[id])||revisions[id]==UINT64_MAX||receipt.clearEpochs[id]!=epochs[id]
   ||receipt.delaySettingsRevisions[id]!=revisions[id]+1||audio.clearEpoch<epochs[id])return false;
  if(audio.clearEpoch==epochs[id]){if(audio.delaySettingsRevision!=receipt.delaySettingsRevisions[id]||audio.delay!=candidate[id])return false;}
  else if(audio.delaySettingsRevision==0){if(audio.delay!=DelaySettings{})return false;}
  else if(record.clearEpoch!=audio.clearEpoch||record.revision!=audio.delaySettingsRevision||record.committed!=audio.delay)return false;
 }
 for(unsigned id=0;id<records.size();++id){auto& record=records[id];const auto& audio=view.tracks[id];
  if(audio.clearEpoch==epochs[id])record={candidate[id],audio.clearEpoch,audio.delaySettingsRevision,receipt.requestId,DelayEditStatus::ready,false};
  else if(audio.delaySettingsRevision==0)record={DelaySettings{},audio.clearEpoch,0,receipt.requestId,DelayEditStatus::cancelled,false};
  // A proven newer post-clear owner keeps its exact values and receipt.
 }
 generation=receipt.generation;attached=true;return true;
}
void DelaySettingsOwner::serviceBank() noexcept {
 DelayBankResult result;if(!engine.popDelayBank(result))return;
 if(!bank.active||result.requestId!=bank.request||result.generation!=bank.generation||result.transaction!=bank.transaction){attached=false;return;}
 bankReceipt=result;bool committed=result.status==DelayEditStatus::ready;
 if(committed)for(unsigned id=0;id<records.size();++id)
  if(result.clearEpochs[id]!=bank.epochs[id]||result.settingsRevisions[id]!=bank.revisions[id]+1){committed=false;attached=false;break;}
 for(unsigned id=0;id<records.size();++id){auto& record=records[id];const bool cleared=record.clearEpoch>bank.epochs[id];
  if(committed&&!cleared){record.committed=bank.candidate[id];record.clearEpoch=result.clearEpochs[id];record.revision=result.settingsRevisions[id];}
  record.status=cleared?DelayEditStatus::cancelled:committed?DelayEditStatus::ready:result.status==DelayEditStatus::ready?DelayEditStatus::stale:result.status;
  record.pending=false;
 }
 bank={};
}
void DelaySettingsOwner::service(const EngineView& view) noexcept {
 if(attached&&generation!=engine.generation()){attached=false;cancel();}
 if(attached&&view.generation==generation)for(unsigned id=0;id<records.size();++id){auto& record=records[id];
  if(view.tracks[id].clearEpoch>record.clearEpoch){
   record.committed={};record.clearEpoch=view.tracks[id].clearEpoch;record.revision=0;
   if(record.pending)cancel(id);
  }
 }
 serviceBank();
 DelayEditResult result;
 for(unsigned n=0;n<records.size()&&engine.popDelayEdit(result);++n){
  if(result.track>=records.size()){attached=false;continue;}
  auto& record=records[result.track];const auto& p=pending[result.track];
  if(!record.pending||p.request!=result.requestId||p.generation!=result.generation||p.transaction!=result.transaction){attached=false;continue;}
  const bool committed=result.status==DelayEditStatus::ready&&record.clearEpoch<=p.epoch
   &&result.clearEpoch==p.epoch&&result.settingsRevision==p.revision+1;
  if(committed){record.committed=p.candidate;record.clearEpoch=result.clearEpoch;record.revision=result.settingsRevision;}
  record.status=committed?DelayEditStatus::ready:result.status==DelayEditStatus::ready?DelayEditStatus::cancelled:result.status;
  record.pending=false;pending[result.track]={};
 }
 if(attached&&view.generation==generation)for(unsigned id=0;id<records.size();++id){const auto& record=records[id];const auto& audio=view.tracks[id];
  if(record.pending||audio.clearEpoch!=record.clearEpoch||audio.delaySettingsRevision<record.revision)continue;
  if(audio.delaySettingsRevision!=record.revision||audio.delay!=record.committed){attached=false;break;}
 }
}
}
