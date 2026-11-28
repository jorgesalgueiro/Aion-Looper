// SPDX-License-Identifier: AGPL-3.0-only
#include "core/PlateReverb.h"
#include "core/ReverbSettings.h"
#include "core/ReverbStorage.h"
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
static thread_local bool watch=false;static unsigned heapOps=0;
static bool failNext=false;
void* operator new(std::size_t n){if(watch)++heapOps;if(failNext){failNext=false;throw std::bad_alloc();}if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete(void* p)noexcept{if(watch&&p)++heapOps;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
static void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
static double tone(const std::vector<float>& x,double f,double sr,size_t a,size_t b){
 double re=0,im=0;for(size_t i=a;i<b;++i){const double p=2*3.14159265358979323846*f*double(i)/sr;re+=x[i]*std::cos(p);im+=x[i]*std::sin(p);}return 2*std::hypot(re,im)/double(b-a);
}
static double rms(const std::vector<float>& x,size_t a,size_t b){double s=0;for(size_t i=a;i<b;++i)s+=double(x[i])*double(x[i]);return std::sqrt(s/double(b-a));}
static void run(double sr,unsigned block){
 auto render=[&](aion::PlateReverb& p,const std::vector<float>& inL,const std::vector<float>& inR,std::vector<float>& outL,std::vector<float>& outR){
  outL.assign(inL.size(),0);outR.assign(inR.size(),0);
  for(size_t i=0;i<inL.size();i+=block){const auto n=std::min<size_t>(block,inL.size()-i);
   watch=true;check(p.process(std::span<const float>(inL.data()+i,n),std::span<const float>(inR.data()+i,n),std::span<float>(outL.data()+i,n),std::span<float>(outR.data()+i,n)),"block process");watch=false;check(!heapOps,"zero plate callback heap");}
 };
 aion::PlateSettings s{};s.enabled=true;s.decay=2.5f;s.damping=12000;s.lowCut=80;s.predelay=.02f;s.mix=.5f;s.unmask=0.f;
 aion::PlateReverb p;std::atomic<bool> cancel{false};
 check(p.prepare(sr,s),"prepare plate");
 // Disabled is an exact dry passthrough.
 {aion::PlateSettings off=s;off.enabled=false;aion::PlateReverb q;check(q.prepare(sr,off),"prepare off");
  std::vector<float> l(256),r(256),ol,or_;for(size_t i=0;i<256;++i){l[i]=float(.1*std::sin(i*.07));r[i]=float(.05*std::cos(i*.05));}
  render(q,l,r,ol,or_);check(ol==l&&or_==r,"disabled plate is exactly dry");}
 // Impulse: finite, nonzero, decaying tail; a longer decay keeps more energy.
 auto impulseRun=[&](float decay,float damping,std::vector<float>& out){
  aion::PlateSettings v=s;v.decay=decay;v.damping=damping;v.mix=1.f;aion::PlateReverb q;check(q.prepare(sr,v),"prepare impulse");
  std::vector<float> in(size_t(sr*3),0.f),zero(in.size()),o,o2;in[0]=1;render(q,in,zero,o,o2);
  for(float x:o)check(std::isfinite(x),"finite plate output");out=o;};
 std::vector<float> longTail,shortTail;impulseRun(10.f,12000,longTail);impulseRun(1.f,12000,shortTail);
 check(rms(longTail,size_t(sr*.5),size_t(sr*.9))>rms(shortTail,size_t(sr*.5),size_t(sr*.9)),"longer decay keeps a larger tail");
 check(rms(longTail,size_t(sr*.5),size_t(sr*.9))>1e-6&&rms(longTail,0,size_t(sr*.1))>1e-4,"plate tail and onset are audible");
 if(sr>=44100){
  std::vector<float> bright,dark;impulseRun(10.f,18000,bright);impulseRun(10.f,1500,dark);
  check(tone(bright,6000,sr,size_t(sr*.2),size_t(sr*.7))>tone(dark,6000,sr,size_t(sr*.2),size_t(sr*.7)),"lower damping removes more high end");
 }
 // Dry-aware carving: a 2 kHz dry tone must reduce the wet 2 kHz content, while
 // the dry contribution itself is unchanged (mix 1 exposes wet only for the test
 // of the carved path; the mixed test checks the total).
 {aion::PlateSettings v=s;v.mix=1.f;v.decay=3.f; // wet-only view for the carver measurement
  auto carve=[&](float unmask){
   aion::PlateSettings u=v;u.unmask=unmask;aion::PlateReverb q;check(q.prepare(sr,u),"prepare carver");
   std::vector<float> l(size_t(sr*1.6)),r(l.size()),o,o2;for(size_t i=0;i<l.size();++i){l[i]=float(.2*std::sin(2*3.14159265358979323846*2000*double(i)/sr));r[i]=l[i];}
   render(q,l,r,o,o2);return tone(o,2000,sr,size_t(sr*1.2),size_t(sr*1.6));};
  const auto open=carve(0.f),carved=carve(1.f);
  check(carved<open*.9,"dry-aware carver reduces reverb in the dry band");
  check(carved>.001,"carver leaves a usable wet signal");
  // mix zero leaves dry untouched even with the carver active.
  aion::PlateSettings dry=s;dry.mix=0.f;dry.unmask=1.f;aion::PlateReverb q;check(q.prepare(sr,dry),"prepare dry");
  std::vector<float> l(size_t(sr*.2)),r(l.size()),o,o2;for(size_t i=0;i<l.size();++i){l[i]=float(.3*std::sin(2*3.14159265358979323846*2000*double(i)/sr));r[i]=-l[i];}
  render(q,l,r,o,o2);check(o==l&&o2==r,"dry path is never carved");}
 // Invalid set keeps the old settings; cancellation rejects prepare.
 {aion::PlateSettings bad=s;bad.decay=20;const auto before=p.settings();check(!p.set(bad)&&p.settings().decay==before.decay,"invalid plate edit rejected atomically");
  cancel.store(true);check(!p.prepare(sr,s,&cancel),"cancelled plate prepare rejected");cancel.store(false);}
}
static void storageEngineSwitch(){
 aion::ReverbStorage st;check(st.prepareStopped(8000),"storage prepare");
 aion::ReverbSettings m{};m.engine=aion::ReverbEngine::machine;m.enabled=true;m.plate=aion::plateDefaults();m.plate[0]=2;m.plate[4]=.5;
 check(st.reservationBytes(8000,m)>0,"machine reservation");
 check(st.request(0,m,1),"machine request");
 for(unsigned n=0;n<400&&!st.view(0).active;++n){st.serviceWorker();(void)st.poll();}
 check(st.view(0).active&&st.plateGraph(0)&&!st.graph(0),"machine graph installed");
 std::vector<float> l(64,0.f),r(64,0.f),ol(64,0.f),or_(64,0.f);l[0]=1;
 check(st.process(0,l,r,ol,or_),"machine process");
 double peak=0;for(float x:ol)peak=std::max(peak,std::abs(double(x)));check(peak>1e-6,"machine produced wet audio");
 check(st.setControl(0,0,5.0),"machine live control");
 for(unsigned n=0;n<100&&st.view(0).transitioning;++n)(void)st.process(0,l,r,ol,or_);
 aion::ReverbSettings c{};c.enabled=true;c.engine=aion::ReverbEngine::convolution;
 check(st.request(0,c,2),"convolution request");
 for(unsigned n=0;n<400&&!st.graph(0);++n){st.serviceWorker();(void)st.poll();}
 check(st.graph(0)&&!st.plateGraph(0),"engine switch installs convolution");
 c.values[1]=.4;watch=true;check(st.set(0,c),"live convolution edit during mixed-engine fade");watch=false;check(!heapOps,"mixed-engine edit has no callback heap");
 for(unsigned n=0;n<4;++n)st.process(0,l,r,ol,or_);
 aion::ReverbStorage::Bank bank{};
 for(unsigned id=0;id<20;++id){bank[id]=id%2?c:m;bank[id].enabled=true;}
 check(st.requestBank(bank),"mixed all-twenty Machine/convolution bank admitted");
 for(unsigned n=0;n<40&&st.bankStatus()==aion::ReverbStorage::BankStatus::pending;++n)st.serviceWorker();
 check(st.bankStatus()==aion::ReverbStorage::BankStatus::ready,"all mixed members prepared");
 watch=true;check(st.prepareBankCommit(),"Machine bank commit preparation");st.commitBank();watch=false;check(!heapOps,"mixed bank commit has no callback heap");
 for(unsigned id=0;id<20;++id){check(id%2?st.graph(id)!=nullptr:st.plateGraph(id)!=nullptr,"every mixed member adopts its correct graph type");for(unsigned n=0;n<4;++n)st.process(id,l,r,ol,or_);}
 st.serviceWorker();
 auto replacement=bank;replacement[0].plate[0]=8;replacement[0].preset=0;
 check(st.requestBank(replacement),"Machine OOM bank admitted");failNext=true;st.serviceWorker();check(!failNext,"worker OOM injected into Machine preparation");
 watch=true;const auto failed=st.poll();watch=false;
 check(failed.bankFailed&&st.plateGraph(0)&&st.plateGraph(0)->settings().decay==2,"Machine OOM bank preserves previous prepared owners");check(!heapOps,"Machine OOM rollback has no callback heap");
}
int main(){run(48000,64);run(48000,128);run(8000,64);run(8000,128);storageEngineSwitch();std::cout<<"PASS native Plate/Machine: stable finite tail, decay/damping behavior, dry-aware carving, untouched dry path and zero callback heap at 8/48 kHz 64/128\n";}
