// SPDX-License-Identifier: AGPL-3.0-only
#include "core/ArpDelayStorage.h"
#include "core/LoopBank.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <thread>

namespace {
thread_local bool watch=false,failNext=false,pauseNextAllocation=false;
thread_local unsigned operations=0;
std::atomic<bool> allocationEntered{false},releaseAllocation{false};
void check(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
struct AudioScope {
 AudioScope(){check(!watch,"nested audio watcher");operations=0;watch=true;}
 ~AudioScope(){watch=false;check(operations==0,"allocation/deallocation on audio owner");}
};
}
void* operator new(std::size_t n){
 if(watch)++operations;
 if(failNext){failNext=false;throw std::bad_alloc();}
 if(pauseNextAllocation){pauseNextAllocation=false;allocationEntered.store(true,std::memory_order_release);while(!releaseAllocation.load(std::memory_order_acquire))std::this_thread::yield();}
 if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void operator delete(void* p) noexcept {if(watch&&p)++operations;std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void* p) noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept{::operator delete(p);}void operator delete[](void* p,std::size_t) noexcept{::operator delete(p);}
using namespace aion;

static void samePcm(ArpDelayGraph& a,ArpDelayGraph& b,unsigned block,unsigned callbacks=32){
 std::array<float,128> l{},r{},al{},ar{},bl{},br{};
 for(unsigned n=0;n<callbacks;++n){
  for(unsigned i=0;i<block;++i){const auto f=n*block+i;l[i]=static_cast<float>(.15*std::sin(f*.071));r[i]=static_cast<float>(.12*std::cos(f*.033));}
  AudioScope audio;
  check(a.process(l.data(),r.data(),2,al.data(),ar.data(),block)&&b.process(l.data(),r.data(),2,bl.data(),br.data(),block),"owned graph render");
  for(unsigned i=0;i<block;++i)check(al[i]==bl[i]&&ar[i]==br[i],"storage must preserve exact graph PCM");
  check(a.state()==b.state()&&a.position()==b.position(),"storage must preserve exact graph state/clock");
 }
}
static void admission(unsigned block){
 ArpDelayStorage bank;auto v=arpModDefaults();v[0]=.01f;v[1]=0;
 check(bank.prepareStopped(8000),"bank prepare");
 {
  AudioScope audio;
  check(!bank.request(20,v,1)&&!bank.graph(20)&&!bank.view(20).pending,"invalid track is atomic");
  auto bad=v;bad[2]=std::numeric_limits<float>::quiet_NaN();check(!bank.request(0,bad,1),"invalid settings rejected");
  for(unsigned id=0;id<20;++id)check(bank.request(id,v,100+id),"all twenty storage requests admitted");
  check(!bank.request(0,v,999)&&bank.view(0).pending,"duplicate pending request preserves owner");
  check(bank.poll(0).installed==0,"unprepared graphs never adopted");
 }
 for(unsigned id=0;id<20;++id)bank.serviceWorker();
 {AudioScope audio;const auto result=bank.poll(257);check(result.installed==0xfffff&&result.failed==0,"all twenty prepared graphs adopted");}
 ArpDelayGraph continuity;check(continuity.prepare(8000,v,100,257),"continuity oracle prepare");
 for(unsigned id=0;id<20;++id){
  ArpDelayGraph oracle;check(oracle.prepare(8000,v,100+id,257),"fresh oracle prepare");
  check(bank.graph(id)&&bank.view(id).active&&!bank.view(id).pending,"active owner view");
  samePcm(*bank.graph(id),id?oracle:continuity,block);
 }
 // Old active state survives rejected duplicate requests, cancellation and OOM.
 auto* old=bank.graph(0);const auto oldState=old->state();const auto oldFrame=old->position();
 {AudioScope audio;check(bank.request(0,v,777),"replacement request");}
 failNext=true;bank.serviceWorker();check(!failNext,"OOM injected in worker ring allocation");
 {AudioScope audio;const auto result=bank.poll(oldFrame);check(result.failed==1&&!result.installed,"explicit failed preparation receipt");
  check(bank.graph(0)==old&&bank.graph(0)->state()==oldState&&bank.graph(0)->position()==oldFrame,"OOM preserves complete active owner");
  check(bank.view(0).failed&&!bank.view(0).pending,"OOM state permits retry");}
 samePcm(*old,continuity,block); // allocation failure must not alter later sound
 bank.serviceWorker();
 // Cancel after worker completion, then reuse the same track. Stale ready graph
 // must not complete the replacement, reset its clock or replace its seed.
 {AudioScope audio;check(bank.request(0,v,1234),"cancelled replacement admitted");}
 bank.serviceWorker();
 {AudioScope audio;bank.cancel(0);check(bank.request(0,v,4321),"fresh request after cancellation");const auto result=bank.poll(1000);
  check(!result.installed&&!result.failed&&bank.graph(0)==old&&bank.view(0).pending,"stale ready result cannot adopt or cancel newer request");}
 bank.serviceWorker();
 {AudioScope audio;check(bank.poll(1000).installed==1,"replacement accepted only after its own preparation");}
 ArpDelayGraph replacement;check(replacement.prepare(8000,v,4321,1000),"replacement oracle");samePcm(*bank.graph(0),replacement,block);
 // Bad adoption clock and invalid stopped-rate requests preserve the old owner.
 auto* current=bank.graph(0);const auto state=current->state();
 {AudioScope audio;check(bank.request(0,v,1),"terminal clock request");}
 bank.serviceWorker();
 {AudioScope audio;check(bank.poll(UINT64_MAX).failed==1&&bank.graph(0)==current&&current->state()==state,"invalid adoption clock rollback");}
 check(!bank.prepareStopped(7999)&&!bank.prepareStopped(384001)&&!bank.prepareStopped(NAN)&&bank.graph(0)==current,"invalid rate preserves bank");
 // Clear retires every prepared owner; worker reclaims, allowing a full bank again.
 {AudioScope audio;bank.clearAll();for(unsigned id=0;id<20;++id)check(!bank.graph(id)&&!bank.view(id).pending,"clear detaches active ownership immediately");}
 bank.serviceWorker();
 for(unsigned id=0;id<20;++id){AudioScope audio;check(bank.request(id,v,id),"full bank recovery after retirement");}
 // Stopped rate change destroys queued/active/retired work and resets every slot.
 check(bank.prepareStopped(96000),"new rate prepare cancels every old request");
 {AudioScope audio;check(!bank.poll(0).installed&&!bank.graph(0),"no old-rate resurrection");check(bank.request(19,v,22),"new-rate admission");}
 bank.serviceWorker();
 {AudioScope audio;check(bank.poll(513).installed==(1u<<19),"new-rate adoption");}
 ArpDelayGraph newRate;check(newRate.prepare(96000,v,22,513),"new-rate oracle");samePcm(*bank.graph(19),newRate,block,256);
}
static void saturation(){
 ArpDelayStorage bank;const auto v=arpModDefaults();check(bank.prepareStopped(8000),"overflow bank prepare");
 // A stalled worker cannot cause unbounded requests/retirement allocation.
 {AudioScope audio;
  for(unsigned i=0;i<ArpDelayStorage::capacity;++i){check(bank.request(i%20,v,i),"reserve exactly bounded slots");bank.cancel(i%20);}
  check(!bank.request(0,v,1),"full storage rejects before mutation");
  check(!bank.view(0).active&&!bank.view(0).pending&&!bank.view(0).failed,"overflow leaves track unchanged");
 }
 for(unsigned i=0;i<ArpDelayStorage::capacity;++i)bank.serviceWorker();
 {AudioScope audio;const auto done=bank.poll(0);check(!done.installed&&!done.failed,"cancelled queued work never becomes active");}
 bank.serviceWorker();
 // A cancelled request before service must not even attempt an allocation.
 {AudioScope audio;check(bank.request(0,v,1),"cancel allocation fixture");bank.cancel(0);}
 failNext=true;bank.serviceWorker();check(failNext,"cancel before service skips ring allocation");failNext=false;
 {AudioScope audio;(void)bank.poll(0);}
 bank.serviceWorker();
 {AudioScope audio;for(unsigned id=0;id<20;++id)check(bank.request(id,v,id),"overflow recovery restores all twenty requests");}
 for(unsigned id=0;id<20;++id)bank.serviceWorker();
 {AudioScope audio;check(bank.poll(0).installed==0xfffff,"full bank recovers without leaked slots");
  // Twenty active plus twenty cancelled replacements exactly fill the bound.
  for(unsigned id=0;id<20;++id){check(bank.request(id,v,id),"full-bank replacements fit");bank.cancel(id);}
  check(!bank.request(0,v,1)&&bank.graph(0),"replacement overflow preserves active graph");bank.clearAll();}
 bank.releaseStopped(); // includes active, cancelled queued and retired owners
 check(bank.prepareStopped(8000),"release recovery");
 {AudioScope audio;check(bank.request(0,v,1),"release restores capacity");}
}
static void concurrent(){
 ArpDelayStorage bank;const auto v=arpModDefaults();check(bank.prepareStopped(8000),"concurrent bank prepare");
 std::atomic<bool> running{true};std::thread worker([&]{while(running.load(std::memory_order_acquire)){bank.serviceWorker();std::this_thread::yield();}});
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
 unsigned accepted=0,adopted=0;
 for(unsigned round=0;round<400;++round){
  {AudioScope audio;
   (void)bank.poll(round*64);
   const unsigned id=round%20;if(bank.graph(id))++adopted;
   if(round%3==0)bank.clear(id);else bank.cancel(id);
   if(bank.request(id,v,round))++accepted;
   if(round%7==0)bank.cancel(id);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  check(std::chrono::steady_clock::now()<deadline,"concurrent storage bounded progress");
 }
 {AudioScope audio;bank.clearAll();}
 running.store(false,std::memory_order_release);worker.join();
 bank.releaseStopped();check(accepted>100&&adopted>0,"concurrent handoff exercised adoption/cancellation/retirement");
}
static void cancellationInFlight(unsigned block,bool clear){
 ArpDelayStorage bank;auto v=arpModDefaults();v[0]=.01f;v[1]=0;check(bank.prepareStopped(8000),"in-flight bank prepare");
 {AudioScope audio;check(bank.request(0,v,11),"original in-flight owner");}
 bank.serviceWorker();
 {AudioScope audio;check(bank.poll(0).installed==1,"original owner installed");check(bank.request(0,v,22),"in-flight replacement admitted");}
 auto* old=bank.graph(0);
 allocationEntered.store(false,std::memory_order_relaxed);releaseAllocation.store(false,std::memory_order_relaxed);
 std::thread worker([&]{pauseNextAllocation=true;bank.serviceWorker();});
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 while(!allocationEntered.load(std::memory_order_acquire)){check(std::chrono::steady_clock::now()<deadline,"worker allocation barrier timeout");std::this_thread::yield();}
 {AudioScope audio;
  if(clear)bank.clear(0);else bank.cancel(0);
  check(bank.graph(0)==(clear?nullptr:old),"cancellation has the selected active-owner policy");
  check(bank.request(0,v,33),"new request admitted while obsolete allocation is in progress");
  check(!bank.poll(100).installed,"partially allocated graph is never visible");
 }
 releaseAllocation.store(true,std::memory_order_release);worker.join();
 {AudioScope audio;const auto done=bank.poll(200);check(!done.installed&&!done.failed&&bank.view(0).pending,"obsolete completed allocation cannot adopt or consume new request");
  check(bank.graph(0)==(clear?nullptr:old),"obsolete completed allocation cannot revive or replace active graph");}
 bank.serviceWorker();
 {AudioScope audio;check(bank.poll(385).installed==1,"fresh request adopts at its actual audio clock");}
 ArpDelayGraph expected;check(expected.prepare(8000,v,33,385),"fresh in-flight oracle prepare");samePcm(*bank.graph(0),expected,block);
}
static void loopWorker(unsigned block){
 auto bank=std::make_unique<LoopBank>();bank->prepare(8192,0,8000);auto values=arpModDefaults();values[0]=.01f;
 {AudioScope audio;check(!bank->requestArpStorage(0,values,1,99),"old clear epoch rejected before reservation");
  check(bank->requestArpStorage(0,values,17,0)&&bank->requestArpStorage(19,values,18,0),"existing LoopBank worker requests");}
 auto wait=[&](unsigned id){
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(!bank->preparedArp(id)){
   {AudioScope audio;bank->beginBlock();for(unsigned i=0;i<block;++i)(void)bank->tick({});}
   check(std::chrono::steady_clock::now()<deadline,"existing worker storage completion timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
 };
 wait(0);wait(19);
 {AudioScope audio;auto* first=bank->preparedArp(0);check(bank->apply(LoopAction::play,0)==false,"empty PLAY unchanged");
  check(bank->preparedArp(0)==first,"transport cannot recreate persistent storage");
  check(bank->requestArpStorage(0,values,99,0),"clear pending request");check(bank->apply(LoopAction::clear,0),"DEL clears storage");
  check(!bank->preparedArp(0)&&!bank->arpStorageView(0).pending&&bank->preparedArp(19),"DEL is scoped and cancels pending replacement");
  check(!bank->requestArpStorage(0,values,99,0)&&bank->requestArpStorage(0,values,99,1),"DEL epoch rejects stale work and accepts fresh request");}
 wait(0);
 {AudioScope audio;bank->resetTimeline();check(!bank->preparedArp(0)&&!bank->preparedArp(19),"timeline reset retires all graphs");}
 for(unsigned n=0;n<16;++n){AudioScope audio;bank->beginBlock();for(unsigned i=0;i<block;++i)(void)bank->tick({});check(!bank->preparedArp(0),"reset cannot resurrect late result");}
 bank->prepare(8192,0,48000);
 {AudioScope audio;check(bank->requestArpStorage(19,values,4,0),"LoopBank rate-change admission");}
 wait(19);check(bank->preparedArp(19)->targets()==values,"new-rate worker values preserved");
 {AudioScope audio;check(bank->apply(LoopAction::clearAll,0)&&!bank->preparedArp(19),"DEL ALL retires storage");}
 // Destruction while worker work is outstanding joins before releasing slots.
 {AudioScope audio;for(unsigned id=0;id<20;++id)check(bank->requestArpStorage(id,values,id,1),"shutdown pending request");}
}
int main(){
 for(unsigned block:{64u,128u}){admission(block);cancellationInFlight(block,false);cancellationInFlight(block,true);loopWorker(block);}
 saturation();concurrent();
 std::cout<<"PASS Arp storage: 20 tracks, exact graph PCM, worker OOM rollback, cancellation/stale results, fixed overflow/recovery, DEL/reset/rate/shutdown, concurrent ownership and zero audio new/delete at 64/128\n";
}
