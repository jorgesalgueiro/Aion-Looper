// SPDX-License-Identifier: AGPL-3.0-only
#include "Delay.h"
#include "ArpModulation.h"
#include <algorithm>
#include <cmath>
#include <new>
#include <numbers>
namespace aion {
namespace {
constexpr double root2=std::numbers::sqrt2;
double clean(double v) noexcept {return std::isfinite(v)?std::clamp(v,-1.e6,1.e6):0;}
void smooth(double& v,double t,double k) noexcept {v+=k*(t-v);if(std::abs(t-v)<1.e-12)v=t;}
bool validBpm(double bpm) noexcept{return std::isfinite(bpm)&&bpm>=10&&bpm<=999;}
}
std::size_t Delay::requiredStorageFrames(double sr,const DelaySettings& s) noexcept {
 if(!std::isfinite(sr)||sr<8000||sr>384000||!validDelaySettings(s))return 0;
 return static_cast<std::size_t>(std::ceil(sr*4*(s.repeatMode==DelayRepeatMode::exact?s.repeatCount:1)))+2;
}
bool Delay::prepare(double sr,const DelaySettings& s,double bpm) noexcept {
 const auto frames=requiredStorageFrames(sr,s);
 if(!frames||!validBpm(bpm))return false;
 const unsigned repeats=s.repeatMode==DelayRepeatMode::exact?s.repeatCount:1;
 try {
  std::vector<std::array<float,2>> storage(frames);
  ring.swap(storage);
 }catch(const std::bad_alloc&){return false;}
 rate=sr;target=s;preparedRepeats=repeats;smoothing=1-std::exp(-1/(.01*rate));
 fadeLength=static_cast<unsigned>(std::ceil(.02*rate));prepared=true;
 cachedDamp=cachedPanSpeed=-1;derive(bpm);reset();return true;
}
void Delay::derive(double bpm) noexcept {
 // Preserve stored time; bound only the derived synchronized read position to
 // the four-second prepared capacity (including very slow external tempos).
 wanted=std::clamp(target.values[0]*(target.values[6]!=0?60/bpm:1)*rate,1.,4*rate);
 if(cachedDamp!=target.values[2]){
  cachedDamp=target.values[2];gTarget=std::tan(std::numbers::pi*std::min(cachedDamp,.45*rate)/rate);
 }
 if(cachedPanSpeed!=target.values[4]){
  cachedPanSpeed=target.values[4];const double angle=2*std::numbers::pi*cachedPanSpeed/rate;
  rotationSin=std::sin(angle);rotationCos=std::cos(angle);
 }
}
bool Delay::set(const DelaySettings& s,double bpm) noexcept {
 if(!prepared||!validDelaySettings(s)||!validBpm(bpm))return false;
 if(s.repeatMode!=target.repeatMode||(s.repeatMode==DelayRepeatMode::exact&&s.repeatCount>preparedRepeats))return false;
 // Derive once per accepted change, not per frame. Preserve oscillator phase,
 // feedback memory and any in-progress crossfade; latest delay request wins.
 if(s.values!=target.values||wanted!=std::clamp(s.values[0]*(s.values[6]!=0?60/bpm:1)*rate,1.,4*rate)){
  target=s;derive(bpm);
 }else target=s;
 if(asleep){reset();enable=0;}
 return true;
}
void Delay::reset() noexcept {
 write=history=0;fade=0;delayA=delayB=wanted;ic1={};ic2={};
 sine=0;cosine=1;phaseAge=0;mix=target.values[3];feedback=target.values[1]/11.1;
 depth=target.values[5];g=gTarget;a1=1/(1+g*(g+root2));
 enable=target.enabled?1:0;asleep=!target.enabled;
 tapIc1={};tapIc2={};tapGain.fill(0);
 double level=1;
 for(unsigned tap=0;tap<preparedRepeats;++tap){tapGain[tap]=tap<target.repeatCount?level:0;level*=.65;}
}
double Delay::damp(double input,double& state1,double& state2) noexcept {
 const double v1=a1*(state1+g*(input-state2)),v2=state2+g*v1;
 state1=clean(2*v1-state1);state2=clean(2*v2-state2);
 if(std::abs(state1)<1.e-24)state1=0;
 if(std::abs(state2)<1.e-24)state2=0;
 return v2;
}
double Delay::read(unsigned c,double frames) const noexcept {
 const auto age=static_cast<std::size_t>(frames);const double fraction=frames-static_cast<double>(age);
 const auto sample=[&](std::size_t back) noexcept ->double {
  if(back>history)return 0;
  const auto index=write>=back?write-back:write+ring.size()-back;
  return ring[index][c];
 };
 const double first=sample(age);
 return fraction==0?first:first+(sample(age+1)-first)*fraction;
}
void Delay::tick(float left,float right,float& outLeft,float& outRight) noexcept {
 if(!prepared){outLeft=left;outRight=right;return;}
 if(asleep&&!target.enabled){outLeft=left;outRight=right;return;}
 asleep=false;
 smooth(enable,target.enabled?1.:0.,smoothing);
 smooth(mix,target.values[3],smoothing);smooth(feedback,target.values[1]/11.1,smoothing);
 smooth(depth,target.values[5],smoothing);
 if(g!=gTarget){smooth(g,gTarget,smoothing);a1=1/(1+g*(g+root2));}
 if(!fade&&wanted!=delayA){delayB=wanted;fade=fadeLength;}
 const double blend=fade?1-static_cast<double>(fade)/fadeLength:0;
 std::array<double,2> wet{};const std::array<double,2> input{clean(left),clean(right)};
 if(target.repeatMode==DelayRepeatMode::feedback){
  for(unsigned c=0;c<2;++c){
   wet[c]=read(c,delayA);if(fade)wet[c]+=(read(c,delayB)-wet[c])*blend;
   const double next=clean(input[c]+feedback*damp(wet[c],ic1[c],ic2[c]));
   ring[write][c]=std::abs(next)<1.e-24?0:static_cast<float>(next);
  }
 }else{
  // Feed-forward taps: overlapping input never replenishes a repeat counter.
  // Each input contributes exactly N delayed copies, with no recirculation.
  // Later copies are damped; filter decay is an echo's tail, not
  // another delay repeat. The fixed .65 taper is independent of legacy Reps.
  double level=1;
  for(unsigned tap=0;tap<preparedRepeats;++tap){
   smooth(tapGain[tap],tap<target.repeatCount?level:0.,smoothing);level*=.65;
   if(tapGain[tap]==0){if(tap){tapIc1[tap-1]={};tapIc2[tap-1]={};}continue;}
   for(unsigned c=0;c<2;++c){
    double delayed=read(c,delayA*(tap+1));
    if(fade)delayed+=(read(c,delayB*(tap+1))-delayed)*blend;
    if(tap)delayed=damp(delayed,tapIc1[tap-1][c],tapIc2[tap-1][c]);
    wet[c]+=tapGain[tap]*delayed;
   }
  }
  for(unsigned c=0;c<2;++c)ring[write][c]=static_cast<float>(input[c]);
 }
 if(fade&&!--fade)delayA=delayB;
 if(++write==ring.size())write=0;
 history=std::min(history+1,ring.size()-1);
 float wl=static_cast<float>(wet[0]),wr=static_cast<float>(wet[1]);
 const float pan=static_cast<float>(sine*depth);
 if(pan!=0)ArpModulation::panStereo(wl,wr,pan,wl,wr);
 const double nextSine=sine*rotationCos+cosine*rotationSin;
 cosine=cosine*rotationCos-sine*rotationSin;sine=nextSine;
 if(++phaseAge==4096){const double norm=1/std::hypot(sine,cosine);sine*=norm;cosine*=norm;phaseAge=0;}
 outLeft=static_cast<float>(clean(input[0]+enable*mix*(wl-input[0])));
 outRight=static_cast<float>(clean(input[1]+enable*mix*(wr-input[1])));
 if(!target.enabled&&enable==0)reset();
}
}
