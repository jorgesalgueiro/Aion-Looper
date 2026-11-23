// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Delay.h"
#include <atomic>
#include <memory>
#include <optional>
namespace aion {
// Single audio owner + sole existing worker. Internal preparation component;
// used by LoopBank's existing worker. Borrowed graphs never enter UI snapshots.
class DelayStorage {
public:
 static constexpr unsigned trackCount=20,capacity=40;
 static constexpr std::size_t defaultBudget=256u*1024u*1024u;
 struct View {bool active=false,pending=false,failed=false;std::uint64_t installation=0;bool transitioning=false;};
 struct Completion {std::uint32_t installed=0,failed=0;bool bankInstalled=false,bankFailed=false;};
 using Bank=std::array<DelaySettings,trackCount>;
 explicit DelayStorage(std::size_t byteBudget=defaultBudget);
 DelayStorage(const DelayStorage&)=delete;
 DelayStorage& operator=(const DelayStorage&)=delete;
 // Both owners must be quiescent. Invalid rates leave the bank untouched.
 bool prepareStopped(double sampleRate) noexcept;
 void releaseStopped() noexcept;
 // Audio-only. Fixed slot and ring-byte limits reject before mutation. One
 // pending preparation per track; old owner survives cancellation or failure.
 bool request(unsigned track,const DelaySettings&,double bpm) noexcept;
 bool requestBank(const Bank&,double bpm) noexcept;
 bool canRequestBank(const Bank&,double bpm) const noexcept;
 enum class BankStatus {none,pending,ready,failed};
 BankStatus bankStatus() const noexcept;
 bool prepareBankCommit(double bpm=0) noexcept;
 void commitBank() noexcept; // Same audio owner, immediately after successful prepare.
 void cancelBank() noexcept;
 void cancel(unsigned track) noexcept;
 void clear(unsigned track) noexcept;
 void clearAll() noexcept;
 void reset(unsigned track,bool fadeIn=false) noexcept; // cancel, clear history, retain active settings/storage
 bool setTempo(unsigned track,double bpm) noexcept; // both pinned transition owners
 Completion poll(double bankBpm=0,bool deferBank=false) noexcept;
 bool hasOwner(unsigned track) const noexcept {return track<trackCount&&(active[track]||previous[track]);}
 Delay* graph(unsigned track) noexcept; // control/inspection borrow until poll/clear/prepare; render through tick
 // Audio-only: crossfade prepared replacements for 20ms, retaining both owners.
 // Further replacement admission waits for this bounded transition to finish.
 void tick(unsigned track,float left,float right,float& outLeft,float& outRight) noexcept;
 View view(unsigned track) const noexcept;
 // Sole worker: reclaim retired storage before allocating at most one request.
 void serviceWorker() noexcept;
private:
 enum class Phase : unsigned {free,queued,preparing,ready,failed,active,retired};
 static_assert(std::atomic<Phase>::is_always_lock_free);
 static_assert(std::atomic<bool>::is_always_lock_free);
 struct Slot {
  std::atomic<Phase> phase{Phase::free};std::atomic<bool> cancelled{false};
  unsigned track=0;std::size_t bytes=0;double bpm=120;DelaySettings settings{};
  bool bankMember=false; // immutable from queued publication through retirement
  std::optional<Delay> graph; // created/destroyed only off audio
 };
 std::unique_ptr<std::array<Slot,capacity>> slots;
 std::array<Slot*,trackCount> active{},pending{},previous{};
 std::array<Slot*,trackCount> bankSlots{};
 bool bankActive=false;
 bool bankPrepared=false;
 bool collectBankSlots(const Bank&,double,std::array<Slot*,trackCount>&) const noexcept;
 void pollBank(double bpm,Completion&) noexcept;
 std::array<unsigned,trackCount> remaining{};
 std::array<bool,trackCount> failed{};
 std::array<std::uint64_t,trackCount> installations{};
 const std::size_t budget;
 double rate=0,bankTempo=120;unsigned cursor=0,fadeFrames=0;
};
}
