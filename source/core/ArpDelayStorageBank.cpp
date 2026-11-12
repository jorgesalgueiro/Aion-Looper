// SPDX-License-Identifier: AGPL-3.0-only
#include "ArpDelayStorage.h"
namespace aion {
bool ArpDelayStorage::requestBank(const Bank& settings) noexcept {
 if(!rate||bankActive)return false;
 unsigned needed=0,available=0;
 for(unsigned id=0;id<trackCount;++id){
  if(pending[id]||!validArpRenderSettings(settings[id]))return false;
  needed+=settings[id].enabled;
 }
 std::array<Slot*,trackCount> reserved{};
 for(auto& slot:*slots)if(available<needed&&slot.phase.load(std::memory_order_acquire)==Phase::free)reserved[available++]=&slot;
 if(available<needed)return false;
 // Only audio can claim free slots. Worker can only add more free slots, so
 // the reserved pointers cannot lose ownership after this bounded preflight.
 unsigned cursor=0;
 for(unsigned id=0;id<trackCount;++id){
  if(!settings[id].enabled){bankSlots[id]=nullptr;continue;}
  auto& slot=*reserved[cursor++];slot.track=id;slot.seed=settings[id].seed;slot.values=settings[id].values;slot.bankMember=true;
  slot.cancelled.store(false,std::memory_order_relaxed);bankSlots[id]=&slot;++outstanding;
  slot.phase.store(Phase::queued,std::memory_order_release);
 }
 bankActive=true;return true;
}
void ArpDelayStorage::cancelBank() noexcept {
 if(!bankActive)return;
 for(auto*& slot:bankSlots){if(slot)slot->cancelled.store(true,std::memory_order_release);slot=nullptr;}
 bankActive=false;
}
void ArpDelayStorage::pollBank(std::uint64_t firstFrame,Completion& result) noexcept {
 if(!bankActive)return;
 bool ready=true,anyFailed=false;
 for(auto* slot:bankSlots)if(slot){
  const auto phase=slot->phase.load(std::memory_order_acquire);
  anyFailed|=phase==Phase::failed;ready&=phase==Phase::ready;
 }
 if(!anyFailed&&!ready)return;
 if(!anyFailed)for(auto* slot:bankSlots)if(slot&&!slot->graph->reset(slot->seed,firstFrame)){anyFailed=true;break;}
 if(anyFailed){cancelBank();result.bankFailed=true;return;}
 // Every replacement is now ready; no old graph/settings were touched above.
 // Retire and install all twenty owners at this one audio callback boundary.
 for(unsigned id=0;id<trackCount;++id){
  if(auto* old=active[id])old->phase.store(Phase::retired,std::memory_order_release);
  active[id]=bankSlots[id];failed[id]=false;
  if(auto* slot=active[id]){--outstanding;++installations[id];slot->phase.store(Phase::active,std::memory_order_release);}
 }
 bankSlots.fill(nullptr);bankActive=false;result.bankInstalled=true;
}
}
