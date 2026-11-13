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
thread_local bool watch=false,failLocal=false;
thread_local unsigned operations=0;
std::atomic<bool> failWorker{false},pauseWorker{false},entered{false},releaseWorker{false};
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct AudioScope {AudioScope(){check(!watch,"nested audio watcher");operations=0;watch=true;}~AudioScope(){watch=false;check(!operations,"audio new/delete in atomic Arp bank");}};
template<class Ready> void waitFor(Ready ready,const char* why){
 const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);
 while(!ready()){check(std::chrono::steady_clock::now()<end,why);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
}
}
void* operator new(std::size_t n){
 if(watch)++operations;
 if(failLocal||failWorker.exchange(false)){failLocal=false;throw std::bad_alloc();}
 if(pauseWorker.exchange(false)){entered.store(true,std::memory_order_release);while(!releaseWorker.load(std::memory_order_acquire))std::this_thread::yield();}
 if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void operator delete(void* p) noexcept {if(watch&&p)++operations;std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void* p) noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept{::operator delete(p);}void operator delete[](void* p,std::size_t) noexcept{::operator delete(p);}
using namespace aion;

static ArpDelayStorage::Bank settings(unsigned seed,bool enabled=true){
 ArpDelayStorage::Bank bank;
 for(unsigned id=0;id<20;++id){auto& s=bank[id];s.enabled=enabled;s.seed=seed+id;s.values[0]=.01f+id*.0001f;s.values[1]=.43f;s.values[2]=.5f;s.values[3]=1;s.values[4]=8;s.values[8]=.8f;s.values[9]=.2f;}
 return bank;
}
static void compare(ArpDelayGraph& a,ArpDelayGraph& b,unsigned block){
 std::array<float,128> l{},r{},al{},ar{},bl{},br{};
 for(unsigned n=0;n<16;++n){
  for(unsigned i=0;i<block;++i){l[i]=static_cast<float>(.15*std::sin((n*block+i)*.071));r[i]=static_cast<float>(.12*std::cos((n*block+i)*.033));}
  AudioScope guard;
  check(a.process(l.data(),r.data(),2,al.data(),ar.data(),block)&&b.process(l.data(),r.data(),2,bl.data(),br.data(),block),"bank graph render");
  for(unsigned i=0;i<block;++i)check(al[i]==bl[i]&&ar[i]==br[i],"failed/cancelled bank preserves exact old tail/STAY/LFO PCM");
  check(a.state()==b.state()&&a.position()==b.position(),"bank preserves graph state and clock");
 }
}
static void storage(unsigned block){
 ArpDelayStorage store;check(store.prepareStopped(8000),"storage prepare");auto old=settings(100),next=settings(200);
 {AudioScope guard;check(store.requestBank(old),"all twenty reservation");check(!store.requestBank(next)&&!store.request(0,next[0].values,1),"bank excludes concurrent individual/bank requests");}
 for(unsigned n=0;n<19;++n){store.serviceWorker();AudioScope guard;const auto out=store.poll(37);check(!out.bankInstalled&&!out.bankFailed&&!out.installed,"nineteen ready graphs cannot install a prefix");for(unsigned id=0;id<20;++id)check(!store.graph(id)&&store.view(id).pending,"all members stay pending");}
 store.serviceWorker();
 {AudioScope guard;const auto out=store.poll(37);check(out.bankInstalled&&!out.bankFailed,"twentieth preparation commits whole bank");}
 std::array<std::unique_ptr<ArpDelayGraph>,20> refs;
 std::array<ArpDelayGraph*,20> owners{};
 for(unsigned id=0;id<20;++id){refs[id]=std::make_unique<ArpDelayGraph>();check(refs[id]->prepare(8000,old[id].values,old[id].seed,37),"independent old owner");owners[id]=store.graph(id);check(owners[id]&&owners[id]->targets()==old[id].values,"whole bank targets");compare(*owners[id],*refs[id],block);}
 const auto frame=owners[0]->position();
 auto unchanged=[&]{for(unsigned id=0;id<20;++id)check(store.graph(id)==owners[id]&&owners[id]->state()==refs[id]->state()&&owners[id]->position()==refs[id]->position(),"old bank ownership/state unchanged");};
 // Forty slots are sufficient for twenty old + twenty staged. Fail the last
 // allocation after nineteen successful preparations, never a prefix commit.
 {AudioScope guard;check(store.requestBank(next),"full replacement reservation");}
 for(unsigned n=0;n<19;++n){store.serviceWorker();AudioScope guard;check(!store.poll(frame).bankInstalled,"late OOM preflight keeps old bank");unchanged();}
 failLocal=true;store.serviceWorker();check(!failLocal,"last allocation failed");
 {AudioScope guard;const auto out=store.poll(frame);check(out.bankFailed&&!out.bankInstalled&&!out.installed,"late OOM has one failed bank outcome");unchanged();check(!store.requestBank(next),"unreclaimed failed bank rejects before mutating old owners");}
 for(unsigned id=0;id<20;++id)compare(*owners[id],*refs[id],block);
 store.serviceWorker();
 {AudioScope guard;check(store.requestBank(next),"capacity recovered after failed bank reclamation");}
 for(unsigned n=0;n<19;++n)store.serviceWorker();
 {AudioScope guard;store.cancelBank();check(!store.poll(frame).bankInstalled,"cancel nineteen-ready bank");unchanged();}
 for(unsigned n=0;n<42;++n){store.serviceWorker();AudioScope guard;const auto out=store.poll(frame);check(!out.bankInstalled&&!out.bankFailed,"cancelled late result never completes another request");}
 for(unsigned id=0;id<20;++id)compare(*owners[id],*refs[id],block);
 // A reset failure touches only candidates; even an all-ready bank rolls back.
 {AudioScope guard;check(store.requestBank(next),"invalid clock fixture");}
 for(unsigned n=0;n<20;++n)store.serviceWorker();
 {AudioScope guard;const auto out=store.poll(UINT64_MAX);check(out.bankFailed&&!out.bankInstalled,"invalid clock rolls back whole bank");unchanged();}
 store.serviceWorker();
 auto mixed=next;for(unsigned id=1;id<19;++id)mixed[id].enabled=false;
 {AudioScope guard;check(store.requestBank(mixed),"mixed enabled/disabled bank");}
 store.serviceWorker();{AudioScope guard;check(!store.poll(77).bankInstalled,"disabled members cannot force early adoption");unchanged();}
 store.serviceWorker();{AudioScope guard;check(store.poll(77).bankInstalled,"mixed bank commits together");for(unsigned id=0;id<20;++id)check((store.graph(id)!=nullptr)==mixed[id].enabled,"mixed members retire/adopt together");}
 auto off=settings(300,false);
 {AudioScope guard;auto bad=off;bad[19].values[0]=NAN;check(!store.requestBank(bad),"invalid disabled last member rejected atomically");check(store.requestBank(off),"all-off bank requires no new slots");check(store.poll(78).bankInstalled,"all-off bank commits with zero outstanding allocations");for(unsigned id=0;id<20;++id)check(!store.graph(id)&&!store.view(id).pending,"all-off bank clears all owners");}
 store.serviceWorker();
 {AudioScope guard;check(store.request(19,next[19].values,9),"individual request fixture");check(!store.requestBank(next),"pending individual prevents bank admission");store.cancel(19);(void)store.poll(0);}
 store.serviceWorker();{AudioScope guard;(void)store.poll(0);}store.serviceWorker();
 {AudioScope guard;check(store.requestBank(next),"bank recovers after individual cancellation");store.clear(19);check(!store.poll(0).bankInstalled,"track clear cancels entire pending bank");}
}

struct Fixture {
 std::unique_ptr<Engine> engine=std::make_unique<Engine>();unsigned block;
 std::array<float,128> l{},r{};
 explicit Fixture(unsigned frames):block(frames){engine->prepare(8000,32768);engine->setMonitoring(false);engine->setTarget(1);}
 void pump(bool zero=true,bool nonRealtime=false){float* channels[]{l.data(),r.data()};AudioScope guard;engine->process(zero?nullptr:channels,zero?0:2,zero?0:static_cast<int>(block),{},nonRealtime);}
 ArpBankRequest request(unsigned id,bool enabled=true){
  ArpBankRequest r;r.requestId=id;r.generation=engine->generation();r.settings=settings(id,enabled);
  for(unsigned track=0;track<20;++track){const auto info=engine->inspectAudioThread(track).track;r.clearEpochs[track]=info.clearEpoch;r.settingsRevisions[track]=info.arpSettingsRevision;}
  return r;
 }
 ArpBankResult result(unsigned expected){ArpBankResult out;const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);
  for(;;){pump();if(engine->popArpBank(out))return out;
   if(std::chrono::steady_clock::now()>=end){std::cerr<<"request "<<expected<<", track 0/19 committed IDs "<<engine->inspectAudioThread(0).track.arpRequest<<'/'<<engine->inspectAudioThread(19).track.arpRequest<<'\n';check(false,"Engine bank terminal timeout");}
   std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 }
 void expect(const ArpBankRequest& r,ArpPreparationStatus status){const auto out=result(r.requestId);if(out.requestId!=r.requestId||out.generation!=r.generation||out.status!=status){std::cerr<<"request "<<r.requestId<<", status "<<static_cast<unsigned>(out.status)<<", expected "<<static_cast<unsigned>(status)<<'\n';check(false,"exact bank request/generation/status receipt");}}
 PreparedArpInspection inspect(unsigned id){AudioScope guard;return engine->inspectPreparedArpAudioThread(id);}
 void command(LoopAction action,unsigned track=0,bool nonRealtime=false){check(engine->enqueueLoop({action,track,900,engine->generation()}),"ordinary command fixture");pump(false,nonRealtime);}
 void install(unsigned id){auto r=request(id);check(engine->enqueueArpBank(r),"initial Engine bank");expect(r,ArpPreparationStatus::ready);}
};
static bool same(const PreparedArpInspection& a,const PreparedArpInspection& b){return a.active==b.active&&a.targets==b.targets&&a.state==b.state&&a.frame==b.frame;}
static void protocol(unsigned block){
 Fixture f(block);auto r=f.request(1);ArpBankResult out;
 std::array<EqSettings,20> eq{};std::array<CompressorSettings,20> compressor{};std::array<SignalChain,20> chain{};
 eq[0].enabled=true;eq[0].values[4]=4;compressor[19].enabled=true;compressor[19].values[0]=-30;
 chain[0]=normalizedSignalChain("ACQ");chain[19]=normalizedSignalChain("CQ"); // omit A on purpose
 f.engine->prepareEqBank(eq);f.engine->prepareCompressorBank(compressor);f.engine->prepareChainBank(chain);
 auto unrelated=[&]{for(unsigned id=0;id<20;++id){const auto info=f.engine->inspectAudioThread(id).track;check(info.equalizer==eq[id]&&info.compressor==compressor[id]&&info.signalChain==chain[id],"Arp-only bank preserves independent Q/C and user order/omission");}};
 {auto bad=r;bad.requestId=0;check(!f.engine->enqueueArpBank(bad),"zero bank ID rejected");bad=r;bad.generation=0;check(!f.engine->enqueueArpBank(bad),"zero generation rejected");bad=r;bad.settings[19].values[1]=NAN;check(!f.engine->enqueueArpBank(bad),"invalid last settings rejected on message owner");}
 check(f.engine->enqueueArpBank(r)&&!f.engine->popArpBank(out),"enqueue is not bank success");
 ArpPreparationRequest single;single.track=0;single.requestId=55;single.generation=r.generation;
 check(!f.engine->enqueueArpPreparation(single)&&!f.engine->enqueueArpBank(r),"one bank excludes all individual/bank messages");
 check(f.engine->enqueueLoop({LoopAction::volume,0,777,r.generation,0,.4}),"ordinary command while bank pending");
 waitFor([&]{f.pump(false);unsigned committed=0;for(unsigned id=0;id<20;++id){const auto info=f.engine->inspectAudioThread(id).track;committed+=info.arpRequest==r.requestId;check(info.arpRequest==0||info.arp==r.settings[id],"bank settings match request");}check(committed==0||committed==20,"Engine never exposes partially applied settings");return committed==20;},"whole bank commit timeout");
 for(unsigned n=0;n<40;++n)f.pump(false); // closed telemetry/receipt consumer
 check(!f.engine->enqueueArpBank(f.request(2))&&!f.engine->enqueueArpPreparation(single),"unread bank receipt retains bound");
 f.expect(r,ArpPreparationStatus::ready);check(!f.engine->popArpBank(out),"one terminal receipt only");unrelated();
 EngineView view,last;while(f.engine->popView(view))last=view;check(last.lastRequestId==777&&!last.rejected,"bank preserves ordinary acknowledgement");
 f.engine->cancelArpBank();f.pump();check(f.engine->inspectAudioThread(19).track.arp==r.settings[19],"cancel after ready does not undo committed settings");
 auto stale=f.request(3);++stale.settingsRevisions[19];check(f.engine->enqueueArpBank(stale),"stale revision validates on audio");f.expect(stale,ArpPreparationStatus::stale);
 stale=f.request(4);++stale.clearEpochs[19];check(f.engine->enqueueArpBank(stale),"stale clear epoch validates on audio");f.expect(stale,ArpPreparationStatus::stale);
 auto cancelled=f.request(5);check(f.engine->enqueueArpBank(cancelled),"queued bank cancel");f.engine->cancelArpPreparation(19);f.expect(cancelled,ArpPreparationStatus::cancelled);
 const auto old=f.inspect(19);auto failed=f.request(6);failWorker.store(true);check(f.engine->enqueueArpBank(failed),"worker OOM bank admitted");f.expect(failed,ArpPreparationStatus::failed);check(!failWorker.load()&&same(old,f.inspect(19)),"Engine failed bank preserves previous state/clock");unrelated();
 single.activate=true;single.settingsRevision=f.engine->inspectAudioThread(0).track.arpSettingsRevision;
 check(f.engine->enqueueArpPreparation(single),"individual after bank receipt consumed");
 // The failed bank may still occupy cancelled slots until its worker catches
 // up. Test-only probes consume explicit busy receipts before trying again.
 bool retry=false;
 waitFor([&]{
  if(retry){++single.requestId;check(f.engine->enqueueArpPreparation(single),"individual recovery probe");retry=false;}
  f.pump();check(!f.engine->enqueueArpBank(f.request(7)),"unread individual receipt excludes bank");
  ArpPreparationResult one;if(!f.engine->popArpPreparation(one))return false;
  check(one.status==ArpPreparationStatus::ready||one.status==ArpPreparationStatus::busy,"individual receipt unchanged");retry=one.status==ArpPreparationStatus::busy;return !retry;
 },"individual ready fixture");
 // The bank cancellation path must bypass a saturated ordinary command FIFO.
 for(unsigned n=0;n<255;++n)check(f.engine->enqueueLoop({LoopAction::volume,0,1000+n,r.generation,UINT64_MAX,.5}),"fill performance FIFO");
 auto queued=f.request(8);check(f.engine->enqueueArpBank(queued),"bank independent of ordinary queue capacity");f.engine->cancelArpBank();f.expect(queued,ArpPreparationStatus::cancelled);
 // Unread old-generation terminal results survive stopped prepare, including
 // repeated prepare and rate changes, without releasing message ownership.
 auto before=f.request(9);check(f.engine->enqueueArpBank(before),"queued before prepare");f.engine->prepare(8000,32768);f.engine->prepare(96000,32768);
 check(!f.engine->enqueueArpBank(f.request(10)),"prepare cannot reuse unread bank result slot");f.expect(before,ArpPreparationStatus::cancelled);
 auto fresh=f.request(10,false);fresh.settings[19].enabled=true;check(f.engine->enqueueArpBank(fresh),"new-rate bank after receipt");f.expect(fresh,ArpPreparationStatus::ready);check(f.inspect(19).active&&!f.inspect(0).active,"mixed new-rate bank");
 auto off=f.request(11,false);check(f.engine->enqueueArpBank(off),"all-off Engine bank");f.expect(off,ArpPreparationStatus::ready);for(unsigned id=0;id<20;++id)check(!f.inspect(id).active&&!f.engine->inspectAudioThread(id).track.arp.enabled,"all-off Engine settings/owners commit");
}
static void paused(unsigned block,unsigned barrier){
 Fixture f(block);f.install(20);const auto oldInfo=f.engine->inspectAudioThread(0).track;
 entered.store(false);releaseWorker.store(false);pauseWorker.store(true);
 auto r=f.request(21);check(f.engine->enqueueArpBank(r),"paused bank admitted");f.pump();waitFor([&]{return entered.load(std::memory_order_acquire);},"worker allocation pause entry");
 // Callback must continue without waiting for the allocating worker.
 f.pump(false);check(f.engine->clock()==block,"paused worker does not block audio clock");const auto old=f.inspect(0);
 if(barrier==0)f.engine->cancelArpBank();
 else if(barrier==1)f.command(LoopAction::clear,19);
 else if(barrier==2)f.command(LoopAction::clearAll);
 else if(barrier==3)f.command(LoopAction::stopAll);
 else if(barrier==4){SyncSettings sync;sync.mode=SyncMode::fixed;sync.bpm=120;sync.bars=1;f.engine->configureSync(sync);f.command(LoopAction::record,19);check(f.engine->inspectAudioThread(19).track.state==LoopState::armed,"armed CANCEL fixture");f.command(LoopAction::finish,19);}
 else {const auto info=f.engine->inspectAudioThread(19).track;LoopCommand edit{LoopAction::arpDelay,19,901,f.engine->generation()};edit.arp=info.arp;edit.arp.values[2]=.1f;edit.arpMask=1u<<2;edit.arpClearEpoch=info.clearEpoch;edit.arpSettingsRevision=info.arpSettingsRevision;check(f.engine->enqueueLoop(edit),"newer masked edit");f.pump(false);}
 f.expect(r,ArpPreparationStatus::cancelled);
 if(barrier!=2){check(f.engine->inspectAudioThread(0).track.arp==oldInfo.arp,"cancelled bank never commits its unaffected prefix");if(barrier==0)check(same(old,f.inspect(0)),"explicit cancellation preserves complete active graph");else if(barrier!=3)check(f.inspect(0).targets==old.targets&&f.inspect(0).frame==f.engine->clock(),"unaffected graph continues behind the cancellation barrier");}
 releaseWorker.store(true,std::memory_order_release);
 auto recovery=f.request(22,false);check(f.engine->enqueueArpBank(recovery),"all-off recovery while obsolete allocation finishes");f.expect(recovery,ArpPreparationStatus::ready);
 for(unsigned n=0;n<32;++n)f.pump(false);for(unsigned id=0;id<20;++id)check(!f.inspect(id).active,"late cancelled graph never resurrects");
}
static void connected(unsigned block){
 Fixture a(block),b(block);
 for(unsigned id:{0u,19u}){
  for(auto* f:{&a,&b}){f->engine->setAutoPlay(true);f->command(LoopAction::record,id);}
  for(unsigned n=0;n<2048/block;++n)for(auto* f:{&a,&b}){for(unsigned i=0;i<block;++i){f->l[i]=static_cast<float>(.14*std::sin((n*block+i)*.071));f->r[i]=static_cast<float>(.11*std::cos((n*block+i)*.033));}f->pump(false);}
  for(auto* f:{&a,&b}){f->command(LoopAction::finish,id,true);check(!f->engine->finalizationPending(),"paired committed recording");}
 }
 a.install(40);b.install(40);check(a.engine->clock()==b.engine->clock(),"paired render clocks");
 auto render=[&]{a.l.fill(0);a.r.fill(0);b.l.fill(0);b.r.fill(0);a.pump(false);b.pump(false);double energy=0;for(unsigned i=0;i<block;++i){check(a.l[i]==b.l[i]&&a.r[i]==b.r[i],"failed bank preserves exact connected stereo Engine PCM");energy+=std::abs(a.l[i])+std::abs(a.r[i]);}return energy;};
 double energy=0;for(unsigned n=0;n<32;++n)energy+=render();check(energy>1,"paired Engine bank comparison is audible");
 auto r=a.request(41);failWorker.store(true);check(a.engine->enqueueArpBank(r),"audible bank OOM request");a.expect(r,ArpPreparationStatus::failed);
 for(unsigned id=0;id<20;++id)check(a.engine->inspectAudioThread(id).track.arp==b.engine->inspectAudioThread(id).track.arp&&same(a.inspect(id),b.inspect(id)),"failed bank preserves complete connected settings/state");
 for(unsigned n=0;n<64;++n)(void)render();
 entered.store(false);releaseWorker.store(false);pauseWorker.store(true);
 auto cancelled=a.request(43);check(a.engine->enqueueArpBank(cancelled),"audible cancellation request");
 waitFor([&]{a.pump();if(entered.load(std::memory_order_acquire))return true;ArpBankResult out;
  if(a.engine->popArpBank(out)){check(out.status==ArpPreparationStatus::busy,"explicit busy before audible allocation begins");++cancelled.requestId;check(a.engine->enqueueArpBank(cancelled),"audible capacity recovery probe");}return false;
 },"audible allocation pause");
 a.engine->cancelArpBank();a.expect(cancelled,ArpPreparationStatus::cancelled);releaseWorker.store(true,std::memory_order_release);
 for(unsigned n=0;n<32;++n)(void)render();
 // Successful replacement must also adopt all settings at the same audio clock.
 auto replace=[&](Fixture& f,ArpBankRequest request){
  const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);
  for(;;){check(f.engine->enqueueArpBank(request),"paired audible bank replacement");const auto out=f.result(request.requestId);
   check(out.requestId==request.requestId&&out.generation==request.generation,"replacement recovery correlation");if(out.status==ArpPreparationStatus::ready)return;
   check(out.status==ArpPreparationStatus::busy&&std::chrono::steady_clock::now()<end,"bounded capacity recovery probes");++request.requestId;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 };
 replace(a,a.request(42));replace(b,b.request(42));
 for(unsigned n=0;n<64;++n)(void)render();
}
static void prepareLifetime(unsigned block,bool rateChange){
 Fixture f(block);f.install(60);
 entered.store(false);releaseWorker.store(false);pauseWorker.store(true);
 auto r=f.request(61);check(f.engine->enqueueArpBank(r),"in-flight before prepare");f.pump();waitFor([&]{return entered.load(std::memory_order_acquire);},"prepare worker pause");
 releaseWorker.store(true,std::memory_order_release); // prepare must join its worker
 f.engine->prepare(rateChange?48000:8000,32768);
 check(!f.engine->enqueueArpBank(f.request(62,false)),"prepare retains unread in-flight ownership");f.expect(r,ArpPreparationStatus::cancelled);
 auto ready=f.request(62,false);check(f.engine->enqueueArpBank(ready),"unread ready lifetime fixture");
 f.pump();f.pump();check(f.engine->inspectAudioThread(19).track.arpRequest==ready.requestId,"bank committed before stopped prepare");
 f.engine->prepare(8000,32768);f.engine->prepare(8000,32768);f.expect(ready,ArpPreparationStatus::ready);
 ArpBankResult extra;check(!f.engine->popArpBank(extra),"prepare cannot duplicate a terminal receipt");
}
static void variableBlocks(double rate){
 auto engine=std::make_unique<Engine>();engine->prepare(rate,8192);engine->setMonitoring(false);
 std::array<float,128> left{},right{};float* channels[]{left.data(),right.data()};
 {AudioScope guard;engine->process(channels,2,37);} // deliberately partial quantum
 ArpBankRequest request;request.settings=settings(70);request.requestId=70;request.generation=engine->generation();
 check(engine->enqueueArpBank(request),"variable-block bank request");ArpBankResult result;
 waitFor([&]{{AudioScope guard;engine->process(nullptr,0,0);}return engine->popArpBank(result);},"variable-block bank adoption");
 check(result.status==ArpPreparationStatus::ready&&engine->clock()==37,"bank adopts at arbitrary current frame without advancing it");
 ArpDelayGraph oracle;check(oracle.prepare(rate,request.settings[19].values,request.settings[19].seed,37),"partial-quantum independent graph");
 std::array<float,128> ol{},orr{};
 for(unsigned round=0;round<8;++round)for(unsigned frames:{0u,1u,7u,63u,64u,65u,127u,128u,3u}){
  AudioScope guard;engine->process(channels,2,static_cast<int>(frames));check(oracle.process(nullptr,nullptr,2,ol.data(),orr.data(),frames),"variable-block oracle");
  for(unsigned id=0;id<20;++id){const auto info=engine->inspectAudioThread(id).track;const auto graph=engine->inspectPreparedArpAudioThread(id);
   check(info.arp==request.settings[id]&&info.arpSettingsRevision==1&&graph.active&&graph.frame==engine->clock(),"all bank owners retain settings and variable callback clock");
   if(id==19)check(graph.state==oracle.state()&&graph.frame==oracle.position(),"partial-quantum bank graph state matches independent owner");}
 }
}
static void disabledPanic(unsigned block){
 auto bank=std::make_unique<LoopBank>();bank->prepare(8192,0,8000);LoopBank::ArpEpochs epochs{},revisions{};
 {AudioScope guard;check(bank->requestArpBank(settings(80),epochs,revisions,80),"direct bank with all old stages disabled");
  check(bank->apply(LoopAction::stopAll,0)&&bank->arpBankResult().status==LoopBank::ArpBankStatus::cancelled,"direct panic cancels bank without an old enabled member");}
 for(unsigned n=0;n<32;++n){
  {AudioScope guard;bank->beginBlock();for(unsigned i=0;i<block;++i)(void)bank->tick({});
   for(unsigned id=0;id<20;++id)check(!bank->preparedArp(id)&&!bank->trackView(id).arp.enabled,"late bank cannot survive all-disabled panic");}
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
 }
}
static void inactiveImmediate(unsigned block){
 Fixture f(block);auto r=f.request(400,false);r.restoreEffects=true;r.effects.equalizers[19].enabled=true;r.effects.chains[19]=normalizedSignalChain("CQ");
 check(f.engine->enqueueArpBank(r),"inactive full restore admission");f.pump();ArpBankResult out;
 check(f.engine->popArpBank(out)&&out.status==ArpPreparationStatus::ready&&out.callbackSerial==f.engine->inspectCallbackClockAudioThread().serial&&out.callbackSerial,"inactive full bank completes in its first zero-frame callback");
 check(f.engine->inspectAudioThread(19).track.equalizer==r.effects.equalizers[19]&&f.engine->inspectAudioThread(19).track.signalChain==r.effects.chains[19],"inactive first-callback FX commit");
 r.requestId=401;r.settingsRevisions=out.settingsRevisions;check(f.engine->enqueueArpBank(r),"unread callback serial fixture");f.pump();const auto serial=f.engine->inspectCallbackClockAudioThread().serial;f.pump();
 check(f.engine->popArpBank(out)&&out.callbackSerial==serial&&out.callbackSerial<f.engine->inspectCallbackClockAudioThread().serial,"unread terminal callback serial stays immutable");
 r.requestId=402;r.settingsRevisions=out.settingsRevisions;check(f.engine->enqueueArpBank(r),"stopped cancellation serial fixture");f.engine->prepare(8000,32768);
 check(f.engine->popArpBank(out)&&out.status==ArpPreparationStatus::cancelled&&out.callbackSerial==0,"stopped prepare never invents a producing callback");
}
static void concurrent(){
 auto engine=std::make_unique<Engine>();engine->prepare(8000,8192);const auto generation=engine->generation();
 std::atomic<bool> run{true};std::thread audio([&]{while(run.load(std::memory_order_acquire)){{AudioScope guard;engine->process(nullptr,0,0);}std::this_thread::yield();}});
 // Off banks stress cross-thread ownership/results cheaply; enabled every fifth
 // transaction also exercises allocation, adoption and worker retirement.
 std::array<std::uint64_t,20> revisions{};
 for(unsigned n=0;n<80;++n){ArpBankRequest r;r.requestId=100+n;r.generation=generation;r.settings=settings(n,n%5==0);r.settingsRevisions=revisions;
  check(engine->enqueueArpBank(r),"concurrent bank enqueue");if(n%3==0)engine->cancelArpBank();ArpBankResult result;
  waitFor([&]{return engine->popArpBank(result);},"concurrent bank result");check(result.requestId==r.requestId&&result.generation==generation,"concurrent bank correlation");
  check(result.status==ArpPreparationStatus::ready||(n%3==0&&result.status==ArpPreparationStatus::cancelled),"concurrent cancel linearizes around commit");if(result.status==ArpPreparationStatus::ready)for(auto& revision:revisions)++revision;
 }
 run.store(false,std::memory_order_release);audio.join();ArpBankResult result;check(!engine->popArpBank(result),"concurrent bank has no duplicate result");
}
int main(){
 for(unsigned block:{64u,128u}){storage(block);protocol(block);for(unsigned barrier=0;barrier<6;++barrier)paused(block,barrier);connected(block);prepareLifetime(block,false);prepareLifetime(block,true);disabledPanic(block);inactiveImmediate(block);}
 variableBlocks(32000);variableBlocks(48000);concurrent();
 std::cout<<"PASS atomic Arp bank: all-20/mixed/off adoption, last-slot OOM/exhaustion/reset-clock rollback, exact old graph and connected stereo PCM, queue/receipt bounds, generation/epoch/revision cancellation, DEL/panic/armed-CANCEL/live-edit barriers, concurrent ownership and zero audio new/delete at 64/128\n";
}
