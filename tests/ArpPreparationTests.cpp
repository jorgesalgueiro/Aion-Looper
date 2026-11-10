// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <thread>

namespace {
thread_local bool watch=false;
thread_local unsigned operations=0;
std::atomic<bool> failAllocation{false},pauseAllocation{false},allocationEntered{false},releaseAllocation{false};
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct AudioScope {AudioScope(){operations=0;watch=true;}~AudioScope(){watch=false;check(!operations,"audio new/delete in Arp preparation protocol");}};
}
void* operator new(std::size_t n){
 if(watch)++operations;
 if(failAllocation.exchange(false))throw std::bad_alloc();
 if(pauseAllocation.exchange(false)){allocationEntered.store(true,std::memory_order_release);while(!releaseAllocation.load(std::memory_order_acquire))std::this_thread::yield();}
 if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void operator delete(void* p) noexcept {if(watch&&p)++operations;std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
using namespace aion;
struct Fixture {
 std::unique_ptr<Engine> engine=std::make_unique<Engine>();unsigned block;
 std::array<float,128> l{},r{};
 explicit Fixture(unsigned frames):block(frames){engine->prepare(8000,8192);engine->setMonitoring(false);}
 void pump(bool zero=false){float* audio[]{l.data(),r.data()};AudioScope guard;engine->process(zero?nullptr:audio,zero?0:2,zero?0:static_cast<int>(block));}
 ArpPreparationRequest request(unsigned track,unsigned id,float mix=.5f){ArpPreparationRequest request;request.track=track;request.requestId=id;request.generation=engine->generation();request.seed=id;request.values[2]=mix;return request;}
 ArpPreparationResult result(){
  ArpPreparationResult result;const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  for(;;){if(engine->popArpPreparation(result))return result;pump();check(std::chrono::steady_clock::now()<deadline,"Arp terminal result timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 }
 void expect(unsigned track,unsigned id,unsigned generation,ArpPreparationStatus status){const auto out=result();check(out.track==track&&out.requestId==id&&out.generation==generation&&out.status==status,"exact correlated terminal result");}
 PreparedArpInspection inspection(unsigned id){AudioScope guard;return engine->inspectPreparedArpAudioThread(id);}
};
static bool same(const PreparedArpInspection& a,const PreparedArpInspection& b){return a.active==b.active&&a.targets==b.targets&&a.state==b.state&&a.frame==b.frame;}
static void transactions(unsigned block){
 Fixture f(block);const auto generation=f.engine->generation();auto request=f.request(19,1);
 {auto bad=request;bad.requestId=0;check(!f.engine->enqueueArpPreparation(bad),"zero correlation rejected");bad=request;bad.generation=0;check(!f.engine->enqueueArpPreparation(bad),"zero generation rejected");
  bad=request;bad.track=20;check(!f.engine->enqueueArpPreparation(bad),"invalid track rejected");bad=request;bad.values[0]=NAN;check(!f.engine->enqueueArpPreparation(bad),"nonfinite values rejected");}
 check(f.engine->enqueueArpPreparation(request),"expanded track request accepted");ArpPreparationResult out;
 check(!f.engine->popArpPreparation(out)&&!f.inspection(19).active,"enqueue is not terminal success or adoption");
 check(!f.engine->enqueueArpPreparation(request),"one outstanding per track before dispatch");
 check(f.engine->enqueueLoop({LoopAction::volume,0,700,generation}),"ordinary command alongside prepare");f.pump();
 EngineView view,last;while(f.engine->popView(view))last=view;check(last.lastRequestId==700&&!last.rejected,"Arp does not hijack ordinary command acknowledgement");
 f.expect(19,1,generation,ArpPreparationStatus::ready);const auto first=f.inspection(19);
 check(first.active&&first.targets==request.values&&first.frame%block==0&&first.frame<f.engine->clock(),"ready graph adopted at audio callback clock");
 request=f.request(19,2,.25f);request.clearEpoch=1;check(f.engine->enqueueArpPreparation(request),"stale epoch enters bounded audio validation");
 f.expect(19,2,generation,ArpPreparationStatus::stale);check(same(first,f.inspection(19)),"stale request preserves complete prior graph");
 request=f.request(19,3,.25f);check(f.engine->enqueueArpPreparation(request),"cancellable queued request");f.engine->cancelArpPreparation(19);
 f.expect(19,3,generation,ArpPreparationStatus::cancelled);check(same(first,f.inspection(19)),"pre-dispatch cancel preserves old graph");
 request=f.request(19,4,.25f);failAllocation.store(true);check(f.engine->enqueueArpPreparation(request),"OOM request accepted, not reported ready");
 f.expect(19,4,generation,ArpPreparationStatus::failed);check(!failAllocation.load()&&same(first,f.inspection(19)),"worker OOM preserves exact old graph and returns failure");
 request=f.request(19,5,.8f);check(f.engine->enqueueArpPreparation(request),"fresh request after failed prepare");f.expect(19,5,generation,ArpPreparationStatus::ready);
 check(f.inspection(19).targets==request.values,"fresh request recovers without old settings");
 // Old, unread terminal results survive prepare and retain old generation/ID.
 request=f.request(0,6);check(f.engine->enqueueArpPreparation(request),"queued before prepare");
 {AudioScope guard;f.engine->cancelArpPreparation(999);} // invalid target is a no-op
 f.engine->prepare(96000,8192);
 check(!f.engine->enqueueArpPreparation(f.request(0,7)),"unread cancelled receipt still owns per-track bound");
 f.expect(0,6,generation,ArpPreparationStatus::cancelled);check(!f.inspection(19).active,"prepare resets old graphs");
 request=f.request(0,7);check(f.engine->enqueueArpPreparation(request),"new generation after receipt consumed");
 f.expect(0,7,request.generation,ArpPreparationStatus::ready);check(f.inspection(0).active,"new rate/generation graph prepared");
 check(!f.engine->enqueueArpPreparation({0,8,generation}),"old generation rejected on message owner");
}
static void pausedWorker(unsigned block,LoopAction barrier){
 Fixture f(block);const auto generation=f.engine->generation();auto request=f.request(0,10,.1f);
 check(f.engine->enqueueArpPreparation(request),"initial owner before barrier");f.expect(0,10,generation,ArpPreparationStatus::ready);const auto original=f.inspection(0);
 allocationEntered.store(false);releaseAllocation.store(false);pauseAllocation.store(true);
 request=f.request(0,11,.2f);check(f.engine->enqueueArpPreparation(request),"paused replacement accepted");f.pump();
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 while(!allocationEntered.load(std::memory_order_acquire)){check(std::chrono::steady_clock::now()<deadline,"allocation barrier entry timeout");std::this_thread::yield();}
 if(barrier==LoopAction::volume)f.engine->cancelArpPreparation(0);
 else check(f.engine->enqueueLoop({barrier,0,801,generation}),"ordered clear/panic barrier");
 f.pump();f.expect(0,11,generation,ArpPreparationStatus::cancelled);
 const bool clear=barrier==LoopAction::clear||barrier==LoopAction::clearAll;
 check(clear?!f.inspection(0).active:same(original,f.inspection(0)),"barrier has correct existing-owner policy");
 // Replacement waits while the obsolete result is delivered. Correlation and
 // slot identity must prevent an obsolete success or a stolen fresh completion.
 request=f.request(0,12,.9f);request.clearEpoch=clear?1:0;
 check(f.engine->enqueueArpPreparation(request),"fresh request while obsolete allocation paused");f.pump();
 ArpPreparationResult result;check(!f.engine->popArpPreparation(result),"fresh request must wait for its own preparation");
 releaseAllocation.store(true,std::memory_order_release);
 f.expect(0,12,generation,ArpPreparationStatus::ready);check(f.inspection(0).targets==request.values,"only fresh result commits graph");
 for(unsigned n=0;n<4;++n)f.pump();check(!f.engine->popArpPreparation(result),"each accepted operation has exactly one terminal result");
}
static void boundedReceipts(unsigned block){
 Fixture f(block);const auto generation=f.engine->generation();
 for(unsigned id=0;id<20;++id)check(f.engine->enqueueArpPreparation(f.request(id,100+id)),"all twenty transactions accepted");
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 // Saturate the ordinary snapshots without consuming any terminal receipts.
 for(;;){f.pump();bool ready=true;for(unsigned id=0;id<20;++id)ready&=f.inspection(id).active;
  if(ready)break;check(std::chrono::steady_clock::now()<deadline,"all-track ready timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 for(unsigned id=0;id<20;++id)check(!f.engine->enqueueArpPreparation(f.request(id,200+id)),"unread receipt prevents unbounded result generation");
 for(unsigned n=0;n<24;++n)f.pump();
 std::array<bool,20> seen{};
 for(unsigned n=0;n<20;++n){ArpPreparationResult result;check(f.engine->popArpPreparation(result),"closed/slow consumer retains all twenty receipts");
  check(result.track<20&&!seen[result.track]&&result.requestId==100+result.track&&result.generation==generation&&result.status==ArpPreparationStatus::ready,"all-track receipt identity/status");seen[result.track]=true;}
 ArpPreparationResult extra;check(!f.engine->popArpPreparation(extra),"no duplicate terminal result");
 for(unsigned id=0;id<20;++id)check(f.engine->enqueueArpPreparation(f.request(id,200+id)),"all twenty slots recover after result consumption");
 f.engine->cancelArpPreparation();f.pump(true);
 for(unsigned n=0;n<20;++n){check(f.engine->popArpPreparation(extra)&&extra.status==ArpPreparationStatus::cancelled&&extra.requestId==200+extra.track,"global cancellation drains all queued requests without rendering");}
 const auto frame=f.engine->clock();check(f.engine->enqueueArpPreparation(f.request(19,300)),"zero-block preparation request");
 const auto zeroDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 while(!f.engine->popArpPreparation(extra)){f.pump(true);check(std::chrono::steady_clock::now()<zeroDeadline,"zero-block ready timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 check(extra.requestId==300&&extra.status==ArpPreparationStatus::ready&&f.engine->clock()==frame&&f.inspection(19).frame==frame,"zero-block storage service does not advance PCM clock");
}
static void receiptLifetime(unsigned block){
 Fixture f(block);const auto generation=f.engine->generation();
 check(f.engine->enqueueArpPreparation(f.request(0,400)),"unread ready lifetime request");
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 while(!f.inspection(0).active){f.pump();check(std::chrono::steady_clock::now()<deadline,"unread ready timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 const auto before=f.inspection(0);f.engine->cancelArpPreparation(0);f.pump();
 check(same(before,f.inspection(0)),"cancellation after commit cannot undo an already-ready owner");
 check(f.engine->enqueueArpPreparation(f.request(19,401)),"queued request alongside unread ready result");
 f.engine->prepare(8000,8192);f.engine->prepare(8000,8192);
 check(!f.engine->enqueueArpPreparation(f.request(0,402))&&!f.engine->enqueueArpPreparation(f.request(19,403)),"repeated prepare retains outstanding message ownership until read");
 f.expect(0,400,generation,ArpPreparationStatus::ready);f.expect(19,401,generation,ArpPreparationStatus::cancelled);
 ArpPreparationResult result;check(!f.engine->popArpPreparation(result)&&!f.inspection(0).active,"prepare cannot duplicate receipts or retain old graphs");
 auto request=f.request(0,402);check(f.engine->enqueueArpPreparation(request),"new-generation request after both receipts consumed");f.expect(0,402,request.generation,ArpPreparationStatus::ready);
}
static void exhaustedStorage(unsigned block){
 Fixture f(block);const auto generation=f.engine->generation();
 check(f.engine->enqueueArpPreparation(f.request(0,500,.1f)),"overflow original owner");f.expect(0,500,generation,ArpPreparationStatus::ready);const auto original=f.inspection(0);
 allocationEntered.store(false);releaseAllocation.store(false);pauseAllocation.store(true);
 check(f.engine->enqueueArpPreparation(f.request(0,501,.2f)),"overflow blocked allocation");f.pump(true);
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 while(!allocationEntered.load(std::memory_order_acquire)){check(std::chrono::steady_clock::now()<deadline,"overflow allocation barrier timeout");std::this_thread::yield();}
 // One active + thirty-nine cancelled requests exhaust the forty storage slots
 // while the worker is paused. Each receipt is consumed before the next request.
 for(unsigned n=0;n<ArpDelayStorage::capacity-1;++n){
  f.engine->cancelArpPreparation(0);f.pump(true);f.expect(0,501+n,generation,ArpPreparationStatus::cancelled);
  check(f.engine->enqueueArpPreparation(f.request(0,502+n,.8f)),"bounded message admission while storage fills");f.pump(true);
  if(n+1==ArpDelayStorage::capacity-1)f.expect(0,502+n,generation,ArpPreparationStatus::busy);
  else {ArpPreparationResult result;check(!f.engine->popArpPreparation(result),"worker-stalled storage is pending, never falsely ready");}
 }
 check(same(original,f.inspection(0)),"busy and cancelled receipts preserve complete old owner");
 releaseAllocation.store(true,std::memory_order_release);
 // Test-only bounded capacity probes: the application acquires no blind retry.
 const auto recovery=std::chrono::steady_clock::now()+std::chrono::seconds(5);unsigned id=600;
 for(;;){check(f.engine->enqueueArpPreparation(f.request(0,id,.8f)),"capacity recovery probe admission");const auto result=f.result();
  check(result.track==0&&result.requestId==id&&result.generation==generation,"capacity recovery receipt identity");
  if(result.status==ArpPreparationStatus::ready)break;
  check(result.status==ArpPreparationStatus::busy&&same(original,f.inspection(0)),"only explicit busy before worker reclamation");
  check(std::chrono::steady_clock::now()<recovery,"storage capacity recovery timeout");++id;std::this_thread::sleep_for(std::chrono::milliseconds(1));
 }
 check(f.inspection(0).targets[2]==.8f,"recovered preparation commits fresh settings");
}
static void concurrentMessages(){
 auto engine=std::make_unique<Engine>();engine->prepare(8000,8192);const auto generation=engine->generation();
 std::atomic<bool> running{true};
 std::thread audio([&]{while(running.load(std::memory_order_acquire)){{AudioScope guard;engine->process(nullptr,0,0);}std::this_thread::sleep_for(std::chrono::milliseconds(1));}});
 for(unsigned n=0;n<80;++n){
  ArpPreparationRequest request;request.track=n%20;request.requestId=1000+n;request.generation=generation;request.seed=n;request.values[2]=static_cast<float>(n%10)/10;
  check(engine->enqueueArpPreparation(request),"concurrent message enqueue");const bool cancel=n%3==0;if(cancel)engine->cancelArpPreparation(request.track);
  ArpPreparationResult result;const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(!engine->popArpPreparation(result)){check(std::chrono::steady_clock::now()<deadline,"concurrent message result timeout");std::this_thread::yield();}
  check(result.track==request.track&&result.requestId==request.requestId&&result.generation==generation,"concurrent receipt identity");
  check(result.status==ArpPreparationStatus::ready||(cancel&&result.status==ArpPreparationStatus::cancelled),"concurrent cancellation linearizes before or after ready commit");
 }
 running.store(false,std::memory_order_release);audio.join();ArpPreparationResult result;
 {AudioScope guard;engine->process(nullptr,0,0);}
 check(!engine->popArpPreparation(result),"concurrent transactions produce no duplicate results");
}
int main(){
 for(unsigned block:{64u,128u}){
  transactions(block);boundedReceipts(block);receiptLifetime(block);exhaustedStorage(block);
  for(auto barrier:{LoopAction::volume,LoopAction::clear,LoopAction::clearAll,LoopAction::stopAll})pausedWorker(block,barrier);
 }
 concurrentMessages();
 std::cout<<"PASS Engine Arp preparation: terminal IDs/generations/epochs, zero-block/64/128, queue-independent cancellation, clear/panic/in-flight stale results, OOM preservation/recovery, all-20 closed-consumer bounds, ordinary acknowledgements and zero callback new/delete\n";
}
