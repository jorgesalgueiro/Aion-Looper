// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>
namespace {
thread_local bool watch=false;thread_local unsigned operations=0;
std::atomic<std::size_t> failBytes{0};
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
}
void* operator new(std::size_t n){if(watch)++operations;auto expected=n;if(failBytes.compare_exchange_strong(expected,0))throw std::bad_alloc();if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{if(watch&&p)++operations;std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
// Native-only injection of the owner that is otherwise unavailable to product
// controls. All service/prepare/barriers use the production Processor methods.
struct AionDelayProcessorTestAccess {
 static void message(AionProcessor& p){p.timerCallback();}
 static bool set(AionProcessor& p,unsigned id,unsigned parameter,double value,std::uint64_t frame=0){
  std::lock_guard guard(p.midiSettingsMutex);const auto request=p.nextRequest+1;
  if(!p.delayOwner.set(id,parameter,value,request,frame))return false;
  p.nextRequest=request;return true;
 }
 static bool restore(AionProcessor& p,const aion::DelaySettingsOwner::Bank& bank){
  std::lock_guard guard(p.midiSettingsMutex);const auto request=p.nextRequest+1;
  if(!p.delayOwner.restore(bank,request))return false;
  p.nextRequest=request;return true;
 }
 static bool pending(AionProcessor& p){std::lock_guard guard(p.midiSettingsMutex);
  bool result=p.delayRestorePending||p.delayRestoreActive;
  for(unsigned id=0;id<20;++id)result|=p.delayOwner.track(id)->pending;
  return result;
 }
 static bool bound(AionProcessor& p){std::lock_guard guard(p.midiSettingsMutex);return p.delayOwner.bound();}
 static auto shadow(AionProcessor& p){std::lock_guard guard(p.midiSettingsMutex);return p.delaySettings;}
 static auto record(AionProcessor& p,unsigned id){std::lock_guard guard(p.midiSettingsMutex);return *p.delayOwner.track(id);}
 static bool failed(AionProcessor& p){std::lock_guard guard(p.midiSettingsMutex);return p.delayRestoreFailed;}
 static void cancel(AionProcessor& p){p.engine.cancelDelayEdit();}
 static void fillQueue(AionProcessor& p){std::lock_guard guard(p.midiSettingsMutex);
  for(unsigned n=0;n<255;++n){const auto request=p.nextRequest+1;
   check(p.engine.enqueueLoop({aion::LoopAction::volume,19,request,p.engine.generation(),0,1}),"fill native ordinary FIFO");p.nextRequest=request;
  }
 }
};
using Access=AionDelayProcessorTestAccess;
struct Fixture {
 std::unique_ptr<AionProcessor> processor=std::make_unique<AionProcessor>();
 unsigned block;juce::AudioBuffer<float> audio;juce::MidiBuffer midi;
 explicit Fixture(unsigned size):block(size),audio(2,static_cast<int>(size)){
  midi.ensureSize(2048);processor->setPlayConfigDetails(2,2,8000,static_cast<int>(block));processor->prepareToPlay(8000,static_cast<int>(block));pump();service();check(Access::bound(*processor),"initial Processor D bind");
 }
 void pump(){audio.clear();operations=0;watch=true;processor->processBlock(audio,midi);watch=false;check(!operations,"Processor D audio heap operation");midi.clear();}
 void service(){Access::message(*processor);}
 void settle(){for(unsigned n=0;n<2000;++n){pump();service();if(!Access::pending(*processor))return;std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(false,"Processor D bounded completion");}
 void fade(){const auto frames=static_cast<unsigned>(std::ceil(.02*processor->inspectCallbackClockAudioThread().sampleRate));for(unsigned n=0;n<frames/block+2;++n){pump();service();}}
 juce::var barrier(const char* action,unsigned id=0){auto* object=new juce::DynamicObject;object->setProperty("operation","loop");object->setProperty("action",action);object->setProperty("track",static_cast<int>(id));
  object->setProperty("generation",static_cast<int>(processor->inspectCallbackClockAudioThread().generation));object->setProperty("expiresAt",static_cast<double>(juce::Time::currentTimeMillis()+4000));return processor->request({juce::var(object)});
 }
};
int main(int argc,char** argv){
 juce::ScopedJuceInitialiser_GUI gui;const unsigned block=argc==2?static_cast<unsigned>(juce::String(argv[1]).getIntValue()):64;
 check(block==64||block==128,"64/128 callback");Fixture f(block),other(block);
 check(f.processor->getActiveEditor()==nullptr&&other.processor->getActiveEditor()==nullptr,"no editors own these services");
 check(Access::set(*f.processor,19,0,.123456789012345),"internal settings admission");
 check(Access::shadow(*f.processor)[19].values[0]==.375,"pending candidate cannot overwrite confirmed Processor shadow");f.settle();
 check(Access::shadow(*f.processor)[19].values[0]==.123456789012345&&Access::shadow(*other.processor)[19]==aion::DelaySettings{},"production Timer service commits exact values, instances isolated");
 auto bank=Access::shadow(*f.processor);bank[0].enabled=bank[19].enabled=true;bank[0].values[0]=.01;
 bank[19].repeatMode=aion::DelayRepeatMode::exact;bank[19].repeatCount=2;
 check(Access::restore(*f.processor,bank),"internal native bank admission");f.settle();f.fade();
 check(Access::shadow(*f.processor)==bank&&f.processor->inspectAudioThread(19).track.delay==bank[19],"confirmed Processor bank reaches native D targets");
 // A higher ordinary barrier must not be stranded behind an obsolete future D.
 check(Access::set(*f.processor,19,3,.71,UINT64_MAX),"future edit before DEL");f.pump();
 check(f.barrier("clear",19)["ok"],"validated DEL admission");f.settle();
 check(Access::record(*f.processor,19).status==aion::DelayEditStatus::cancelled&&Access::shadow(*f.processor)[19]==aion::DelaySettings{}
  &&f.processor->inspectAudioThread(19).track.delay==aion::DelaySettings{},"message cancellation frees future head and DEL cannot revive D");
 check(Access::set(*f.processor,0,3,.72,UINT64_MAX),"future edit before panic");f.pump();check(f.barrier("stopAll")["ok"],"STOP ALL admission");f.settle();
 check(Access::record(*f.processor,0).status==aion::DelayEditStatus::cancelled&&Access::shadow(*f.processor)[0]==bank[0],"panic cancels pending D but preserves confirmed settings");
 check(Access::set(*f.processor,0,3,.73),"unread ready before stopped prepare");f.pump();
 f.processor->prepareToPlay(8000,static_cast<int>(block));f.settle();f.fade();
 check(Access::shadow(*f.processor)[0].values[3]==.73&&f.processor->inspectAudioThread(0).track.delay.values[3]==.73,"same-rate prepare retains unread committed precision");
 const auto saved=Access::shadow(*f.processor);f.processor->prepareToPlay(48000,static_cast<int>(block));f.settle();f.fade();
 check(!Access::failed(*f.processor)&&Access::shadow(*f.processor)==saved&&f.processor->inspectAudioThread(0).track.delay==saved[0],"changed-rate D lifecycle restore commits through exact bank receipt");
 check(Access::set(*f.processor,0,3,.74,UINT64_MAX),"pending edit before release");f.pump();f.processor->releaseResources();
 f.processor->prepareToPlay(48000,static_cast<int>(block));f.settle();
 check(Access::shadow(*f.processor)==saved,"release/reprepare cancels candidate without persisting it");
 check(f.barrier("clear",0)["ok"],"queued DEL before stopped prepare");
 f.processor->prepareToPlay(48000,static_cast<int>(block));f.settle();f.fade();
 check(Access::shadow(*f.processor)[0]==aion::DelaySettings{}&&!f.processor->inspectAudioThread(0).track.delay.enabled,"accepted but unrendered DEL survives prepare without restoring old D intent");
 check(Access::restore(*f.processor,saved),"restore internal bank for remaining lifecycle tests");f.settle();f.fade();
 check(Access::shadow(*f.processor)==saved&&f.processor->inspectAudioThread(0).track.delay==saved[0],"re-enabled bank is confirmed before remaining tests");
 // Invalid barrier validation must not cancel an otherwise accepted edit.
 check(Access::set(*f.processor,0,3,.75,UINT64_MAX),"future edit before invalid barrier");f.pump();
 auto* bad=new juce::DynamicObject;bad->setProperty("operation","loop");bad->setProperty("action","clear");bad->setProperty("track",20);
 bad->setProperty("generation",static_cast<int>(f.processor->inspectCallbackClockAudioThread().generation));bad->setProperty("expiresAt",static_cast<double>(juce::Time::currentTimeMillis()+4000));
 check(!static_cast<bool>(f.processor->request({juce::var(bad)})["ok"]),"invalid DEL rejected");f.pump();f.service();check(Access::record(*f.processor,0).pending,"invalid barrier cannot cancel a valid pending edit");Access::cancel(*f.processor);f.settle();
 check(Access::set(*f.processor,0,3,.76,UINT64_MAX),"future edit before full FIFO barrier");f.pump();Access::fillQueue(*f.processor);
 check(!static_cast<bool>(f.barrier("clear",0)["ok"]),"full FIFO DEL rejected");f.pump();f.service();
 check(Access::record(*f.processor,0).pending&&Access::shadow(*f.processor)==saved,"failed barrier admission changes neither pending ownership nor clear intent");Access::cancel(*f.processor);f.settle();f.fade();
 check(Access::set(*f.processor,0,3,.77,UINT64_MAX),"future edit before host state load");f.pump();
 f.processor->setStateInformation("bad",3);f.service();check(Access::record(*f.processor,0).pending,"invalid state load cannot cancel accepted D intent");
 juce::MemoryBlock state;f.processor->getStateInformation(state);f.processor->setStateInformation(state.getData(),static_cast<int>(state.getSize()));f.settle();f.fade();
 check(Access::record(*f.processor,0).status==aion::DelayEditStatus::cancelled&&Access::shadow(*f.processor)==saved,"validated host restore cancels obsolete D edit without a second receipt consumer");
 check(Access::shadow(*f.processor)==saved,"cancelled invalid-barrier candidate preserves prior shadow");
 f.processor->prepareToPlay(96000,static_cast<int>(block));
 check(Access::shadow(*f.processor)==saved&&Access::pending(*f.processor),"prepare retains confirmed intent and schedules restoration");
 auto allocationBytes=aion::Delay::requiredStorageFrames(96000,saved[0])*sizeof(std::array<float,2>);
 #if defined(_MSC_VER) && defined(_WIN64)
 // Pinned MSVC xmemory overallocates large vector blocks for 32-byte alignment.
 // Fail that exact operator-new size, not just the vector's user payload.
 allocationBytes+=sizeof(void*)+31;
 #ifdef _DEBUG
 allocationBytes+=sizeof(void*);
 #endif
 #endif
 failBytes=allocationBytes;f.settle();
 if(failBytes||!Access::failed(*f.processor)||Access::shadow(*f.processor)!=saved||f.processor->inspectAudioThread(0).track.delay.enabled){
  std::cerr<<"OOM bytes="<<failBytes<<" failed="<<Access::failed(*f.processor)<<" shadowMatches="<<(Access::shadow(*f.processor)==saved)<<" enabled="<<f.processor->inspectAudioThread(0).track.delay.enabled<<" pending="<<Access::pending(*f.processor)<<'\n';
 }
 check(!failBytes&&Access::failed(*f.processor)&&Access::shadow(*f.processor)==saved&&!f.processor->inspectAudioThread(0).track.delay.enabled,"failed changed-rate allocation retains explicit prior intent without claiming restored audio");
 const auto failedRevision=f.processor->inspectAudioThread(0).track.delaySettingsRevision;f.fade();
 check(f.processor->inspectAudioThread(0).track.delaySettingsRevision==failedRevision&&!Access::pending(*f.processor),"failed restore does not spin automatic allocation retries");
 check(f.barrier("clear",0)["ok"],"clear after failed restore");f.settle();
 check(Access::shadow(*f.processor)[0]==aion::DelaySettings{},"clear overrides retained failed-restore intent");
 f.processor->prepareToPlay(96000,static_cast<int>(block));f.settle();
 check(!Access::failed(*f.processor)&&!f.processor->inspectAudioThread(0).track.delay.enabled,"later prepare cannot resurrect cleared failed intent");
 check(Access::shadow(*other.processor)==aion::DelaySettingsOwner::Bank{},"other Processor remained unchanged");
 std::cout<<"PASS native Processor Delay "<<block<<": editor-free message service, exact shadows, DEL/panic/release, rate restore and instance isolation; zero callback heap operations\n";
}
