// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"
#include "core/ReverbImpulse.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>
namespace {
thread_local bool watched=false;thread_local unsigned operations=0;
std::atomic<bool> failNext{false};
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct AudioScope {AudioScope(){operations=0;watched=true;}~AudioScope(){watched=false;check(!operations,"heap operation on reverb bank audio owner");}};
}
void* operator new(std::size_t n){if(watched)++operations;if(failNext.exchange(false))throw std::bad_alloc();
 if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watched&&p)++operations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace aion;
static ReverbStorage::Bank enabledBank(){
 ReverbStorage::Bank bank{};for(unsigned id=0;id<20;++id){bank[id].enabled=true;bank[id].values[2]=.1;bank[id].values[1]=.5;}return bank;
}
static void storage(){
 ReverbStorage bank;check(bank.prepareStopped(8000),"storage bank prepare");const auto original=enabledBank();
 auto bad=original;bad[19].values[2]=.05;
 {AudioScope guard;check(!bank.requestBank(bad),"invalid final member rejects entire bank");}
 failNext=true;bank.serviceWorker();check(failNext.exchange(false),"invalid bank published no allocation prefix");
 {AudioScope guard;check(bank.requestBank(original)&&!bank.request(0,original[0],0),"bank excludes individual preparation");}
 for(unsigned n=0;n<19;++n)bank.serviceWorker();
 {AudioScope guard;check(!bank.poll().bankInstalled,"nineteen ready members cannot commit prefix");for(unsigned id=0;id<20;++id)check(!bank.hasOwner(id),"no partial initial owner");}
 bank.serviceWorker();{AudioScope guard;check(bank.poll().bankInstalled,"all twenty commit together");}
 auto models=std::make_unique<std::array<Reverb,20>>();
 for(unsigned id=0;id<20;++id){ReverbImpulse ir;check(ir.prepare(8000,original[id],0),"independent IR");check((*models)[id].prepare(8000,original[id],ir.channel(0),ir.channel(1)),"independent graph");}
 {AudioScope guard;for(unsigned n=0;n<160;++n)for(unsigned id=0;id<20;++id){float l,r,a,b;bank.tick(id,0,0,l,r);(*models)[id].tick(0,0,a,b);}}
 unsigned sample=0;
 const auto compare=[&](unsigned frames){AudioScope guard;
  for(unsigned n=0;n<frames;++n,++sample)for(unsigned id=0;id<20;++id){
   const float input=static_cast<float>(.1*std::sin(sample*.073+id*.1));float l,r,a,b;
   bank.tick(id,input,-.5f*input,l,r);(*models)[id].tick(input,-.5f*input,a,b);
   check(l==a&&r==b,"old audible bank unchanged sample-for-sample");
  }
 };
 compare(256);
 auto replacement=original;for(auto& settings:replacement)settings.values[2]=.2;
 {AudioScope guard;check(bank.requestBank(replacement),"replacement admission");}
 for(unsigned n=0;n<19;++n)bank.serviceWorker();
 {AudioScope guard;const auto partial=bank.poll();check(!partial.bankInstalled&&!partial.bankFailed,"ready replacement prefix cannot adopt any owner");
  for(unsigned id=0;id<20;++id)check(bank.graph(id)&&bank.graph(id)->settings()==original[id],"partial preparation retains all old graph targets");}
 compare(64);
 failNext=true;bank.serviceWorker();check(!failNext.load(),"bank worker OOM injected after partial preparation");
 {AudioScope guard;const auto result=bank.poll();check(result.bankFailed&&!result.bankInstalled,"one failure rolls back complete bank");
  for(unsigned id=0;id<20;++id)check(bank.graph(id)&&bank.graph(id)->settings()==original[id],"OOM retains all old target owners");}
 compare(256);
 for(unsigned n=0;n<40;++n){bank.serviceWorker();AudioScope guard;(void)bank.poll();}
 auto off=replacement;for(auto& settings:off)settings.enabled=false;
 {AudioScope guard;check(bank.requestBank(off)&&bank.poll().bankInstalled,"all-off bank needs no worker allocation");
  for(unsigned n=0;n<160;++n,++sample)for(unsigned id=0;id<20;++id){
   const float input=static_cast<float>(.1*std::sin(sample*.073+id*.1));float l,r,a,b;
   (*models)[id].tick(input,-.5f*input,a,b);bank.tick(id,input,-.5f*input,l,r);const double blend=n/160.;
   check(std::abs(l-(a+blend*(double(input)-a)))<1.e-7&&std::abs(r-(b+blend*(-.5*input-b)))<1.e-7,"bank off old-to-dry PCM transition");
  }
  for(unsigned id=0;id<20;++id)check(!bank.hasOwner(id)&&!bank.view(id).transitioning,"all-off fades retire all old leases");
 }bank.serviceWorker();
 const auto bytes=ReverbStorage::reservationBytes(8000,original[0]);
 ReverbStorage limited(bytes*20-1);check(limited.prepareStopped(8000),"bank budget prepare");
 {AudioScope guard;check(!limited.requestBank(enabledBank()),"aggregate twenty reservations exceed exact one-track budget");
  check(limited.requestBank(off),"off bank needs no storage budget");}
 std::cout<<"PASS atomic reverb storage: twenty members, partial OOM PCM rollback, all-off fade and aggregate budget\n";
}
struct Fixture {
 std::unique_ptr<Engine> engine=std::make_unique<Engine>();unsigned block;std::array<float,128> l{},r{};
 explicit Fixture(unsigned size):block(size){engine->prepare(8000,16384);engine->setMonitoring(false);}
 void pump(bool zero=false,std::span<const MidiEvent> events={}){float* channels[]{l.data(),r.data()};AudioScope guard;engine->process(zero?nullptr:channels,zero?0:2,zero?0:static_cast<int>(block),events);}
 ReverbBankRequest request(unsigned id,bool enabled){ReverbBankRequest request;request.requestId=id;request.generation=engine->generation();
  for(unsigned track=0;track<20;++track){const auto info=engine->inspectAudioThread(track).track;
   request.clearEpochs[track]=info.clearEpoch;request.settingsRevisions[track]=info.reverbSettingsRevision;
   request.settings[track].values[2]=.1;request.settings[track].enabled=enabled&&(track==0||track==19);
  }return request;
 }
 std::uint64_t enqueue(ReverbBankRequest request){std::uint64_t transaction=0;check(engine->enqueueReverbBank(request,transaction)&&transaction,"bank enqueue");return transaction;}
 ReverbBankResult result(){ReverbBankResult result;const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(!engine->popReverbBank(result)){pump();check(std::chrono::steady_clock::now()<deadline,"bank receipt timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}return result;
 }
 ReverbBankResult expect(unsigned id,std::uint64_t transaction,ReverbEditStatus status){const auto r=result();check(r.requestId==id&&r.transaction==transaction&&r.status==status,"exact bank terminal identity/status");return r;}
 void settle(){for(unsigned n=0;n<512/block+1;++n)pump();}
 void same(const LoopBank::ReverbBank& targets){for(unsigned track=0;track<20;++track)check(engine->inspectAudioThread(track).track.reverb==targets[track],"all bank targets coherent");}
};
static void protocol(unsigned block){
 Fixture f(block);auto request=f.request(1,true);auto bad=request;bad.settings[19].values[2]=.05;std::uint64_t unchanged=77;
 check(!f.engine->enqueueReverbBank(bad,unchanged)&&unchanged==77,"last invalid member atomic message rejection");
 for(unsigned n=0;n<255;++n)check(f.engine->enqueueLoop({LoopAction::volume,0,1000+n,f.engine->generation()}),"fill ordinary FIFO before bank");
 check(!f.engine->enqueueReverbBank(request,unchanged)&&unchanged==77,"full FIFO rejects bank before descriptor/bound mutation");f.pump();f.pump();
 ReverbBankResult absent;check(!f.engine->popReverbBank(absent),"failed bank admission leaves no orphan result");
 LoopCommand single{LoopAction::reverb,1,91,f.engine->generation()};single.reverb.values[2]=.1;
 std::uint64_t singleTransaction=0;check(f.engine->enqueueReverbEdit(single,singleTransaction),"individual receipt before bank");f.pump();
 check(!f.engine->enqueueReverbBank(request,unchanged),"unread individual receipt excludes bank");
 ReverbEditResult singleResult;check(f.engine->popReverbEdit(singleResult)&&singleResult.status==ReverbEditStatus::ready,"consume individual receipt");
 request=f.request(1,true);const auto transaction=f.enqueue(request);
 LoopCommand individual{LoopAction::reverb,0,90,f.engine->generation()};individual.reverb.enabled=true;individual.reverb.values[2]=.1;
 check(!f.engine->enqueueReverbEdit(individual,unchanged)&&!f.engine->enqueueReverbBank(request,unchanged),"bank excludes other edits and banks");
 f.expect(1,transaction,ReverbEditStatus::ready);f.same(request.settings);
 auto busy=f.request(80,false);const auto busyToken=f.enqueue(busy);f.expect(80,busyToken,ReverbEditStatus::busy);f.same(request.settings);
 auto stale=f.request(81,false);stale.clearEpochs[19]++;const auto staleToken=f.enqueue(stale);f.expect(81,staleToken,ReverbEditStatus::stale);f.same(request.settings);f.settle();
 auto cancelled=f.request(82,false);const auto cancelledToken=f.enqueue(cancelled);f.engine->cancelReverbBank();f.pump();
 const auto cancelledResult=f.expect(82,cancelledToken,ReverbEditStatus::cancelled);check(cancelledResult.clearEpochs[0]==f.request(0,false).clearEpochs[0],"cancelled bank retains epoch ownership");f.same(request.settings);
 request=f.request(2,false);request.frame=f.engine->clock()+17;const auto zero=f.enqueue(request);
 f.pump();check(f.engine->inspectAudioThread(0).track.reverb.enabled,"all-off does not commit at FIFO sample seventeen");
 const auto off=f.result();check(off.status==ReverbEditStatus::ready&&off.transaction==zero&&off.frame%block==0&&off.frame>request.frame,"all-off bank next-boundary commit");
 f.same(request.settings);f.settle();
 // An unread bank receipt retains the old generation across prepare; a bank
 // queued when prepare stops audio is cancelled with its own old identity.
 auto first=f.request(3,false);const auto firstToken=f.enqueue(first);
 for(unsigned n=0;n<4;++n){f.pump();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 const auto generation=f.engine->generation();
 f.engine->prepare(96000,16384);
 const auto old=f.expect(3,firstToken,ReverbEditStatus::ready);check(old.generation==generation,"prepare cannot rewrite bank generation");
 auto queued=f.request(4,false);const auto queuedToken=f.enqueue(queued);
 f.engine->prepare(96000,16384);
 const auto stopped=f.expect(4,queuedToken,ReverbEditStatus::cancelled);check(stopped.generation==queued.generation,"prepare cancels a queued bank with its own generation");
 auto next=f.request(5,false);const auto nextToken=f.enqueue(next);const auto recovered=f.expect(5,nextToken,ReverbEditStatus::ready);
 check(recovered.generation==f.engine->generation()&&recovered.generation!=generation&&nextToken>queuedToken,"new generation bank recovery without reused transaction");
}
int main(){storage();for(unsigned block:{64u,128u}){protocol(block);
 std::cout<<"PASS Engine reverb bank "<<block<<": atomic admission, unread bounds, busy/stale/cancel, all-off boundary and generation recovery; zero callback heap operations\n";}}
