// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ArpDelayGraph.h"
#include <atomic>
#include <memory>
#include <optional>

namespace aion {
// One audio owner and the EXISTING LoopBank worker. No extra thread or message
// owner may call these methods. Graph pointers are audio-local, borrowed only
// until the next poll/clear/prepare; never put them in UI/state snapshots.
class ArpDelayStorage {
public:
 static constexpr unsigned trackCount=20,capacity=trackCount*2;
 struct View {bool active=false,pending=false,failed=false;};
 struct Completion {std::uint32_t installed=0,failed=0;};
 ArpDelayStorage();
 ArpDelayStorage(const ArpDelayStorage&)=delete;
 ArpDelayStorage& operator=(const ArpDelayStorage&)=delete;
 // STOPPED owner only: both audio and worker must be quiescent. Invalid rates
 // preserve the entire previous bank. Destruction/reprepare never run on audio.
 bool prepareStopped(double sampleRate) noexcept;
 void releaseStopped() noexcept;
 // Audio only. At most one pending request per track. Rejection is atomic;
 // callers must cancel explicitly before replacing a pending request.
 bool request(unsigned track,const ArpModValues&,std::uint32_t seed) noexcept;
 void cancel(unsigned track) noexcept; // preserve active PCM/tails/automation
 void clear(unsigned track) noexcept; // cancel + retire, no destruction
 void clearAll() noexcept;
 Completion poll(std::uint64_t firstFrame) noexcept; // adopt at current audio clock
 ArpDelayGraph* graph(unsigned track) noexcept;
 View view(unsigned track) const noexcept;
 // Sole worker only: reclaim retired owners, then prepare at most one request.
 // The fixed slot bound also bounds retirement; no dropped/leaked overflow.
 void serviceWorker() noexcept;
private:
 enum class Phase : unsigned {free,queued,preparing,ready,failed,active,retired};
 static_assert(std::atomic<Phase>::is_always_lock_free);
 static_assert(std::atomic<bool>::is_always_lock_free);
 struct Slot {
  std::atomic<Phase> phase{Phase::free};
  std::atomic<bool> cancelled{false};
  unsigned track=0;
  std::uint32_t seed=0;
  ArpModValues values{};
  std::optional<ArpDelayGraph> graph; // constructed/destroyed ONLY off audio
 };
 std::unique_ptr<std::array<Slot,capacity>> slots;
 std::array<Slot*,trackCount> active{},pending{}; // audio owner only
 std::array<bool,trackCount> failed{};
 unsigned outstanding=0,workerCursor=0;
 double rate=0; // immutable while worker/audio run
};
}
