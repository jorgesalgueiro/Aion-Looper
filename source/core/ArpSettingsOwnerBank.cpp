// SPDX-License-Identifier: AGPL-3.0-only
#include "ArpSettingsOwner.h"
namespace aion {
bool ArpSettingsOwner::restore(const Bank& candidate,std::uint32_t request,const LoopBank::EffectsBank* effects) noexcept {
 if(!bound()||bank.active||!request)return false;
 ArpBankRequest command;command.requestId=request;command.generation=generation;
 if(effects){command.effects=*effects;command.restoreEffects=true;}
 for(unsigned id=0;id<records.size();++id){const auto& r=records[id];
  if(r.pending||r.revision==UINT64_MAX||!compileArpSettings(candidate[id].controls,candidate[id].bpm,candidate[id].seed,command.settings[id]))return false;
  command.clearEpochs[id]=r.clearEpoch;command.settingsRevisions[id]=r.revision;
 }
 if(!engine.enqueueArpBank(command))return false;
 bank={candidate,command.clearEpochs,command.settingsRevisions,generation,request,true};
 for(auto& r:records){r.pending=true;r.requestId=request;}
 return true;
}
void ArpSettingsOwner::serviceBank() noexcept {
 ArpBankResult result;if(!engine.popArpBank(result))return;
 bankReceipt=result;
 if(!bank.active||result.requestId!=bank.request||result.generation!=bank.generation){attached=false;return;}
 bool committed=result.status==ArpPreparationStatus::ready;
 // Validate the entire immutable completion before changing any original value.
 // A same-generation clear observed after adoption wins only on its own track;
 // the other nineteen accepted records must still keep their original precision.
 if(committed)for(unsigned id=0;id<records.size();++id)
  if(result.clearEpochs[id]!=bank.epochs[id]||result.settingsRevisions[id]!=bank.revisions[id]+1){committed=false;attached=false;break;}
 bankStatus=committed?ArpPreparationStatus::ready:result.status==ArpPreparationStatus::ready?ArpPreparationStatus::failed:result.status;
 for(unsigned id=0;id<records.size();++id){auto& r=records[id];
  const bool cleared=r.clearEpoch>bank.epochs[id];
  if(committed&&!cleared){r.committed=bank.candidate[id];r.clearEpoch=result.clearEpochs[id];r.revision=result.settingsRevisions[id];}
  r.status=cleared?ArpPreparationStatus::cancelled:bankStatus;r.pending=false;pending[id]={};
 }
 bank={};
}
}
