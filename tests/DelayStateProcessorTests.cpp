// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <thread>
namespace {
thread_local bool watch=false;thread_local unsigned heapOperations=0;
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
using Bank=aion::DelaySettingsOwner::Bank;
juce::String key(std::string_view name){return juce::String::fromUTF8(name.data(),static_cast<int>(name.size()));}
}
void* operator new(std::size_t n){if(watch)++heapOperations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watch&&p)++heapOperations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
// Public binary state is the only input under test. Friend access observes
// completion and drives the production message service without an editor.
struct AionJointEffectsProcessorTestAccess {
 static void message(AionProcessor& p){p.timerCallback();}
 static bool pending(AionProcessor& p){std::lock_guard guard(p.midiSettingsMutex);
  return p.effectsRestore.active||p.eqRestorePending||p.compressorRestorePending||p.chainRestorePending||p.driveRestorePending||p.arpRestorePending||p.delayRestorePending;
 }
 static bool bound(AionProcessor& p){std::lock_guard guard(p.midiSettingsMutex);return p.delayOwner.bound()&&p.arpOwner.bound();}
 static auto status(AionProcessor& p){std::lock_guard guard(p.midiSettingsMutex);return p.arpOwner.restoreReceipt().status;}
 static void diagnostic(AionProcessor& p){std::lock_guard guard(p.midiSettingsMutex);std::cerr<<"joint status="<<static_cast<unsigned>(p.arpOwner.restoreReceipt().status)<<" failed="<<p.delayRestoreFailed<<'\n';}
};
using Access=AionJointEffectsProcessorTestAccess;
struct Fixture {
 std::unique_ptr<AionProcessor> p=std::make_unique<AionProcessor>();unsigned block;
 juce::AudioBuffer<float> audio;juce::MidiBuffer midi;
 explicit Fixture(unsigned size):block(size),audio(2,static_cast<int>(size)){
  midi.ensureSize(2048);p->setPlayConfigDetails(2,2,8000,static_cast<int>(block));p->prepareToPlay(8000,static_cast<int>(block));pump();service();
 }
 void pump(){audio.clear();heapOperations=0;watch=true;p->processBlock(audio,midi);watch=false;check(!heapOperations,"state restore callback new/delete");midi.clear();}
 void zero(){audio.setSize(2,0,false,false,true);pump();audio.setSize(2,static_cast<int>(block),false,false,true);}
 void service(){Access::message(*p);}
 void settle(bool finishTransition=true){for(unsigned n=0;n<5000;++n){pump();service();if(!Access::pending(*p)){
   // Receipt adoption precedes the existing 20ms owner crossfade. Ordinary
   // roundtrip cases begin from a settled graph; the explicit busy case below
   // intentionally omits this drain and verifies rejection/rollback instead.
   if(finishTransition)for(unsigned j=0;j<static_cast<unsigned>(.02*p->inspectCallbackClockAudioThread().sampleRate)/block+2;++j){pump();service();}
   return;
  }std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(false,"state restore completion deadline");}
 juce::MemoryBlock save(){juce::MemoryBlock b;p->getStateInformation(b);return b;}
 static std::unique_ptr<juce::XmlElement> xml(const juce::MemoryBlock& b){return AionProcessor::getXmlFromBinary(b.getData(),static_cast<int>(b.getSize()));}
 static juce::MemoryBlock binary(const juce::XmlElement& x){juce::MemoryBlock b;AionProcessor::copyXmlToBinary(x,b);return b;}
 void load(const juce::MemoryBlock& b){p->setStateInformation(b.getData(),static_cast<int>(b.getSize()));}
 juce::var clear(const char* action,unsigned id=0){auto* o=new juce::DynamicObject;o->setProperty("operation","loop");o->setProperty("action",action);o->setProperty("track",static_cast<int>(id));o->setProperty("generation",static_cast<int>(p->inspectCallbackClockAudioThread().generation));o->setProperty("expiresAt",static_cast<double>(juce::Time::currentTimeMillis()+4000));return p->request({juce::var(o)});}
 unsigned verification=0;
 void verify(const Bank& expected){++verification;check(Access::bound(*p),"both exact settings owners bound");
  auto saved=xml(save());auto* bank=saved->getChildByName("LoopDelays");check(bank&&bank->getIntAttribute("version")==1&&bank->getNumChildElements()==20,"versioned complete D bank");
  for(unsigned id=0;id<20;++id){const auto native=p->inspectAudioThread(id).track;const auto* row=bank->getChildElement(static_cast<int>(id));const auto& s=expected[id];
   if(native.delay!=s){std::cerr<<"verification="<<verification<<" track="<<id<<" enabled="<<native.delay.enabled<<"/"<<s.enabled<<" time="<<std::setprecision(17)<<native.delay.values[0]<<"/"<<s.values[0]<<" mode="<<static_cast<unsigned>(native.delay.repeatMode)<<"/"<<static_cast<unsigned>(s.repeatMode)<<'\n';Access::diagnostic(*p);}
   check(native.delay==s,"native D matches serialized candidate exactly");check(native.delayRequest==native.arpRequest&&native.delayRequest==native.eqRequest&&native.delayRequest==native.chainRequest,"single joint D/A/Q/chain identity");
   check(row->getIntAttribute("track")==static_cast<int>(id)&&row->getBoolAttribute("enabled")==s.enabled&&row->getIntAttribute("preset")==s.preset,"D identity/enabled/preset persisted");
   check(row->getStringAttribute("repeatMode")== (s.repeatMode==aion::DelayRepeatMode::exact?"exact":"feedback")&&row->getIntAttribute("repeatCount")==static_cast<int>(s.repeatCount),"D repeat modes/count persisted");
   for(unsigned j=0;j<aion::delayControls.size();++j)check(row->getDoubleAttribute(key(aion::delayControls[j].name))==s.values[j],"max_digits10 original D precision");
  }
 }
};
static Bank scene(unsigned variant){Bank bank{};
 for(unsigned id=0;id<20;++id){auto& s=bank[id];s.enabled=id==0||id==19;
  s.values={.0123456789012345+variant*.001+id*.0001,static_cast<double>(id%11),1234.56789012345,.234567890123456+variant*.1,.123456789012345,.876543210987654,static_cast<double>(id%2)};
  s.repeatMode=id%2?aion::DelayRepeatMode::exact:aion::DelayRepeatMode::feedback;s.repeatCount=1+id%10;check(aion::validDelaySettings(s),"valid test scene");
 }check(aion::applyDelayPreset(bank[5],9),"named original preset");return bank;
}
static juce::MemoryBlock stateWith(const juce::MemoryBlock& base,const Bank& settings){auto x=Fixture::xml(base);auto* bank=x->getChildByName("LoopDelays");check(bank,"new save includes Delay schema");
 for(unsigned id=0;id<20;++id){auto* row=bank->getChildElement(static_cast<int>(id));const auto& s=settings[id];row->setAttribute("enabled",s.enabled?1:0);row->setAttribute("preset",s.preset);
  row->setAttribute("repeatMode",s.repeatMode==aion::DelayRepeatMode::exact?"exact":"feedback");row->setAttribute("repeatCount",static_cast<int>(s.repeatCount));
  // Reference expectations are the parsed fixture strings, independent of the production serializer.
  for(unsigned j=0;j<aion::delayControls.size();++j){std::ostringstream text;text<<std::setprecision(std::numeric_limits<double>::max_digits10)<<s.values[j];row->setAttribute(key(aion::delayControls[j].name),juce::String(text.str()));}
 }return Fixture::binary(*x);
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;const unsigned block=argc==2?static_cast<unsigned>(juce::String(argv[1]).getIntValue()):64;check(block==64||block==128,"64/128");
 Fixture f(block),other(block);const auto initial=f.save();const auto one=scene(1),two=scene(2);const auto a=stateWith(initial,one),b=stateWith(initial,two);
 f.load(a);check(Access::pending(*f.p),"host state publishes joint intent");
 auto immediate=Fixture::xml(f.save());check(immediate->getChildByName("LoopDelays")->getChildElement(19)->getDoubleAttribute("time")==one[19].values[0],"accepted not-yet-admitted intent saved immediately");f.settle();f.verify(one);
 auto first=f.save();other.load(first);other.settle();other.verify(one); // independent instance, public roundtrip
 // Stopped host load before first prepare also retains the exact new fields.
 {auto stopped=std::make_unique<AionProcessor>();stopped->setStateInformation(first.getData(),static_cast<int>(first.getSize()));stopped->setPlayConfigDetails(2,2,8000,static_cast<int>(block));stopped->prepareToPlay(8000,static_cast<int>(block));
  f.p.swap(stopped);f.settle();f.verify(one);f.p.swap(stopped);}
 unsigned rejected=0;auto invalid=[&](auto mutate){auto x=Fixture::xml(first);mutate(*x);x->getChildByName("LoopEqualizers")->getChildElement(0)->setAttribute("p1Gain",9);
  for(auto* child:x->getChildIterator())if(child->hasTagName("PARAM")&&child->getStringAttribute("id")=="masterGain")child->setAttribute("value",.123);
  const auto before=f.save();const bool wasPending=Access::pending(*f.p);auto bad=Fixture::binary(*x);f.load(bad);check(Access::pending(*f.p)==wasPending,"malformed state preserves pending restore ownership");const auto after=f.save();check(before==after,"malformed D rejects entire APVTS/MIDI/FX state unchanged");++rejected;
 };
 invalid([](auto& x){x.getChildByName("LoopDelays")->setAttribute("version",2);});
 invalid([](auto& x){x.getChildByName("LoopDelays")->removeAttribute("version");});
 invalid([](auto& x){x.addChildElement(new juce::XmlElement(*x.getChildByName("LoopDelays")));});
 invalid([](auto& x){x.getChildByName("LoopDelays")->removeChildElement(x.getChildByName("LoopDelays")->getChildElement(19),true);});
 invalid([](auto& x){x.getChildByName("LoopDelays")->getChildElement(19)->setAttribute("track",0);});
 invalid([](auto& x){x.getChildByName("LoopDelays")->getChildElement(19)->setTagName("Unknown");});
 invalid([](auto& x){x.getChildByName("LoopDelays")->getChildElement(19)->createNewChildElement("Nested");});
 for(const auto* field:{"track","enabled","preset","repeatMode","repeatCount","time","repetitions","damp","mix","panSpeed","panDepth","sync"})invalid([&](auto& x){x.getChildByName("LoopDelays")->getChildElement(19)->removeAttribute(field);});
 for(const auto* value:{"nan","inf","-inf","","0.1junk","1e9999","-0.1","4.1"," 0.2"})invalid([&](auto& x){x.getChildByName("LoopDelays")->getChildElement(19)->setAttribute("time",value);});
 for(const auto* field:{"track","preset","repeatCount"})for(const auto* value:{"-1","1.0","4294967296","999999999999999999999999",""})invalid([&](auto& x){x.getChildByName("LoopDelays")->getChildElement(19)->setAttribute(field,value);});
 for(const auto* value:{"0","11"})invalid([&](auto& x){x.getChildByName("LoopDelays")->getChildElement(19)->setAttribute("repeatCount",value);});
 invalid([](auto& x){x.getChildByName("LoopDelays")->getChildElement(19)->setAttribute("repeatMode","unknown");});
 invalid([](auto& x){x.getChildByName("LoopDelays")->getChildElement(19)->setAttribute("enabled","true");});
 invalid([](auto& x){x.getChildByName("LoopDelays")->getChildElement(19)->setAttribute("sync",.5);});
 invalid([](auto& x){x.getChildByName("LoopDelays")->getChildElement(19)->setAttribute("preset",10);});
 invalid([](auto& x){x.getChildByName("LoopDelays")->getChildElement(19)->setAttribute("preset",1);});
 invalid([](auto& x){x.getChildByName("LoopDelays")->getChildElement(5)->setAttribute("repeatMode","exact");});
 for(unsigned j=0;j<aion::delayControls.size();++j)for(const bool high:{false,true})invalid([&](auto& x){const auto& c=aion::delayControls[j];x.getChildByName("LoopDelays")->getChildElement(19)->setAttribute(key(c.name),high?c.maximum+1:c.minimum-1);});
 f.verify(one);
 f.load(b);f.service();invalid([](auto& x){x.getChildByName("LoopDelays")->getChildElement(19)->setAttribute("mix","nan");});f.settle();f.verify(two); // rejected candidate cannot cancel an admitted valid load
 f.load(a);f.settle();
 f.load(b);f.settle(false);f.verify(two);f.load(a);f.service();f.zero();f.service();f.zero();f.service();
 check(Access::status(*f.p)==aion::ArpPreparationStatus::busy,"zero-frame admission during owner crossfade explicitly reports busy");f.settle();f.verify(two);
 f.load(a);f.settle();f.verify(one); // explicit later load recovers after bounded busy rejection
 // A newer load follows a still-unprocessed older DEL. A later DEL after
 // adoption resets only its own track (preparing-bank cancellation is covered
 // separately by the existing joint-owner tests).
 check(f.clear("clear",19)["ok"],"older queued DEL admitted");f.load(b);f.settle();f.verify(two);
 f.load(a);f.settle();check(f.clear("clear",19)["ok"],"newer DEL admitted");f.settle();auto cleared=one;cleared[19]={};f.verify(cleared);
 // Supersede while the previous result is unread and telemetry is saturated.
 f.load(a);f.service();for(unsigned n=0;n<32;++n)f.pump();f.load(b);auto newest=Fixture::xml(f.save());check(newest->getChildByName("LoopDelays")->getChildElement(0)->getDoubleAttribute("mix")==two[0].values[3],"unread old completion cannot overwrite newest saved intent");f.settle();f.verify(two);
 auto legacy=Fixture::xml(first);legacy->removeChildElement(legacy->getChildByName("LoopDelays"),true);f.load(Fixture::binary(*legacy));f.settle();f.verify(Bank{}); // old native session resets D defaults/off
 f.load(a);f.settle();f.p->prepareToPlay(48000,static_cast<int>(block));f.settle();f.verify(one);other.verify(one);
 check(f.p->getActiveEditor()==nullptr&&other.p->getActiveEditor()==nullptr,"state independent of editors/instances");
 std::cout<<"PASS Delay state "<<block<<": exact versioned roundtrip, "<<rejected<<" whole-state rejections, legacy defaults, ordered clear/load, supersession, stopped/rate prepare, isolated instances; zero callback heap operations\n";
}
