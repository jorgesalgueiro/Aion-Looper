// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Delay.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <string_view>
namespace {
bool watched=false,failAllocation=false;unsigned operations=0;
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct Guard {Guard(){operations=0;watched=true;}~Guard(){watched=false;check(!operations,"callback heap operation");}};
}
void* operator new(std::size_t n){if(watched)++operations;if(failAllocation)throw std::bad_alloc();if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watched&&p)++operations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace aion;
static void catalog(){
 std::cout<<"{\"controls\":[";
 for(unsigned p=0;p<delayControls.size();++p){const auto& d=delayControls[p];if(p)std::cout<<',';
  std::cout<<"{\"name\":\""<<d.name<<"\",\"initial\":"<<d.initial<<",\"min\":"<<d.minimum<<",\"max\":"<<d.maximum<<",\"step\":"<<d.step<<'}';}
 std::cout<<"],\"presets\":[";
 for(unsigned id=1;id<=delayPresetNames.size();++id){DelaySettings s;check(applyDelayPreset(s,id),"preset");if(id>1)std::cout<<',';
  std::cout<<"{\"name\":\""<<delayPresetNames[id-1]<<"\",\"values\":[";
  for(unsigned p=0;p<s.values.size();++p){if(p)std::cout<<',';std::cout<<s.values[p];}std::cout<<"]}";}
 std::cout<<"]}\n";
}
int main(int argc,char** argv){
 if(argc==2&&std::string_view(argv[1])=="midi"){unsigned p=0,cc=0;std::cout.precision(17);while(std::cin>>p>>cc){if(p>=6||cc>127)return 1;std::cout<<aion::delayMidiValue(p,cc)<<'\n';}return 0;}
 if(argc==2&&std::string_view(argv[1])=="catalog"){catalog();return 0;}
 DelaySettings settings;
 check(validDelaySettings(settings)&&!settings.enabled&&!settings.preset,"disabled defaults");
 for(unsigned id=1;id<=9;++id){settings.enabled=id%2!=0;const bool enabled=settings.enabled;Guard guard;
  check(applyDelayPreset(settings,id)&&settings.enabled==enabled&&settings.preset==id,"preset preserves enable");
  auto old=settings;check(!applyDelayPreset(settings,0)&&!applyDelayPreset(settings,10)&&settings==old,"invalid preset atomic");
  check(setDelayControl(settings,0,settings.values[0])&&!settings.preset,"same-value manual edit selects Custom");
  for(unsigned p=0;p<7;++p){old=settings;
   check(!setDelayControl(settings,p,NAN)&&!setDelayControl(settings,p,delayControls[p].maximum+1)&&settings==old,"bad control atomic");
   check(setDelayControl(settings,p,delayControls[p].minimum)&&setDelayControl(settings,p,delayControls[p].maximum),"control endpoints");}
 }
 settings={};check(setDelayControl(settings,1,6.123456789),"fractional repetitions retained");
 auto old=settings;check(!setDelayControl(settings,6,.5)&&settings==old,"boolean sync");
 settings.preset=1;check(!validDelaySettings(settings),"false preset metadata rejected");
 unsigned samples=0;
 for(double rate:{8000.,22050.,32000.,44100.,48000.,96000.,192000.,384000.}){
  for(unsigned block:{64u,128u})for(bool sync:{false,true}){
   settings={};settings.enabled=true;settings.values={.01,0,3000,1,0,.8,0};
   settings.values[6]=sync?1:0;
   Delay delay;check(delay.prepare(rate,settings),"prepare");
   // Analytic fractional impulse: no feedback, no pan, full wet. This checks
   // actual echo placement and weights without reimplementing the kernel.
   const double frames=.01*(sync?.5:1)*rate;const unsigned first=static_cast<unsigned>(frames);
   {Guard guard;for(unsigned base=0;base<8192;base+=block)for(unsigned j=0;j<block;++j){
    const unsigned n=base+j;float l,r;delay.tick(n==0?1.f:0.f,n==0?-.5f:0.f,l,r);
    const double expected=n==first?1-(frames-first):(n==first+1?frames-first:0);
    check(std::abs(l-expected)<1.e-7&&std::abs(r+.5*expected)<1.e-7,"fractional impulse timing/stereo");++samples;
   }}
   // Failed prepare must preserve the entire running ring, not merely settings.
   Delay other;check(other.prepare(rate,settings),"second owner");delay.reset();
   for(unsigned n=0;n<32;++n){float l,r,a,b;delay.tick(n==0?1.f:0.f,0,l,r);other.tick(n==0?1.f:0.f,0,a,b);}
   failAllocation=true;const bool prepared=delay.prepare(rate,settings);failAllocation=false;
   check(!prepared&&!delay.prepare(NAN)&&!delay.prepare(rate,settings,0),"OOM/invalid prepare rejected");
   {Guard guard;for(unsigned n=0;n<4096;++n){float l,r,a,b;delay.tick(0,0,l,r);other.tick(0,0,a,b);check(l==a&&r==b,"failed prepare preserves ring");}}
   // Sync upper limit stays inside prepared storage; stored values unchanged.
   settings.values[0]=4;settings.values[6]=1;
   {Guard guard;check(delay.set(settings,10)&&delay.settings()==settings,"slow sync preserves stored time");delay.reset();}
   // A reset hides previous PCM in O(1), even after ring wrap / a time edit.
   {Guard guard;for(unsigned n=0;n<1024;++n){float l,r;delay.tick(0,0,l,r);check(l==0&&r==0,"reset no stale tail");}}
  }
 }
 // Four-second limit is observable, not just a settings-clamp assertion.
 settings={};settings.enabled=true;settings.values={4,0,3000,1,0,.8,1};
 Delay limited;check(limited.prepare(8000,settings,10),"slow sync prepare");
 {Guard guard;for(unsigned n=0;n<=96000;++n){float l,r;limited.tick(n==0||n==64000?1.f:0.f,0,l,r);
  check(l==(n==32000||n==96000?1.f:0.f)&&r==0,"bounded sync echo before/after ring wrap");}
  limited.reset();for(unsigned n=0;n<33000;++n){float l,r;limited.tick(0,0,l,r);check(l==0&&r==0,"clear after wrap hides stale PCM");}
 }
 // Fully bypassed is bit-transparent. Disabled edits must not reveal old PCM.
 settings={};Delay bypass;check(bypass.prepare(48000,settings),"bypass prepare");
 {Guard guard;for(unsigned n=0;n<1024;++n){const float x=static_cast<float>(n)*.001f;float l,r;bypass.tick(x,-x,l,r);check(l==x&&r==-x,"exact bypass");}}
 // Independent analytic read-head crossfade: no pitch glide, constant sum,
 // initial history at the requested time, and a bounded latest-pending edit.
 settings={};settings.enabled=true;settings.values={.01,0,3000,1,0,.8,0};
 Delay transition;check(transition.prepare(8000,settings),"transition prepare");
 const auto wave=[](unsigned n){return static_cast<float>(std::sin(double(n)*.05));};
 float previous=0;
 {Guard guard;for(unsigned n=0;n<1400;++n){
  if(n==1000){setDelayControl(settings,0,.02);check(transition.set(settings,120),"time edit");}
  if(n==1040||n==1080){setDelayControl(settings,0,n==1040?.03:.04);check(transition.set(settings,120),"coalesced time edit");}
  float l,r;transition.tick(wave(n),wave(n),l,r);
  if(n>=1000){const double blend=std::min(1.,(n-1000)/160.);
   const double latestBlend=std::clamp((double(n)-1160)/160.,0.,1.);
   const double expected=n<1160?(1-blend)*wave(n-80)+blend*wave(n-160)
    :(1-latestBlend)*wave(n-160)+latestBlend*wave(n-320);
   check(std::abs(l-expected)<1.e-6&&l==r,"constant-sum dual-head analytic PCM");
   check(std::abs(l-previous)<.065,"time change no discontinuity");}
  previous=l;
 }}
 // Enable must fade from exact bypass; disable must settle, flush history and
 // re-enable without resurrecting old echoes. Rapid repeated settings are safe.
 settings.enabled=false;check(bypass.set(settings,120),"disabled controls");
 settings.enabled=true;
 {Guard guard;check(bypass.set(settings,120),"enable");float l,r;bypass.tick(1,1,l,r);
  check(l>.98&&l<1&&l==r,"enable ramp starts at dry input");
  settings.enabled=false;check(bypass.set(settings,120),"disable");
  for(unsigned n=0;n<20000;++n)bypass.tick(0,0,l,r);
  bypass.tick(.123f,-.456f,l,r);check(l==.123f&&r==-.456f,"settled exact bypass");
  settings.enabled=true;check(bypass.set(settings,120),"re-enable");
  for(unsigned n=0;n<20000;++n){bypass.tick(0,0,l,r);check(l==0&&r==0,"no old echo after bypass");}
 }
 // Every preset, rapid parameter/tempo edits, bad input, clear and variable
 // callback segmentation. This component has no block-size assumption.
 double peak=0;
 for(unsigned id=1;id<=9;++id){settings={};settings.enabled=true;check(applyDelayPreset(settings,id),"stress preset");
  Delay a;check(a.prepare(32000,settings),"stress prepare");
  {Guard guard;for(unsigned n=0;n<96000;++n){
   if(n%997==0){setDelayControl(settings,0,.01+(n%399)*.01);setDelayControl(settings,2,200+(n%199)*100);
    check(a.set(settings,n%2?10:999),"rapid set");}
   if(n==30000)a.reset();
   float l,r;const float input=n%131==0?.8f:0.f;
   a.tick(input,-input*.5f,l,r);
   check(std::isfinite(l)&&std::isfinite(r),"finite persistent DSP");
   peak=std::max(peak,std::max(std::abs(double(l)),std::abs(double(r))));++samples;
  }
  float l,r;a.tick(NAN,INFINITY,l,r);check(std::isfinite(l)&&std::isfinite(r),"nonfinite input containment");}
 }
 check(peak>0&&peak<4,"nonzero bounded preset stress");
 // Worst feedback at low sample rate must decay to silence, including both
 // damping extremes; the first echo remains unfiltered as in the original.
 for(double damp:{200.,20000.}){
  settings={};settings.enabled=true;settings.values={.01,10,damp,1,0,.8,0};
  Delay tail;check(tail.prepare(8000,settings),"tail prepare");double ending=0;
  {Guard guard;for(unsigned n=0;n<100000;++n){float l,r;tail.tick(n==0?1.f:0.f,0,l,r);
   check(std::isfinite(l)&&std::isfinite(r)&&std::abs(l)<2&&r==0,"feedback stability/channel isolation");
   if(n==80)check(l==1,"first echo bypasses feedback damping");
   if(n>99000)ending=std::max(ending,std::abs(double(l)));}
  }
  check(ending<1.e-9,"maximum-feedback tail decays");
 }
 std::cout<<"PASS delay component: "<<samples<<" samples, eight rates, 64/128, OOM, reset, zero tracked callback heap operations; peak="<<peak<<'\n';
}
