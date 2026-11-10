// SPDX-License-Identifier: AGPL-3.0-only
#include "core/ArpDelay.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <memory>
#include <string_view>
namespace {thread_local bool watch=false,failNext=false;unsigned operations=0;
void check(bool condition,const char* message){if(!condition){std::cerr<<message<<'\n';std::exit(1);}}}
void* operator new(std::size_t size){if(watch)++operations;if(failNext){failNext=false;throw std::bad_alloc();}if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {if(watch)++operations;std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void* p) noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept{::operator delete(p);}void operator delete[](void* p,std::size_t) noexcept{::operator delete(p);}
static int benchmark(){
 constexpr unsigned warm=512,measured=2048;
 for(unsigned block:{64u,128u}){
  auto delays=std::make_unique<std::array<aion::ArpDelay,20>>();
  auto values=aion::arpDefaults();values[1]=static_cast<float>(2/11.1);
  for(unsigned i=0;i<20;++i)check((*delays)[i].prepare(48000,values,123456789+i),"benchmark prepare");
  std::array<float,128> l{},r{},ol{},orr{};for(unsigned i=0;i<128;++i){l[i]=static_cast<float>(.15*std::sin(i*.13));r[i]=static_cast<float>(.1*std::cos(i*.07));}
  std::vector<double> times(measured);double checksum=0;
  for(unsigned n=0;n<warm+measured;++n){
   const auto begin=std::chrono::steady_clock::now();
   for(auto& delay:*delays){check(delay.process(l.data(),r.data(),2,ol.data(),orr.data(),block),"benchmark render");checksum+=std::abs(ol[block-1])+std::abs(orr[block-1]);}
   if(n>=warm)times[n-warm]=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count();
  }
  check(checksum>1,"benchmark nonzero PCM");std::sort(times.begin(),times.end());double sum=0;unsigned over=0;const double deadline=block/48000.0*1e6;
  for(double time:times){sum+=time;over+=time>deadline;}
  std::cout<<"{\"scope\":\"20 isolated stereo Arp Delay worklet kernels; excludes automation/post-pan/Engine/device\",\"block\":"<<block<<",\"warmCallbacks\":"<<warm<<",\"measuredCallbacks\":"<<measured
   <<",\"meanUs\":"<<sum/measured<<",\"p99Us\":"<<times[measured*99/100]<<",\"maximumUs\":"<<times.back()<<",\"periodUs\":"<<deadline<<",\"observedOverruns\":"<<over<<",\"checksum\":"<<checksum<<"}\n";
 }
 return 0;
}
int main(int argc,char** argv){
 if(argc==2&&std::string_view(argv[1])=="--benchmark")return benchmark();
 check(argc==1,"Usage: aion_arp_tests [--benchmark]");
 using namespace aion;
 for(double rate:{8000.,22050.,32000.,44100.,48000.,96000.,192000.,384000.})for(unsigned block:{64u,128u}){
  ArpDelay a,b;auto values=arpDefaults();values[0]=.01f;values[1]=.7f;values[8]=3;
  check(a.prepare(rate,values,7)&&b.prepare(rate,values,7),"prepare valid four-second owners");
  check(a.capacity()==static_cast<std::size_t>(std::floor(rate*4)),"reference four-second capacity");
  std::array<float,128> l{},r{},al{},ar{},bl{},br{};double energy=0;
  for(unsigned n=0;n<320;++n){
   for(unsigned i=0;i<block;++i){l[i]=static_cast<float>(.3*std::sin((n*block+i)*.13));r[i]=static_cast<float>(.2*std::cos((n*block+i)*.07));}
   if(n==80||n==120||n==160){values[3]=n==80?1.f:0.f;values[4]=n==160?16.f:2.f;values[8]=n==160?.25f:3.f;}
   watch=true;check(a.setValues(values)&&b.setValues(values),"bounded complete controls");
   bl=l;br=r;
   check(a.process(l.data(),r.data(),2,al.data(),ar.data(),block)&&b.process(bl.data(),br.data(),2,bl.data(),br.data(),block),"bounded stereo render, including in-place input/output");watch=false;
   check(a.state()==b.state(),"deterministic sequence/random ownership");
   for(unsigned i=0;i<block;++i){check(al[i]==bl[i]&&ar[i]==br[i]&&std::isfinite(al[i])&&std::isfinite(ar[i]),"finite deterministic stereo");energy+=al[i]*al[i]+ar[i]*ar[i];}
  }
  check(energy>1,"fixture must contain real audio");
  const auto before=a.state();auto bad=values;bad[0]=std::numeric_limits<float>::quiet_NaN();
  watch=true;check(!a.setValues(bad)&&a.state()==before&&a.values()==values,"invalid controls atomic");
  check(!a.process(nullptr,nullptr,3,al.data(),ar.data(),block)&&a.state()==before,"invalid channel count is atomic");
  check(!a.process(nullptr,nullptr,0,al.data(),ar.data(),129)&&a.state()==before,"unbounded frame request rejected");watch=false;
  failNext=true;check(!a.prepare(rate,arpDefaults(),9)&&a.state()==before&&a.values()==values,"worker OOM preserves complete old owner");
  check(!a.prepare(0)&&!a.prepare(std::numeric_limits<double>::infinity())&&a.state()==before,"invalid prepare preserves old owner");
  watch=true;check(a.reset(7),"constant-work generation reset");watch=false;
  check(b.prepare(rate,values,7),"fresh owner reference for reset");
  for(unsigned n=0;n<64;++n){watch=true;check(a.process(nullptr,nullptr,0,al.data(),ar.data(),block)&&b.process(nullptr,nullptr,0,bl.data(),br.data(),block),"tail render after reset");watch=false;
   check(a.state()==b.state(),"generation reset reproduces fresh clock/sequence");for(unsigned i=0;i<block;++i)check(al[i]==0&&ar[i]==0&&al[i]==bl[i]&&ar[i]==br[i],"old grain/delay PCM cannot revive after reset");}
  // Finite overflow and non-finite input are quarantined, without poisoning state.
  l.fill(std::numeric_limits<float>::max());r.fill(std::numeric_limits<float>::infinity());
  for(unsigned n=0;n<64;++n){watch=true;check(a.process(l.data(),r.data(),2,al.data(),ar.data(),block),"extreme input render");watch=false;for(unsigned i=0;i<block;++i)check(std::isfinite(al[i])&&std::isfinite(ar[i]),"numeric safety");}
 }
 // Ring wrap and nonstandard direct-worklet invocation sizes are bounded too.
 ArpDelay a;check(a.prepare(8000),"wrap prepare");std::array<float,128> in{},l{},r{};in.fill(.1f);unsigned frames=0;
 for(unsigned n=0;n<1000;++n){const unsigned count=1+(n*37)%128;watch=true;check(a.process(in.data(),nullptr,1,l.data(),r.data(),count),"short mono block");watch=false;frames+=count;}
 check(frames>a.capacity()&&a.state().write==frames%a.capacity(),"actual ring wrap with variable direct-worklet blocks");
 check(operations==0,"process/control/reset new/delete prohibited");
 std::cout<<"PASS Arp Delay fixed storage, mono/stereo/tails, rates/64/128, note/STAY/range controls, generation reset, prepare OOM rollback and numerical quarantine; no callback new/delete\n";
}
