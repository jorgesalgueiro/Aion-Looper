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
std::atomic<bool> pauseNext{false},entered{false},resume{false};
void check(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
struct AudioScope {AudioScope(){operations=0;watched=true;}~AudioScope(){watched=false;check(!operations,"heap operation in Reverb Engine callback");}};
}
void* operator new(std::size_t n){if(watched)++operations;
 if(pauseNext.exchange(false)){entered.store(true,std::memory_order_release);while(!resume.load(std::memory_order_acquire))std::this_thread::yield();}
 if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watched&&p)++operations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace aion;
struct Fixture {
 std::unique_ptr<Engine> engine=std::make_unique<Engine>();unsigned block;
 std::array<float,128> l{},r{};
 explicit Fixture(unsigned size):block(size){engine->prepare(8000,16384);engine->setMonitoring(false);}
 void pump(bool zero=false,std::span<const MidiEvent> events={}){float* channels[]{l.data(),r.data()};AudioScope guard;
  engine->process(zero?nullptr:channels,zero?0:2,zero?0:static_cast<int>(block),events);}
 LoopView info(unsigned track=0){return engine->inspectAudioThread(track).track;}
 LoopCommand command(unsigned id,unsigned track=0,bool enabled=true,double duration=.1){const auto view=info(track);
  LoopCommand c{LoopAction::reverb,track,id,engine->generation()};c.reverb.enabled=enabled;c.reverb.values[2]=duration;
  c.reverbClearEpoch=view.clearEpoch;c.reverbSettingsRevision=view.reverbSettingsRevision;return c;
 }
 std::uint64_t enqueue(LoopCommand c){std::uint64_t transaction=0;check(engine->enqueueReverbEdit(c,transaction)&&transaction,"tracked enqueue");return transaction;}
 ReverbEditResult result(){const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);ReverbEditResult result;
  while(!engine->popReverbEdit(result)){pump();check(std::chrono::steady_clock::now()<deadline,"Reverb receipt timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  return result;
 }
 ReverbEditResult expect(unsigned id,std::uint64_t transaction,ReverbEditStatus status,unsigned track=0){const auto r=result();
  check(r.track==track&&r.requestId==id&&r.transaction==transaction&&r.status==status,"exact terminal correlation/status");return r;
 }
 void settle(){for(unsigned n=0;n<512/block+1;++n)pump();}
};
static void validationAndOrdering(unsigned block){
 Fixture f(block);auto c=f.command(1);std::uint64_t untouched=77;
 for(unsigned bad=0;bad<8;++bad){auto invalid=c;
  if(bad==0)invalid.requestId=0;if(bad==1)invalid.generation=0;if(bad==2)invalid.track=20;
  if(bad==3)invalid.reverb.values[3]=NAN;if(bad==4)invalid.reverb.preset=99;
  if(bad==5)invalid.reverbTransaction=1;if(bad==6)invalid.reverbSettingsRevision=UINT64_MAX;
  if(bad==7)invalid.generation=f.engine->generation()+1;
  check(!f.engine->enqueueReverbEdit(invalid,untouched)&&untouched==77,"invalid admission preserves output/queues");
 }
 check(!f.engine->enqueueLoop(c),"bare Reverb commands cannot bypass receipt protocol");
 const auto transaction=f.enqueue(c);check(!f.engine->enqueueReverbEdit(c,untouched),"one outstanding before dispatch");
 check(f.engine->enqueueLoop({LoopAction::volume,0,700,f.engine->generation()}),"ordinary alongside Reverb");
 // No prepared owner exists yet: the first enable defers its receipt to adoption.
 const auto ready=f.expect(1,transaction,ReverbEditStatus::ready);
 check(ready.generation==c.generation&&ready.settingsRevision==1&&!ready.clearEpoch&&ready.frame%block==0,"prepared receipt captures actual adoption ownership");
 EngineView view,last;while(f.engine->popView(view))last=view;
 check(last.lastRequestId==700&&!last.rejected,"Reverb never hijacks ordinary loop acknowledgements");
 check(f.info().reverb==c.reverb,"adopted settings are audio-owned");
 // The 20ms adoption fade still holds the storage; a second shape cannot prepare.
 auto busy=f.command(90,0,true,.2);const auto busyTransaction=f.enqueue(busy);
 const auto notAdmitted=f.expect(90,busyTransaction,ReverbEditStatus::busy);
 check(notAdmitted.settingsRevision==1&&f.info().reverb==c.reverb,"transition-busy receipt preserves current owner/settings");
 f.settle();const auto old=f.info();c=f.command(2);c.reverbClearEpoch++;
 const auto stale=f.enqueue(c);f.expect(2,stale,ReverbEditStatus::stale);
 check(f.info().reverb==old.reverb&&f.info().reverbSettingsRevision==old.reverbSettingsRevision,"stale epoch preserves active state");
 // Live same-shape edits commit in the ordered callback without a worker wait.
 c=f.command(3);c.reverb.values[1]=.31;c.frame=f.engine->clock()+17;const auto at=c.frame;
 const auto live=f.enqueue(c);const auto liveResult=f.expect(3,live,ReverbEditStatus::ready);
 check(liveResult.frame==at&&f.info().reverb==c.reverb,"live edit exact sample offset");
 // Disabling is live metadata too; no graph is allocated or destroyed.
 c=f.command(4,0,false,.2);const auto disabled=f.enqueue(c);const auto off=f.expect(4,disabled,ReverbEditStatus::ready);
 check(off.settingsRevision==old.reverbSettingsRevision+2&&!f.info().reverb.enabled,"disabled shape metadata commits without preparation");
 f.settle();
 // Re-enable with the new shape only through the worker.
 c=f.command(5,0,true,.2);const auto enable=f.enqueue(c);const auto on=f.expect(5,enable,ReverbEditStatus::ready);
 check(on.settingsRevision==off.settingsRevision+1&&f.info().reverb==c.reverb,"enabled shape change adopts a prepared graph");
 f.settle();
 // Cancel a far-future FIFO head, consume its result with zero samples, then
 // reuse the request ID. The old command must neither block nor complete it.
 c=f.command(6);c.frame=f.engine->clock()+100000;const auto obsolete=f.enqueue(c);
 f.engine->cancelReverbEdit(0);f.pump(true);f.expect(6,obsolete,ReverbEditStatus::cancelled);
 c=f.command(6);c.reverb.values[1]=.47;const auto fresh=f.enqueue(c);check(fresh>obsolete,"transaction identities never reused");
 f.expect(6,fresh,ReverbEditStatus::ready);check(f.info().reverb==c.reverb,"obsolete FIFO identity cannot steal fresh same-ID completion");
 // MIDI Stop purges a registered but still-future FIFO command too, without
 // turning its tracked request into an unrelated ordinary UI acknowledgement.
 c=f.command(7);c.frame=f.engine->clock()+100000;const auto midiCancelled=f.enqueue(c);
 MidiSettings midi;midi.enabled=true;check(f.engine->configureMidi(midi),"MIDI Stop configuration");
 const MidiEvent stop{0,252,0,0,1};f.pump(false,{&stop,1});f.expect(7,midiCancelled,ReverbEditStatus::cancelled);
 // Once a success is committed its receipt is immutable, even if DEL occurs
 // before the message owner reads it. The epoch lets that owner fence stale UI.
 c=f.command(8,19,false);const auto committed=f.enqueue(c);f.pump();
 check(f.engine->enqueueLoop({LoopAction::clear,19,901,f.engine->generation()}),"clear after committed unread success");f.pump();
 const auto beforeClear=f.expect(8,committed,ReverbEditStatus::ready,19);
 check(beforeClear.clearEpoch==0&&f.info(19).clearEpoch==1&&f.info(19).reverb==ReverbSettings{},"unread success retains commit ownership across later clear");
}
static void saturationAndLifetime(unsigned block){
 Fixture f(block);std::uint64_t value=91;
 for(unsigned n=0;n<255;++n)check(f.engine->enqueueLoop({LoopAction::volume,0,1000+n,f.engine->generation()}),"fill performance FIFO");
 auto c=f.command(1,0,false);check(!f.engine->enqueueReverbEdit(c,value)&&value==91,"full FIFO rejects before descriptor or outstanding mutation");
 f.pump();f.pump();ReverbEditResult extra;check(!f.engine->popReverbEdit(extra)&&f.info().reverbSettingsRevision==0,"failed admission leaves no orphan result/edit");
 c=f.command(2);c.frame=f.engine->clock()+100000;const auto blocked=f.enqueue(c);
 for(unsigned n=0;n<254;++n)check(f.engine->enqueueLoop({LoopAction::volume,0,2000+n,f.engine->generation()}),"saturate behind tracked future head");
 f.engine->cancelReverbEdit(0);f.pump(true);f.expect(2,blocked,ReverbEditStatus::cancelled);
 check(f.info().reverbSettingsRevision==0,"zero-block cancellation needs neither FIFO room nor a fresh snapshot");f.pump();f.pump();
 std::array<std::uint64_t,20> transactions{};
 for(unsigned id=0;id<20;++id)transactions[id]=f.enqueue(f.command(100+id,id,false));
 f.pump();for(unsigned n=0;n<24;++n)f.pump(); // Saturate lossy telemetry; keep all receipts unread.
 for(unsigned id=0;id<20;++id){auto next=f.command(200+id,id,false);check(!f.engine->enqueueReverbEdit(next,value),"unread receipt retains per-track bound");}
 std::array<bool,20> seen{};
 for(unsigned n=0;n<20;++n){check(f.engine->popReverbEdit(extra)&&extra.track<20&&!seen[extra.track],"all twenty unread results retained");
  check(extra.requestId==100+extra.track&&extra.transaction==transactions[extra.track]&&extra.status==ReverbEditStatus::ready,"slow consumer result identity");seen[extra.track]=true;}
 check(!f.engine->popReverbEdit(extra),"exactly one receipt per accepted edit");
 // Preserve both an already-ready unread receipt and a queued cancellation
 // through a sample-rate/generation change, with immutable old ownership.
 c=f.command(300,0,false);const auto first=f.enqueue(c);f.pump();
 c=f.command(301,19,false);const auto queued=f.enqueue(c);const auto generation=f.engine->generation();
 f.engine->prepare(96000,16384);
 check(!f.engine->enqueueReverbEdit(f.command(302,0,false),value),"prepare retains unread ownership bound");
 const auto ready=f.expect(300,first,ReverbEditStatus::ready);const auto cancelled=f.expect(301,queued,ReverbEditStatus::cancelled,19);
 check(ready.generation==generation&&cancelled.generation==generation,"prepare cannot rewrite old result generation/revision");
 c=f.command(302,0,false);const auto next=f.enqueue(c);const auto recovered=f.expect(302,next,ReverbEditStatus::ready);
 check(recovered.generation==f.engine->generation()&&recovered.generation!=generation&&next>queued,"new generation recovery without reused transaction");
}
static void cancellationBarriers(unsigned block,int barrier){
 Fixture f(block);auto c=f.command(1);const auto first=f.enqueue(c);f.expect(1,first,ReverbEditStatus::ready);f.settle();
 const auto original=f.info();
 // A paused worker allocation proves cancellation before adoption.
 entered=false;resume=false;pauseNext=true;c=f.command(2,0,true,.25);const auto transaction=f.enqueue(c);f.pump();
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 while(!entered.load(std::memory_order_acquire)){check(std::chrono::steady_clock::now()<deadline,"paused worker entry");std::this_thread::yield();}
 if(barrier==0){f.engine->cancelReverbEdit(0);f.pump(true);}
 else if(barrier==4){MidiSettings midi;midi.enabled=true;check(f.engine->configureMidi(midi),"synthetic MIDI configuration");const MidiEvent stop{0,252,0,0,1};f.pump(false,{&stop,1});}
 else {const auto action=barrier==1?LoopAction::stopAll:barrier==2?LoopAction::clear:LoopAction::clearAll;
  check(f.engine->enqueueLoop({action,0,900,f.engine->generation()}),"ordered barrier");f.pump();}
 f.expect(2,transaction,ReverbEditStatus::cancelled);
 const bool clear=barrier==2||barrier==3;
 check(clear?f.info().reverb==ReverbSettings{}:f.info().reverb==original.reverb,"barrier preserves/reset settings as specified");
 c=f.command(3,0,true,.35);const auto fresh=f.enqueue(c);f.pump();ReverbEditResult result;
 check(!f.engine->popReverbEdit(result),"fresh operation cannot use obsolete paused allocation");
 resume.store(true,std::memory_order_release);f.expect(3,fresh,ReverbEditStatus::ready);
 check(f.info().reverb==c.reverb,"only fresh prepared owner commits after cancellation");
}
int main(){for(unsigned block:{64u,128u}){
 validationAndOrdering(block);saturationAndLifetime(block);for(int barrier=0;barrier<5;++barrier)cancellationBarriers(block,barrier);
 std::cout<<"PASS Engine Reverb "<<block<<": ordered/sample-offset edits, worker adoption, unread receipts, FIFO bounds, generations, cancel/panic/DEL/MIDI Stop; zero tracked callback heap operations\n";
}}
