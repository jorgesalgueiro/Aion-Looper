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
std::atomic<bool> failNext{false},pauseNext{false},entered{false},resume{false};
void check(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
struct AudioScope {AudioScope(){operations=0;watched=true;}~AudioScope(){watched=false;check(!operations,"heap operation in Delay Engine callback");}};
}
void* operator new(std::size_t n){if(watched)++operations;if(failNext.exchange(false))throw std::bad_alloc();
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
 LoopCommand command(unsigned id,unsigned track=0,bool enabled=true){const auto view=info(track);
  LoopCommand c{LoopAction::delay,track,id,engine->generation()};c.delay.enabled=enabled;c.delay.values[0]=.01;
  c.delayClearEpoch=view.clearEpoch;c.delaySettingsRevision=view.delaySettingsRevision;return c;
 }
 std::uint64_t enqueue(LoopCommand c){std::uint64_t transaction=0;check(engine->enqueueDelayEdit(c,transaction)&&transaction,"tracked enqueue");return transaction;}
 DelayEditResult result(){const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);DelayEditResult result;
  while(!engine->popDelayEdit(result)){pump();check(std::chrono::steady_clock::now()<deadline,"Delay receipt timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  return result;
 }
 DelayEditResult expect(unsigned id,std::uint64_t transaction,DelayEditStatus status,unsigned track=0){const auto r=result();
  check(r.track==track&&r.requestId==id&&r.transaction==transaction&&r.status==status,"exact terminal correlation/status");return r;
 }
 void settle(){for(unsigned n=0;n<512/block+1;++n)pump();}
};
static void validationAndOrdering(unsigned block){
 Fixture f(block);auto c=f.command(1,19);std::uint64_t untouched=77;
 for(unsigned bad=0;bad<7;++bad){auto invalid=c;
  if(bad==0)invalid.requestId=0;if(bad==1)invalid.generation=0;if(bad==2)invalid.track=20;
  if(bad==3)invalid.delay.values[3]=NAN;if(bad==4)invalid.delay.repeatCount=11;
  if(bad==5)invalid.delayTransaction=1;if(bad==6)invalid.delaySettingsRevision=UINT64_MAX;
  check(!f.engine->enqueueDelayEdit(invalid,untouched)&&untouched==77,"invalid admission preserves output/queues");
 }
 check(!f.engine->enqueueLoop(c),"bare Delay commands cannot bypass receipt protocol");
 const auto transaction=f.enqueue(c);check(!f.engine->enqueueDelayEdit(c,untouched),"one outstanding before dispatch");
 check(f.engine->enqueueLoop({LoopAction::volume,0,700,f.engine->generation()}),"ordinary alongside Delay");
 const auto ready=f.expect(1,transaction,DelayEditStatus::ready,19);
 check(ready.generation==c.generation&&ready.settingsRevision==1&&!ready.clearEpoch&&ready.frame%block==0,"prepared receipt captures actual adoption ownership");
 EngineView view,last;while(f.engine->popView(view))last=view;
 check(last.lastRequestId==700&&!last.rejected,"Delay never hijacks ordinary loop acknowledgements");
 f.settle();const auto old=f.info(19);c=f.command(2,19);c.delayClearEpoch++;
 const auto stale=f.enqueue(c);f.expect(2,stale,DelayEditStatus::stale,19);
 check(f.info(19).delay==old.delay&&f.info(19).delaySettingsRevision==old.delaySettingsRevision,"stale epoch preserves active state");
 // Live metadata edits have an exact FIFO/sample deadline and no worker wait.
 c=f.command(3,0,false);c.delay.values[3]=.17;c.frame=f.engine->clock()+17;const auto at=c.frame;
 const auto live=f.enqueue(c);const auto liveResult=f.expect(3,live,DelayEditStatus::ready);
 check(liveResult.frame==at&&f.info().delay==c.delay,"live edit exact sample offset");
 // Cancel a far-future FIFO head, consume its result with zero samples, then
 // reuse the request ID. The old command must neither block nor complete it.
 c=f.command(4);c.frame=f.engine->clock()+100000;const auto obsolete=f.enqueue(c);
 f.engine->cancelDelayEdit(0);f.pump(true);f.expect(4,obsolete,DelayEditStatus::cancelled);
 c=f.command(4);c.delay.values[3]=.29;const auto fresh=f.enqueue(c);check(fresh>obsolete,"transaction identities never reused");
 f.expect(4,fresh,DelayEditStatus::ready);check(f.info().delay==c.delay,"obsolete FIFO identity cannot steal fresh same-ID completion");
 // MIDI Stop purges a registered but still-future FIFO command too, without
 // turning its tracked request into an unrelated ordinary UI acknowledgement.
 c=f.command(5);c.frame=f.engine->clock()+100000;const auto midiCancelled=f.enqueue(c);
 MidiSettings midi;midi.enabled=true;check(f.engine->configureMidi(midi),"MIDI Stop configuration");
 const MidiEvent stop{0,252,0,0,1};f.pump(false,{&stop,1});f.expect(5,midiCancelled,DelayEditStatus::cancelled);
 // Once a success is committed its receipt is immutable, even if DEL occurs
 // before the message owner reads it. The epoch lets that owner fence stale UI.
 c=f.command(6,19,false);const auto committed=f.enqueue(c);f.pump();
 check(f.engine->enqueueLoop({LoopAction::clear,19,901,f.engine->generation()}),"clear after committed unread success");f.pump();
 const auto beforeClear=f.expect(6,committed,DelayEditStatus::ready,19);
 check(beforeClear.clearEpoch==0&&beforeClear.settingsRevision==old.delaySettingsRevision+1
  &&f.info(19).clearEpoch==1&&f.info(19).delay==DelaySettings{},"unread success retains commit ownership across later clear");
}
static void saturationAndLifetime(unsigned block){
 Fixture f(block);std::uint64_t value=91;
 for(unsigned n=0;n<255;++n)check(f.engine->enqueueLoop({LoopAction::volume,0,1000+n,f.engine->generation()}),"fill performance FIFO");
 auto c=f.command(1,0,false);check(!f.engine->enqueueDelayEdit(c,value)&&value==91,"full FIFO rejects before descriptor or outstanding mutation");
 f.pump();f.pump();DelayEditResult extra;check(!f.engine->popDelayEdit(extra)&&f.info().delaySettingsRevision==0,"failed admission leaves no orphan result/edit");
 c=f.command(2);c.frame=f.engine->clock()+100000;const auto blocked=f.enqueue(c);
 for(unsigned n=0;n<254;++n)check(f.engine->enqueueLoop({LoopAction::volume,0,2000+n,f.engine->generation()}),"saturate behind tracked future head");
 f.engine->cancelDelayEdit(0);f.pump(true);f.expect(2,blocked,DelayEditStatus::cancelled);
 check(f.info().delaySettingsRevision==0,"zero-block cancellation needs neither FIFO room nor a fresh snapshot");f.pump();f.pump();
 std::array<std::uint64_t,20> transactions{};
 for(unsigned id=0;id<20;++id)transactions[id]=f.enqueue(f.command(100+id,id,false));
 f.pump();for(unsigned n=0;n<24;++n)f.pump(); // Saturate lossy telemetry; keep all receipts unread.
 for(unsigned id=0;id<20;++id){auto next=f.command(200+id,id,false);check(!f.engine->enqueueDelayEdit(next,value),"unread receipt retains per-track bound");}
 std::array<bool,20> seen{};
 for(unsigned n=0;n<20;++n){check(f.engine->popDelayEdit(extra)&&extra.track<20&&!seen[extra.track],"all twenty unread results retained");
  check(extra.requestId==100+extra.track&&extra.transaction==transactions[extra.track]&&extra.status==DelayEditStatus::ready,"slow consumer result identity");seen[extra.track]=true;}
 check(!f.engine->popDelayEdit(extra),"exactly one receipt per accepted edit");
 // Preserve both an already-ready unread receipt and a queued cancellation
 // through a sample-rate/generation change, with immutable old ownership.
 c=f.command(300,0,false);const auto first=f.enqueue(c);f.pump();
 c=f.command(301,19,false);const auto queued=f.enqueue(c);const auto generation=f.engine->generation();
 f.engine->prepare(96000,16384);
 check(!f.engine->enqueueDelayEdit(f.command(302,0,false),value),"prepare retains unread ownership bound");
 const auto ready=f.expect(300,first,DelayEditStatus::ready);const auto cancelled=f.expect(301,queued,DelayEditStatus::cancelled,19);
 check(ready.generation==generation&&ready.settingsRevision==2&&cancelled.generation==generation,"prepare cannot rewrite old result generation/revision");
 c=f.command(302,0,false);const auto next=f.enqueue(c);const auto recovered=f.expect(302,next,DelayEditStatus::ready);
 check(recovered.generation==f.engine->generation()&&recovered.generation!=generation&&next>queued,"new generation recovery without reused transaction");
}
static void failureAndBarriers(unsigned block,int barrier){
 Fixture f(block);auto c=f.command(1);auto transaction=f.enqueue(c);f.expect(1,transaction,DelayEditStatus::ready);f.settle();
 const auto original=f.info();
 c=f.command(2);check(setDelayRepeats(c.delay,DelayRepeatMode::exact,3),"OOM exact settings");
 failNext=true;transaction=f.enqueue(c);f.expect(2,transaction,DelayEditStatus::failed);
 check(!failNext.load()&&f.info().delay==original.delay&&f.info().delaySettingsRevision==original.delaySettingsRevision,"worker OOM reports failure and preserves active settings");
 entered=false;resume=false;pauseNext=true;c=f.command(3);setDelayRepeats(c.delay,DelayRepeatMode::exact,3);transaction=f.enqueue(c);f.pump();
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 while(!entered.load(std::memory_order_acquire)){check(std::chrono::steady_clock::now()<deadline,"paused worker entry");std::this_thread::yield();}
 if(barrier==0){f.engine->cancelDelayEdit(0);f.pump(true);}
 else if(barrier==4){MidiSettings midi;midi.enabled=true;check(f.engine->configureMidi(midi),"synthetic MIDI configuration");const MidiEvent stop{0,252,0,0,1};f.pump(false,{&stop,1});}
 else {const auto action=barrier==1?LoopAction::stopAll:barrier==2?LoopAction::clear:LoopAction::clearAll;
  check(f.engine->enqueueLoop({action,0,900,f.engine->generation()}),"ordered barrier");f.pump();}
 f.expect(3,transaction,DelayEditStatus::cancelled);
 const bool clear=barrier==2||barrier==3;
 check(clear?f.info().delay==DelaySettings{}:f.info().delay==original.delay,"barrier preserves/reset settings as specified");
 c=f.command(4);setDelayRepeats(c.delay,DelayRepeatMode::exact,2);const auto fresh=f.enqueue(c);f.pump();DelayEditResult result;
 check(!f.engine->popDelayEdit(result),"fresh operation cannot use obsolete paused allocation");
 resume.store(true,std::memory_order_release);f.expect(4,fresh,DelayEditStatus::ready);
 check(f.info().delay==c.delay,"only fresh prepared owner commits after cancellation");
}
int main(){for(unsigned block:{64u,128u}){
 validationAndOrdering(block);saturationAndLifetime(block);for(int barrier=0;barrier<5;++barrier)failureAndBarriers(block,barrier);
 std::cout<<"PASS Engine Delay "<<block<<": ordered/sample-offset edits, worker adoption, unread receipts, FIFO bounds, generations, OOM, cancel/panic/DEL/MIDI Stop; zero tracked callback heap operations\n";
}}
