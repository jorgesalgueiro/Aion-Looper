// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
void AionProcessor::clearPendingReverbShapes(unsigned track,bool all){
 for(unsigned id=0;id<pendingReverbShapes.size();++id)if(all||id==track)pendingReverbShapes[id]={};
}
void AionProcessor::reverbCleared(unsigned track,bool all,std::uint32_t request){
 std::lock_guard guard(midiSettingsMutex);
 clearPendingReverbShapes(track,all);
 reverbOwner.cancel(all?aion::LoopBank::trackCount:track);
 for(unsigned id=0;id<reverbSettings.size();++id)if(all||id==track){
  reverbSettings[id]={};pendingReverbClears[id]={request,engine.generation(),reverbOwner.track(id)->clearEpoch};
 }
 lastReverbRequest=0;reverbRestoreFailed=false;
}
void AionProcessor::serviceReverb(){
 // Sole serialized non-audio B service, shared by state and GUI/CLI commands.
 // State/prepare restores travel in the joint Arp/effects bank; this service
 // binds/mirrors intent, services edits and never submits a competing B bank.
 reverbOwner.service(latest);
 aion::ReverbMidiShapeRequest shape;
 for(unsigned n=0;n<64&&engine.popReverbMidiShape(shape);++n){
  if(!engine.reverbMidiShapeCurrent(shape)||shape.track>=reverbSettings.size())continue;
  const unsigned slot=shape.parameter==2?0:shape.parameter==3?1:2;
  if(slot<2)pendingReverbShapes[shape.track][slot]={shape,true};
 }
 if(!reverbOwner.bound()){
  // Equal values alone cannot prove ownership of a newer joint revision.
  if(effectsRestore.active&&effectsRestore.generation==engine.generation())return;
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
 // Accepted whole-bank intent is protected until its exact joint receipt.
 // Never submit a B-only lifecycle bank while that owner is preparing.
 if(effectsRestore.active||eqRestorePending||compressorRestorePending||chainRestorePending||arpRestorePending||driveRestorePending||delayRestorePending)return;
 if(reverbRestorePending){
  bool matches=true;
  for(unsigned id=0;id<reverbSettings.size();++id)
   matches&=!reverbOwner.track(id)->pending&&reverbSettings[id]==reverbOwner.track(id)->committed;
  if(matches){reverbRestorePending=false;reverbRestoreFailed=false;}
  else return; // The enclosing joint owner prepares the bank after edits settle.
 }
 // Individual edit receipts are terminal, including failure/cancellation.
 if(!reverbRestoreFailed)for(unsigned id=0;id<reverbSettings.size();++id)
  if(!reverbOwner.track(id)->pending&&!pendingReverbClears[id].request)reverbSettings[id]=reverbOwner.track(id)->committed;
 // Latest shape-control CC per track is prepared by this owner. One queued
 // edit per track/parameter; newer values replace older intents.
 unsigned submitted=0;
 for(unsigned id=0;id<reverbSettings.size()&&submitted<8;++id)for(unsigned slot=0;slot<2&&submitted<8;++slot){
  auto& pending=pendingReverbShapes[id][slot];
  if(!pending.active)continue;
  const auto& request=pending.request;const auto* record=reverbOwner.track(id);
  if(!engine.reverbMidiShapeCurrent(request)||request.epoch!=record->clearEpoch
   ||request.revision>record->revision||record->committed.engine!=aion::ReverbEngine::convolution){pending={};continue;}
  if(record->pending||pendingReverbClears[id].request||reverbRestoreFailed)continue;
  if(nextReverbRequest==UINT32_MAX)return;
  if(reverbOwner.set(id,slot?3:2,request.value,nextReverbRequest+1)){
   ++nextReverbRequest;reverbSettings[id]=*reverbOwner.intent(id);pending.active=false;++submitted;
  }
 }
}
