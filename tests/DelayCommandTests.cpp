// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include "plugin/LocalCommandServer.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <cstdlib>
#include <chrono>
#include <iostream>
#include <thread>
static thread_local bool watch=false;static unsigned heap=0;
void* operator new(std::size_t n){if(watch)++heap;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watch&&p)++heap;std::free(p);}void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
static void check(bool b,const char* why){if(!b){std::cerr<<why<<'\n';std::exit(1);}}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;const int block=argc==2?juce::String(argv[1]).getIntValue():64;check(block==64||block==128,"64/128");
 auto p=std::make_unique<AionProcessor>();p->setPlayConfigDetails(2,2,8000,block);p->prepareToPlay(8000,block);
 juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;midi.ensureSize(2048);
 auto pump=[&]{audio.clear();watch=true;p->processBlock(audio,midi);watch=false;check(!heap,"zero Delay command callback new/delete");};
 auto snapshot=[&]{return p->request({juce::JSON::parse(R"({"operation":"snapshot"})")});};pump();auto s=snapshot();
 auto request=[&](const char* json){auto r=juce::JSON::parse(json);r.getDynamicObject()->setProperty("generation",s["generation"]);r.getDynamicObject()->setProperty("expiresAt",static_cast<double>(juce::Time::currentTimeMillis()+4000));return p->request({r});};
 auto settle=[&](juce::var reply){check(reply["ok"]&&reply["queued"],"Delay admission");const auto id=static_cast<int>(reply["requestId"]);
  for(unsigned n=0;n<3000;++n){pump();s=snapshot();const auto t=s["tracks"][19];if(!static_cast<bool>(t["delayPending"])){check(static_cast<int>(t["delayRequestId"])==id&&static_cast<bool>(t["delayReady"]),"exact successful D receipt");for(unsigned j=0;j<6;++j){pump();s=snapshot();}return;}std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(false,"D command deadline");};
 auto r=request(R"({"operation":"delay","track":19,"action":"set","parameter":"time","value":0.12345678901234566})");
 check(!request(R"({"operation":"delay","track":19,"action":"toggle"})")["ok"],"one outstanding edit per track");
 juce::MemoryBlock pending;p->getStateInformation(pending);check(p->getXmlFromBinary(pending.getData(),static_cast<int>(pending.getSize()))->getChildByName("LoopDelays")->getChildElement(19)->getDoubleAttribute("time")==.12345678901234566,"accepted UI/CLI intent saved immediately");settle(r);
 check(static_cast<double>(s["tracks"][19]["delay"]["values"]["time"])==.12345678901234566,"exact snapshot originals");
 settle(request(R"({"operation":"delay","track":19,"action":"enable","enabled":true})"));
 for(int count:{1,2,3,10}){auto json=juce::String(R"({"operation":"delay","track":19,"action":"repeats","repeatMode":"exact","repeatCount":)")+juce::String(count)+"}";settle(request(json.toRawUTF8()));check(p->inspectAudioThread(19).track.delay.repeatCount==static_cast<unsigned>(count),"exact count native target");}
 const auto before=p->inspectAudioThread(19).track.delay;
 for(const auto* bad:{R"({"operation":"delay","track":20,"action":"toggle"})",R"({"operation":"delay","track":19,"action":"enable","enabled":1})",R"({"operation":"delay","track":19,"action":"repeats","repeatMode":"exact","repeatCount":0})",R"({"operation":"delay","track":19,"action":"set","parameter":"mix","value":2})",R"({"operation":"delay","track":19,"action":"preset","preset":"missing"})"})check(!request(bad)["ok"],"invalid Delay command rejected");
 check(p->inspectAudioThread(19).track.delay==before,"invalid commands preserve native settings");
 auto expired=juce::JSON::parse(R"({"operation":"delay","track":19,"action":"toggle","generation":0,"expiresAt":0})");check(!p->request({expired})["ok"],"expired generation rejected");
 settle(request(R"({"operation":"delay","track":19,"action":"reset"})"));check(p->inspectAudioThread(19).track.delay.enabled&&p->inspectAudioThread(19).track.delay.values==aion::delayDefaults(),"reset preserves bypass and restores defaults");
 settle(request(R"({"operation":"delay","track":19,"action":"enable","enabled":false})"));
 const auto root=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("delay-cli-"+juce::Uuid().toString());
 {aion::LocalCommandServer server([&](const juce::var& v){return p->request({v});},root);check(server.ready(),"Delay CLI server");juce::ChildProcess child;
  check(child.start(juce::StringArray{AION_CLI_PYTHON,AION_DELAY_CLI_TEST,root.getFullPathName(),server.id()}),"external Delay CLI start");const auto deadline=juce::Time::getMillisecondCounterHiRes()+40000;
  struct Pump final:juce::Timer{std::function<void()> tick;void timerCallback()override{tick();}} timer;
  timer.tick=[&]{pump();if(!child.isRunning()||juce::Time::getMillisecondCounterHiRes()>deadline)juce::MessageManager::getInstance()->stopDispatchLoop();};timer.startTimer(3);juce::MessageManager::getInstance()->runDispatchLoop();timer.stopTimer();
  if(child.isRunning())child.kill();std::cout<<child.readAllProcessOutput();check(child.getExitCode()==0,"external Delay CLI acceptance");
 }root.deleteRecursively();
 check(!p->inspectAudioThread(0).track.delay.enabled,"other track unchanged");
 std::cout<<"PASS production Delay commands "<<block<<": shared exact owner, precision, repeat controls, strict rejection, persistence, reset and external CLI; zero callback heap\n";
}
