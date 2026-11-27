// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include "plugin/LocalCommandServer.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
namespace {
thread_local bool watching=false;
thread_local unsigned heap=0;
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
juce::String name(std::string_view s){return juce::String::fromUTF8(s.data(),static_cast<int>(s.size()));}
}
void* operator new(std::size_t n){if(watching)++heap;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watching&&p)++heap;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
int main(int argc,char** argv){
 juce::ScopedJuceInitialiser_GUI gui;
 const int block=argc==2?juce::String(argv[1]).getIntValue():64;check(block==64||block==128,"64/128");
 auto p=std::make_unique<AionProcessor>();p->setPlayConfigDetails(2,2,8000,block);p->prepareToPlay(8000,block);
 juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;midi.ensureSize(2048);
 auto pump=[&]{audio.clear();watching=true;p->processBlock(audio,midi);watching=false;check(!heap,"zero Reverb command callback new/delete");};
 auto snapshot=[&]{return p->request({juce::JSON::parse(R"({"operation":"snapshot"})")});};
 pump();auto s=snapshot();
 auto body=[&](const char* action){auto* o=new juce::DynamicObject;
  o->setProperty("operation","reverb");o->setProperty("track",19);o->setProperty("action",action);
  o->setProperty("generation",s["generation"]);o->setProperty("expiresAt",static_cast<double>(juce::Time::currentTimeMillis()+4000));return juce::var(o);};
 auto call=[&](const juce::var& request){return p->request({request});};
 auto settle=[&](juce::var reply){check(reply["ok"]&&reply["queued"],"Reverb admission");
  for(unsigned n=0;n<6000;++n){pump();s=snapshot();const auto t=s["tracks"][19];
   if(!static_cast<bool>(t["reverbPending"])){check(t["reverbRequestId"]==reply["requestId"]&&static_cast<bool>(t["reverbReady"]),"exact successful B receipt");
    // Drain the 20ms prepared-graph fade before the next edit.
    for(unsigned j=0;j<6;++j){pump();s=snapshot();}return;}
   std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }check(false,"B command deadline");};
 auto set=[&](unsigned control,double value){auto request=body("set");request.getDynamicObject()->setProperty("parameter",name(aion::reverbControls[control].name));request.getDynamicObject()->setProperty("value",value);return call(request);};
 auto r=set(1,.12345678901234566);
 check(!call(body("toggle"))["ok"],"one outstanding edit per track");
 juce::MemoryBlock pending;p->getStateInformation(pending);
 check(p->getXmlFromBinary(pending.getData(),static_cast<int>(pending.getSize()))->getChildByName("LoopReverbs")->getChildElement(19)->getDoubleAttribute("mix")==.12345678901234566,"accepted command intent saved immediately");settle(r);
 check(static_cast<double>(s["tracks"][19]["reverb"]["values"]["mix"])==.12345678901234566,"exact snapshot originals");
 for(unsigned control=0;control<aion::reverbControls.size();++control){const auto& c=aion::reverbControls[control];
  for(double value:{c.minimum,c.maximum,c.initial}){settle(set(control,value));check(p->inspectAudioThread(19).track.reverb.values[control]==value,"all controls and endpoints committed exactly");}}
 for(unsigned preset=0;preset<aion::reverbPresetNames.size();++preset){auto request=body("preset");request.getDynamicObject()->setProperty("preset",name(aion::reverbPresetNames[preset]));settle(call(request));
  aion::ReverbSettings expected;check(aion::applyReverbPreset(expected,preset+1),"valid preset oracle");check(p->inspectAudioThread(19).track.reverb==expected,"all named presets through production commands");}
 for(unsigned room=0;room<aion::reverbRooms.size();++room){auto request=body("room");request.getDynamicObject()->setProperty("room",name(aion::reverbRooms[room].name));settle(call(request));
  const auto settings=p->inspectAudioThread(19).track.reverb;check(static_cast<unsigned>(settings.room)==room&&settings.preset==0&&settings.values[2]==aion::reverbRooms[room].duration&&settings.values[3]==aion::reverbRooms[room].decay,"room selection retains original duration/decay semantics");}
 settle(call(body("reset")));settle(call(body("toggle")));check(p->inspectAudioThread(19).track.reverb.enabled,"prepared enable adopted");
 settle(set(1,.42));check(p->inspectAudioThread(19).track.reverb.values[1]==.42,"enabled live edit committed");
 auto invalid=[&](auto mutate){const auto before=p->inspectAudioThread(19).track.reverb;const auto saved=s["tracks"][19]["reverb"].clone();auto request=body("set");
  request.getDynamicObject()->setProperty("parameter","mix");request.getDynamicObject()->setProperty("value",.2);mutate(*request.getDynamicObject());
  check(!call(request)["ok"],"invalid Reverb command rejected");pump();s=snapshot();check(p->inspectAudioThread(19).track.reverb==before&&juce::JSON::toString(saved)==juce::JSON::toString(s["tracks"][19]["reverb"]),"rejection preserves audio and saved intent");};
 invalid([](auto& o){o.setProperty("track",20);});invalid([](auto& o){o.setProperty("track",-1);});invalid([](auto& o){o.setProperty("track","19");});
 invalid([](auto& o){o.setProperty("action","enable");o.setProperty("enabled",1);});
 invalid([](auto& o){o.setProperty("action","preset");o.setProperty("preset","missing");});
 invalid([](auto& o){o.setProperty("action","room");o.setProperty("room","missing");});
 invalid([](auto& o){o.setProperty("parameter","missing");});invalid([](auto& o){o.setProperty("value","0.2");});invalid([](auto& o){o.setProperty("value",true);});
 invalid([](auto& o){o.setProperty("value",2);});invalid([](auto& o){o.setProperty("generation",0);});invalid([](auto& o){o.setProperty("expiresAt",0);});
 invalid([](auto& o){o.setProperty("expiresAt",static_cast<double>(juce::Time::currentTimeMillis()+6000));});
 settle(call(body("reset")));check(p->inspectAudioThread(19).track.reverb.enabled&&p->inspectAudioThread(19).track.reverb.values==aion::reverbDefaults(),"reset preserves bypass and restores defaults");
 auto off=body("enable");off.getDynamicObject()->setProperty("enabled",false);settle(call(off));
 const auto root=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("reverb-cli-"+juce::Uuid().toString());
 {aion::LocalCommandServer server([&](const juce::var& v){return p->request({v});},root);check(server.ready(),"Reverb CLI server");juce::ChildProcess child;
  check(child.start(juce::StringArray{AION_CLI_PYTHON,AION_REVERB_CLI_TEST,root.getFullPathName(),server.id()}),"external Reverb CLI start");const auto deadline=juce::Time::getMillisecondCounterHiRes()+40000;
  struct Pump final:juce::Timer{std::function<void()> tick;void timerCallback()override{tick();}} timer;
  timer.tick=[&]{pump();if(!child.isRunning()||juce::Time::getMillisecondCounterHiRes()>deadline)juce::MessageManager::getInstance()->stopDispatchLoop();};timer.startTimer(3);juce::MessageManager::getInstance()->runDispatchLoop();timer.stopTimer();
  const bool timedOut=child.isRunning();if(timedOut)child.kill();std::cout<<child.readAllProcessOutput();check(!timedOut&&child.getExitCode()==0,"external Reverb CLI acceptance");
 }root.deleteRecursively();
 check(!p->inspectAudioThread(0).track.reverb.enabled,"other track unchanged");
 std::cout<<"PASS production Reverb commands "<<block<<": exact receipts, six controls/endpoints, 17 presets, eight rooms, strict rollback, persistence, reset/bypass and external CLI; zero callback heap\n";
}
