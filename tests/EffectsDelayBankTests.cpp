// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"
#include "core/ArpSettings.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
namespace {
thread_local bool watch=false;thread_local unsigned heapOperations=0;
std::atomic<unsigned> pauseAt{0},failAt{0};std::atomic<bool> entered{false},resume{false};
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct Guard {Guard(){heapOperations=0;watch=true;}~Guard(){watch=false;check(!heapOperations,"coordinated bank callback heap operation");}};
template<class F>void wait(F ready,const char* why){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(!ready()){check(std::chrono::steady_clock::now()<end,why);std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
}
void* operator new(std::size_t n){if(watch)++heapOperations;
 if(failAt.load()&&failAt.fetch_sub(1)==1)throw std::bad_alloc();
 if(pauseAt.load()&&pauseAt.fetch_sub(1)==1){entered.store(true,std::memory_order_release);while(!resume.load(std::memory_order_acquire))std::this_thread::yield();}
 if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watch&&p)++heapOperations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace aion;
struct Fixture {
 std::unique_ptr<Engine> engine=std::make_unique<Engine>();unsigned block;std::array<float,128> l{},r{};
 explicit Fixture(unsigned frames):block(frames){engine->prepare(8000,16384);engine->setMonitoring(false);}
 void pump(std::span<const MidiEvent> events={}){float* audio[]{l.data(),r.data()};Guard guard;engine->process(audio,2,static_cast<int>(block),events);}
 ArpBankRequest request(unsigned id,unsigned variant,bool all=true){ArpBankRequest request;request.requestId=id;request.generation=engine->generation();request.restoreEffects=true;request.effects.restoreDelays=true;
  for(unsigned track=0;track<20;++track){const auto info=engine->inspectAudioThread(track).track;
   request.clearEpochs[track]=info.clearEpoch;request.settingsRevisions[track]=info.arpSettingsRevision;request.effects.delayRevisions[track]=info.delaySettingsRevision;
   ArpSettings a;a.enabled=all||track==0;a.values[0]=.01+.001*variant;check(compileArpSettings(a,120,id+track,request.settings[track]),"joint Arp descriptor");
   auto& d=request.effects.delays[track];d.enabled=all||track==19;d.values[0]=.01+.001*variant;
   request.effects.equalizers[track].enabled=true;request.effects.equalizers[track].values[4]=static_cast<float>(variant);
   request.effects.compressors[track].enabled=true;request.effects.compressors[track].values[0]=-20-static_cast<float>(variant);
   request.effects.chains[track]=normalizedSignalChain(variant%2?"QCADTFO":"TFOADQC");
   for(unsigned k=0;k<3;++k){auto& drive=request.effects.drives[track][k];drive.enabled=true;check(setDriveControl(static_cast<DriveKind>(k),drive,4,.1*variant),"joint drive descriptor");}
  }return request;
 }
 ArpBankResult result(){ArpBankResult receipt;wait([&]{pump();return engine->popArpBank(receipt);},"joint result timeout");return receipt;}
 void same(const ArpBankRequest& request){for(unsigned id=0;id<20;++id){const auto info=engine->inspectAudioThread(id).track;
  check(info.arp==request.settings[id]&&info.delay==request.effects.delays[id]&&info.equalizer==request.effects.equalizers[id]
   &&info.compressor==request.effects.compressors[id]&&info.signalChain==request.effects.chains[id]&&info.drives==request.effects.drives[id],"complete old or new coordinated bank");
 }}
 void settle(){for(unsigned n=0;n<8;++n){pump();std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
};
static void run(unsigned block,unsigned action){
 Fixture f(block);auto old=f.request(1,1);check(f.engine->enqueueArpBank(old),"initial full coordinated bank");
 const auto first=f.result();check(first.status==ArpPreparationStatus::ready&&first.includesDelays,"initial joint receipt");f.same(old);f.settle();
 if(action==9){MidiSettings midi;midi.enabled=true;check(f.engine->configureMidi(midi),"joint synthetic MIDI routing");f.pump();}
 auto next=f.request(2,2);entered=false;resume=false;
 // Reverse readiness: one D is ready while the final (twentieth) Arp waits.
 if(action==7||action==8)for(unsigned id=1;id<20;++id)next.effects.delays[id].enabled=false;
 // The existing worker alternates Arp and D storage. Last D allocation is held
 // after every Arp and nineteen D graphs are ready; old owners fill the other slots.
 if(action==1||action==8)failAt=action==8?21:40;else pauseAt=action==7?21:40;
 check(f.engine->enqueueArpBank(next),"replacement full coordinated bank");f.pump();
 if(action!=1&&action!=8){wait([&]{return entered.load(std::memory_order_acquire);},"last allocation pause");
  for(unsigned n=0;n<20;++n){f.pump();f.same(old);ArpBankResult absent;check(!f.engine->popArpBank(absent),"ready Arp bank cannot commit before final D");}
  DelayBankRequest independent;independent.requestId=77;independent.generation=f.engine->generation();std::uint64_t token=88;
  check(!f.engine->enqueueDelayBank(independent,token)&&token==88,"coordinator excludes independent D bank including unread receipts");
  LoopCommand edit{LoopAction::delay,0,78,f.engine->generation()};check(!f.engine->enqueueDelayEdit(edit,token),"coordinator excludes individual D edit");
  if(action==2)f.engine->cancelDelayEdit(19);
  if(action==3)f.engine->cancelArpPreparation();
  if(action==4)check(f.engine->enqueueLoop({LoopAction::stopAll,0,80,f.engine->generation()}),"joint panic");
  if(action==5)check(f.engine->enqueueLoop({LoopAction::clear,19,81,f.engine->generation()}),"joint DEL");
  if(action==9){const MidiEvent stop{0,252,0,0,1};f.pump({&stop,1});}
  if(action>=2&&action<=5)f.pump();
  resume.store(true,std::memory_order_release);
  if(action==6)f.engine->prepare(48000,16384);
 }
 const auto result=f.result();const auto expected=action==0||action==7?ArpPreparationStatus::ready:action==1||action==8?ArpPreparationStatus::failed:ArpPreparationStatus::cancelled;
 check(result.status==expected&&result.requestId==2&&result.generation==next.generation&&result.includesDelays,"exact coordinated terminal receipt");
 if(action==0||action==7){f.same(next);for(unsigned id=0;id<20;++id)check(result.settingsRevisions[id]==next.settingsRevisions[id]+1&&result.delaySettingsRevisions[id]==next.effects.delayRevisions[id]+1,"one receipt owns both twenty-track revisions");}
 else if(action!=5&&action!=6)f.same(old);
 else if(action==5){for(unsigned id=0;id<19;++id)check(f.engine->inspectAudioThread(id).track.delay==old.effects.delays[id]&&f.engine->inspectAudioThread(id).track.arp==old.settings[id],"DEL cancels joint bank without changing other nineteen owners");check(f.engine->inspectAudioThread(19).track.delay==DelaySettings{},"DEL owns reset target");}
 for(unsigned n=0;n<40;++n){f.pump();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 ArpBankResult absent;check(!f.engine->popArpBank(absent),"obsolete allocations cannot emit another terminal result");
 auto off=f.request(2,3);for(auto& a:off.settings)a.enabled=false;for(auto& d:off.effects.delays)d.enabled=false;
 check(f.engine->enqueueArpBank(off),"same ID fresh coordinated recovery");check(f.result().status==ArpPreparationStatus::ready,"all-off coordinated recovery");f.same(off);
 std::cout<<"PASS coordinated bank "<<block<<" action "<<action<<": full forty-owner preparation, atomic metadata, rollback/cancellation and recovery\n";
}
static void admission(unsigned block){Fixture f(block);auto r=f.request(1,1);r.effects.delays[19].repeatCount=0;check(!f.engine->enqueueArpBank(r),"invalid final D rejects before any worker job");
 auto request=f.request(2,1);++request.effects.delayRevisions[19];check(f.engine->enqueueArpBank(request),"stale D descriptor queued");check(f.result().status==ArpPreparationStatus::stale,"stale last D revision blocks entire group");
 DelayBankRequest d;d.requestId=3;d.generation=f.engine->generation();std::uint64_t transaction=0;check(f.engine->enqueueDelayBank(d,transaction),"independent D admission");
 check(!f.engine->enqueueArpBank(f.request(4,1)),"outstanding D descriptor excludes joint bank");f.pump();f.pump();
 check(!f.engine->enqueueArpBank(f.request(4,1)),"unread independent D receipt excludes joint bank");DelayBankResult result;check(f.engine->popDelayBank(result),"consume independent receipt");
 f.engine->prepare(384000,16384);auto oversized=f.request(5,1);for(auto& settings:oversized.effects.delays){settings.repeatMode=DelayRepeatMode::exact;settings.repeatCount=10;}
 check(f.engine->enqueueArpBank(oversized),"valid but over-budget coordinated descriptor");failAt=1;
 check(f.result().status==ArpPreparationStatus::busy&&failAt.exchange(0)==1,"D aggregate budget rejects before publishing any Arp allocation");
 auto off=f.request(6,1);for(auto& a:off.settings)a.enabled=false;for(auto& delay:off.effects.delays)delay.enabled=false;
 check(f.engine->enqueueArpBank(off),"all-off coordinated receipt admission");f.pump();
 d.requestId=7;d.generation=f.engine->generation();for(unsigned id=0;id<20;++id){const auto info=f.engine->inspectAudioThread(id).track;d.clearEpochs[id]=info.clearEpoch;d.settingsRevisions[id]=info.delaySettingsRevision;}
 check(!f.engine->enqueueDelayBank(d,transaction),"unread coordinated receipt still excludes independent D bank");
 ArpBankResult coordinated;check(f.engine->popArpBank(coordinated)&&coordinated.status==ArpPreparationStatus::ready&&coordinated.includesDelays,"consume exact coordinated receipt");
 check(f.engine->enqueueDelayBank(d,transaction),"independent D recovers only after coordinated receipt consumption");f.pump();f.pump();check(f.engine->popDelayBank(result),"consume fresh independent D receipt");
}
int main(){for(unsigned block:{64u,128u}){admission(block);for(unsigned action=0;action<=9;++action)run(block,action);}}
