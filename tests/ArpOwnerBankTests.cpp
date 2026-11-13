// SPDX-License-Identifier: AGPL-3.0-only
#include "core/ArpSettingsOwner.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>
namespace {
thread_local bool watch=false;thread_local unsigned allocations=0;
std::atomic<bool> failWorker{false},pauseWorker{false},entered{false},releaseWorker{false};
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct Guard{Guard(){check(!watch,"nested audio guard");allocations=0;watch=true;}~Guard(){watch=false;check(!allocations,"bank owner callback new/delete");}};
template<class F>void wait(F ready,const char* why){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(15);while(!ready()){check(std::chrono::steady_clock::now()<end,why);std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
}
void* operator new(std::size_t n){if(watch)++allocations;if(failWorker.exchange(false))throw std::bad_alloc();
 if(pauseWorker.exchange(false)){entered.store(true,std::memory_order_release);while(!releaseWorker.load(std::memory_order_acquire))std::this_thread::yield();}
 if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{if(watch&&p)++allocations;std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace aion;
using Bank=ArpSettingsOwner::Bank;
static Bank settings(unsigned seed,bool enabled=true){Bank bank;
 for(unsigned id=0;id<20;++id){auto& s=bank[id];check(applyArpPreset(s.controls,id%8+1),"original bank preset");s.controls.enabled=enabled&&(id==0||id==7||id==19);s.bpm=137.123456789+id;s.seed=seed+id;
  if(id%2){check(setArpControl(s.controls,0,.0123456789+id*.0001),"bank decimal original control");}}
 return bank;
}
struct Fixture{
 std::unique_ptr<Engine> engine=std::make_unique<Engine>();ArpSettingsOwner owner{*engine};unsigned block;EngineView latest{};
 std::array<float,128> l{},r{};
 explicit Fixture(unsigned frames):block(frames){engine->prepare(8000,32768);engine->setMonitoring(false);engine->setAutoPlay(true);engine->setTarget(1);pump();service();check(owner.bind(latest),"bind original bank defaults");}
 void pump(bool zero=true,bool offline=false){float* channels[]{l.data(),r.data()};Guard guard;engine->process(zero?nullptr:channels,zero?0:2,zero?0:static_cast<int>(block),{},offline);}
 void service(){while(engine->popView(latest)){}owner.service(latest);}
 Bank originals(){Bank result;for(unsigned id=0;id<20;++id)result[id]=owner.track(id)->committed;return result;}
 void done(){wait([&]{pump();service();return !owner.restoring();},"bank owner terminal timeout");}
 void install(const Bank& bank,unsigned request){check(owner.restore(bank,request),"original bank restore admission");done();check(owner.restoreStatus()==ArpPreparationStatus::ready&&originals()==bank&&owner.bound(),"entire original bank accepted");}
 void command(LoopAction action,unsigned id=0){check(engine->enqueueLoop({action,id,99999,engine->generation()}),"ordinary fixture command");pump(false,true);}
};
static void ownership(unsigned block){
 Fixture f(block);auto old=f.originals(),next=settings(200),off=settings(300,false);
 auto bad=next;bad[19].bpm=NAN;check(!f.owner.restore(bad,1)&&!f.owner.restore(next,0)&&f.originals()==old&&!f.owner.restoring(),"last-member validation rejects whole original bank");
 check(f.owner.set(0,1,6.123456789,137,2)&&!f.owner.restore(next,3),"individual pending original excludes bank");
 wait([&]{f.pump(false);f.service();return !f.owner.track(0)->pending;},"individual completion before bank");old=f.originals();
 check(f.owner.restore(next,4)&&f.owner.restoring()&&f.originals()==old,"bank admission is not original-state success");
 for(unsigned id=0;id<20;++id)check(f.owner.track(id)->pending&&!f.owner.set(id,1,9.9,120,5)&&!f.owner.enable(id,true,120,6),"bank excludes individual original edits");
 check(!f.owner.restore(off,7),"one original bank including unread completion");
 // Keep ordinary snapshots closed until the exact bank result is ready. A newer
 // unrelated global acknowledgement cannot establish a bank's result.
 check(f.engine->enqueueLoop({LoopAction::volume,19,99999,f.engine->generation(),0,.9}),"newer global ack fixture");
 wait([&]{f.pump();return f.engine->inspectAudioThread(19).track.arpRequest==4;},"native bank adoption before message service");
 for(unsigned n=0;n<40;++n)f.pump(false);
 check(f.originals()==old&&f.owner.restoring()&&!f.owner.restore(off,8),"unread result retains old originals and admission bound");
 const auto stale=f.latest;f.owner.service(stale);check(!f.owner.restoring()&&f.originals()==next&&f.owner.bound(),"exact bank receipt commits all doubles/presets/seeds despite stale snapshot");
 f.service();check(f.latest.lastRequestId==99999&&f.owner.bound(),"unrelated ack does not invalidate exact accepted bank");
 f.install(off,9);for(unsigned id=0;id<20;++id)check(!f.engine->inspectPreparedArpAudioThread(id).active,"all-off restore retains originals and retires every graph");
 f.install(next,10);
 failWorker.store(true);check(f.owner.restore(settings(400),11),"bank worker OOM admission");f.done();
 check(!failWorker.load()&&f.owner.restoreStatus()==ArpPreparationStatus::failed&&f.originals()==next&&f.owner.bound(),"bank OOM rolls entire original bank back");
 f.install(off,12);f.install(next,13);
 auto all=next;for(auto& s:all)s.controls.enabled=true;f.install(all,14);f.install(off,15);
}
static void cancellation(unsigned block){
 Fixture f(block);const auto old=settings(100),next=settings(200);f.install(old,20);
 entered=false;releaseWorker=false;pauseWorker=true;check(f.owner.restore(next,21),"paused bank admission");f.pump();
 wait([&]{return entered.load(std::memory_order_acquire);},"worker bank allocation pause");
 f.owner.cancel(7);f.pump();f.service();check(!f.owner.restoring()&&f.owner.restoreStatus()==ArpPreparationStatus::cancelled&&f.originals()==old,"track-scoped cancellation rolls whole staged original bank back");
 releaseWorker.store(true,std::memory_order_release);
 // Let late cancelled workers reclaim before a new reservation. Busy is an
 // explicit terminal result; it must preserve the old originals and recover.
 wait([&]{check(f.owner.restore(next,21),"same-ID bank retry admission");f.done();check(f.owner.restoreStatus()==ArpPreparationStatus::ready||f.owner.restoreStatus()==ArpPreparationStatus::busy,"cancel recovery status");return f.owner.restoreStatus()==ArpPreparationStatus::ready;},"cancelled storage recovery");
 check(f.originals()==next&&f.owner.bound(),"late cancelled bank cannot steal reused caller ID");
 check(f.owner.restore(settings(300),22),"clear-cancel original bank");f.command(LoopAction::clear,7);f.done();auto expected=next;expected[7]={};
 check(f.owner.restoreStatus()==ArpPreparationStatus::cancelled&&f.originals()==expected&&f.owner.bound(),"clear before adoption cancels bank and clears only its original record");
 // Ready remains an accepted fact if DEL happens before message consumption.
 auto accepted=settings(400);check(f.owner.restore(accepted,23),"ready then clear admission");
 wait([&]{f.pump();return f.engine->inspectAudioThread(19).track.arpRequest==23;},"ready bank before clear");
 f.command(LoopAction::clear,7);f.service();accepted[7]={};
 check(f.owner.restoreStatus()==ArpPreparationStatus::ready&&f.originals()==accepted&&f.owner.bound(),"post-ready clear wins one track without losing other accepted originals");
}
static void lifecycle(unsigned block){
 Fixture f(block);const auto next=settings(500);check(f.owner.restore(next,30),"ready before repeated prepare");
 wait([&]{f.pump();return f.engine->inspectAudioThread(19).track.arpRequest==30;},"ready before generation change");
 f.engine->prepare(8000,32768);f.engine->prepare(8000,32768);f.pump();f.service();
 check(!f.owner.bound()&&!f.owner.restoring()&&f.owner.restoreStatus()==ArpPreparationStatus::ready&&f.originals()==next,"unread bank receipt preserves all original precision across prepare");
 check(f.owner.bind(f.latest,next),"explicit known bank rebind after same-rate prepare");
 auto before=f.originals();check(f.owner.restore(settings(600),31),"queued original bank before reset");f.engine->prepare(48000,32768);f.pump();f.service();
 check(!f.owner.bound()&&!f.owner.restoring()&&f.owner.restoreStatus()==ArpPreparationStatus::cancelled&&f.originals()==before,"prepare cancels queued bank exactly without committing candidate");
 check(f.owner.bind(f.latest),"explicit default bind after new-rate reset");
 // Earlier unrelated Arp changes make restore's revision obsolete. Fail closed
 // rather than infer original values from a newer Float32 target snapshot.
 LoopCommand raw{LoopAction::arpDelay,0,32,f.engine->generation()};raw.arp=f.latest.tracks[0].arp;raw.arpMask=0;check(f.engine->enqueueLoop(raw),"external revision fixture");
 check(f.owner.restore(settings(700),33),"candidate before queued revision dispatch");f.pump(false);f.done();
 check(f.owner.restoreStatus()==ArpPreparationStatus::cancelled&&f.originals()==Bank{}&&!f.owner.bound(),"external revision cancels bank without partial original reconstruction");
}
static void pcm(unsigned block){
 Fixture a(block),b(block);const auto old=settings(800);a.install(old,40);
 ArpBankRequest direct;direct.requestId=40;direct.generation=b.engine->generation();
 for(unsigned id=0;id<20;++id){check(compileArpSettings(old[id].controls,old[id].bpm,old[id].seed,direct.settings[id]),"raw reference compilation");const auto info=b.latest.tracks[id];direct.clearEpochs[id]=info.clearEpoch;direct.settingsRevisions[id]=info.arpSettingsRevision;}
 check(b.engine->enqueueArpBank(direct),"accepted raw Engine bank reference");ArpBankResult result;
 wait([&]{b.pump();return b.engine->popArpBank(result);},"raw Engine bank reference completion");check(result.status==ArpPreparationStatus::ready,"raw bank ready");
 for(auto* f:{&a,&b})f->command(LoopAction::record);
 for(unsigned n=0;n<2048/block;++n)for(auto* f:{&a,&b}){for(unsigned i=0;i<block;++i){f->l[i]=static_cast<float>(.15*std::sin((n*block+i)*.117));f->r[i]=static_cast<float>(.12*std::cos((n*block+i)*.037));}f->pump(false);}
 for(auto* f:{&a,&b})f->command(LoopAction::finish);
 auto compare=[&]{for(auto* f:{&a,&b}){f->l.fill(0);f->r.fill(0);f->pump(false);}double energy=0;
  for(unsigned i=0;i<block;++i){check(a.l[i]==b.l[i]&&a.r[i]==b.r[i],"failed/cancelled original bank preserves exact connected stereo PCM");energy+=std::abs(a.l[i])+std::abs(a.r[i]);}return energy;};
 double energy=0;for(unsigned n=0;n<64;++n)energy+=compare();
 failWorker=true;check(a.owner.restore(settings(900),41),"audible bank OOM");a.done();check(a.owner.restoreStatus()==ArpPreparationStatus::failed&&a.originals()==old,"audible original bank rollback");
 for(unsigned n=0;n<64;++n)energy+=compare();
 check(a.owner.restore(settings(1000),42),"audible queued cancellation");a.owner.cancel();a.done();
 for(unsigned n=0;n<64;++n)energy+=compare();check(energy>1&&a.originals()==old,"nonzero PCM equality after failed/cancelled bank");
}
static void concurrent(){
 Fixture f(64);std::atomic<bool> run{true},inside{false};
 std::thread audio([&]{std::array<float,4096> l{},r{};float* channels[]{l.data(),r.data()};while(run.load(std::memory_order_acquire)){inside.store(true,std::memory_order_release);{Guard guard;f.engine->process(channels,2,4096);}inside.store(false,std::memory_order_release);}});
 auto expected=f.originals();unsigned during=0;
 for(unsigned n=0;n<16;++n){const auto next=settings(2000+n*20);wait([&]{return inside.load(std::memory_order_acquire);},"concurrent callback entry");
  check(f.owner.restore(next,50+n),"concurrent original bank admission");during+=inside.load(std::memory_order_acquire);if(n%3==0)f.owner.cancel(n%20);
  wait([&]{f.service();return !f.owner.restoring();},"concurrent original bank terminal receipt");
  const auto status=f.owner.restoreStatus();check(status==ArpPreparationStatus::ready||status==ArpPreparationStatus::busy||(n%3==0&&status==ArpPreparationStatus::cancelled),"concurrent original bank status");
  if(status==ArpPreparationStatus::ready)expected=next;
  check(f.owner.bound()&&f.originals()==expected,"concurrent original bank model commits all or none");
 }
 run.store(false,std::memory_order_release);audio.join();check(during,"bank publication during actual callback");
}
int main(){for(unsigned block:{64u,128u}){ownership(block);cancellation(block);lifecycle(block);pcm(block);}concurrent();
 std::cout<<"PASS original Arp bank owner: atomic precision/presets/seeds, exact terminal revisions, OOM/cancel/clear/prepare rollback, unread results, same-ID recovery, exact nonzero stereo PCM / 64/128 / zero callback new/delete\n";}
