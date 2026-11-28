// SPDX-License-Identifier: AGPL-3.0-only
#include "PlateReverb.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <new>
#include <vector>
namespace aion {
namespace {
constexpr float pi=3.14159265358979323846f;
constexpr unsigned bandCount=8;
constexpr std::array<float,bandCount> bandHz{125,250,500,1000,2000,4000,8000,12000};
constexpr std::array<float,8> lineMs{31.7f,37.1f,41.3f,43.7f,47.9f,53.3f,59.9f,67.1f};
constexpr std::array<float,2> diffuseMs{4.7f,12.7f};
constexpr float modulationSeconds=.0002f;
unsigned tankFrames(unsigned length,double rate) noexcept {
 // The interpolated tap must retain the oldest positive modulation excursion.
 // Two interpolation guards alone let that delay wrap into recent history.
 return length+static_cast<unsigned>(std::ceil(modulationSeconds*float(rate)))+2;
}
float fractionalTap(std::span<const float> data,unsigned pos,unsigned valid,float delay) noexcept {
 // Both pos and the prepared delay are below capacity: at most one wrap.
 // Double index arithmetic preserves the tap fraction at high sample rates.
 double rp=double(pos)-double(delay);if(rp<0)rp+=double(data.size());
 const auto i0=static_cast<unsigned>(rp);const auto i1=(i0+1)%data.size();const float f=static_cast<float>(rp-double(i0));
 return (i0<valid?data[i0]:0.f)*(1-f)+(i1<valid?data[i1]:0.f)*f;
}
struct Biquad {
 float b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
 float process(float x) noexcept {const float y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;return y;}
 void clear() noexcept {z1=z2=0;}
 void bandpass(float f,float sr) noexcept {const float w=2*pi*f/sr,alpha=std::sin(w)/(2*1.4f),a0=1+alpha;
  b0=alpha/a0;b1=0;b2=-alpha/a0;a1=-2*std::cos(w)/a0;a2=(1-alpha)/a0;}
};
struct Peak {
 float b0=1,b1=0,b2=0,a1=0,a2=0;
 std::array<float,2> z1{},z2{};
 float cosw=0,alpha=0;bool active=false;
 float process(float x,unsigned channel) noexcept {const float y=b0*x+z1[channel];z1[channel]=b1*x-a1*y+z2[channel];z2[channel]=b2*x-a2*y;return y;}
 void clear() noexcept {z1={};z2={};}
 void setup(float f,float sr,float q) noexcept {const float w=2*pi*f/sr;cosw=std::cos(w);alpha=std::sin(w)/(2*q);active=f<sr*.45f;}
 void gainDb(float db) noexcept {if(!active){b0=1;b1=b2=a1=a2=0;return;}const float A=std::pow(10.f,db/40.f),a0=1+alpha/A;
  b0=(1+alpha*A)/a0;b1=-2*cosw/a0;b2=(1-alpha*A)/a0;a1=-2*cosw/a0;a2=(1-alpha/A)/a0;}
};
struct OnePole {float z=0,prevIn=0,prevOut=0;float lp(float x,float a) noexcept {z=a*z+(1-a)*x;return z;}float hp(float x,float a) noexcept {const float y=a*(prevOut+x-prevIn);prevIn=x;prevOut=y;return y;}};
}
struct PlateReverb::State {
 double sr=48000;PlateSettings settings{};
 struct Line {std::vector<float> data;unsigned pos=0,len=1,valid=0;float gain=0;OnePole damp,low;};
 std::array<Line,8> lines;
 std::array<std::array<std::vector<float>,2>,2> diffusion; // [channel][stage]
 std::array<std::array<unsigned,2>,2> diffusionPos{};
 std::array<std::array<unsigned,2>,2> diffusionValid{};
 std::array<std::vector<float>,2> predelay;
 std::array<unsigned,2> predelayPos{};
 std::array<unsigned,2> predelayValid{};
 std::array<Peak,bandCount> peaks;
 std::array<Biquad,bandCount> analysis;
 std::array<float,bandCount> env{};
 std::array<bool,bandCount> bandActive{};
 float dampCoef=.5f,lowCoef=.1f,attCoef=.9f,relCoef=.999f;
 unsigned predelaySamples=0,unmaskCounter=0;float modPhase=0;
 void setCoefficients() noexcept {
  for(auto& l:lines){const float sec=float(l.len)/float(sr);l.gain=std::min(.9995f,std::pow(10.f,-3.f*sec/std::max(.1f,settings.decay)));}
  dampCoef=std::exp(-2*pi*std::min(settings.damping,float(sr*.45))/float(sr));
  lowCoef=std::exp(-2*pi*std::max(20.f,settings.lowCut)/float(sr));
  predelaySamples=static_cast<unsigned>(std::clamp(settings.predelay,0.f,.1f)*sr);
  attCoef=std::exp(-1.f/(.002f*float(sr)));relCoef=std::exp(-1.f/(.15f*float(sr)));
  for(unsigned b=0;b<bandCount;++b){bandActive[b]=bandHz[b]<float(sr)*.45f;peaks[b].setup(bandHz[b],float(sr),1.2f);
   if(bandActive[b])analysis[b].bandpass(bandHz[b],float(sr));else analysis[b]={};}
 }
 void clear() noexcept {
  // Sequential writes make [0,valid) the complete readable prefix after a
  // reset. Old samples remain inaccessible without scanning any delay memory.
  for(auto& l:lines){l.pos=l.valid=0;l.damp={};l.low={};}
  diffusionPos={};diffusionValid={};predelayPos={};predelayValid={};
  for(auto& p:peaks)p.clear();
  for(auto& a:analysis)a.clear();
  env={};unmaskCounter=0;modPhase=0;
 }
 float readLine(unsigned i,float modSamples) noexcept {
  const auto& l=lines[i];return fractionalTap(l.data,l.pos,l.valid,float(l.len)+modSamples);
 }
 void writeLine(unsigned i,float x) noexcept {auto& l=lines[i];l.data[l.pos]=std::isfinite(x)?x:0.f;l.pos=(l.pos+1)%l.data.size();if(l.valid<l.data.size())++l.valid;}
 float allpass(unsigned channel,unsigned stage,float x) noexcept {
  auto& d=diffusion[channel][stage];auto& p=diffusionPos[channel][stage];const auto len=static_cast<unsigned>(d.size());
  auto& valid=diffusionValid[channel][stage];const float y=-.6f*x+(p<valid?d[p]:0.f);d[p]=x+.6f*y;p=(p+1)%len;if(valid<len)++valid;return y;
 }
 void prepare(double rate) {
  sr=rate;
  for(unsigned i=0;i<8;++i){const auto n=std::max(8u,static_cast<unsigned>(lineMs[i]*.001f*float(sr)));lines[i].data.assign(tankFrames(n,sr),0.f);lines[i].pos=0;lines[i].len=n;}
  for(unsigned c=0;c<2;++c)for(unsigned s=0;s<2;++s){const auto n=std::max(4u,static_cast<unsigned>(diffuseMs[s]*.001f*float(sr)));diffusion[c][s].assign(n,0.f);}
  const auto pn=static_cast<size_t>(.1*sr)+4;for(auto& p:predelay)p.assign(pn,0.f);
  setCoefficients();clear();
 }
 void process(float inL,float inR,float& outL,float& outR) noexcept {
  const float dryL=inL,dryR=inR;
  // Dry analysis for the carver (mono sum, pre-reverb).
  const float dryM=(inL+inR)*.5f;
  if(settings.unmask>0.f)for(unsigned b=0;b<bandCount;++b)if(bandActive[b]){const float v=std::abs(analysis[b].process(dryM));env[b]=v>env[b]?attCoef*env[b]+(1-attCoef)*v:relCoef*env[b]+(1-relCoef)*v;}
  // Input diffusion then pre-delay.
  float xL=allpass(0,0,inL),xR=allpass(1,0,inR);xL=allpass(0,1,xL);xR=allpass(1,1,xR);
  auto writePred=[&](unsigned ch,float x){auto& d=predelay[ch];d[predelayPos[ch]]=x;predelayPos[ch]=(predelayPos[ch]+1)%d.size();if(predelayValid[ch]<d.size())++predelayValid[ch];};
  writePred(0,xL);writePred(1,xR);
  const auto readPred=[&](unsigned ch){auto& d=predelay[ch];const auto p=(predelayPos[ch]+d.size()-1-predelaySamples)%d.size();return p<predelayValid[ch]?d[p]:0.f;};
  const float pL=readPred(0),pR=readPred(1);
  // Tank.
  modPhase+=2*pi*.7f/float(sr);if(modPhase>2*pi)modPhase-=2*pi;
  const float mod=std::sin(modPhase)*modulationSeconds*float(sr);
  std::array<float,8> r{};
  for(unsigned i=0;i<8;++i){const float v=readLine(i,i<4?mod:-mod);r[i]=lines[i].low.hp(lines[i].damp.lp(v,dampCoef),lowCoef);}
  float sum=0;
 for(float v:r)sum+=v;
  const float inv=.25f;
  static constexpr std::array<float,8> signL{1,-1,1,-1,1,-1,1,-1};
  for(unsigned i=0;i<8;++i){const float y=r[i]-inv*sum;const float inject=(i%2?pR:pL)*(signL[i]>0?1.f:-1.f);
   writeLine(i,inject+lines[i].gain*y);}
  // Wet stereo from the two halves.
  float wetL=(r[0]-r[1]+r[2]-r[3])*.25f,wetR=(r[4]-r[5]+r[6]-r[7])*.25f;
  if(settings.unmask>0.f){if(++unmaskCounter>=32){unmaskCounter=0;
    for(unsigned b=0;b<bandCount;++b){const float db=env[b]>1e-9f?20*std::log10(env[b]):-120.f;
     const float n=std::clamp((db+50.f)/35.f,0.f,1.f);peaks[b].gainDb(-12.f*settings.unmask*n);}}
    for(unsigned b=0;b<bandCount;++b){wetL=peaks[b].process(wetL,0);wetR=peaks[b].process(wetR,1);}}
  const float mix=std::clamp(settings.mix,0.f,1.f);
  outL=dryL*(1-mix)+wetL*mix;outR=dryR*(1-mix)+wetR*mix;
 }
};
bool validPlateSettings(const PlateSettings& s) noexcept {
 return s.decay>=.1f&&s.decay<=10.f&&s.damping>=500.f&&s.damping<=20000.f&&s.lowCut>=20.f&&s.lowCut<=500.f&&
        s.predelay>=0.f&&s.predelay<=.1f&&s.mix>=0.f&&s.mix<=1.f&&s.unmask>=0.f&&s.unmask<=1.f;
}
PlateReverb::PlateReverb() noexcept=default;
PlateReverb::~PlateReverb()=default;
bool PlateReverb::prepare(double sampleRate,const PlateSettings& s,const std::atomic<bool>* cancelled) noexcept {
 if(!std::isfinite(sampleRate)||sampleRate<8000||sampleRate>384000||!validPlateSettings(s)||(cancelled&&cancelled->load(std::memory_order_acquire)))return false;
 try{auto next=std::make_unique<State>();next->settings=s;next->prepare(sampleRate);
  if(cancelled&&cancelled->load(std::memory_order_acquire))return false;
  state=std::move(next);return true;}catch(const std::bad_alloc&){return false;}
}
bool PlateReverb::set(const PlateSettings& s) noexcept {
 if(!state||!validPlateSettings(s))return false;
 state->settings=s;state->setCoefficients();return true;
}
bool PlateReverb::process(std::span<const float> left,std::span<const float> right,std::span<float> outLeft,std::span<float> outRight) noexcept {
 if(left.size()!=right.size()||left.size()!=outLeft.size()||left.size()!=outRight.size())return false;
 if(!state){for(size_t i=0;i<left.size();++i){outLeft[i]=left[i];outRight[i]=right[i];}return true;}
 if(!state->settings.enabled){for(size_t i=0;i<left.size();++i){outLeft[i]=left[i];outRight[i]=right[i];}return true;}
 for(size_t i=0;i<left.size();++i){float l=0,r=0;state->process(left[i],right[i],l,r);outLeft[i]=l;outRight[i]=r;}
 return true;
}
void PlateReverb::reset() noexcept {if(state)state->clear();}
const PlateSettings& PlateReverb::settings() const noexcept {static const PlateSettings empty{};return state?state->settings:empty;}
double PlateReverb::sampleRate() const noexcept {return state?state->sr:0;}
std::size_t PlateReverb::preparationBytes(double sampleRate,const PlateSettings&) noexcept {
 if(!std::isfinite(sampleRate)||sampleRate<8000||sampleRate>384000)return 0;
 size_t frames=0;
 for(float ms:lineMs)frames+=tankFrames(std::max(8u,static_cast<unsigned>(ms*.001f*float(sampleRate))),sampleRate);
 for(float ms:diffuseMs)frames+=2*(static_cast<size_t>(ms*.001*sampleRate)+4);
 frames+=static_cast<size_t>(.1*sampleRate+4)*2;
 return frames*sizeof(float)+sizeof(State)+1024;
}
}
