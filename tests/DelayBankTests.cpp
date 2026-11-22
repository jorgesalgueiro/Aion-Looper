// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>
namespace {
thread_local bool watched=false;thread_local unsigned operations=0;
std::atomic<bool> failNext{false},entered{false},resume{false};
std::atomic<unsigned> pauseAfter{0};
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct AudioScope {AudioScope(){operations=0;watched=true;}~AudioScope(){watched=false;check(!operations,"heap operation on bank audio owner");}};
}
void* operator new(std::size_t n){if(watched)++operations;if(failNext.exchange(false))throw std::bad_alloc();
 auto countdown=pauseAfter.load();if(countdown&&pauseAfter.fetch_sub(1)==1){entered.store(true,std::memory_order_release);while(!resume.load(std::memory_order_acquire))std::this_thread::yield();}
 if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watched&&p)++operations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace aion;
static DelayStorage::Bank enabledBank(){DelayStorage::Bank bank{};
 for(unsigned id=0;id<20;++id){bank[id].enabled=true;bank[id].values[0]=.01;bank[id].values[3]=.7;bank[id].values[6]=1;}return bank;}
static void storage(){
 DelayStorage bank;check(bank.prepareStopped(8000),"storage bank prepare");const auto original=enabledBank();
 auto bad=original;bad[19].values[3]=NAN;
 {AudioScope guard;check(!bank.requestBank(bad,90),"invalid final member rejects entire bank");}
 failNext=true;bank.serviceWorker();check(failNext.exchange(false),"invalid bank published no allocation prefix");
 {AudioScope guard;check(bank.requestBank(original,90)&&!bank.request(0,original[0],90),"bank excludes individual preparation");}
 for(unsigned n=0;n<19;++n)bank.serviceWorker();
 {AudioScope guard;check(!bank.poll().bankInstalled,"nineteen ready members cannot commit prefix");for(unsigned id=0;id<20;++id)check(!bank.hasOwner(id),"no partial initial owner");}
 bank.serviceWorker();{AudioScope guard;check(bank.poll().bankInstalled,"all twenty commit together");}
 auto models=std::make_unique<std::array<Delay,20>>();
 for(unsigned id=0;id<20;++id)check((*models)[id].prepare(8000,original[id],90),"independent old bank");
 {AudioScope guard;for(unsigned n=0;n<160;++n)for(unsigned id=0;id<20;++id){float l,r,a,b;bank.tick(id,0,0,l,r);(*models)[id].tick(0,0,a,b);}}
 unsigned sample=0;
 const auto compare=[&](unsigned frames){AudioScope guard;
  for(unsigned n=0;n<frames;++n,++sample)for(unsigned id=0;id<20;++id){
   const float input=static_cast<float>(.1*std::sin(sample*.073+id*.1));float l,r,a,b;
   bank.tick(id,input,-.5f*input,l,r);(*models)[id].tick(input,-.5f*input,a,b);
   check(l==a&&r==b,"old audible bank unchanged sample-for-sample");
  }
 };
 compare(256);auto replacement=original;for(auto& settings:replacement)check(setDelayRepeats(settings,DelayRepeatMode::exact,3),"replacement exact settings");
 {AudioScope guard;check(bank.requestBank(replacement,90),"full forty-slot replacement admission");}
 bank.serviceWorker();bank.serviceWorker();
 {AudioScope guard;const auto partial=bank.poll();check(!partial.bankInstalled&&!partial.bankFailed,"ready replacement prefix cannot adopt any owner");
  for(unsigned id=0;id<20;++id)check(bank.graph(id)->settings()==original[id],"partial preparation retains all old graph targets");}
 compare(64);
 failNext=true;bank.serviceWorker();check(!failNext.load(),"bank worker OOM injected after partial preparation");
 {AudioScope guard;const auto result=bank.poll();check(result.bankFailed&&!result.bankInstalled,"one failure rolls back complete bank");
  for(unsigned id=0;id<20;++id)check(bank.graph(id)&&bank.graph(id)->settings()==original[id],"OOM retains all old target owners");}
 compare(256);
 for(unsigned n=0;n<40;++n){bank.serviceWorker();AudioScope guard;(void)bank.poll();}
 auto off=replacement;for(auto& settings:off)settings.enabled=false;
 {AudioScope guard;check(bank.requestBank(off,90)&&bank.poll().bankInstalled,"all-off bank needs no worker allocation");
  for(unsigned n=0;n<160;++n,++sample)for(unsigned id=0;id<20;++id){
   const float input=static_cast<float>(.1*std::sin(sample*.073+id*.1));float l,r,a,b;
   (*models)[id].tick(input,-.5f*input,a,b);bank.tick(id,input,-.5f*input,l,r);const double blend=n/160.;
   check(std::abs(l-(a+blend*(double(input)-a)))<1.e-7&&std::abs(r-(b+blend*(-.5*input-b)))<1.e-7,"bank off old-to-dry PCM transition");
  }
  for(unsigned id=0;id<20;++id)check(!bank.hasOwner(id)&&!bank.view(id).transitioning,"all-off fades retire all old leases");
 }bank.serviceWorker();
 const auto bytes=Delay::requiredStorageFrames(8000,original[0])*sizeof(std::array<float,2>);
 DelayStorage limited(bytes*20);check(limited.prepareStopped(8000),"bank budget prepare");
 {AudioScope guard;check(limited.requestBank(original,90),"exact initial budget fits");}for(unsigned n=0;n<20;++n)limited.serviceWorker();
 {AudioScope guard;check(limited.poll().bankInstalled,"budget initial adoption");
  for(unsigned n=0;n<160;++n)for(unsigned id=0;id<20;++id){float l,r;limited.tick(id,0,0,l,r);}
  check(!limited.requestBank(original,90),"aggregate active plus staging budget rejects before prefix");
  check(limited.requestBank(off,90)&&limited.poll().bankInstalled,"off bank can release constrained budget");
  for(unsigned n=0;n<160;++n)for(unsigned id=0;id<20;++id){float l,r;limited.tick(id,0,0,l,r);}
  check(!limited.requestBank(original,90),"unreclaimed bank bytes stay reserved after fade");
 }limited.serviceWorker();{AudioScope guard;check(limited.requestBank(original,90),"worker reclamation restores whole-bank capacity");limited.cancelBank();}
 std::cout<<"PASS atomic storage: twenty members, partial OOM PCM rollback, all-off fade, aggregate budget and retirement\n";
}
struct Fixture {
 std::unique_ptr<Engine> engine=std::make_unique<Engine>();unsigned block;std::array<float,128> l{},r{};
 explicit Fixture(unsigned size):block(size){engine->prepare(8000,16384);engine->setMonitoring(false);}
 void pump(bool zero=false,std::span<const MidiEvent> events={}){float* channels[]{l.data(),r.data()};AudioScope guard;engine->process(zero?nullptr:channels,zero?0:2,zero?0:static_cast<int>(block),events);}
 DelayBankRequest request(unsigned id,bool enabled){DelayBankRequest request;request.requestId=id;request.generation=engine->generation();
  for(unsigned track=0;track<20;++track){const auto info=engine->inspectAudioThread(track).track;
   request.clearEpochs[track]=info.clearEpoch;request.settingsRevisions[track]=info.delaySettingsRevision;
   request.settings[track].values[0]=.01;request.settings[track].enabled=enabled&&(track==0||track==19);
  }return request;
 }
 std::uint64_t enqueue(DelayBankRequest request){std::uint64_t transaction=0;check(engine->enqueueDelayBank(request,transaction)&&transaction,"bank enqueue");return transaction;}
 DelayBankResult result(){DelayBankResult result;const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(!engine->popDelayBank(result)){pump();check(std::chrono::steady_clock::now()<deadline,"bank receipt timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}return result;
 }
 DelayBankResult expect(unsigned id,std::uint64_t transaction,DelayEditStatus status){const auto r=result();check(r.requestId==id&&r.transaction==transaction&&r.status==status,"exact bank terminal identity/status");return r;}
 void settle(){for(unsigned n=0;n<512/block+1;++n)pump();}
 void same(const LoopBank::DelayBank& targets){for(unsigned track=0;track<20;++track)check(engine->inspectAudioThread(track).track.delay==targets[track],"all bank targets coherent");}
};
static void protocol(unsigned block){
 Fixture f(block);auto request=f.request(1,true);auto bad=request;bad.settings[19].repeatCount=0;std::uint64_t unchanged=77;
 check(!f.engine->enqueueDelayBank(bad,unchanged)&&unchanged==77,"last invalid member atomic message rejection");
 for(unsigned n=0;n<255;++n)check(f.engine->enqueueLoop({LoopAction::volume,0,1000+n,f.engine->generation()}),"fill ordinary FIFO before bank");
 check(!f.engine->enqueueDelayBank(request,unchanged)&&unchanged==77,"full FIFO rejects bank before descriptor/bound mutation");f.pump();f.pump();
 DelayBankResult absent;check(!f.engine->popDelayBank(absent),"failed bank admission leaves no orphan result");
 LoopCommand single{LoopAction::delay,1,91,f.engine->generation()};std::uint64_t singleTransaction=0;
 check(f.engine->enqueueDelayEdit(single,singleTransaction),"individual receipt before bank");f.pump();
 check(!f.engine->enqueueDelayBank(request,unchanged),"unread individual receipt excludes bank");
 DelayEditResult singleResult;check(f.engine->popDelayEdit(singleResult)&&singleResult.status==DelayEditStatus::ready,"consume individual receipt");
 request=f.request(1,true);
 const auto transaction=f.enqueue(request);LoopCommand individual{LoopAction::delay,0,90,f.engine->generation()};
 check(!f.engine->enqueueDelayEdit(individual,unchanged)&&!f.engine->enqueueDelayBank(request,unchanged),"bank excludes other edits and banks");
 f.expect(1,transaction,DelayEditStatus::ready);f.same(request.settings);
 auto busy=f.request(80,false);const auto busyToken=f.enqueue(busy);f.expect(80,busyToken,DelayEditStatus::busy);f.same(request.settings);
 auto stale=f.request(81,false);stale.clearEpochs[19]++;const auto staleToken=f.enqueue(stale);f.expect(81,staleToken,DelayEditStatus::stale);f.same(request.settings);f.settle();
 auto failing=f.request(90,true);for(unsigned id:{0u,19u})setDelayRepeats(failing.settings[id],DelayRepeatMode::exact,3);
 failNext=true;const auto failed=f.enqueue(failing);f.expect(90,failed,DelayEditStatus::failed);
 check(!failNext.load(),"Engine bank worker OOM consumed");f.same(request.settings);
 request=f.request(2,false);request.frame=f.engine->clock()+17;const auto zero=f.enqueue(request);
 f.pump();const auto old=f.engine->inspectAudioThread().track;check(old.delay.enabled,"all-off does not commit at FIFO sample seventeen");
 const auto off=f.result();check(off.status==DelayEditStatus::ready&&off.transaction==zero&&off.frame%block==0&&off.frame>request.frame,"all-off bank next-boundary commit");
 f.same(request.settings);f.settle();
 // Future bank cancellation and same numeric ID reuse cannot strand the FIFO.
 request=f.request(3,true);request.frame=f.engine->clock()+100000;const auto obsolete=f.enqueue(request);
 f.engine->cancelDelayEdit(19);f.pump(true);f.expect(3,obsolete,DelayEditStatus::cancelled);
 request=f.request(3,true);const auto fresh=f.enqueue(request);check(fresh>obsolete,"bank transaction not reused");f.expect(3,fresh,DelayEditStatus::ready);f.settle();
 request=f.request(4,false);const auto committed=f.enqueue(request);f.pump();f.pump();
 check(!f.engine->enqueueDelayEdit(individual,unchanged),"unread bank receipt retains exclusion");
 const auto oldGeneration=f.engine->generation();f.engine->prepare(96000,16384);
 const auto preserved=f.result();check(preserved.transaction==committed&&preserved.generation==oldGeneration&&preserved.status==DelayEditStatus::ready,"unread success immutable across prepare");
 for(unsigned id=0;id<20;++id)check(preserved.settingsRevisions[id]==request.settingsRevisions[id]+1&&preserved.clearEpochs[id]==0,"bank captures every old revision/epoch");
 request=f.request(5,false);const auto queued=f.enqueue(request);f.engine->prepare(48000,16384);f.expect(5,queued,DelayEditStatus::cancelled);
 request=f.request(6,false);const auto recovered=f.enqueue(request);f.expect(6,recovered,DelayEditStatus::ready);
 std::cout<<"PASS Engine bank "<<block<<": ordered all-off, exclusions, future cancellation, reused IDs and unread/queued prepare ownership\n";
}
static void paused(unsigned block,unsigned barrier){
 Fixture f(block);auto request=f.request(10,true);const auto initial=f.enqueue(request);f.expect(10,initial,DelayEditStatus::ready);f.settle();const auto old=request.settings;
 request=f.request(11,true);for(unsigned id:{0u,19u})check(setDelayRepeats(request.settings[id],DelayRepeatMode::exact,3),"paused bank exact settings");
 entered=false;resume=false;pauseAfter=2;const auto transaction=f.enqueue(request);f.pump();
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 while(!entered.load(std::memory_order_acquire)){check(std::chrono::steady_clock::now()<deadline,"second bank allocation pause");std::this_thread::yield();}
 f.pump();f.same(old); // first new owner is ready; second is still allocating
 DelayBankResult result;check(!f.engine->popDelayBank(result),"no ready prefix receipt");
 auto expected=old;
 if(barrier==1){
  f.engine->cancelDelayEdit(19);f.pump(true);f.expect(11,transaction,DelayEditStatus::cancelled);f.same(old);
  // Reuse the numeric ID while obsolete allocation is still paused. Only its
  // new transaction may finish, and it must wait for its own two owners.
  auto fresh=f.request(11,true);for(unsigned id:{0u,19u})setDelayRepeats(fresh.settings[id],DelayRepeatMode::exact,2);
  const auto next=f.enqueue(fresh);check(next>transaction,"cancelled bank ID reuse has a fresh token");f.pump();
  check(!f.engine->popDelayBank(result),"obsolete prepared prefix cannot complete fresh bank");
  resume.store(true,std::memory_order_release);f.expect(11,next,DelayEditStatus::ready);f.same(fresh.settings);return;
 }
 if(barrier==2||barrier==3){const auto action=barrier==2?LoopAction::clear:LoopAction::stopAll;
  check(f.engine->enqueueLoop({action,19,901,f.engine->generation()}),"queued bank barrier");f.pump();
  if(barrier==2)expected[19]={};
  f.expect(11,transaction,DelayEditStatus::cancelled);
 }
 if(barrier==4){MidiSettings midi;midi.enabled=true;check(f.engine->configureMidi(midi),"bank MIDI Stop configuration");
  const MidiEvent stop{0,252,0,0,1};f.pump(false,{&stop,1});f.expect(11,transaction,DelayEditStatus::cancelled);
 }
 if(barrier==5){const auto generation=f.engine->generation();resume.store(true,std::memory_order_release);
  f.engine->prepare(96000,16384);const auto cancelled=f.expect(11,transaction,DelayEditStatus::cancelled);
  check(cancelled.generation==generation,"paused bank prepare retains old result generation");expected={};
 }
 f.same(expected);
 resume.store(true,std::memory_order_release);
 if(barrier==0){f.expect(11,transaction,DelayEditStatus::ready);f.same(request.settings);}
 else {for(unsigned n=0;n<32;++n){f.pump();std::this_thread::sleep_for(std::chrono::milliseconds(1));}f.same(expected);check(!f.engine->popDelayBank(result),"cancelled bank cannot resurrect");}
}
int main(){storage();for(unsigned block:{64u,128u}){protocol(block);for(unsigned barrier=0;barrier<6;++barrier)paused(block,barrier);}}
