// SPDX-License-Identifier: AGPL-3.0-only
#include "core/ArpDelayGraph.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <string_view>
#include <type_traits>
#include <vector>
#if defined(__linux__)
#include <time.h>
#endif
static_assert(!std::is_copy_constructible_v<aion::ArpDelayGraph> && !std::is_move_assignable_v<aion::ArpDelayGraph>);
namespace {thread_local bool watch=false,failNext=false;unsigned operations=0;
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}}
void* operator new(std::size_t n){if(watch)++operations;if(failNext){failNext=false;throw std::bad_alloc();}if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{if(watch)++operations;std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void* p) noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept{::operator delete(p);}void operator delete[](void* p,std::size_t) noexcept{::operator delete(p);}
using namespace aion;
struct Event {unsigned frame,parameter;float value;};
constexpr std::array<Event,13> events{{{385,0,.02f},{513,2,.1f},{768,6,300},{4097,4,19},{8001,10,5},{16001,3,1},
 {25012,1,.7f},{25013,0,.06f},{27531,10,0},{27532,10,0},{28000,11,0},{29501,3,0},{30003,9,.8f}}};
static std::vector<float> render(ArpDelayGraph& dsp,unsigned block,unsigned channels,bool inPlace){
 constexpr unsigned frames=32768;std::vector<float> result(frames*2);std::array<float,257> l{},r{},ol{},orr{};
 unsigned at=0,event=0;while(at<frames){
  if(event<events.size()&&events[event].frame==at){watch=true;check(dsp.setTarget(events[event].parameter,events[event].value),"scheduled graph target");watch=false;++event;}
  const unsigned end=event<events.size()?events[event].frame:frames,count=std::min(block,end-at);
  for(unsigned i=0;i<count;++i){const unsigned f=at+i;const bool active=f<8192||f>=25003;
   l[i]=active?static_cast<float>(.15*std::sin(f*.117)+.05*std::cos(f*.037)):0;
   r[i]=active?static_cast<float>(.13*std::sin(f*.073)-.03*std::cos(f*.021)):f>=23500&&f<24000?.5f:0;}
  watch=true;check(dsp.process(channels?l.data():nullptr,channels>1?r.data():nullptr,channels,inPlace?l.data():ol.data(),inPlace?r.data():orr.data(),count),"bounded stream render");watch=false;
  for(unsigned i=0;i<count;++i){const float a=inPlace?l[i]:ol[i],b=inPlace?r[i]:orr[i];check(std::isfinite(a)&&std::isfinite(b),"finite stream PCM");result[(at+i)*2]=a;result[(at+i)*2+1]=b;}at+=count;
 }
 return result;
}
static double threadCpuUs(bool enabled){
 if(!enabled)return 0;
#if defined(__linux__)
 timespec t{};check(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t)==0,"thread CPU clock unavailable");
 return t.tv_sec*1e6+t.tv_nsec*.001;
#else
 check(false,"--benchmark-cpu requires Linux");return 0;
#endif
}
static int benchmark(bool cpuDiagnostic=false){
 constexpr unsigned warm=512,measured=2048;
 for(bool singleSample:{false,true})for(unsigned block:{64u,128u}){
  auto owners=std::make_unique<std::array<ArpDelayGraph,20>>();auto v=arpModDefaults();v[10]=.8f;v[1]=static_cast<float>(2/11.1);
  for(unsigned t=0;t<20;++t)check((*owners)[t].prepare(48000,v,123456789+t),"graph timing prepare");
  std::vector<double> times(measured),cpuTimes(measured);std::array<float,128> l{},r{},ol{},orr{};double checksum=0;
  struct Miss {unsigned callback=0;double wall=0,cpu=0;};std::array<Miss,16> misses{};unsigned missCount=0;
  const double period=block/48000.0*1e6;
  for(unsigned i=0;i<128;++i){l[i]=static_cast<float>(.15*std::sin(i*.117)+.05*std::cos(i*.037));r[i]=static_cast<float>(.13*std::sin(i*.073));}
  for(unsigned n=0;n<warm+measured;++n){const auto begin=std::chrono::steady_clock::now();const double cpuBegin=threadCpuUs(cpuDiagnostic);watch=true;
   for(auto& owner:*owners){if(n%8==0)check(owner.setTarget(10,n%16?5.f:.1f),"graph timing controls");
    const unsigned step=singleSample?1:block;for(unsigned i=0;i<block;i+=step)check(owner.process(l.data()+i,r.data()+i,2,ol.data()+i,orr.data()+i,step),"graph timing process");
    checksum+=std::abs(ol[block-1])+std::abs(orr[block-1]);}
   watch=false;const double cpu=threadCpuUs(cpuDiagnostic)-cpuBegin;
   if(n>=warm){const auto i=n-warm;times[i]=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count();cpuTimes[i]=cpu;
    if(times[i]>period&&missCount<misses.size())misses[missCount++]={i,times[i],cpu};}
  }
  check(checksum>1&&operations==0,"timing requires real PCM and no callback allocation");std::sort(times.begin(),times.end());std::sort(cpuTimes.begin(),cpuTimes.end());double sum=0;unsigned over=0,cpuOver=0;
  for(double t:times){sum+=t;over+=t>period;}
  for(double t:cpuTimes)cpuOver+=t>period;
  std::cout<<"{\"scope\":\"20 combined Arp graphs, excludes Engine/devices\",\"block\":"<<block<<",\"singleSampleCalls\":"<<singleSample<<",\"warmCallbacks\":"<<warm<<",\"measuredCallbacks\":"<<measured<<",\"meanUs\":"<<sum/measured<<",\"p99Us\":"<<times[measured*99/100]<<",\"maximumUs\":"<<times.back()<<",\"periodUs\":"<<period<<",\"observedOverruns\":"<<over<<",\"checksum\":"<<checksum;
  if(cpuDiagnostic){std::cout<<",\"maximumThreadCpuUs\":"<<cpuTimes.back()<<",\"p99ThreadCpuUs\":"<<cpuTimes[measured*99/100]<<",\"threadCpuOverruns\":"<<cpuOver<<",\"wallMisses\":[";
   for(unsigned m=0;m<missCount;++m){if(m)std::cout<<',';std::cout<<"{\"callback\":"<<misses[m].callback<<",\"wallUs\":"<<misses[m].wall<<",\"threadCpuUs\":"<<misses[m].cpu<<'}';}std::cout<<']';}
  std::cout<<"}\n";
 }
 return 0;
}
int main(int argc,char** argv){
 if(argc==2&&std::string_view(argv[1])=="--benchmark")return benchmark();
 if(argc==2&&std::string_view(argv[1])=="--benchmark-cpu")return benchmark(true);
 check(argc==1,"Usage: aion_arp_stream_tests [--benchmark|--benchmark-cpu]");
 for(double rate:{8000.,22050.,32000.,44100.,48000.,96000.,192000.,384000.})for(unsigned channels:{0u,1u,2u}){
  auto values=arpModDefaults();values[0]=.01f;values[1]=0;ArpDelayGraph reference;check(reference.prepare(rate,values,7),"stream baseline prepare");
  const auto expected=render(reference,128,channels,false);double energy=0;for(float s:expected)energy+=s*s;check(channels?energy>1:energy==0,"stream fixture audible or true tail silence");
  for(unsigned block:{1u,37u,64u,257u}){ArpDelayGraph actual;check(actual.prepare(rate,values,7),"partition prepare");
   const auto pcm=render(actual,block,channels,block==37);check(pcm==expected&&actual.state()==reference.state(),"exact PCM/state independent of callback partition, idle and sample-offset events");}
 }
 // A valid endpoint gesture must not fail after advancing automation. The
 // original AudioParam clips the cancellation below .01 at its nominal bound.
 {ArpDelayGraph extrema;auto values=arpModDefaults();for(unsigned p=0;p<10;++p)values[p]=arpParameters[p].minimum;
  check(extrema.prepare(48000,values),"endpoint graph prepare");std::array<float,128> l{},r{};l.fill(.1f);r.fill(-.2f);
  watch=true;for(unsigned p=0;p<10;++p)check(extrema.setTarget(p,arpParameters[p].maximum),"valid endpoint gesture");
  check(extrema.process(l.data(),r.data(),2,l.data(),r.data(),128),"nominal clipping preserves valid minimum-to-maximum graph render");watch=false;}
 // Causal wake-up is explicitly different from original whole-quantum lookahead.
 ArpDelayGraph a,b;auto v=arpModDefaults();v[1]=0;v[2]=0;v[10]=0;
 check(a.prepare(48000,v)&&b.prepare(48000,v),"causal fixture prepare");std::array<float,128> zero{},l{},r{},outL{},outR{},bl{},br{};
 for(unsigned n=0;n<120;++n){watch=true;check(a.process(nullptr,nullptr,0,outL.data(),outR.data(),128)&&b.process(nullptr,nullptr,0,bl.data(),br.data(),128),"drain detector to idle");watch=false;}
 const auto idle=a.state();check(idle.tail<1e-5,"real idle state reached");r.fill(.5f);
 watch=true;check(a.process(zero.data(),r.data(),2,outL.data(),outR.data(),64),"right-only idle remains suspended");watch=false;
 check(a.state()==idle&&outR[0]==0,"original left-only idle policy retained");l[17]=.25f;
 watch=true;check(a.process(l.data(),r.data(),2,outL.data(),outR.data(),64),"wake in second half of quantum");watch=false;
 for(unsigned i=0;i<17;++i)check(outL[i]==0&&outR[i]==0,"no output before causal wake sample");
 check(outL[17]==.25f&&outR[17]==.5f&&a.state().write==(idle.write+47)%192000&&a.latencyFrames()==0,"wake sample has no added latency or retroactive clock advancement");
 // OOM must preserve BOTH the ring and the advanced modulation/automation state.
 check(a.reset(9)&&b.reset(9),"reset for OOM comparison");watch=true;check(a.setTarget(10,5)&&b.setTarget(10,5),"live target before failed prepare");
 check(a.process(l.data(),r.data(),2,outL.data(),outR.data(),128)&&b.process(l.data(),r.data(),2,bl.data(),br.data(),128),"advance full graph");watch=false;
 const auto before=a.state();const auto frame=a.position();const auto targets=a.targets();failNext=true;
 check(!a.prepare(96000,arpModDefaults(),42)&&a.state()==before&&a.targets()==targets&&a.position()==frame,"OOM retains complete graph owner");
 check(!a.prepare(0)&&a.state()==before&&a.targets()==targets&&a.position()==frame,"invalid prepare retains graph");
 watch=true;check(a.process(l.data(),r.data(),2,outL.data(),outR.data(),128)&&b.process(l.data(),r.data(),2,bl.data(),br.data(),128),"continue after OOM");watch=false;
 check(outL==bl&&outR==br&&a.state()==b.state(),"PCM and automation phase preserved by OOM");
 const auto state=a.state();const auto position=a.position();watch=true;
 check(a.process(nullptr,nullptr,0,nullptr,nullptr,0)&&a.state()==state&&a.position()==position,"zero-block is no-op");
 check(!a.process(nullptr,nullptr,3,outL.data(),outR.data(),1)&&!a.process(nullptr,nullptr,0,outL.data(),outR.data(),4097)&&a.state()==state&&a.position()==position,"malformed process atomic");
 check(a.reset(7),"constant-work graph reset");watch=false;check(b.prepare(48000,a.targets(),7),"fresh graph reset oracle");
 watch=true;check(a.process(nullptr,nullptr,0,outL.data(),outR.data(),128)&&b.process(nullptr,nullptr,0,bl.data(),br.data(),128),"post-clear tails");watch=false;
 check(outL==bl&&outR==br&&a.state()==b.state(),"old PCM/automation cannot revive after clear");
 {const auto settings=a.targets();ArpDelayGraph fresh;
  check(a.prepare(96000,settings,11,384)&&fresh.prepare(96000,settings,11,384),"successful rate change with unchanged values");
  l.fill(.2f);r.fill(-.1f);
  for(unsigned n=0;n<512;++n){watch=true;
   check(a.process(l.data(),r.data(),2,outL.data(),outR.data(),128)&&fresh.process(l.data(),r.data(),2,bl.data(),br.data(),128),"reprepared rate render");watch=false;
   check(outL==bl&&outR==br&&a.state()==fresh.state(),"cached coefficients, LFO and PCM match fresh owner after rate change");}
  check(a.state().clock==27136,"delay coefficient recomputed at 96k despite unchanged controls");}
 watch=true;check(a.reset(7,UINT64_MAX-65),"late global timeline reset");const auto terminal=a.state();
 check(!a.process(nullptr,nullptr,0,outL.data(),outR.data(),128)&&a.state()==terminal&&a.position()==UINT64_MAX-65,"timeline overflow rejection is atomic");
 check(a.process(nullptr,nullptr,0,outL.data(),outR.data(),65)&&a.position()==UINT64_MAX,"last valid timeline samples render");
 check(!a.process(nullptr,nullptr,0,outL.data(),outR.data(),1)&&!a.setTarget(0,.4f),"terminal timeline cannot wrap audio or control events");watch=false;
 check(operations==0,"all graph processing/control/reset new/delete prohibited");
 std::cout<<"PASS combined causal Arp graph: exact variable-block PCM/state, 64/128/1/37/257, rates/mono/stereo, idle/wake/zero latency, live controls, reset and whole-owner OOM; no callback new/delete\n";
}
