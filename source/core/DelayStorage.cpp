// SPDX-License-Identifier: AGPL-3.0-only
#include "DelayStorage.h"
#include <cmath>
namespace aion {
DelayStorage::DelayStorage(std::size_t byteBudget):slots(std::make_unique<std::array<Slot,capacity>>()),budget(byteBudget){}
bool DelayStorage::prepareStopped(double sr) noexcept {
 if(!std::isfinite(sr)||sr<8000||sr>384000)return false;
 releaseStopped();rate=sr;return true;
}
void DelayStorage::releaseStopped() noexcept {
 for(auto& slot:*slots){slot.graph.reset();slot.phase.store(Phase::free,std::memory_order_relaxed);}
 active.fill(nullptr);pending.fill(nullptr);failed.fill(false);installations.fill(0);cursor=0;rate=0;
}
bool DelayStorage::request(unsigned track,const DelaySettings& settings,double bpm) noexcept {
 if(track>=trackCount||!rate||pending[track]||!std::isfinite(bpm)||bpm<10||bpm>999)return false;
 const auto frames=Delay::requiredStorageFrames(rate,settings);if(!frames)return false;
 const auto bytes=frames*sizeof(std::array<float,2>);std::size_t reserved=0;Slot* available=nullptr;
 for(auto& slot:*slots){
  if(slot.phase.load(std::memory_order_acquire)==Phase::free){if(!available)available=&slot;}
  else {if(slot.bytes>budget-reserved)return false;reserved+=slot.bytes;}
 }
 if(!available||bytes>budget-reserved)return false;
 auto& slot=*available;slot.track=track;slot.bytes=bytes;slot.bpm=bpm;slot.settings=settings;
 slot.cancelled.store(false,std::memory_order_relaxed);pending[track]=&slot;failed[track]=false;
 slot.phase.store(Phase::queued,std::memory_order_release);return true;
}
void DelayStorage::cancel(unsigned track) noexcept {
 if(track>=trackCount)return;
 if(auto* slot=pending[track])slot->cancelled.store(true,std::memory_order_release);
 pending[track]=nullptr;failed[track]=false;
}
void DelayStorage::clear(unsigned track) noexcept {
 if(track>=trackCount)return;
 cancel(track);if(auto* slot=active[track]){active[track]=nullptr;slot->phase.store(Phase::retired,std::memory_order_release);}
}
void DelayStorage::clearAll() noexcept {for(unsigned id=0;id<trackCount;++id)clear(id);}
DelayStorage::Completion DelayStorage::poll() noexcept {
 Completion result;
 for(auto& slot:*slots){
  const auto phase=slot.phase.load(std::memory_order_acquire);
  if(phase!=Phase::ready&&phase!=Phase::failed)continue;
  const auto id=slot.track;
  if(pending[id]!=&slot){slot.phase.store(Phase::retired,std::memory_order_release);continue;}
  pending[id]=nullptr;
  if(phase==Phase::ready){
   if(auto* old=active[id])old->phase.store(Phase::retired,std::memory_order_release);
   active[id]=&slot;failed[id]=false;++installations[id];result.installed|=1u<<id;
   slot.phase.store(Phase::active,std::memory_order_release);
  }else{failed[id]=true;result.failed|=1u<<id;slot.phase.store(Phase::retired,std::memory_order_release);}
 }
 return result;
}
Delay* DelayStorage::graph(unsigned track) noexcept {return track<trackCount&&active[track]?&*active[track]->graph:nullptr;}
DelayStorage::View DelayStorage::view(unsigned track) const noexcept {
 return track<trackCount?View{active[track]!=nullptr,pending[track]!=nullptr,failed[track],installations[track]}:View{};
}
void DelayStorage::serviceWorker() noexcept {
 for(auto& slot:*slots)if(slot.phase.load(std::memory_order_acquire)==Phase::retired){
  slot.graph.reset();slot.phase.store(Phase::free,std::memory_order_release);
 }
 for(unsigned n=0;n<capacity;++n){
  auto& slot=(*slots)[cursor];cursor=(cursor+1)%capacity;
  if(slot.phase.load(std::memory_order_acquire)!=Phase::queued)continue;
  slot.phase.store(Phase::preparing,std::memory_order_relaxed);bool ready=false;
  if(!slot.cancelled.load(std::memory_order_acquire)){slot.graph.emplace();ready=slot.graph->prepare(rate,slot.settings,slot.bpm);}
  slot.phase.store(ready?Phase::ready:Phase::failed,std::memory_order_release);break;
 }
}
}
