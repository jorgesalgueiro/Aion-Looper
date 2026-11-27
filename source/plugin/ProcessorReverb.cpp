// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
void AionProcessor::reverbCleared(unsigned track,bool all,std::uint32_t request){
 std::lock_guard guard(midiSettingsMutex);
 reverbOwner.cancel(all?aion::LoopBank::trackCount:track);
 for(unsigned id=0;id<reverbSettings.size();++id)if(all||id==track){
  reverbSettings[id]={};pendingReverbClears[id]={request,engine.generation(),reverbOwner.track(id)->clearEpoch};
 }
 lastReverbRequest=0;reverbRestoreFailed=false;
}
void AionProcessor::serviceReverb(){
 // Sole serialized non-audio B service, shared by state and GUI/CLI commands.
 reverbOwner.service(latest);
 if(!reverbOwner.bound()){
  if(latest.generation!=engine.generation())return;
  aion::ReverbSettingsOwner::Bank known{};
  for(unsigned id=0;id<known.size();++id){const auto& confirmed=reverbOwner.track(id)->committed;
   if(confirmed==latest.tracks[id].reverb)known[id]=confirmed;
  }
  if(!reverbOwner.bind(latest,known))return;
 }
 for(unsigned id=0;id<reverbSettings.size();++id){auto& clear=pendingReverbClears[id];
  // The ordinary ID is only a FIFO ordering fence. Current owner metadata,
  // never a larger ID alone, supplies the resulting confirmed settings.
  if(clear.request&&(clear.generation!=engine.generation()||reverbOwner.track(id)->clearEpoch>clear.epoch||latest.lastRequestId>=clear.request))clear={};
 }
 if(reverbShadowGeneration!=engine.generation()){
  reverbShadowGeneration=engine.generation();
  for(unsigned id=0;id<reverbSettings.size();++id)reverbShadowEpochs[id]=reverbOwner.track(id)->clearEpoch;
 }else for(unsigned id=0;id<reverbSettings.size();++id){const auto epoch=reverbOwner.track(id)->clearEpoch;
  // A validated newer load follows older FIFO clears. Its candidate survives
  // their acknowledgements; a newer GUI clear already reset that candidate.
  if(epoch>reverbShadowEpochs[id]){if(!reverbRestorePending)reverbSettings[id]={};reverbShadowEpochs[id]=epoch;}
 }
 if(!reverbOwner.restoring()&&lastReverbRequest&&reverbOwner.restoreReceipt().requestId==lastReverbRequest
  &&reverbOwner.restoreReceipt().status!=aion::ReverbEditStatus::ready){reverbRestoreFailed=true;reverbRestorePending=false;}
 // Individual edit receipts are terminal, including failure/cancellation. Only
 // an explicit state load/prepare owns bank intent; never retry a failed edit.
 if(!reverbRestorePending){
  if(!reverbRestoreFailed&&!reverbOwner.restoring())for(unsigned id=0;id<reverbSettings.size();++id)
   if(!reverbOwner.track(id)->pending&&!pendingReverbClears[id].request)reverbSettings[id]=reverbOwner.track(id)->committed;
  return;
 }
 for(unsigned id=0;id<reverbSettings.size();++id)if(reverbOwner.track(id)->pending)return;
 if(reverbOwner.restoring())return;
 bool differs=false;
 for(unsigned id=0;id<reverbSettings.size();++id){
  differs|=reverbSettings[id]!=reverbOwner.track(id)->committed;
 }
 if(!differs){reverbRestorePending=false;reverbRestoreFailed=false;return;}
 if(reverbRestoreFailed)return;
 if(!meterPrepared()||latest.generation!=engine.generation()||nextReverbRequest==UINT32_MAX)return;
 if(reverbOwner.restore(reverbSettings,nextReverbRequest+1))lastReverbRequest=++nextReverbRequest;
}
