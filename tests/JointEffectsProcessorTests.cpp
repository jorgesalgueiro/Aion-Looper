// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>
namespace {
thread_local bool watch=false;thread_local unsigned heapOperations=0;
std::atomic<std::size_t> pauseBytes{0},failBytes{0};std::atomic<bool> entered{false},released{false};
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
std::size_t allocationSize(){std::size_t bytes=32002*sizeof(std::array<float,2>);
 #if defined(_MSC_VER) && defined(_WIN64)
 bytes+=sizeof(void*)+31;
 #ifdef _DEBUG
 bytes+=sizeof(void*);
 #endif
 #endif
 return bytes;
}
}
void* operator new(std::size_t n){if(watch)++heapOperations;auto size=n;
 if(n&&failBytes.compare_exchange_strong(size,0))throw std::bad_alloc();size=n;
 if(n&&pauseBytes.compare_exchange_strong(size,0)){entered.store(true,std::memory_order_release);while(!released.load(std::memory_order_acquire))std::this_thread::yield();}
 if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watch&&p)++heapOperations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
struct Scene {
 std::array<aion::EqSettings,20> eq{};std::array<aion::CompressorSettings,20> comp{};
 std::array<aion::SignalChain,20> chains{};aion::LoopBank::DriveBanks drives=aion::LoopBank::defaultDriveBanks();
 aion::ArpSettingsOwner::Bank arps{};aion::DelaySettingsOwner::Bank delays{};
};
static Scene scene(unsigned variant){Scene result;
 for(unsigned id=0;id<20;++id){const bool active=id==0||id==19;
  result.eq[id].enabled=active;result.eq[id].values[4]=static_cast<float>(variant);
  result.comp[id].enabled=active;result.comp[id].values[0]=-20-static_cast<float>(variant);
  result.chains[id]=aion::normalizedSignalChain(variant%2?"ADQCTFO":"TFOQCDA");
  for(unsigned kind=0;kind<3;++kind){auto& drive=result.drives[id][kind];drive.enabled=active;check(aion::setDriveControl(static_cast<aion::DriveKind>(kind),drive,4,.1*variant),"scene drive descriptor");}
  auto& a=result.arps[id];a.controls.enabled=active;a.controls.values[0]=.01;a.controls.values[2]=.1;a.bpm=137.123456789+variant;a.seed=1000+variant*20+id;
  a.controls.voicing=id%2?aion::ArpVoicing::loose:aion::ArpVoicing::original;
  auto& d=result.delays[id];d.enabled=active;d.values[0]=.012345678901+variant*.001+id*.0001;
  if(id==19){d.repeatMode=aion::DelayRepeatMode::exact;d.repeatCount=2;}
 }return result;
}
struct AionJointEffectsProcessorTestAccess {
 static void message(AionProcessor& p){p.timerCallback();}
 static void delayOnly(AionProcessor& p){std::lock_guard guard(p.midiSettingsMutex);p.serviceDelay();}
 static void stage(AionProcessor& p,const Scene& s){std::lock_guard guard(p.midiSettingsMutex);
  p.eqSettings=s.eq;p.compressorSettings=s.comp;p.chainSettings=s.chains;p.driveSettings=s.drives;p.arpSettings=s.arps;p.delaySettings=s.delays;
  p.eqRestorePending=p.compressorRestorePending=p.chainRestorePending=p.driveRestorePending=p.arpRestorePending=true;
  p.delayRestorePending=false;p.delayRestoreFailed=false;++p.effectsIntent;p.engine.cancelArpPreparation();p.engine.cancelDelayEdit();
 }
 static bool pending(AionProcessor& p){std::lock_guard guard(p.midiSettingsMutex);return p.effectsRestore.active||p.eqRestorePending||p.compressorRestorePending||p.chainRestorePending||p.driveRestorePending||p.arpRestorePending||p.delayRestorePending;}
 static auto delays(AionProcessor& p){std::lock_guard guard(p.midiSettingsMutex);return p.delaySettings;}
 static auto record(AionProcessor& p,unsigned id){std::lock_guard guard(p.midiSettingsMutex);return *p.delayOwner.track(id);}
 static bool bound(AionProcessor& p){std::lock_guard guard(p.midiSettingsMutex);return p.delayOwner.bound()&&p.arpOwner.bound();}
 static bool failed(AionProcessor& p){std::lock_guard guard(p.midiSettingsMutex);return p.delayRestoreFailed;}
 static auto view(AionProcessor& p){std::lock_guard guard(p.midiSettingsMutex);return p.latest;}
 static aion::EngineView takeViews(AionProcessor& p){std::lock_guard guard(p.midiSettingsMutex);aion::EngineView view{};while(p.engine.popView(view)){}return view;}
 static void setView(AionProcessor& p,const aion::EngineView& view){std::lock_guard guard(p.midiSettingsMutex);p.latest=view;}
 static void rawClear(AionProcessor& p,unsigned id){std::lock_guard guard(p.midiSettingsMutex);const auto request=p.nextRequest+1;
  check(p.engine.enqueueLoop({aion::LoopAction::clear,id,request,p.engine.generation()}),"audio-owned clear admission");p.nextRequest=request;
 }
};
using Access=AionJointEffectsProcessorTestAccess;
static bool nativeMatches(AionProcessor& p,const Scene& s,bool cleared=false){
 for(unsigned id=0;id<20;++id){const auto info=p.inspectAudioThread(id).track;
  if(cleared&&id==19){if(info.delay!=aion::DelaySettings{}||info.arp!=aion::ArpRenderSettings{}||info.equalizer!=aion::EqSettings{}||info.compressor!=aion::CompressorSettings{}||info.drives!=aion::defaultDriveBank()||info.signalChain!=aion::SignalChain{})return false;continue;}
  aion::ArpRenderSettings arp;if(!aion::compileArpSettings(s.arps[id].controls,s.arps[id].bpm,s.arps[id].seed,arp))return false;
  if(info.delay!=s.delays[id]||info.arp!=arp||info.equalizer!=s.eq[id]||info.compressor!=s.comp[id]||info.signalChain!=s.chains[id]||info.drives!=s.drives[id])return false;
  if(info.delayRequest!=info.arpRequest||info.delayRequest!=info.eqRequest||info.delayRequest!=info.compressorRequest||info.delayRequest!=info.chainRequest)return false;
 }return true;
}
struct Fixture {
 std::unique_ptr<AionProcessor> p=std::make_unique<AionProcessor>();unsigned block;juce::AudioBuffer<float> audio;juce::MidiBuffer midi;
 explicit Fixture(unsigned size):block(size),audio(2,static_cast<int>(size)){midi.ensureSize(2048);p->setPlayConfigDetails(2,2,8000,static_cast<int>(block));p->prepareToPlay(8000,static_cast<int>(block));pump();service();}
 void pump(){audio.clear();heapOperations=0;watch=true;p->processBlock(audio,midi);watch=false;check(!heapOperations,"joint Processor callback heap operation");midi.clear();}
 void service(){Access::message(*p);}
 template<class F>void wait(F predicate,const char* why){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(!predicate()){check(std::chrono::steady_clock::now()<end,why);std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
 void settle(){wait([&]{pump();service();return !Access::pending(*p);},"joint Processor completion");}
 void load(const Scene& s){Access::stage(*p,s);service();settle();fade();verify(s);}
 void fade(){const auto frames=static_cast<unsigned>(.025*p->inspectCallbackClockAudioThread().sampleRate);for(unsigned n=0;n<frames/block+2;++n){pump();service();}}
 void verify(const Scene& s,bool cleared=false){check(nativeMatches(*p,s,cleared),"joint native values and exact common acknowledgement");check(Access::bound(*p),"both owners bound after joint confirmation");auto expected=s.delays;if(cleared)expected[19]={};check(Access::delays(*p)==expected,"confirmed exact D shadow");
  for(unsigned id=0;id<20;++id){const auto record=Access::record(*p,id);if(cleared&&id==19){check(record.status==aion::DelayEditStatus::cancelled&&record.committed==aion::DelaySettings{},"clear wins only one owner record");continue;}
   check(record.committed==s.delays[id]&&record.requestId==p->inspectAudioThread(id).track.arpRequest,"D record owns exact joint receipt");
  }
 }
};
static void telemetryAndClear(unsigned block){Fixture f(block);const auto one=scene(1),two=scene(2),three=scene(3);f.load(one);
 const auto revision=Access::record(*f.p,0).revision;Access::stage(*f.p,one);f.service();
 f.wait([&]{f.pump();return f.p->inspectAudioThread(0).track.delaySettingsRevision>revision;},"identity restore audio revision");
 (void)Access::takeViews(*f.p);f.pump();Access::setView(*f.p,Access::takeViews(*f.p));Access::delayOnly(*f.p);
 check(!Access::bound(*f.p)&&Access::record(*f.p,0).revision==revision,"identical values cannot auto-bind an unconfirmed joint revision");
 f.service();f.fade();f.verify(one);const auto old=Access::view(*f.p);
 Access::stage(*f.p,two);f.service();f.wait([&]{f.pump();return nativeMatches(*f.p,two);},"audio joint adoption without editor polling");
 (void)Access::takeViews(*f.p);f.pump();const auto fresh=Access::takeViews(*f.p);Access::setView(*f.p,old);f.service();
 check(Access::pending(*f.p)&&Access::record(*f.p,0).committed==one.delays[0]&&Access::delays(*f.p)==two.delays,"receipt cannot confirm D before matching callback telemetry");
 Access::setView(*f.p,fresh);f.service();f.fade();f.verify(two);
 Access::stage(*f.p,three);f.service();f.wait([&]{f.pump();return nativeMatches(*f.p,three);},"unread ready before later clear");
 (void)Access::takeViews(*f.p);Access::rawClear(*f.p,19);f.pump();f.service();f.verify(three,true);f.fade();f.verify(three,true);
}
static void supersession(unsigned block){Fixture f(block);const auto one=scene(1),two=scene(2),three=scene(3);f.load(one);
 entered=false;released=false;pauseBytes=allocationSize();Access::stage(*f.p,two);f.service();f.pump();f.wait([&]{return entered.load(std::memory_order_acquire);},"paused joint D preparation");
 f.pump();f.service();check(nativeMatches(*f.p,one)&&Access::record(*f.p,0).committed==one.delays[0],"preparation leaves complete old bank");
 Access::stage(*f.p,three);f.pump();f.service();check(Access::delays(*f.p)==three.delays&&nativeMatches(*f.p,one),"obsolete cancellation cannot overwrite newest accepted intent");released.store(true,std::memory_order_release);
 f.wait([&]{f.pump();check(nativeMatches(*f.p,one)||nativeMatches(*f.p,three),"no obsolete or partial scene while late allocations finish");f.service();return !Access::pending(*f.p);},"latest joint scene recovery");f.fade();f.verify(three);
}
static void failureAndPrepare(unsigned block){Fixture f(block);const auto one=scene(1),two=scene(2),three=scene(3);f.load(one);
 failBytes=allocationSize();Access::stage(*f.p,two);f.service();f.settle();check(!failBytes&&Access::failed(*f.p),"real D allocation failure reported");f.verify(one);const auto revision=f.p->inspectAudioThread(0).track.delaySettingsRevision;f.fade();check(f.p->inspectAudioThread(0).track.delaySettingsRevision==revision,"no automatic failed-load retry");
 f.load(three);check(!Access::failed(*f.p),"explicit later load recovers after failure");
 entered=false;released=false;pauseBytes=allocationSize();Access::stage(*f.p,two);f.service();f.pump();f.wait([&]{return entered.load(std::memory_order_acquire);},"pause before host prepare");released.store(true,std::memory_order_release);
 f.p->prepareToPlay(48000,static_cast<int>(block));f.settle();f.fade();f.verify(two);check(!Access::failed(*f.p),"prepare re-stages newest accepted joint intent in new generation");
}
static void actualTimer(unsigned block){Fixture f(block);const auto two=scene(2),three=scene(3);Access::stage(*f.p,two);
 unsigned ticks=0;bool next=false;const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
 struct Pump final:juce::Timer{std::function<void()> tick;void timerCallback()override{tick();}} pump;
 pump.tick=[&]{f.pump();if(++ticks==20){Access::stage(*f.p,three);next=true;}if(next&&!Access::pending(*f.p)&&nativeMatches(*f.p,three))juce::MessageManager::getInstance()->stopDispatchLoop();check(std::chrono::steady_clock::now()<deadline,"actual closed-editor Timer deadline");};
 pump.startTimer(3);juce::MessageManager::getInstance()->runDispatchLoop();pump.stopTimer();check(next&&f.p->getActiveEditor()==nullptr,"closed-editor second load");f.verify(three);
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;const auto block=argc==2?static_cast<unsigned>(juce::String(argv[1]).getIntValue()):64;check(block==64||block==128,"64/128 callback");
 telemetryAndClear(block);supersession(block);failureAndPrepare(block);actualTimer(block);
 std::cout<<"PASS joint Processor owner "<<block<<": exact receipt/telemetry, clear, latest intent, OOM/prepare rollback, closed-editor actual Timer; zero callback heap operations\n";
}
