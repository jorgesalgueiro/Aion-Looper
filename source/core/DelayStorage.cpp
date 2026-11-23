// SPDX-License-Identifier: AGPL-3.0-only
#include "DelayStorage.h"
#include <cmath>
namespace aion {
DelayStorage::DelayStorage(std::size_t byteBudget):slots(std::make_unique<std::array<Slot,capacity>>()),budget(byteBudget){}
bool DelayStorage::prepareStopped(double sr) noexcept {
 if(!std::isfinite(sr)||sr<8000||sr>384000)return false;
 releaseStopped();rate=sr;fadeFrames=static_cast<unsigned>(std::ceil(.02*sr));return true;
}
void DelayStorage::releaseStopped() noexcept {
 for(auto& slot:*slots){slot.graph.reset();slot.phase.store(Phase::free,std::memory_order_relaxed);}
 active.fill(nullptr);pending.fill(nullptr);previous.fill(nullptr);remaining.fill(0);bankSlots.fill(nullptr);bankActive=bankPrepared=false;
 failed.fill(false);installations.fill(0);cursor=fadeFrames=0;rate=0;bankTempo=120;
}
bool DelayStorage::request(unsigned track,const DelaySettings& settings,double bpm) noexcept {
 if(track>=trackCount||!rate||bankActive||pending[track]||remaining[track]||!std::isfinite(bpm)||bpm<10||bpm>999)return false;
 const auto frames=Delay::requiredStorageFrames(rate,settings);if(!frames)return false;
 const auto bytes=frames*sizeof(std::array<float,2>);std::size_t reserved=0;Slot* available=nullptr;
 for(auto& slot:*slots){
  if(slot.phase.load(std::memory_order_acquire)==Phase::free){if(!available)available=&slot;}
  else {if(slot.bytes>budget-reserved)return false;reserved+=slot.bytes;}
 }
 if(!available||bytes>budget-reserved)return false;
 auto& slot=*available;slot.track=track;slot.bytes=bytes;slot.bpm=bpm;slot.settings=settings;slot.bankMember=false;
 slot.cancelled.store(false,std::memory_order_relaxed);pending[track]=&slot;failed[track]=false;
 slot.phase.store(Phase::queued,std::memory_order_release);return true;
}
void DelayStorage::cancel(unsigned track) noexcept {
 if(track>=trackCount)return;
 cancelBank();
 if(auto* slot=pending[track])slot->cancelled.store(true,std::memory_order_release);
 pending[track]=nullptr;failed[track]=false;
}
void DelayStorage::clear(unsigned track) noexcept {
 if(track>=trackCount)return;
 cancel(track);if(auto* slot=active[track]){active[track]=nullptr;slot->phase.store(Phase::retired,std::memory_order_release);}
 if(auto* slot=previous[track]){previous[track]=nullptr;slot->phase.store(Phase::retired,std::memory_order_release);}
 remaining[track]=0;
}
void DelayStorage::clearAll() noexcept {for(unsigned id=0;id<trackCount;++id)clear(id);}
void DelayStorage::reset(unsigned track,bool fadeIn) noexcept {
 if(track>=trackCount)return;
 cancel(track);
 if(auto* old=previous[track]){previous[track]=nullptr;old->phase.store(Phase::retired,std::memory_order_release);}
 if(auto* slot=active[track])slot->graph->reset();
 remaining[track]=active[track]&&fadeIn?fadeFrames:0;
}
bool DelayStorage::setTempo(unsigned track,double bpm) noexcept {
 if(track>=trackCount||!std::isfinite(bpm)||bpm<10||bpm>999)return false;
 if(auto* slot=active[track])(void)slot->graph->set(slot->graph->settings(),bpm);
 if(auto* slot=previous[track])(void)slot->graph->set(slot->graph->settings(),bpm);
 return true;
}
DelayStorage::Completion DelayStorage::poll(double bankBpm,bool deferBank) noexcept {
 Completion result;if(!deferBank)pollBank(bankBpm,result);
 for(auto& slot:*slots){
  const auto phase=slot.phase.load(std::memory_order_acquire);
  if(phase!=Phase::ready&&phase!=Phase::failed)continue;
  const auto id=slot.track;
  if(slot.bankMember&&bankActive&&bankSlots[id]==&slot)continue;
  if(pending[id]!=&slot){slot.phase.store(Phase::retired,std::memory_order_release);continue;}
  pending[id]=nullptr;
  if(phase==Phase::ready){
   previous[id]=active[id];remaining[id]=fadeFrames;
   active[id]=&slot;failed[id]=false;++installations[id];result.installed|=1u<<id;
   slot.phase.store(Phase::active,std::memory_order_release);
  }else{failed[id]=true;result.failed|=1u<<id;slot.phase.store(Phase::retired,std::memory_order_release);}
 }
 return result;
}
Delay* DelayStorage::graph(unsigned track) noexcept {return track<trackCount&&active[track]?&*active[track]->graph:nullptr;}
DelayStorage::View DelayStorage::view(unsigned track) const noexcept {
 return track<trackCount?View{active[track]!=nullptr,pending[track]!=nullptr||bankActive,failed[track],installations[track],remaining[track]!=0}:View{};
}
void DelayStorage::tick(unsigned track,float left,float right,float& outLeft,float& outRight) noexcept {
 if(track>=trackCount){outLeft=left;outRight=right;return;}
 if(auto* current=active[track])current->graph->tick(left,right,outLeft,outRight);
 else {outLeft=left;outRight=right;}
 if(!remaining[track])return;
 float oldLeft=left,oldRight=right;
 if(auto* old=previous[track])old->graph->tick(left,right,oldLeft,oldRight);
 const double blend=1-static_cast<double>(remaining[track])/fadeFrames;
 outLeft=static_cast<float>(oldLeft+blend*(double(outLeft)-oldLeft));
 outRight=static_cast<float>(oldRight+blend*(double(outRight)-oldRight));
 if(!--remaining[track])if(auto* old=previous[track]){
  previous[track]=nullptr;old->phase.store(Phase::retired,std::memory_order_release);
 }
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
