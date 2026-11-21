// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Delay.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
namespace {
bool watched=false,failNext=false;unsigned heapOperations=0;
void check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
struct AudioScope {AudioScope(){heapOperations=0;watched=true;}~AudioScope(){watched=false;check(!heapOperations,"heap operation in repeat DSP/control/reset");}};
}
void* operator new(std::size_t n){if(watched)++heapOperations;if(failNext){failNext=false;throw std::bad_alloc();}if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watched&&p)++heapOperations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace aion;
static DelaySettings exact(unsigned count){
 DelaySettings s;s.enabled=true;s.values={.02,4,20000,1,0,.8,0};
 check(setDelayRepeats(s,DelayRepeatMode::exact,count),"exact count settings");return s;
}
int main(){
 DelaySettings s;const auto values=s.values;
 {AudioScope guard;
  for(unsigned count=1;count<=10;++count){check(setDelayRepeats(s,DelayRepeatMode::exact,count)&&s.values==values&&!s.preset,"count does not repurpose legacy values");}
  const auto old=s;check(!setDelayRepeats(s,DelayRepeatMode::exact,0)&&!setDelayRepeats(s,DelayRepeatMode::exact,11)
   &&!setDelayRepeats(s,static_cast<DelayRepeatMode>(2),1)&&s==old,"mode/count invalid edits atomic");
  check(applyDelayPreset(s,2)&&s.repeatMode==DelayRepeatMode::feedback&&s.values[1]==1,"legacy preset restores feedback semantics");
 }
 for(double rate:{8000.,32000.,48000.})for(unsigned count:{1u,2u,3u,10u})for(unsigned block:{64u,128u}){
  s=exact(count);Delay delay;check(delay.prepare(rate,s),"exact prepare");
  check(delay.storageFrames()==static_cast<std::size_t>(rate*4*count)+2,"prepare capacity follows exact count");
  const auto period=static_cast<unsigned>(rate*.02);
  std::array<double,10> peaks{};
  // Peak windows identify exactly N delayed events. Filter tails may extend an
  // echo by a few samples; there must be no recirculated N+1 event.
  {AudioScope guard;for(unsigned base=0;base<period*(count+2);base+=block)for(unsigned j=0;j<block;++j){
   const unsigned frame=base+j;float l,r;delay.tick(frame==0?1.f:0.f,frame==0?-.5f:0.f,l,r);
   check(std::isfinite(l)&&std::isfinite(r)&&std::abs(r+.5*l)<1.e-6,"exact stereo phase/finite");
   for(unsigned tap=0;tap<count;++tap)if(frame>=(tap+1)*period&&frame<(tap+1)*period+32)
    peaks[tap]=std::max(peaks[tap],std::abs(double(l)));
   if(frame==period)check(l==1,"first exact echo is unfiltered and on time");
   if(frame>=(count+1)*period)check(std::abs(l)<1.e-9,"no N+1 echo");
  }}
  for(unsigned tap=0;tap<count;++tap)check(peaks[tap]>.1*std::pow(.65,tap),"every requested echo audible");
  // A second pulse arrives during the first pulse's repeats. It must get its
  // own complete N-echo train, not reset a global input/silence counter.
  Delay a,b,pair;check(a.prepare(rate,s)&&b.prepare(rate,s)&&pair.prepare(rate,s),"overlap prepare");
  {AudioScope guard;for(unsigned n=0;n<period*(count+3);++n){float al,ar,bl,br,pl,pr;
   a.tick(n==0?.4f:0.f,0,al,ar);b.tick(n==period/2?.2f:0.f,0,bl,br);
   pair.tick(n==0?.4f:(n==period/2?.2f:0.f),0,pl,pr);
   check(std::abs(pl-double(al)-bl)<1.e-6&&pr==0,"overlapping inputs keep independent complete repeat trains");
  }}
 }
 // Mode/reallocation rejection and OOM cannot damage an active feedback tail.
 s={};s.enabled=true;s.values={.01,8,3000,1,0,.8,0};Delay original,oracle;
 check(original.prepare(8000,s)&&oracle.prepare(8000,s),"legacy prepare");
 auto replacement=s;check(setDelayRepeats(replacement,DelayRepeatMode::exact,3),"replacement settings");
 {AudioScope guard;check(!original.set(replacement,120)&&original.settings()==s,"mode switch requires prepared owner");}
 failNext=true;check(!original.prepare(8000,replacement)&&!failNext,"exact storage OOM injected");
 {AudioScope guard;for(unsigned n=0;n<16000;++n){float l,r,x,y;
  original.tick(n==0?1.f:0.f,0,l,r);oracle.tick(n==0?1.f:0.f,0,x,y);check(l==x&&r==y,"failed exact prepare preserves feedback PCM");
 }}
 // Existing Feedback mode must still produce an open-ended decaying train.
 Delay feedback;check(feedback.prepare(8000,s),"feedback regression prepare");double fourth=0;
 {AudioScope guard;for(unsigned n=0;n<450;++n){float l,r;feedback.tick(n==0?1.f:0.f,0,l,r);if(n>=320&&n<352)fourth=std::max(fourth,std::abs(double(l)));}}
 check(fourth>.01,"feedback retains fourth echo independently of exact count");
 // Prepared high-count storage allows live count reductions/increases with
 // bounded gain fades; a larger unprepared count is rejected atomically.
 s=exact(3);Delay editable;check(editable.prepare(8000,s),"editable count prepare");
 {AudioScope guard;check(setDelayRepeats(s,DelayRepeatMode::exact,1)&&editable.set(s,120),"live reduce count");
  for(unsigned n=0;n<4000;++n){float l,r;editable.tick(0,0,l,r);}
  auto more=s;check(setDelayRepeats(more,DelayRepeatMode::exact,4)&&!editable.set(more,120)&&editable.settings()==s,"growth beyond capacity atomic");
  check(setDelayRepeats(s,DelayRepeatMode::exact,3)&&editable.set(s,120),"live increase within capacity");
  editable.reset();for(unsigned n=0;n<4000;++n){float l,r;editable.tick(0,0,l,r);check(l==0&&r==0,"exact clear no stale echoes");}
 }
 std::cout<<"PASS exact 1/2/3/10 and legacy Feedback modes; overlap, 64/128 grouping, OOM and zero tracked callback heap operations\n";
}
