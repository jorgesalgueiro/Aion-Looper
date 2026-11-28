// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <thread>
namespace {
thread_local unsigned heapOperations=0;
thread_local bool watching=false;
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
using Bank=aion::ReverbSettingsOwner::Bank;
juce::String key(std::string_view name){return juce::String::fromUTF8(name.data(),static_cast<int>(name.size()));}
}
void* operator new(std::size_t n){if(watching)++heapOperations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watching&&p)++heapOperations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
struct AionReverbStateTestAccess {
 static void message(AionProcessor& p){p.timerCallback();}
 static bool pending(AionProcessor& p){std::lock_guard guard(p.midiSettingsMutex);
  if(p.reverbRestorePending||p.reverbOwner.restoring()||p.reverbRestoreFailed)return true;
  for(unsigned id=0;id<aion::LoopBank::trackCount;++id){
   if(p.reverbOwner.track(id)->pending)return true;
   if(p.reverbSettings[id]!=p.reverbOwner.track(id)->committed)return true;
  }
  return false;}
 static bool bound(AionProcessor& p){std::lock_guard guard(p.midiSettingsMutex);return p.reverbOwner.bound();}
 static bool failed(AionProcessor& p){std::lock_guard guard(p.midiSettingsMutex);return p.reverbRestoreFailed;}
 static void service(AionProcessor& p){std::lock_guard guard(p.midiSettingsMutex);p.serviceReverb();}
};
using Access=AionReverbStateTestAccess;
struct Fixture {
 std::unique_ptr<AionProcessor> p=std::make_unique<AionProcessor>();unsigned block;
 juce::AudioBuffer<float> audio;juce::MidiBuffer midi;
 explicit Fixture(unsigned size):block(size),audio(2,static_cast<int>(size)){
  midi.ensureSize(2048);p->setPlayConfigDetails(2,2,8000,static_cast<int>(block));p->prepareToPlay(8000,static_cast<int>(block));pump();service();
 }
 void pump(){audio.clear();heapOperations=0;watching=true;p->processBlock(audio,midi);watching=false;check(heapOperations==0,"state restore callback new/delete");midi.clear();}
 void service(){Access::message(*p);}
 void settle(bool finishTransition=true){
  for(unsigned n=0;n<6000;++n){pump();service();if(!Access::pending(*p)){
   if(finishTransition)for(unsigned j=0;j<static_cast<unsigned>(.02*p->inspectCallbackClockAudioThread().sampleRate)/block+2;++j){pump();service();}
   return;
  }std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  check(false,"reverb state restore completion deadline");
 }
 juce::MemoryBlock save(){juce::MemoryBlock b;p->getStateInformation(b);return b;}
 static std::unique_ptr<juce::XmlElement> xml(const juce::MemoryBlock& b){return AionProcessor::getXmlFromBinary(b.getData(),static_cast<int>(b.getSize()));}
 static juce::MemoryBlock binary(const juce::XmlElement& x){juce::MemoryBlock b;AionProcessor::copyXmlToBinary(x,b);return b;}
 void load(const juce::MemoryBlock& b){p->setStateInformation(b.getData(),static_cast<int>(b.getSize()));}
 void verify(const Bank& expected){check(Access::bound(*p),"reverb owner bound");
  auto saved=xml(save());auto* bank=saved->getChildByName("LoopReverbs");check(bank&&bank->getIntAttribute("version")==2&&bank->getNumChildElements()==20,"versioned complete B bank");
  for(unsigned id=0;id<20;++id){const auto native=p->inspectAudioThread(id).track;const auto* row=bank->getChildElement(static_cast<int>(id));const auto& s=expected[id];
   check(native.reverb==s,"native B matches serialized candidate exactly");
   check(row->getIntAttribute("track")==static_cast<int>(id)&&row->getBoolAttribute("enabled")==s.enabled&&row->getIntAttribute("preset")==s.preset,"B identity/enabled/preset persisted");
   check(row->getStringAttribute("room")==key(aion::reverbRooms[static_cast<unsigned>(s.room)].name),"B room persisted");
   for(unsigned j=0;j<aion::reverbControls.size();++j)check(row->getDoubleAttribute(key(aion::reverbControls[j].name))==s.values[j],"max_digits10 original B precision");
  }
 }
};
static Bank scene(unsigned variant){Bank bank{};
 for(unsigned id=0;id<20;++id){auto& s=bank[id];s.enabled=id==0||id==19;
  s.values={.123456789012345+variant*.001+id*.0001,.5+.01*(id%5),.1+.02*(id%10),.5+.1*(id%4),8000.123456789+id,.01*id};
  s.room=aion::ReverbRoom::studio;s.preset=0;check(aion::validReverbSettings(s),"valid test scene");
 }check(aion::applyReverbPreset(bank[5],5),"named original preset");return bank;
}
static juce::MemoryBlock stateWith(const juce::MemoryBlock& base,const Bank& settings){
 auto x=Fixture::xml(base);auto* bank=x->getChildByName("LoopReverbs");check(bank,"new save includes B schema");
 for(unsigned id=0;id<20;++id){auto* row=bank->getChildElement(static_cast<int>(id));const auto& s=settings[id];
  row->setAttribute("enabled",s.enabled?1:0);row->setAttribute("preset",s.preset);
  row->setAttribute("room",key(aion::reverbRooms[static_cast<unsigned>(s.room)].name));
  for(unsigned j=0;j<aion::reverbControls.size();++j){std::ostringstream text;text<<std::setprecision(std::numeric_limits<double>::max_digits10)<<s.values[j];row->setAttribute(key(aion::reverbControls[j].name),juce::String(text.str()));}
 }
 return Fixture::binary(*x);
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;const unsigned block=argc==2?static_cast<unsigned>(juce::String(argv[1]).getIntValue()):64;check(block==64||block==128,"64/128");
 Fixture f(block),other(block);const auto initial=f.save();const auto one=scene(1),two=scene(2);const auto a=stateWith(initial,one),b=stateWith(initial,two);
 f.load(stateWith(initial,one));check(Access::pending(*f.p),"host state publishes B intent");
 {auto immediate=Fixture::xml(f.save());check(immediate->getChildByName("LoopReverbs")->getChildElement(19)->getDoubleAttribute("mix")==one[19].values[1],"accepted not-yet-admitted B intent saved immediately");}
 f.settle();f.verify(one);
 const auto first=f.save();other.load(first);other.settle();other.verify(one);
 {auto stopped=std::make_unique<AionProcessor>();stopped->setStateInformation(first.getData(),static_cast<int>(first.getSize()));stopped->setPlayConfigDetails(2,2,8000,static_cast<int>(block));stopped->prepareToPlay(8000,static_cast<int>(block));
  f.p.swap(stopped);f.pump();f.settle();f.verify(one);f.p.swap(stopped);}
 unsigned rejected=0;auto invalid=[&](auto mutate){auto x=Fixture::xml(first);mutate(*x);x->getChildByName("LoopEqualizers")->getChildElement(0)->setAttribute("p1Gain",9);
  const auto before=f.save();const bool wasPending=Access::pending(*f.p);const auto bad=Fixture::binary(*x);f.load(bad);
  check(Access::pending(*f.p)==wasPending,"malformed state preserves pending ownership");check(before==f.save(),"malformed B rejects entire state unchanged");++rejected;
 };
 invalid([](auto& x){x.getChildByName("LoopReverbs")->setAttribute("version",3);});
 invalid([](auto& x){x.getChildByName("LoopReverbs")->removeAttribute("version");});
 invalid([](auto& x){x.addChildElement(new juce::XmlElement(*x.getChildByName("LoopReverbs")));});
 invalid([](auto& x){x.getChildByName("LoopReverbs")->removeChildElement(x.getChildByName("LoopReverbs")->getChildElement(19),true);});
 invalid([](auto& x){x.getChildByName("LoopReverbs")->getChildElement(19)->setAttribute("track",0);});
 invalid([](auto& x){x.getChildByName("LoopReverbs")->getChildElement(19)->setTagName("Unknown");});
 invalid([](auto& x){x.getChildByName("LoopReverbs")->getChildElement(19)->createNewChildElement("Nested");});
 for(const auto* field:{"track","enabled","room","preset","volume","mix","duration","decay","damp","predelay"})invalid([&](auto& x){x.getChildByName("LoopReverbs")->getChildElement(19)->removeAttribute(field);});
 for(const auto* value:{"nan","inf","-inf","","0.1junk","1e9999","-0.1","3.1"," 0.2"})invalid([&](auto& x){x.getChildByName("LoopReverbs")->getChildElement(19)->setAttribute("volume",value);});
 invalid([](auto& x){x.getChildByName("LoopReverbs")->getChildElement(19)->setAttribute("enabled","true");});
 invalid([](auto& x){x.getChildByName("LoopReverbs")->getChildElement(19)->setAttribute("room","unknown");});
 invalid([](auto& x){x.getChildByName("LoopReverbs")->getChildElement(19)->setAttribute("preset",18);});
 invalid([](auto& x){x.getChildByName("LoopReverbs")->getChildElement(5)->setAttribute("preset",1);});
 invalid([](auto& x){x.getChildByName("LoopReverbs")->getChildElement(19)->setAttribute("duration",6);});
 f.verify(one);
 // A rejected candidate cannot cancel an admitted older load; the valid one
 // still commits, and a pre-B state restores original defaults.
 f.load(b);f.service();invalid([](auto& x){x.getChildByName("LoopReverbs")->getChildElement(5)->setAttribute("preset",1);});f.settle();f.verify(two);
 auto legacy=Fixture::xml(first);legacy->removeChildElement(legacy->getChildByName("LoopReverbs"),true);f.load(Fixture::binary(*legacy));f.settle();
 for(unsigned id=0;id<20;++id)check(f.p->inspectAudioThread(id).track.reverb==aion::ReverbSettings{},"pre-B state restores original defaults");
 check(!Access::failed(*f.p),"pre-B defaults do not become a failure");
 // Shared command layer used by the product GUI/CLI. The same request path is
 // exercised without an editor; no alternate audio/state owner is created.
 const auto call=[&](const juce::var& body){return f.p->request({body});};
 const auto requestObject=[&](const char* action,auto configure){
  auto* o=new juce::DynamicObject;o->setProperty("operation","reverb");o->setProperty("action",action);o->setProperty("track",0);
  o->setProperty("generation",static_cast<int>(f.p->inspectCallbackClockAudioThread().generation));
  o->setProperty("expiresAt",static_cast<double>(juce::Time::currentTimeMillis()+3000));configure(*o);return juce::var(o);
 };
 check(call(requestObject("set",[](auto& o){o.setProperty("parameter","mix");o.setProperty("value",.33);}))["ok"],"reverb set admitted");
 f.settle();check(f.p->inspectAudioThread(0).track.reverb.values[1]==.33,"reverb set committed natively");
 check(call(requestObject("toggle",[](auto& o){(void)o;}))["ok"],"reverb toggle admitted");
 f.settle();check(f.p->inspectAudioThread(0).track.reverb.enabled,"reverb toggle enabled natively");
 // A panic before the callback cancels an admitted individual edit. Servicing
 // its receipt must roll intent back, never resubmit it as a lifecycle bank.
 const auto beforeCancel=f.p->inspectAudioThread(0).track.reverb;
 check(call(requestObject("set",[](auto& o){o.setProperty("parameter","mix");o.setProperty("value",.71);}))["ok"],"cancel candidate admitted");
 auto panic=requestObject("stopAll",[](auto& o){o.setProperty("operation","loop");});
 check(call(panic)["ok"],"panic admitted before edit callback");
 f.settle();
 check(f.p->inspectAudioThread(0).track.reverb==beforeCancel,"cancelled reverb edit must not be replayed by state service");
 {auto saved=Fixture::xml(f.save());check(saved->getChildByName("LoopReverbs")->getChildElement(0)->getDoubleAttribute("mix")==.33,"cancelled intent rolls back in saved state");}
 check(call(requestObject("set",[](auto& o){o.setProperty("parameter","mix");o.setProperty("value",.33);}))["ok"],"fresh command after cancellation admitted");f.settle();
 {auto* o=new juce::DynamicObject;o->setProperty("operation","snapshot");const auto snap=call(juce::var(o));
  check(snap["tracks"][0]["reverb"]["values"]["mix"].toString().getDoubleValue()==.33,"snapshot exposes reverb values");
  check(static_cast<bool>(snap["tracks"][0]["reverbReady"]),"snapshot reports reverb ready");}
 // A later load owns its candidate even when an older FIFO clear is not yet
 // reflected in telemetry. A fresh clear after that load still wins.
 check(call(requestObject("clear",[](auto& o){o.setProperty("operation","loop");}))["ok"],"clear before load admitted");
 f.load(a);f.settle();f.verify(one);
 f.load(b);f.service();
 check(call(requestObject("clear",[](auto& o){o.setProperty("operation","loop");}))["ok"],"clear after load admitted");
 auto cleared=two;cleared[0]={};f.settle();f.verify(cleared);
 // Superseding an individual edit or an in-flight bank must preserve the most
 // recent accepted load, including a return to the old committed settings.
 check(call(requestObject("set",[](auto& o){o.setProperty("parameter","mix");o.setProperty("value",.71);}))["ok"],"edit before load admitted");
 f.load(a);f.settle();f.verify(one);
 f.load(b);f.service();f.load(a);f.settle();f.verify(one);
 std::cout<<"PASS Reverb state "<<block<<": exact versioned roundtrip, "<<rejected<<" whole-state rejections, pre-B defaults, stopped prepare, command cancellation and ordered load/clear recovery; zero callback heap operations\n";
}
