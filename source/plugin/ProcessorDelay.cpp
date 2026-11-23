// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
void AionProcessor::delayCleared(unsigned track,bool all,std::uint32_t request){
 std::lock_guard guard(midiSettingsMutex);
 // An admitted FX bank is serviced before ordinary audio commands. Preserve
 // that ordering: a ready bank may commit before DEL; a still-preparing bank
 // is cancelled by DEL on audio. There cannot be a competing owned D edit.
 if(!effectsRestore.active)delayOwner.cancel(all?aion::LoopBank::trackCount:track);
 for(unsigned id=0;id<delaySettings.size();++id)if(all||id==track){
  delaySettings[id]={};pendingDelayClears[id]={request,engine.generation(),delayOwner.track(id)->clearEpoch};
 }
}
void AionProcessor::serviceDelay(){
 // Sole serialized non-audio service, reached by Timer and ordinary requests.
 // Product commands remain disabled; versioned state uses this same owner.
 delayOwner.service(latest);
 if(!delayOwner.bound()){
  // Equal knob values do not prove ownership of a newer joint revision.
  // Only the enclosing matched receipt may bind this generation's transaction.
  if(effectsRestore.active&&effectsRestore.generation==engine.generation())return;
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
  // A validated newer load follows older FIFO clears. Its candidate survives
  // their acknowledgements; a newer GUI clear already reset that candidate.
  const bool restoreIntent=eqRestorePending||compressorRestorePending||chainRestorePending||arpRestorePending||driveRestorePending;
  if(epoch>delayShadowEpochs[id]){if(!restoreIntent)delaySettings[id]={};delayShadowEpochs[id]=epoch;}
 }
 // Accepted whole-bank intent is protected until its exact joint receipt.
 // In particular, never submit a competing D-only lifecycle bank.
 if(effectsRestore.active||eqRestorePending||compressorRestorePending||chainRestorePending||arpRestorePending||driveRestorePending)return;
 if(delayRestorePending){
  bool matches=true;
  for(unsigned id=0;id<delaySettings.size();++id)matches&=delaySettings[id]==delayOwner.track(id)->committed;
  if(matches){delayRestorePending=false;delayRestoreFailed=false;}
  else {
   return; // The enclosing FX owner prepares both banks together.
  }
 }
 if(!delayRestoreFailed)for(unsigned id=0;id<delaySettings.size();++id)
  if(!delayOwner.track(id)->pending&&!pendingDelayClears[id].request)delaySettings[id]=delayOwner.track(id)->committed;
}
