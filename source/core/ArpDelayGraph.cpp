// SPDX-License-Identifier: AGPL-3.0-only
#include "ArpDelayGraph.h"
#include <algorithm>
#include <limits>
namespace aion {
bool ArpDelayGraph::prepare(double rate,const ArpModValues& values,std::uint32_t seed,std::uint64_t firstFrame) noexcept {
 // Prepare fixed modulation first, then allocate the delay. A failed allocation
 // leaves BOTH the old delay and automation/phase untouched. No graph copying.
 ArpModulation candidate;if(!candidate.prepare(rate,values,firstFrame))return false;
 ArpValues worklet{};std::copy_n(values.begin(),worklet.size(),worklet.begin());
 if(!delay.prepare(rate,worklet,seed,static_cast<unsigned>(firstFrame%128)))return false;
 modulation=candidate;prepared=true;freshWorklet=true;return true;
}
bool ArpDelayGraph::reset(std::uint32_t seed,std::uint64_t firstFrame) noexcept {
 if(!prepared||firstFrame==UINT64_MAX||!delay.reset(seed,static_cast<unsigned>(firstFrame%128)))return false;
 modulation.reset(firstFrame);freshWorklet=true;return true;
}
bool ArpDelayGraph::process(const float* left,const float* right,unsigned channels,float* outLeft,float* outRight,std::size_t frames) noexcept {
 if(!prepared||channels>2||frames>maxProcessFrames||frames>UINT64_MAX-modulation.position()||(frames&&(!outLeft||!outRight)))return false;
 for(std::size_t at=0;at<frames;){
  // AudioParam first values latch once per original quantum, including a fresh
  // graph starting mid-quantum. Single-sample Engine calls cannot change them.
  const bool update=freshWorklet||!(modulation.position()%128);
  const auto count=std::min(frames-at,std::size_t(128-delay.quantumPosition()));ArpControlFrame controls{};ArpValues worklet{};
  for(std::size_t i=0;i<count;++i){if(!modulation.next(controls))return false;if(!i&&update)worklet=controls.worklet;pan[i]=controls.pan;}
  if(update&&!delay.setValues(worklet))return false;
  freshWorklet=false;
  if(!delay.processCausal(left?left+at:nullptr,right?right+at:nullptr,channels,outLeft+at,outRight+at,count))return false;
  for(std::size_t i=0;i<count;++i)modulation.panStereo(outLeft[at+i],outRight[at+i],pan[i],outLeft[at+i],outRight[at+i]);
  at+=count;
 }
 return true;
}
}
