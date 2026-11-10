// SPDX-License-Identifier: AGPL-3.0-only
#include "ArpDelayStorage.h"
#include <cmath>

namespace aion {
ArpDelayStorage::ArpDelayStorage():slots(std::make_unique<std::array<Slot,capacity>>()){}
bool ArpDelayStorage::prepareStopped(double sampleRate) noexcept {
 if(!std::isfinite(sampleRate)||sampleRate<8000||sampleRate>384000)return false;
 releaseStopped();rate=sampleRate;return true;
}
void ArpDelayStorage::releaseStopped() noexcept {
 for(auto& slot:*slots){slot.graph.reset();slot.phase.store(Phase::free,std::memory_order_relaxed);}
 active.fill(nullptr);pending.fill(nullptr);failed.fill(false);
 outstanding=workerCursor=0;rate=0;
}
bool ArpDelayStorage::request(unsigned track,const ArpModValues& values,std::uint32_t seed) noexcept {
 if(track>=trackCount||!rate||pending[track]||!validArpModValues(values))return false;
 for(auto& slot:*slots)if(slot.phase.load(std::memory_order_acquire)==Phase::free){
  slot.track=track;slot.seed=seed;slot.values=values;
  slot.cancelled.store(false,std::memory_order_relaxed);
  pending[track]=&slot;failed[track]=false;++outstanding;
  slot.phase.store(Phase::queued,std::memory_order_release);return true;
 }
 return false;
}
void ArpDelayStorage::cancel(unsigned track) noexcept {
 if(track>=trackCount)return;
 if(auto* slot=pending[track])slot->cancelled.store(true,std::memory_order_release);
 pending[track]=nullptr;failed[track]=false;
}
void ArpDelayStorage::clear(unsigned track) noexcept {
 if(track>=trackCount)return;
 cancel(track);
 if(auto* slot=active[track]){active[track]=nullptr;slot->phase.store(Phase::retired,std::memory_order_release);}
}
void ArpDelayStorage::clearAll() noexcept {for(unsigned id=0;id<trackCount;++id)clear(id);}
ArpDelayStorage::Completion ArpDelayStorage::poll(std::uint64_t firstFrame) noexcept {
 Completion result;if(!outstanding)return result;
 for(auto& slot:*slots){
  const auto phase=slot.phase.load(std::memory_order_acquire);
  if(phase!=Phase::ready&&phase!=Phase::failed)continue;
  --outstanding;
  const auto id=slot.track;
  if(pending[id]!=&slot){slot.phase.store(Phase::retired,std::memory_order_release);continue;}
  pending[id]=nullptr;
  if(phase==Phase::ready&&slot.graph->reset(slot.seed,firstFrame)){
   if(auto* old=active[id])old->phase.store(Phase::retired,std::memory_order_release);
   active[id]=&slot;failed[id]=false;result.installed|=1u<<id;
   slot.phase.store(Phase::active,std::memory_order_release);
  }else{
   failed[id]=true;result.failed|=1u<<id;
   slot.phase.store(Phase::retired,std::memory_order_release);
  }
 }
 return result;
}
ArpDelayGraph* ArpDelayStorage::graph(unsigned track) noexcept {
 return track<trackCount&&active[track]?&*active[track]->graph:nullptr;
}
ArpDelayStorage::View ArpDelayStorage::view(unsigned track) const noexcept {
 return track<trackCount?View{active[track]!=nullptr,pending[track]!=nullptr,failed[track]}:View{};
}
void ArpDelayStorage::serviceWorker() noexcept {
 for(auto& slot:*slots)if(slot.phase.load(std::memory_order_acquire)==Phase::retired){
  slot.graph.reset();slot.phase.store(Phase::free,std::memory_order_release);
 }
 for(unsigned i=0;i<capacity;++i){
  auto& slot=(*slots)[workerCursor];workerCursor=(workerCursor+1)%capacity;
  if(slot.phase.load(std::memory_order_acquire)!=Phase::queued)continue;
  slot.phase.store(Phase::preparing,std::memory_order_relaxed);
  bool ready=false;
  if(!slot.cancelled.load(std::memory_order_acquire)){
   slot.graph.emplace();ready=slot.graph->prepare(rate,slot.values,slot.seed);
  }
  // Cancellation during allocation is allowed; audio rejects the result by
  // exact slot identity. Neither owner touches a graph held by the other.
  slot.phase.store(ready?Phase::ready:Phase::failed,std::memory_order_release);
  break;
 }
}
}
