// SPDX-License-Identifier: AGPL-3.0-only
// Independently expressed C++ port of the preserved Ascii-Looper
// ArpDelayProcessor algorithm (effects.js). Original-asset license/permission
// release checks remain in docs/LICENSING.md; reference sources are unchanged.
#include "core/ArpDelay.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <numbers>
namespace aion {
namespace {
constexpr std::array<std::array<int,12>,20> scales{{
 {{0,12,24}},{{0,4,7,12}},{{0,3,7,12}},{{0,7,12}},{{0,2,4,7,9}},
 {{0,3,5,7,10}},{{0,2,4,6,8,10}},{{0,3,6,9}},{{0,4,7,11}},{{0,3,7,10}},
 {{0,1,5,7,8}},{{0,1,4,5,7,8,11}},{{0,2,3,5,7,8,10}},{{0,2,4,5,7,9,11}},
 {{-12,0,12}},{{0,5,12}},{{0,7}},{{0,4,7,10}},{{0,3,6,10}},{{0,1,2,3,4,5,6,7,8,9,10,11}}
}};
constexpr std::array<unsigned,20> lengths{{3,4,4,3,5,5,6,4,4,4,5,7,7,7,3,3,2,4,4,12}};
constexpr std::array<double,10> subdivisions{{.166,.25,.333,.5,.666,.75,1,1.333,1.5,2}};
float finiteFloat(double value) noexcept {
 return std::isfinite(value)&&std::abs(value)<=std::numeric_limits<float>::max()?static_cast<float>(value):0;
}
double inputAt(const float* input,std::size_t i) noexcept {return input&&std::isfinite(input[i])?input[i]:0;}
}
bool validArpValues(const ArpValues& values) noexcept {
 for(std::size_t p=0;p<values.size();++p)if(!std::isfinite(values[p])||values[p]<arpParameters[p].minimum||values[p]>arpParameters[p].maximum)return false;
 return true;
}
bool ArpDelay::prepare(double rate,const ArpValues& values,std::uint32_t seed) noexcept {
 if(!std::isfinite(rate)||rate<8000||rate>384000||!validArpValues(values))return false;
 try {
  std::vector<Frame> storage(static_cast<std::size_t>(std::floor(rate*4)));
  ring=std::move(storage);
 }catch(const std::bad_alloc&){return false;}
 sampleRate=rate;controls=values;generation=0;
 for(unsigned i=0;i<hann.size();++i)hann[i]=static_cast<float>(.5*(1-std::cos(2*std::numbers::pi*(i/1023.0))));
 return reset(seed);
}
bool ArpDelay::setValues(const ArpValues& values) noexcept {
 if(!validArpValues(values))return false;
 controls=values;return true;
}
bool ArpDelay::reset(std::uint32_t seed) noexcept {
 if(ring.empty()||generation==std::numeric_limits<std::uint32_t>::max())return false;
 ++generation;status={};randomState=seed;rotations={};lowpass={};highpassInput={};highpassOutput={};
 wander=wanderTarget=envelopeTarget=0;envelopeAttack=0;return true;
}
void ArpDelay::Rotation::rotate(double cosine,double sine,bool normalize) noexcept {
 const double next=c*cosine-s*sine;s=s*cosine+c*sine;c=next;
 if(normalize){const double a=c*c+s*s;if(a>0){const double n=2/(1+a);c*=n;s*=n;}}
}
double ArpDelay::random() noexcept {
 randomState=randomState*1664525u+1013904223u;++status.randomDraws;return randomState/4294967296.0;
}
double ArpDelay::read(std::size_t channel,std::size_t position) const noexcept {
 const auto& frame=ring[position];return frame.generation==generation?(channel?frame.right:frame.left):0;
}
bool ArpDelay::process(const float* left,const float* right,unsigned channels,float* outLeft,float* outRight,std::size_t frames) noexcept {
 if(ring.empty()||channels>2||frames>128||(frames&&(!outLeft||!outRight)))return false;
 // Preserve the reference's LEFT-ONLY idle gate, including clock suspension.
 if(status.tail<1e-5){
  double peak=0;if(channels)for(std::size_t i=0;i<frames;++i){peak=std::max(peak,std::abs(inputAt(left,i)));if(peak>=1e-5)break;}
  if(peak<1e-5){if(frames){std::fill_n(outLeft,frames,0.0f);std::fill_n(outRight,frames,0.0f);}return true;}
 }
 const double time=controls[0],feedback=controls[1],mix=controls[2],bpm=controls[6],amplitude=controls[7],range=controls[8],humanize=controls[9];
 const bool stay=controls[3]>.5f,sync=controls[5]>.5f;
 const auto scaleIndex=static_cast<unsigned>(std::floor(controls[4]));
 double delayTime=time;
 if(sync){
  double nearest=subdivisions[0],distance=std::abs(time-nearest);
  for(std::size_t i=1;i<subdivisions.size();++i){const double next=std::abs(time-subdivisions[i]);if(next<distance){nearest=subdivisions[i];distance=next;}}
  delayTime=nearest*(60/bpm);
 }
 const auto delay=static_cast<unsigned>(std::max(32.0,std::floor(delayTime*sampleRate)));
 const double factor=44100/sampleRate,driftSpeed=.0001*factor;
 const std::array<double,4> speed{{driftSpeed,driftSpeed*1.61803398875,.0007*factor,.0005*factor}};
 std::array<double,4> cosine{};for(unsigned i=0;i<4;++i)cosine[i]=1-speed[i]*speed[i]*.5;
 const double decay=std::exp(-1/std::max(64.0,delay*.5));
 const auto attack=static_cast<unsigned>(std::max(32.0,std::min(std::floor(sampleRate*.012),std::floor(delay*.08))));
 const double attackCoefficient=1-std::exp(-4.0/attack);
 const double glide=1-std::exp(-1/(sampleRate*(.008+humanize*.016)));
 const double wanderCoefficient=1-std::exp(-1/(sampleRate*.15));
 double writePeak=0;const double size=static_cast<double>(ring.size());
 for(std::size_t i=0;i<frames;++i){
  for(unsigned q=0;q<4;++q)rotations[q].rotate(cosine[q],speed[q],(i&511)==0);
  wander+=(wanderTarget-wander)*wanderCoefficient;
  if(++status.clock>=delay){
   status.clock=0;const auto& scale=scales[scaleIndex];const int length=static_cast<int>(lengths[scaleIndex]);
   if(!stay){
    int limit=length;
    if(range<=1)limit=std::min(length,std::max(3,static_cast<int>(std::floor(length*range))));
    const int octaves=static_cast<int>(std::ceil(range));
    status.semitone=scale[static_cast<unsigned>(status.step%limit)];
    if(range>1)status.semitone+=(status.step/limit)%octaves*12;
    status.step+=status.direction;const int maximum=range<=1?limit:limit*octaves;
    if(status.step>=maximum){status.step=std::max(0,maximum-2);status.direction=-1;}
    else if(status.step<0){status.step=std::min(1,maximum-1);status.direction=1;}
    status.sequence[status.sequenceIndex]=static_cast<float>(status.semitone);
   }else status.semitone=status.sequence[status.sequenceIndex];
   status.sequenceIndex=(status.sequenceIndex+1)%16;
   const double detune=(random()-.5)*humanize*.08;
   status.grainTarget=(1-std::pow(2,(status.semitone+detune)/12))/4096;
   const double accent=status.sequenceIndex%2==0?1:1-.10*humanize;
   envelopeTarget=accent*(1-humanize*.16*random());envelopeAttack=attack;
   wanderTarget=(random()-.5)*humanize*2.5;
  }
  const double pan=rotations[3].s;
  status.grainRate+=(status.grainTarget-status.grainRate)*glide;
  if(envelopeAttack){status.envelope+=(envelopeTarget-status.envelope)*attackCoefficient;--envelopeAttack;}
  else status.envelope*=decay;
  const double stepGain=.38+status.envelope;
  status.phase+=status.grainRate;if(status.phase>=1)status.phase-=1;if(status.phase<0)status.phase+=1;
  const double window=hann[static_cast<unsigned>(status.phase*1023)],otherWindow=1-window;
  const double offset1=status.phase*4096,offset2=std::fmod(status.phase+.5,1.0)*4096;
  const auto write=(status.write+i)%ring.size();
  const double drift=(rotations[0].s*.62+rotations[1].s*.38)*5*.8+wander;
  const double flutter=rotations[2].s*2;
  // Capture both inputs before writing outputs, permitting in-place stereo.
  const std::array<double,2> in{{channels?inputAt(left,i):0,channels?(channels==1?inputAt(left,i):inputAt(right,i)):0}};
  auto& destination=ring[write];std::array<float,2> written{};
  for(unsigned ch=0;ch<2;++ch){
   const double modulation=ch==0?1:-1;
   const double base=std::fmod(static_cast<double>(write)-delay+4096*.5+drift+flutter*.5*modulation+size,size);
   const double stereoOffset=(ch==0?-8:8)+pan*10;
   const double pos1=std::fmod(base-offset1+stereoOffset+size,size),pos2=std::fmod(base-offset2+stereoOffset+size,size);
   const auto r1=static_cast<std::size_t>(std::floor(pos1)),r2=static_cast<std::size_t>(std::floor(pos2));
   const double a=read(ch,r1),b=read(ch,r2);
   const double grain1=a+(read(ch,(r1+1)%ring.size())-a)*(pos1-r1);
   const double grain2=b+(read(ch,(r2+1)%ring.size())-b)*(pos2-r2);
   const double wet=(grain1*window+grain2*otherWindow)*amplitude*stepGain;
   const double wx=wet*.9,wx2=wx*wx;
   const double feedbackSignal=wx*(27+wx2)/(.9*(27+9*wx2));
   const double highpass=.995*(highpassOutput[ch]+feedbackSignal-highpassInput[ch]);
   highpassInput[ch]=feedbackSignal;highpassOutput[ch]=std::abs(highpass)<1e-9?0:highpass;
   lowpass[ch]+=(highpass-lowpass[ch])*.28;if(std::abs(lowpass[ch])<1e-9)lowpass[ch]=0;
   double sample=in[ch]+lowpass[ch]*feedback;
   if(std::abs(sample)<1e-9||!std::isfinite(sample)||std::abs(sample)>std::numeric_limits<float>::max())sample=0;
   written[ch]=static_cast<float>(sample);writePeak=std::max(writePeak,std::abs(sample));
   (ch?outRight:outLeft)[i]=finiteFloat(in[ch]*(1-mix)+wet*mix);
  }
  destination={written[0],written[1],generation};
 }
 status.tail=std::max(writePeak,status.tail*.9);
 status.write=static_cast<std::uint32_t>((status.write+frames)%ring.size());return true;
}
}
