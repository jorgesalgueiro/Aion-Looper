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
thread_local bool watch=false;thread_local unsigned operations=0;
std::atomic<bool> failAllocation{false};
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct Guard {Guard(){check(!watch,"nested watcher");operations=0;watch=true;}~Guard(){watch=false;check(!operations,"owner protocol callback new/delete");}};
template<class F>void wait(F f,const char* why){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(!f()){check(std::chrono::steady_clock::now()<end,why);std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
}
void* operator new(std::size_t n){if(watch)++operations;if(failAllocation.exchange(false))throw std::bad_alloc();if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{if(watch&&p)++operations;std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace aion;
struct Fixture {
 std::unique_ptr<Engine> engine=std::make_unique<Engine>();ArpSettingsOwner owner{*engine};unsigned block;
 std::array<float,128> l{},r{};EngineView latest{},initial{};
 explicit Fixture(unsigned frames):block(frames){engine->prepare(8000,32768);engine->setMonitoring(false);engine->setAutoPlay(true);engine->setTarget(1);pump(true);drain();initial=latest;check(owner.bind(latest),"bind known original defaults");}
 void pump(bool zero=false,bool offline=false){float* audio[]{l.data(),r.data()};Guard guard;engine->process(zero?nullptr:audio,zero?0:2,zero?0:static_cast<int>(block),{},offline);}
 void drain(){while(engine->popView(latest)){};}
 void service(){drain();owner.service(latest);}
 void done(unsigned id,bool zero=false){wait([&]{pump(zero);service();return !owner.track(id)->pending;},"owner terminal timeout");}
 void command(LoopAction action,unsigned id=0){check(engine->enqueueLoop({action,id,900,engine->generation()}),"fixture ordinary command");pump(false,true);}
 ArpSettingsOwner::Bank originals(){ArpSettingsOwner::Bank result;for(unsigned id=0;id<20;++id)result[id]=owner.track(id)->committed;return result;}
};
static void model(unsigned block){
 Fixture f(block);const auto defaults=f.owner.track(0)->committed;
 check(!f.owner.set(20,1,2,120,1)&&!f.owner.set(0,99,2,120,1)&&!f.owner.set(0,1,NAN,120,1)&&!f.owner.preset(0,9,120,1),"invalid original edits atomic");
 check(f.owner.set(0,1,6.123456789,173.123456789,10),"decimal original edit admission");check(f.owner.track(0)->pending&&f.owner.track(0)->committed==defaults,"enqueue is not committed original state");
 check(!f.owner.set(0,2,.2,120,11),"one pending original gesture per track");
 check(f.engine->enqueueLoop({LoopAction::volume,19,99999,f.engine->generation(),0,.8}),"unrelated newer acknowledgement");
 f.done(0);check(f.owner.track(0)->status==ArpPreparationStatus::ready&&f.owner.track(0)->committed.controls.values[1]==6.123456789&&f.latest.lastRequestId==99999,"exact result commits original precision despite newer global ack");
 f.owner.service(f.initial);check(f.owner.bound()&&f.owner.track(0)->committed.controls.values[1]==6.123456789,"stale telemetry cannot roll original values back");
 check(f.owner.preset(0,5,137.123456789,12),"original preset candidate");f.done(0);check(f.owner.track(0)->committed.controls.preset==5,"preset metadata committed with native result");
 const auto old=f.owner.track(0)->committed;
 check(f.owner.set(0,1,9.9,120,13,UINT64_MAX),"future cancellable original edit");f.pump(true);f.service();f.owner.cancel(0);f.done(0,true);
 check(f.owner.track(0)->status==ArpPreparationStatus::cancelled&&f.owner.track(0)->committed==old,"cancelled edit rolls original candidate back");
 // Reuse the caller's request ID after cancellation; the internal transaction
 // must distinguish it from the obsolete future FIFO head and release that head.
 check(f.owner.set(0,1,9.9,120,13),"same-ID fresh transaction");f.done(0);check(f.owner.track(0)->committed.controls.values[1]==9.9&&!f.owner.track(0)->committed.controls.preset,"old future command cannot block or steal new commit");
 check(f.owner.set(0,2,.31,120,14),"commit-before-cancel candidate");f.pump();f.owner.cancel(0);f.owner.service(f.initial);
 check(f.owner.track(0)->status==ArpPreparationStatus::ready&&f.owner.track(0)->committed.controls.values[2]==.31,"cancel after native ready cannot undo confirmed original values");
 // Preparation OOM is a failure, not a persisted enabled state. Controls stay
 // precise and disabled; the worker is the only allocating producer here.
 const auto before=f.owner.track(0)->committed;failAllocation.store(true);check(f.owner.enable(0,true,150,15),"enable preparation");f.done(0,true);
 check(!failAllocation.load()&&f.owner.track(0)->status==ArpPreparationStatus::failed&&f.owner.track(0)->committed==before,"OOM preserves original disabled controls/precision");
 check(f.owner.enable(0,true,150,16),"fresh enable after OOM");f.done(0,true);check(f.owner.track(0)->committed.controls.enabled,"enable commits after storage ready");
 const auto active=f.engine->inspectPreparedArpAudioThread(0);check(f.owner.enable(0,true,299,17),"idempotent explicit enable uses edit path");f.pump();f.service();
 check(f.owner.track(0)->committed.bpm==150&&f.engine->inspectPreparedArpAudioThread(0).targets==active.targets,"repeated enable does not reset storage or retarget BPM");
 check(f.owner.enable(0,false,299,18),"disable tracked control");f.done(0);check(!f.owner.track(0)->committed.controls.enabled&&!f.engine->inspectPreparedArpAudioThread(0).active,"disable commits without losing original controls");
}
static void clearAndGeneration(unsigned block){
 Fixture f(block);check(f.owner.set(0,1,6.123456789,137,30),"unread ready before clear");f.pump();f.command(LoopAction::clear);f.service();
 check(f.owner.bound()&&f.owner.track(0)->status==ArpPreparationStatus::cancelled&&f.owner.track(0)->committed==ArpOwnedSettings{},"clear wins over older unread ready result");f.owner.service(f.initial);check(f.owner.track(0)->clearEpoch==1,"old clear epoch cannot revive removed controls");
 check(f.owner.set(0,1,6.123456789,137,31),"unread ready before prepare");f.pump();f.engine->prepare(8000,32768);f.pump(true);f.service();
 check(!f.owner.bound()&&f.owner.track(0)->committed.controls.values[1]==6.123456789,"old ready precision retained while new generation requires explicit bind");
 const auto known=f.originals();check(f.owner.bind(f.latest,known),"known original state can bind preserved same-rate generation");
 f.engine->prepare(48000,32768);f.pump(true);f.service();check(!f.owner.bound()&&!f.owner.bind(f.latest,known),"new-rate reset is never guessed from old raw targets");
 check(f.owner.bind(f.latest),"explicit fresh default binding after native reset");
}
static void rejectedAndFull(unsigned block){
 Fixture f(block);LoopCommand raw{LoopAction::arpDelay,0,40,f.engine->generation()};raw.arp=f.latest.tracks[0].arp;raw.arpMask=0;
 check(f.engine->enqueueLoop(raw),"external no-op revision before tracked edit");check(f.owner.set(0,2,.2,120,41),"candidate invalidated by earlier revision");check(f.engine->enqueueLoop({LoopAction::volume,19,99999,f.engine->generation(),0,1}),"later success masks generic failure");f.done(0);
 check(f.owner.track(0)->status==ArpPreparationStatus::failed&&f.owner.track(0)->committed==ArpOwnedSettings{}&&!f.owner.bound()&&!f.latest.rejected,"exact rejection rolls back and detects unexplained external revision");
 check(f.owner.bind(f.latest),"explicit known rebind after external no-op");
 for(unsigned n=0;n<255;++n)check(f.engine->enqueueLoop({LoopAction::volume,19,1000+n,f.engine->generation(),UINT64_MAX,1}),"fill ordinary FIFO");
 const auto old=*f.owner.track(0);check(!f.owner.set(0,1,9.9,120,42)&&!f.owner.track(0)->pending&&f.owner.track(0)->committed==old.committed,"full queue rejects both publications before owner mutation");
 f.pump(true);f.service();check(f.owner.bound()&&f.owner.track(0)->requestId==old.requestId,"full queue publishes no orphan result descriptor");
 f.engine->prepare(8000,32768);f.pump(true);f.service();check(f.owner.bind(f.latest),"post-saturation explicit rebind");check(f.owner.set(0,1,9.9,120,43),"full-queue recovery");f.done(0);
}
static void allTracks(unsigned block){
 Fixture f(block);
 for(unsigned id=0;id<20;++id)check(f.owner.set(id,1,.070000001+id*.1,137+id,100+id),"all twenty original candidates");
 for(unsigned n=0;n<40;++n)f.pump(); // saturate ordinary telemetry, keep all receipts unread
 for(unsigned id=0;id<20;++id)check(f.owner.track(id)->pending&&!f.owner.set(id,2,.8,120,200+id),"unread receipt retains per-track bound");
 f.owner.service(f.initial);
 for(unsigned id=0;id<20;++id)check(!f.owner.track(id)->pending&&f.owner.track(id)->status==ArpPreparationStatus::ready&&f.owner.track(id)->committed.controls.values[1]==.070000001+id*.1,"all original precision survives closed telemetry consumer");
 check(f.owner.bound(),"old snapshot cannot invalidate newer confirmed bank");
}
static void pcm(unsigned block){
 Fixture actual(block),reference(block);
 for(auto* f:{&actual,&reference})f->command(LoopAction::record);
 for(unsigned n=0;n<2048/block;++n)for(auto* f:{&actual,&reference}){for(unsigned i=0;i<block;++i){f->l[i]=static_cast<float>(.15*std::sin((n*block+i)*.117));f->r[i]=static_cast<float>(.12*std::cos((n*block+i)*.037));}f->pump();}
 for(auto* f:{&actual,&reference}){f->command(LoopAction::finish);f->service();check(f->owner.enable(0,true,137,300),"audible original enable");f->done(0,true);}
 auto compare=[&]{for(auto* f:{&actual,&reference}){f->l.fill(0);f->r.fill(0);f->pump();}actual.service();double energy=0;
  for(unsigned i=0;i<block;++i){check(actual.l[i]==reference.l[i]&&actual.r[i]==reference.r[i],"owned control/preset preserves exact existing Engine stereo PCM");energy+=std::abs(actual.l[i])+std::abs(actual.r[i]);}return energy;};
 double energy=0;
 for(unsigned preset=1;preset<=8;++preset){ArpSettings next;next.enabled=true;check(applyArpPreset(next,preset),"reference original preset");const auto at=actual.engine->clock()+13;
  check(actual.owner.preset(0,preset,173.123456789,400+preset,at),"tracked original preset");const auto info=reference.engine->inspectAudioThread().track;LoopCommand c{LoopAction::arpDelay,0,400+preset,reference.engine->generation(),at};c.arp=info.arp;c.arpClearEpoch=info.clearEpoch;c.arpSettingsRevision=info.arpSettingsRevision;
  check(reference.engine->enqueueArpPreset(c,next,173.123456789),"accepted untracked preset reference");for(unsigned n=0;n<4096/block;++n)energy+=compare();check(actual.owner.track(0)->committed.controls==next,"exact original preset metadata alongside exact sound");
 }
 check(energy>1&&actual.owner.bound(),"nonzero owned preset PCM and synchronized model");
}
static void concurrent(){
 auto engine=std::make_unique<Engine>();engine->prepare(8000,8192);ArpSettingsOwner owner(*engine);EngineView view;
 {Guard guard;engine->process(nullptr,0,0);}check(engine->popView(view)&&owner.bind(view),"concurrent owner bind");
 std::atomic<bool> running{true},inside{false};std::thread audio([&]{std::array<float,4096> l{},r{};float* channels[]{l.data(),r.data()};while(running.load(std::memory_order_acquire)){inside.store(true,std::memory_order_release);{Guard guard;engine->process(channels,2,4096);}inside.store(false,std::memory_order_release);}});
 unsigned during=0;
 for(unsigned n=0;n<80;++n){wait([&]{return inside.load(std::memory_order_acquire);},"concurrent callback entry");
  check(owner.set(n%20,1,.070000001+(n%20)*.1,137,1000+n),"concurrent original control admission");during+=inside.load(std::memory_order_acquire);if(n%3==0)owner.cancel(n%20);
  wait([&]{while(engine->popView(view)){}owner.service(view);return !owner.track(n%20)->pending;},"concurrent tracked terminal receipt");
  check(owner.track(n%20)->status==ArpPreparationStatus::ready||(n%3==0&&owner.track(n%20)->status==ArpPreparationStatus::cancelled),"concurrent control/cancel linearization");
 }
 running.store(false,std::memory_order_release);audio.join();check(during>0&&owner.bound(),"tracked descriptors/commands published while callback runs");
}
static void protocol(unsigned block){
 auto engine=std::make_unique<Engine>();engine->prepare(8000,8192);std::array<float,128> l{},r{};float* audio[]{l.data(),r.data()};
 auto pump=[&](bool zero=false,bool midiStop=false){const std::array<MidiEvent,1> stop{{{0,0xfc,0,0,1}}};Guard guard;engine->process(zero?nullptr:audio,zero?0:2,zero?0:static_cast<int>(block),midiStop?std::span<const MidiEvent>(stop):std::span<const MidiEvent>{});};
 auto command=[&](unsigned id,unsigned request,std::uint64_t revision=0){LoopCommand c{LoopAction::arpDelay,id,request,engine->generation()};c.arp.values[2]=.27f;c.arpMask=(1u<<2);c.arpSettingsRevision=revision;return c;};
 std::uint64_t token=999;auto bad=command(20,1);check(!engine->enqueueArpEdit(bad,token)&&token==999,"invalid tracked admission preserves output");
 auto c=command(0,500);c.frame=UINT64_MAX;check(engine->enqueueArpEdit(c,token)&&token&&token!=999,"tracked internal identity");const auto first=token;
 ArpPreparationRequest prep;prep.track=0;prep.requestId=501;prep.generation=engine->generation();check(!engine->enqueueArpPreparation(prep),"tracked edit excludes per-track prepare");ArpBankRequest bank;bank.requestId=502;bank.generation=engine->generation();check(!engine->enqueueArpBank(bank),"tracked edit excludes bank");
 bad=c;bad.arpTransaction=token;check(!engine->enqueueLoop(bad),"raw caller cannot forge tracked identity");
 engine->cancelArpPreparation(0);pump(true);ArpPreparationResult result;check(engine->popArpPreparation(result)&&result.requestId==500&&result.transaction==first&&result.status==ArpPreparationStatus::cancelled,"queue-independent exact queued cancellation");
 c=command(0,500);check(engine->enqueueArpEdit(c,token)&&token!=first,"caller ID reuse gets a non-reused transaction");pump();check(engine->popArpPreparation(result)&&result.status==ArpPreparationStatus::ready&&result.transaction==token&&result.settingsRevision==1,"obsolete future head yields to fresh exact receipt");
 MidiSettings midi;midi.enabled=true;check(engine->configureMidi(midi),"enable actual MIDI Stop fixture");pump(true);
 // Both tracked commands remain future heads until actual MIDI Stop purges
 // performance work. Their descriptors still need exactly one terminal result.
 for(unsigned id:{0u,19u}){c=command(id,600+id,id==0?1:0);c.frame=UINT64_MAX;check(engine->enqueueArpEdit(c,token),"future MIDI-purge fixture");}
 pump(false,true);std::array<bool,20> seen{};for(unsigned n=0;n<2;++n){check(engine->popArpPreparation(result)&&result.status==ArpPreparationStatus::cancelled&&result.requestId==600+result.track&&!seen[result.track],"MIDI Stop purge retains exact tracked terminal results");seen[result.track]=true;}
 check(!engine->popArpPreparation(result),"no duplicate cancellation result");
 c=command(0,700,1);const auto oldGeneration=c.generation;check(engine->enqueueArpEdit(c,token),"unread ready lifetime");pump();engine->prepare(8000,8192);engine->prepare(8000,8192);
 c=command(0,701,2);check(!engine->enqueueArpEdit(c,token),"prepare retains unread tracked ownership");check(engine->popArpPreparation(result)&&result.requestId==700&&result.generation==oldGeneration&&result.status==ArpPreparationStatus::ready&&result.settingsRevision==2,"ready result keeps original generation/revision across prepare");
 check(engine->enqueueArpEdit(c,token),"queued tracked edit before prepare");const auto queuedGeneration=c.generation;engine->prepare(48000,8192);check(engine->popArpPreparation(result)&&result.requestId==701&&result.generation==queuedGeneration&&result.status==ArpPreparationStatus::cancelled,"stopped prepare cancels queued descriptor and command once");
}
static void knownPrecision(unsigned block){
 Fixture f(block);f.engine->prepare(32000,32768);f.pump(true);f.service();ArpSettingsOwner::Bank known;
 known[0].controls.values[1]=2.000000001;ArpRenderSettings render;check(compileArpSettings(known[0].controls,120,known[0].seed,render)&&render==f.latest.tracks[0].arp,"distinct original double aliases the same raw Float32 state");
 auto invalid=known;invalid[19].controls.values[2]=NAN;check(!f.owner.bind(f.latest,invalid)&&!f.owner.bound(),"invalid last known record rejects whole bind");
 check(f.owner.bind(f.latest,known),"explicit known precision bind");f.owner.service(f.latest);check(f.owner.bound()&&f.owner.track(0)->committed.controls.values[1]==2.000000001,"raw target reconciliation never reconstructs or rounds original values");
}
int main(){for(unsigned block:{64u,128u}){model(block);clearAndGeneration(block);rejectedAndFull(block);allTracks(block);pcm(block);protocol(block);knownPrecision(block);}concurrent();
 std::cout<<"PASS original Arp owner: exact committed doubles/presets, exact stereo PCM, enable/OOM rollback, tracked acceptance/rejection, stale views/generations/clears, full queues, future cancellation and same-ID recovery, all-20 receipts, concurrent mid-callback publication, zero callback new/delete / 64/128\n";}
