// SPDX-License-Identifier: AGPL-3.0-only
#include "DelayStorage.h"
#include <cmath>
namespace aion {
bool DelayStorage::collectBankSlots(const Bank& settings,double bpm,std::array<Slot*,trackCount>& slotsToUse) const noexcept {
 if(!rate||bankActive||!std::isfinite(bpm)||bpm<10||bpm>999)return false;
 unsigned needed=0,available=0;std::size_t required=0,reserved=0;
 for(unsigned id=0;id<trackCount;++id){
  if(pending[id]||remaining[id]||!validDelaySettings(settings[id]))return false;
  if(!settings[id].enabled)continue;
  const auto bytes=Delay::requiredStorageFrames(rate,settings[id])*sizeof(std::array<float,2>);
  if(bytes>budget-required)return false;
  required+=bytes;++needed;
 }
 for(auto& slot:*slots){
  if(slot.phase.load(std::memory_order_acquire)==Phase::free){if(available<needed)slotsToUse[available++]=&slot;}
  else {if(slot.bytes>budget-reserved)return false;reserved+=slot.bytes;}
 }
 if(available<needed||required>budget-reserved)return false;
 return true;
}
bool DelayStorage::canRequestBank(const Bank& settings,double bpm) const noexcept {
 std::array<Slot*,trackCount> slotsToUse{};return collectBankSlots(settings,bpm,slotsToUse);
}
bool DelayStorage::requestBank(const Bank& settings,double bpm) noexcept {
 std::array<Slot*,trackCount> slotsToUse{};if(!collectBankSlots(settings,bpm,slotsToUse))return false;
 // Preflight completed before publishing ANY work. Only this audio owner can
 // claim free slots; worker reclamation can only increase available capacity.
 unsigned slotIndex=0;
 for(unsigned id=0;id<trackCount;++id){
  if(!settings[id].enabled){bankSlots[id]=nullptr;continue;}
  auto& slot=*slotsToUse[slotIndex++];slot.track=id;slot.settings=settings[id];slot.bpm=bpm;
  slot.bytes=Delay::requiredStorageFrames(rate,settings[id])*sizeof(std::array<float,2>);slot.bankMember=true;
  slot.cancelled.store(false,std::memory_order_relaxed);bankSlots[id]=&slot;
  slot.phase.store(Phase::queued,std::memory_order_release);
 }
 bankTempo=bpm;bankPrepared=false;bankActive=true;return true;
}
void DelayStorage::cancelBank() noexcept {
 if(!bankActive)return;
 for(auto*& slot:bankSlots){if(slot)slot->cancelled.store(true,std::memory_order_release);slot=nullptr;}
 bankActive=bankPrepared=false;
}
DelayStorage::BankStatus DelayStorage::bankStatus() const noexcept {
 if(!bankActive)return BankStatus::none;
 bool ready=true,anyFailed=false;
 for(auto* slot:bankSlots)if(slot){
  const auto phase=slot->phase.load(std::memory_order_acquire);
  ready&=phase==Phase::ready;anyFailed|=phase==Phase::failed;
 }
 return anyFailed?BankStatus::failed:ready?BankStatus::ready:BankStatus::pending;
}
bool DelayStorage::prepareBankCommit(double bpm) noexcept {
 bankPrepared=false;if(bankStatus()!=BankStatus::ready)return false;
 if(bpm==0)bpm=bankTempo;
 if(!std::isfinite(bpm)||bpm<10||bpm>999)return false;
 for(auto* slot:bankSlots)if(slot){
  if(!slot->graph->set(slot->settings,bpm))return false;
  slot->graph->reset();
 }
 bankPrepared=true;return true;
}
void DelayStorage::commitBank() noexcept {
 if(!bankActive||!bankPrepared)return;
 // No old owner has been touched above. Commit the entire target bank here;
 // off targets fade old -> dry without allocating a dummy disabled processor.
 for(unsigned id=0;id<trackCount;++id){
  previous[id]=active[id];active[id]=bankSlots[id];
  remaining[id]=hasOwner(id)?fadeFrames:0;
  failed[id]=false;++installations[id];
  if(auto* slot=active[id])slot->phase.store(Phase::active,std::memory_order_release);
 }
 bankSlots.fill(nullptr);bankActive=bankPrepared=false;
}
void DelayStorage::pollBank(double bpm,Completion& result) noexcept {
 const auto status=bankStatus();if(status==BankStatus::none||status==BankStatus::pending)return;
 if(status==BankStatus::failed||!prepareBankCommit(bpm)){cancelBank();result.bankFailed=true;return;}
 commitBank();result.bankInstalled=true;
}
}
