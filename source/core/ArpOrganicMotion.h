// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace aion {
enum class ArpVoicing : std::uint8_t { original, loose };
constexpr bool validArpVoicing(ArpVoicing v) noexcept {return v==ArpVoicing::original||v==ArpVoicing::loose;}
// Experimental DSP policy, deliberately absent from product defaults/state until
// the versioned new-instance/legacy-session boundary is integrated. One owner,
// fixed storage. Separate RNG preserves the reference kernel's random sequence.
struct ArpOrganicState {
 unsigned interval=32;
 std::int64_t offset=0;
 double pan=0,targetPan=0,gain=1;
 std::uint64_t notes=0;
 bool operator==(const ArpOrganicState&) const noexcept=default;
};
class ArpOrganicMotion {
public:
 void reset(std::uint32_t seed,unsigned delay,double rate) noexcept {
  randomState=seed^0xa511e9b3u;state_={};state_.interval=delay;
  smoothing=1-std::exp(-1/(rate*.080));lastDelay=delay;
 }
 void retime(unsigned delay) noexcept {
  if(delay==lastDelay)return;
  // A large tempo/time jump must not retain a multi-second jitter interval.
  // Re-anchor the next onset; ordinary fixed-tempo notes never accumulate drift.
  state_.offset=0;state_.interval=delay;lastDelay=delay;
 }
 void note(unsigned delay,double feel) noexcept {
  const double timing=random(),velocity=random(),stereo=random();
  const auto next=static_cast<std::int64_t>(std::llround((2*timing-1)*delay*.25*feel));
  state_.interval=static_cast<unsigned>(std::max<std::int64_t>(1,static_cast<std::int64_t>(delay)+next-state_.offset));
  state_.offset=next;state_.gain=1+feel*((.35+.90*velocity)-1);
  state_.targetPan=(2*stereo-1)*.85*feel;++state_.notes;
 }
 double nextPan() noexcept {state_.pan+=(state_.targetPan-state_.pan)*smoothing;if(state_.targetPan==0&&std::abs(state_.pan)<1e-12)state_.pan=0;return state_.pan;}
 void releasePan() noexcept {state_.targetPan=0;}
 const ArpOrganicState& state() const noexcept{return state_;}
private:
 double random() noexcept {randomState=randomState*1664525u+1013904223u;return randomState/4294967296.;}
 ArpOrganicState state_{};
 std::uint32_t randomState=0;
 unsigned lastDelay=32;
 double smoothing=0;
};
}
