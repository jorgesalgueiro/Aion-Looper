// SPDX-License-Identifier: AGPL-3.0-only
#include "Engine.h"
#include <algorithm>
#include <cmath>
namespace aion {
bool Engine::enqueueReverbEdit(LoopCommand command,std::uint64_t& transaction) noexcept {
 if(command.action!=LoopAction::reverb||command.track>=LoopBank::trackCount||!command.requestId
  ||!command.generation||command.generation!=generation()||command.arpTransaction||command.delayTransaction
  ||command.reverbTransaction||!validReverbSettings(command.reverb)||command.reverbSettingsRevision==UINT64_MAX
  ||coordinatedReverbBank.load(std::memory_order_acquire)
  ||outstandingReverb[command.track]||outstandingReverbBank||nextReverbTransaction==UINT64_MAX)return false;
 if(!reverbRequests->canPush()||!loopCommands->canPush())return false;
 command.reverbTransaction=nextReverbTransaction+1;
 ReverbEditRequest request{command.track,command.requestId,command.generation,command.reverbClearEpoch,
  command.reverbSettingsRevision,command.reverbTransaction,reverbCancellations[command.track].load(std::memory_order_acquire)};
 // Sole producer of both queues. Audio can only free room after preflight.
 // Register the never-reused identity before the FIFO command is published.
 (void)reverbRequests->push(request);(void)loopCommands->push(command);
 nextReverbTransaction=command.reverbTransaction;outstandingReverb[command.track]=true;
 transaction=command.reverbTransaction;return true;
}
bool Engine::popReverbEdit(ReverbEditResult& result) noexcept {
 if(!reverbResults->pop(result))return false;
 outstandingReverb[result.track]=false;return true;
}
void Engine::cancelReverbEdit(unsigned track) noexcept {
 if(track<=LoopBank::trackCount)cancelReverbBank();
 if(track<LoopBank::trackCount)reverbCancellations[track].fetch_add(1,std::memory_order_release);
 else if(track==LoopBank::trackCount)for(auto& serial:reverbCancellations)serial.fetch_add(1,std::memory_order_release);
}
void Engine::finishReverbEdit(unsigned id,ReverbEditStatus status) noexcept {
 auto& p=(*pendingReverb)[id];
 if(!p.finished){const auto& r=p.request;const auto info=loops.trackView(id);
  p.completion={id,r.requestId,r.generation,status,r.transaction,info.reverbSettingsRevision,info.clearEpoch,frame};p.finished=true;
 }
 // At most twenty accepted/unread edits exist; the 32-result queue exceeds
 // that bound. Retain the immutable receipt on unexpected capacity failure.
 if(reverbResults->push(p.completion))p={};
}
void Engine::cancelStaleReverbEdits() noexcept {
 for(unsigned id=0;id<LoopBank::trackCount;++id){auto& p=(*pendingReverb)[id];if(!p.active)continue;
  if(p.finished){finishReverbEdit(id,p.completion.status);continue;}
  const auto& r=p.request;const auto info=loops.trackView(id);
  const bool cancelled=r.generation!=generation()||r.cancellation!=reverbCancellations[id].load(std::memory_order_acquire);
  const bool stale=r.clearEpoch!=info.clearEpoch||r.settingsRevision!=info.reverbSettingsRevision;
  if(cancelled||stale){if(p.preparing)loops.cancelReverb(id);finishReverbEdit(id,cancelled?ReverbEditStatus::cancelled:ReverbEditStatus::stale);}
 }
}
void Engine::serviceReverbEdits() noexcept {
 // Called after LoopBank adoption, before ordinary FIFO commands. An already
 // committed success is terminal even if a later cancellation arrives now.
 for(unsigned id=0;id<LoopBank::trackCount;++id){auto& p=(*pendingReverb)[id];
  if(!p.active||p.finished||!p.preparing)continue;
  const auto view=loops.reverbStorageView(id);if(view.pending)continue;
  const auto info=loops.trackView(id);
  const bool own=view.active&&view.installation!=p.installation&&info.reverbRequest==p.request.requestId
   &&info.clearEpoch==p.request.clearEpoch&&info.reverbSettingsRevision==p.request.settingsRevision+1;
  finishReverbEdit(id,own?ReverbEditStatus::ready:view.failed?ReverbEditStatus::failed:ReverbEditStatus::cancelled);
 }
 ReverbEditRequest request;
 for(unsigned n=0;n<LoopBank::trackCount&&reverbRequests->pop(request);++n){
  seenReverbTransaction=std::max(seenReverbTransaction,request.transaction);
  auto& p=(*pendingReverb)[request.track];p={};p.request=request;p.active=true;
  const auto info=loops.trackView(request.track);
  if(request.generation!=generation()||request.cancellation!=reverbCancellations[request.track].load(std::memory_order_acquire))
   finishReverbEdit(request.track,ReverbEditStatus::cancelled);
  else if(request.clearEpoch!=info.clearEpoch||request.settingsRevision!=info.reverbSettingsRevision)
   finishReverbEdit(request.track,ReverbEditStatus::stale);
 }
}
bool Engine::currentReverbEdit(const LoopCommand& command) const noexcept {
 if(command.track>=LoopBank::trackCount)return false;
 const auto& p=(*pendingReverb)[command.track];
 return p.active&&!p.finished&&p.request.transaction==command.reverbTransaction&&p.request.requestId==command.requestId
  &&p.request.generation==command.generation&&command.generation==generation()
  &&p.request.cancellation==reverbCancellations[command.track].load(std::memory_order_acquire);
}
void Engine::applyReverbEdit(const LoopCommand& command) noexcept {
 auto& p=(*pendingReverb)[command.track];
 if(!currentReverbEdit(command)){
  if(p.active&&!p.finished&&p.request.transaction==command.reverbTransaction)finishReverbEdit(command.track,ReverbEditStatus::cancelled);
  return;
 }
 const auto info=loops.trackView(command.track);
 if(info.clearEpoch!=command.reverbClearEpoch||info.reverbSettingsRevision!=command.reverbSettingsRevision){finishReverbEdit(command.track,ReverbEditStatus::stale);return;}
 // Live same-shape/disabled edits never allocate. A refused live set means the
 // requested enable needs a new impulse/graph, so defer the receipt to adoption.
 if(loops.setReverb(command.track,command.reverb,command.reverbClearEpoch,command.reverbSettingsRevision,command.requestId)){
  finishReverbEdit(command.track,ReverbEditStatus::ready);return;
 }
 if(!command.reverb.enabled){finishReverbEdit(command.track,ReverbEditStatus::failed);return;}
 p.installation=loops.reverbStorageView(command.track).installation;
 if(!loops.requestReverb(command.track,command.reverb,command.reverbClearEpoch,command.reverbSettingsRevision,command.requestId)){
  finishReverbEdit(command.track,ReverbEditStatus::busy);return;
 }
 p.preparing=true;
}
void Engine::cancelAllReverbEdits() noexcept {
 // Stopped prepare temporarily owns the audio endpoints. Keep unread old-
 // generation receipts and message outstanding flags until actually consumed.
 cancelReverbEdit();cancelStaleReverbEdits();cancelQueuedReverbBank();ReverbEditRequest request;
 for(unsigned n=0;n<LoopBank::trackCount&&reverbRequests->pop(request);++n){
  seenReverbTransaction=std::max(seenReverbTransaction,request.transaction);
  auto& p=(*pendingReverb)[request.track];p={};p.request=request;p.active=true;
  finishReverbEdit(request.track,ReverbEditStatus::cancelled);
 }
}
}
