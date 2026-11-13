// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include <algorithm>
AionProcessor::ArpTiming AionProcessor::readArpTiming() const noexcept {
 return {syncBpm->load(),syncMode->load(),syncBars->load(),syncNum->load(),syncDen->load(),countIn->load(),countInAudio->load(),autoPlay->load()};
}
void AionProcessor::observeArpTempo(bool gesture){
 const auto next=readArpTiming();if(gesture||next!=arpTiming){arpTiming=next;++arpTempoIntent;}
}
void AionProcessor::serviceArpTempo(bool submit){
 // Existing message Timer/RPC service, under midiSettingsMutex. No listener,
 // allocation, lock or browser activity is added to the audio callback.
 observeArpTempo();
 if(arpTempo.active){
  if(arpOwner.restoring())return;
  const auto& receipt=arpOwner.restoreReceipt();
  if(receipt.requestId!=arpTempo.request||receipt.generation!=arpTempo.generation)return;
  // Cancellation consumes this accepted gesture; a newer timing gesture remains
  // dirty. prepare/state restore retain their own accepted original bank intent.
  arpTempoSeen=std::max(arpTempoSeen,arpTempo.intent);arpTempo={};
 }
 if(!submit||arpTempoSeen==arpTempoIntent||!arpOwner.bound()||!meterPrepared()||latest.generation!=engine.generation()
    ||eqRestorePending||compressorRestorePending||chainRestorePending||arpRestorePending||effectsRestore.active||arpOwner.restoring())return;
 bool enabled=false;for(unsigned id=0;id<arpSettings.size();++id){const auto* track=arpOwner.track(id);
  if(track->pending)return; // In particular, do not consume a tempo change while the first enable is preparing.
  enabled|=track->committed.controls.enabled;
 }
 if(!enabled){arpTempoSeen=arpTempoIntent;return;} // Original updateSync has no worklet when bypassed.
 if(nextRequest==UINT32_MAX||!arpOwner.tempo(arpTiming[0],nextRequest+1))return; // Busy admission retains bounded latest intent.
 arpTempo={arpTempoIntent,++nextRequest,engine.generation(),true};
 for(auto& original:arpSettings)if(original.controls.enabled)original.bpm=arpTiming[0];
}
