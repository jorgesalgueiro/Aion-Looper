// SPDX-License-Identifier: AGPL-3.0-only
#include "core/DelayStorage.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>
namespace {
thread_local bool watched=false,failNext=false,pauseNext=false;
thread_local unsigned operations=0;
std::atomic<bool> entered{false},resume{false};
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct AudioScope {AudioScope(){operations=0;watched=true;}~AudioScope(){watched=false;check(!operations,"heap operation on Delay audio owner");}};
}
void* operator new(std::size_t n){if(watched)++operations;if(failNext){failNext=false;throw std::bad_alloc();}
 if(pauseNext){pauseNext=false;entered.store(true,std::memory_order_release);while(!resume.load(std::memory_order_acquire))std::this_thread::yield();}
 if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watched&&p)++operations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace aion;
int main(){
 DelaySettings s;s.enabled=true;s.values[0]=.01;
 DelayStorage bank;check(bank.prepareStopped(8000),"prepare storage");
 {AudioScope scope;for(unsigned id=0;id<20;++id)check(bank.request(id,s,120),"twenty admissions");
  check(!bank.request(0,s,120)&&!bank.request(20,s,120)&&!bank.graph(20),"invalid/duplicate atomic");
  check(!bank.poll().installed,"no early adoption");}
 for(unsigned n=0;n<20;++n)bank.serviceWorker();
 {AudioScope scope;check(bank.poll().installed==0xfffff,"twenty worker-prepared owners adopted");}
 Delay oracle;check(oracle.prepare(8000,s),"oracle prepare");auto* previous=bank.graph(0);
 auto exact=s;check(setDelayRepeats(exact,DelayRepeatMode::exact,3),"exact settings");
 {AudioScope scope;check(bank.request(0,exact,120),"replacement request");}
 failNext=true;bank.serviceWorker();check(!failNext,"worker OOM exercised");
 {AudioScope scope;check(bank.poll().failed==1&&bank.graph(0)==previous,"OOM preserves active owner");
  for(unsigned n=0;n<8192;++n){float l,r,a,b;previous->tick(n==0?1.f:0.f,0,l,r);oracle.tick(n==0?1.f:0.f,0,a,b);check(l==a&&r==b,"OOM preserves future stereo PCM");}
  check(bank.request(0,exact,120),"retry");}
 bank.serviceWorker();
 {AudioScope scope;bank.cancel(0);check(bank.request(0,s,120),"new request after cancelled ready");
  check(!bank.poll().installed&&bank.graph(0)==previous&&bank.view(0).pending,"stale result cannot adopt or cancel new request");}
 bank.serviceWorker();
 {AudioScope scope;check(bank.poll().installed==1&&bank.view(0).installation==2,"fresh adoption once");bank.clearAll();}
 bank.serviceWorker();
 // Cancelled queued work skips allocation; stale ready/failed receipts reclaim
 // through worker retirement, never through audio destruction.
 {AudioScope scope;for(unsigned n=0;n<40;++n){check(bank.request(n%20,s,120),"bounded pool reservation");bank.cancel(n%20);}
  check(!bank.request(0,s,120),"full pool atomic overflow");}
 for(unsigned n=0;n<40;++n)bank.serviceWorker();
 {AudioScope scope;check(!bank.poll().installed,"cancelled pool never installs");}
 bank.serviceWorker();
 {AudioScope scope;check(bank.request(0,s,120),"capacity recovery");bank.cancel(0);}
 failNext=true;bank.serviceWorker();check(failNext,"cancel before preparation skips allocation");failNext=false;
 {AudioScope scope;(void)bank.poll();}bank.serviceWorker();
 // Count-aware byte budget includes active AND pending/retired reservations.
 const auto one=Delay::requiredStorageFrames(8000,s)*sizeof(std::array<float,2>);
 DelayStorage limited(one*2);check(limited.prepareStopped(8000),"budget prepare");
 {AudioScope scope;check(limited.request(0,s,120)&&limited.request(1,s,120),"budget exact boundary");
  check(!limited.request(2,s,120),"budget overflow rejected");}
 limited.serviceWorker();limited.serviceWorker();
 {AudioScope scope;check(limited.poll().installed==3,"budget adoption");
  check(!limited.request(0,exact,120)&&limited.graph(0),"larger-count request preserves old owner on budget failure");
  limited.clearAll();check(!limited.request(0,s,120),"retired allocation retains reservation until worker frees");}
 limited.serviceWorker();
 {AudioScope scope;check(limited.request(0,s,120),"budget returned after retirement");}
 // Cancel/clear while the worker is blocked INSIDE allocation. This exercises
 // real ownership publication; the test's pause is never on the audio owner.
 {AudioScope scope;check(bank.request(0,exact,120),"concurrent request");}
 entered=false;resume=false;
 std::thread worker([&]{pauseNext=true;bank.serviceWorker();});
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
 while(!entered.load(std::memory_order_acquire)&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
 check(entered.load(std::memory_order_acquire),"worker reached allocation");
 {AudioScope scope;bank.clear(0);check(!bank.graph(0),"clear during worker prepare");}
 resume.store(true,std::memory_order_release);worker.join();
 {AudioScope scope;check(!bank.poll().installed&&!bank.graph(0),"late allocated owner cannot resurrect cleared track");}
 bank.serviceWorker();
 check(!bank.prepareStopped(NAN)&&bank.prepareStopped(96000),"stopped rate validation/recovery");
 {AudioScope scope;check(!bank.poll().installed&&bank.request(19,s,120),"no old-rate resurrection");}
 bank.serviceWorker();{AudioScope scope;check(bank.poll().installed==(1u<<19),"new-rate recovery");}
 std::cout<<"PASS Delay bounded storage, byte budget, OOM PCM preservation, cancellation during worker allocation and zero tracked audio heap operations\n";
}
