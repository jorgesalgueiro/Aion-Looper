// Copyright 2015 The Chromium Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google LLC nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Copyright 2022, 2026 The Chromium Authors. Oscillator interpolation/phase
// arithmetic follows oscillator_handler.cc and oscillator_kernel_sse2.cc;
// rate-dependent wave size follows periodic_wave_handler.cc (BSD terms above).
// AudioParam arithmetic adapted from the already verified Compressor timeline;
// retained Chromium BSD terms above. Original A uses a-rate parameters throughout.
// New routing/ownership code is AGPL-3.0-only. No original assets are modified.
#include "ArpModulation.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
namespace aion {
namespace {
// Called ONLY by off-audio prepare, so even deferred initialization/its guard
// cannot reach next/process/reset. Static lifetime, one strided table for all rates.
const std::array<float,16384>& sineTable() noexcept {
 static const std::array<float,16384> table=[] {
  std::array<float,16384> result{};
  for(unsigned i=0;i<result.size();++i)result[i]=static_cast<float>(std::sin(2*std::numbers::pi*i/result.size()));
  return result;
 }();
 return table;
}
}
bool validArpModValues(const ArpModValues& values) noexcept {
 ArpValues worklet{};std::copy_n(values.begin(),worklet.size(),worklet.begin());
 return validArpValues(worklet)&&std::isfinite(values[10])&&values[10]>=0&&values[10]<=5&&
        std::isfinite(values[11])&&values[11]>=0&&values[11]<=1;
}
bool ArpModulation::prepare(double rate,const ArpModValues& values,std::uint64_t firstFrame) noexcept {
 if(!std::isfinite(rate)||rate<8000||rate>384000||!validArpModValues(values)||firstFrame==UINT64_MAX)return false;
 sampleRate=rate;target=values;prepared=true;
 sine=sineTable().data();
 waveSize=rate<=24000?2048:rate<=88200?4096:16384;rateScale=waveSize/static_cast<float>(rate);
 const float c=static_cast<float>(1-std::exp(-1/(rate*.05f)));
 steps={c,c*(2-c),c*((c-3)*c+3),c*(c*((4-c)*c-6)+4)};
 reset(firstFrame);return true;
}
void ArpModulation::reset(std::uint64_t firstFrame) noexcept {
 frame=firstFrame;phase=0;ramps={};deferred.fill(false);settled.fill(false);changedAt.fill(frame);oscillatorLanes={};frequencyAutomated=false;oscillatorFresh=true;
 last=start=target;std::copy_n(target.begin(),latched.size(),latched.begin());first=true;immediateBpm=false;
}
void ArpModulation::retarget(std::size_t p,float value) noexcept {
 if(p==6)immediateBpm=false;
 settled[p]=false; // Equal-value gestures still retain their original boundary math.
 oldBoundary[p]=last[p]+(target[p]-last[p])*steps[0];
 deferred[p]=(double(frame)/sampleRate)*sampleRate>double(frame);
 start[p]=last[p];changedAt[p]=frame;target[p]=value;
 if(p==10)frequencyAutomated=true;
}
bool ArpModulation::setTarget(std::size_t p,float value) noexcept {
 if(!prepared||frame==UINT64_MAX||p>=target.size())return false;
 auto candidate=target;candidate[p]=value;if(!validArpModValues(candidate))return false;
 retarget(p,value);return true;
}
bool ArpModulation::setTargets(const ArpModValues& values) noexcept {
 if(!prepared||frame==UINT64_MAX||!validArpModValues(values))return false;
 for(std::size_t p=0;p<values.size();++p)if(values[p]!=target[p])retarget(p,values[p]);
 return true;
}
bool ArpModulation::setBpmImmediate(float value) noexcept {
 if(!prepared||frame==UINT64_MAX||!std::isfinite(value)||value<30||value>300)return false;
 oldBoundary[6]=audioRate(6);target[6]=value;changedAt[6]=frame;
 deferred[6]=(double(frame)/sampleRate)*sampleRate>double(frame);immediateBpm=true;return true;
}
float ArpModulation::audioRate(std::size_t p) noexcept {
 if(p==6&&immediateBpm)return frame==changedAt[p]&&deferred[p]?oldBoundary[p]:target[p];
 // Once the original timeline reaches the exact target, its recurrence is
 // constant. Keep the last rendered Float32; do not
 // approximate convergence or skip any new/repeated automation gesture.
 if(settled[p])return last[p];
 const float dest=target[p];auto& r=ramps[p];
 // A binary timestamp just above F starts at F+1. That leaves 127 samples,
 // so Chromium restarts its four-lane recurrence there and finishes the last
 // three samples serially. Preserve Float32 subtraction at the new anchor too:
 // isolated parameter tolerances hid its error in the connected 96k graph.
 if(frame==changedAt[p]&&deferred[p])return oldBoundary[p];
 const auto begin=changedAt[p]+(deferred[p]?1u:0u);
 if(frame==begin||!(frame%128)){
  if(frame==begin){
   const double age=double(frame)/sampleRate-double(changedAt[p])/sampleRate;
   r.base=static_cast<float>(dest+double(start[p]-dest)*std::exp(-age/.05f));
  }else r.base=last[p]+(dest-last[p])*steps[0];
  if(double(frame-changedAt[p])>10*.05f*sampleRate||
     std::abs(dest-r.base)<4.539992976248485e-05f*(dest==0?1:std::abs(r.base)))r.base=dest;
  // A negative-zero target has -0 in the quantum's first lane and +0 in
  // subsequent lanes. Retain that original path instead of holding its sign.
  settled[p]=r.base==dest&&!std::signbit(dest);
  r.phase=0;
  r.scalarFrom=128-((128-static_cast<unsigned>(frame%128))%4);
 }
 if(frame%128>=r.scalarFrom){const float v=r.base;r.base+=(dest-r.base)*steps[0];return v;}
 if(r.phase==0){
  const float d=dest-r.base;
  r.lane={r.base,r.base+d*steps[0],r.base+d*steps[1],r.base+d*steps[2]};r.next=r.base+d*steps[3];
 }
 const float v=r.lane[r.phase++];if(r.phase==4){r.phase=0;r.base=r.next;}
 return v;
}
bool ArpModulation::next(ArpControlFrame& result) noexcept {
 if(!prepared||frame==UINT64_MAX)return false;
 for(std::size_t p=0;p<last.size();++p){
  const float value=audioRate(p);
  // Worklet AudioParam arrays and their exposed last value are nominal-range
  // clipped AFTER timeline arithmetic. E.g. .01 -> 4 cancellation otherwise
  // yields .00999999046 and a valid graph gesture fails setValues mid-render.
  last[p]=p<arpParameters.size()?std::clamp(value,arpParameters[p].minimum,arpParameters[p].maximum):value;
 }
 if(first||!(frame%128)){std::copy_n(last.begin(),latched.size(),latched.begin());first=false;}
 result.parameters=last;result.worklet=latched;
 result.pan=oscillator(last[10])*last[11];++frame;return true;
}
float ArpModulation::oscillator(float frequency) noexcept {
 const float increment=frequency*rateScale;const unsigned mask=waveSize-1,stride=16384/waveSize;
 const bool fast=!frequencyAutomated&&increment>=.3f;
 if(fast&&(!(frame%128)||oscillatorFresh))for(unsigned k=0;k<4;++k){
  auto& lane=oscillatorLanes[k];lane=static_cast<float>(phase)+(static_cast<float>(k)-static_cast<float>(frame%4))*increment;
  lane-=std::floor(lane/waveSize)*waveSize;
 }
 oscillatorFresh=false;
 const double position=fast?oscillatorLanes[frame%4]:phase;
 const auto index=static_cast<unsigned>(position);const auto at=[&](unsigned i){return sine[(i&mask)*stride];};
 double value=0;
 if(increment>=.3f){
  const float t=static_cast<float>(position)-static_cast<float>(index);
  value=at(index)+t*(at(index+1)-at(index));
 }else if(increment>=.16f){
  const double t=position-index;
  value=.5*t*(t-1)*at(index-1)+(1-t*t)*at(index)+.5*t*(t+1)*at(index+1);
 }else{
  const double t=position-index,t2=t*t;
  value=t*(t2-1)*(t-2)/24*at(index-2)-t*(t-1)*(t2-4)/6*at(index-1)+
        (t2-1)*(t2-4)/4*at(index)-t*(t+1)*(t2-4)/6*at(index+1)+t*(t2-1)*(t+2)/24*at(index+2);
 }
 if(fast&&frame%4==3)for(auto& lane:oscillatorLanes){lane+=4*increment;lane-=std::floor(lane/waveSize)*waveSize;}
 phase+=increment;phase-=std::floor(phase/waveSize)*waveSize;
 return static_cast<float>(value);
}
void ArpModulation::panStereo(float left,float right,float pan,float& outLeft,float& outRight) noexcept {
 if(!std::isfinite(left))left=0;
 if(!std::isfinite(right))right=0;
 pan=std::isfinite(pan)?std::clamp(pan,-1.f,1.f):0;
 const double angle=(pan<=0?1+double(pan):double(pan))*std::numbers::pi/2;
 const float l=static_cast<float>(std::cos(angle)),r=static_cast<float>(std::sin(angle));
 const double a=pan<=0?double(left)+right*l:double(left)*l;
 const double b=pan<=0?double(right)*r:double(right)+left*r;
 outLeft=std::abs(a)<=std::numeric_limits<float>::max()?static_cast<float>(a):0;
 outRight=std::abs(b)<=std::numeric_limits<float>::max()?static_cast<float>(b):0;
}
}
