// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
void AionProcessor::delayCleared(unsigned track,bool all,std::uint32_t request){
 std::lock_guard guard(midiSettingsMutex);
 delayOwner.cancel(all?aion::LoopBank::trackCount:track);
 for(unsigned id=0;id<delaySettings.size();++id)if(all||id==track){
  delaySettings[id]={};pendingDelayClears[id]={request,engine.generation(),delayOwner.track(id)->clearEpoch};
 }
}
void AionProcessor::serviceDelay(){
 // Sole serialized non-audio service, reached by Timer and ordinary requests.
 // No browser, CLI or session field exposes D until coordinated restore exists.
 delayOwner.service(latest);
 if(!delayOwner.bound()){
  if(latest.generation!=engine.generation())return;
  aion::DelaySettingsOwner::Bank known{};
  for(unsigned id=0;id<known.size();++id){const auto& confirmed=delayOwner.track(id)->committed;
   if(confirmed==latest.tracks[id].delay)known[id]=confirmed;
  }
  if(!delayOwner.bind(latest,known))return;
 }
 for(unsigned id=0;id<delaySettings.size();++id){auto& clear=pendingDelayClears[id];
  // The ordinary ID is only a FIFO ordering fence. Current owner metadata,
  // never a larger ID alone, supplies the resulting confirmed settings.
  if(clear.request&&(clear.generation!=engine.generation()||delayOwner.track(id)->clearEpoch>clear.epoch||latest.lastRequestId>=clear.request))clear={};
 }
 if(delayShadowGeneration!=engine.generation()){
  delayShadowGeneration=engine.generation();
  for(unsigned id=0;id<delaySettings.size();++id)delayShadowEpochs[id]=delayOwner.track(id)->clearEpoch;
 }else for(unsigned id=0;id<delaySettings.size();++id){const auto epoch=delayOwner.track(id)->clearEpoch;
  if(epoch>delayShadowEpochs[id]){delaySettings[id]={};delayShadowEpochs[id]=epoch;}
 }
 if(delayRestoreActive){
  if(delayOwner.restoring())return;
  const auto& receipt=delayOwner.restoreReceipt();
  if(receipt.requestId!=delayRestoreRequest||receipt.generation!=delayRestoreGeneration)return;
  delayRestoreActive=false;delayRestorePending=false;
  delayRestoreFailed=receipt.status!=aion::DelayEditStatus::ready;
  // OOM/budget failure after a changed-rate reset retains the confirmed prior
  // intent for an explicit later prepare, without spinning allocations on Timer.
  // Clear/panic cancellation must instead retain the actual surviving bank.
  if(receipt.status==aion::DelayEditStatus::failed||receipt.status==aion::DelayEditStatus::busy)return;
  for(unsigned id=0;id<delaySettings.size();++id)if(!pendingDelayClears[id].request)delaySettings[id]=delayOwner.track(id)->committed;
 }
 if(delayRestorePending){
  bool matches=true;
  for(unsigned id=0;id<delaySettings.size();++id)matches&=delaySettings[id]==delayOwner.track(id)->committed;
  if(matches){delayRestorePending=false;delayRestoreFailed=false;}
  else {
   if(!meterPrepared()||latest.generation!=engine.generation()||nextRequest==UINT32_MAX)return;
   // Internal lifecycle-only restore. Product state loading must eventually use
   // one coordinated D/Arp/effects transaction, not two independent successes.
   if(!delayOwner.restore(delaySettings,nextRequest+1))return;
   delayRestoreRequest=++nextRequest;delayRestoreGeneration=engine.generation();delayRestoreActive=true;return;
  }
 }
 if(!delayRestoreFailed)for(unsigned id=0;id<delaySettings.size();++id)
  if(!delayOwner.track(id)->pending&&!pendingDelayClears[id].request)delaySettings[id]=delayOwner.track(id)->committed;
}
