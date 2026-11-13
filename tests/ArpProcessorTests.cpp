// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include "plugin/LocalCommandServer.h"
#include <cstdlib>
#include <iostream>
#include <thread>
static thread_local bool watch=false;static unsigned allocations=0;
void* operator new(std::size_t n){if(watch)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{if(watch&&p)++allocations;std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
static void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
int main(int argc,char** argv){
 juce::ScopedJuceInitialiser_GUI gui;const int block=argc==2?juce::String(argv[1]).getIntValue():64;check(block==64||block==128,"64/128");
 auto p=std::make_unique<AionProcessor>();p->setPlayConfigDetails(2,2,8000,block);p->prepareToPlay(8000,block);
 juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;midi.ensureSize(2048);
 auto snapshot=[&]{return p->request({juce::JSON::parse(R"({"operation":"snapshot"})")});};
 auto process=[&]{audio.clear();watch=true;p->processBlock(audio,midi);watch=false;return snapshot();};
 auto state=process();
 auto request=[&](const char* text){auto v=juce::JSON::parse(text);v.getDynamicObject()->setProperty("generation",snapshot()["generation"]);v.getDynamicObject()->setProperty("expiresAt",static_cast<double>(juce::Time::currentTimeMillis()+4000));return p->request({v});};
 unsigned stage=0;auto settle=[&]{++stage;for(unsigned n=0;n<2000;++n){state=process();bool pending=false;for(const auto& t:*state["tracks"].getArray())pending|=static_cast<bool>(t["arpPending"]);if(!pending)return;std::this_thread::sleep_for(std::chrono::milliseconds(1));}std::cerr<<"stage "<<stage<<" generation "<<state["generation"].toString()<<" track19 "<<juce::JSON::toString(state["tracks"][19],true)<<'\n';check(false,"bounded Arp preparation completion");};
 juce::MemoryBlock original;p->getStateInformation(original);
 check(request(R"({"operation":"arp","action":"set","track":19,"parameter":"time","value":0.12345678901234566})")["ok"],"original double edit admission");
 juce::MemoryBlock queued;p->getStateInformation(queued);settle();juce::MemoryBlock committed;p->getStateInformation(committed);check(queued==committed,"accepted edit serializes complete original precision before and after receipt");
 check(static_cast<double>(state["tracks"][19]["arp"]["values"]["time"])==0.12345678901234566,"no Float32 reconstruction");
 check(request(R"({"operation":"arp","action":"preset","track":19,"preset":"sync-trip"})")["ok"],"preset admission");settle();
 check(state["tracks"][19]["arp"]["preset"].toString()=="sync-trip","original named preset retained");
 check(request(R"({"operation":"arp","action":"enable","track":19,"enabled":true})")["ok"],"async enable admission");settle();
 check(p->inspectAudioThread(19).track.arp.enabled&&static_cast<bool>(state["tracks"][19]["arpReady"]),"worker-prepared Arp activates on exact receipt");
 p->getStateInformation(committed);
 for(const char* bad:{"nan","inf","1e1000","0.2junk","-1"}){
  auto xml=p->getXmlFromBinary(committed.getData(),static_cast<int>(committed.getSize()));xml->getChildByName("LoopArps")->getChildElement(19)->setAttribute("time",bad);xml->getChildByName("LoopEqualizers")->getChildElement(0)->setAttribute("p1Gain",9);
  juce::MemoryBlock malformed,after;p->copyXmlToBinary(*xml,malformed);p->setStateInformation(malformed.getData(),static_cast<int>(malformed.getSize()));p->getStateInformation(after);check(after==committed,"malformed Arp rejects whole session including other FX");
 }
 auto invalid=[&](auto mutate){auto xml=p->getXmlFromBinary(committed.getData(),static_cast<int>(committed.getSize()));mutate(*xml);juce::MemoryBlock bad,after;p->copyXmlToBinary(*xml,bad);p->setStateInformation(bad.getData(),static_cast<int>(bad.getSize()));p->getStateInformation(after);check(after==committed,"invalid Arp schema preserves complete prior session");};
 invalid([](auto& x){x.getChildByName("LoopArps")->setAttribute("version",2);});
 invalid([](auto& x){x.addChildElement(new juce::XmlElement(*x.getChildByName("LoopArps")));});
 invalid([](auto& x){x.getChildByName("LoopArps")->getChildElement(19)->setAttribute("track",0);});
 invalid([](auto& x){x.getChildByName("LoopArps")->getChildElement(19)->removeAttribute("mix");});
 invalid([](auto& x){x.getChildByName("LoopArps")->getChildElement(19)->setAttribute("seed","4294967296");});
 invalid([](auto& x){x.getChildByName("LoopArps")->getChildElement(19)->setAttribute("enabled","true");});
 invalid([](auto& x){x.getChildByName("LoopArps")->getChildElement(19)->setAttribute("time",.11);}); // Named preset/control mismatch.
 auto load=[&](const juce::MemoryBlock& b){p->setStateInformation(b.getData(),static_cast<int>(b.getSize()));snapshot();settle();};
 load(original);check(!p->inspectAudioThread(19).track.arp.enabled,"all-off original bank restores");
 std::thread host([&]{p->setStateInformation(committed.getData(),static_cast<int>(committed.getSize()));});host.join();snapshot();settle();
 juce::MemoryBlock roundtrip;p->getStateInformation(roundtrip);check(roundtrip==committed,"active bank exact original/preset/BPM/seed state roundtrip");
 check(p->inspectAudioThread(19).track.arp.enabled,"active graph recreated by host-thread intent");
 p->prepareToPlay(8000,block);settle();check(p->inspectAudioThread(19).track.arp.enabled,"active Arp restores after same-rate generation change");
 p->prepareToPlay(22050,block);settle();check(p->inspectAudioThread(19).track.arp.enabled,"active Arp restores after sample-rate generation change");
 // Missing versioned bank is a legacy session, not an instruction to retain a
 // newer Arp from the current session. All old state schemas remain valid.
 auto legacy=p->getXmlFromBinary(original.getData(),static_cast<int>(original.getSize()));legacy->removeChildElement(legacy->getChildByName("LoopArps"),true);juce::MemoryBlock bytes;p->copyXmlToBinary(*legacy,bytes);load(bytes);check(!p->inspectAudioThread(19).track.arp.enabled,"legacy session defaults off");
 check(!request(R"({"operation":"arp","action":"set","track":19,"parameter":"time","value":3})")["ok"],"manual widget range remains distinct from stored preset range");
 check(request(R"({"operation":"loop","action":"clear","track":19})")["ok"],"DEL admission");
 check(!request(R"({"operation":"arp","action":"toggle","track":19})")["ok"],"Arp cannot cross an unacknowledged DEL barrier");settle();
 const auto root=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("arp-cli-"+juce::Uuid().toString());
 {aion::LocalCommandServer server([&](const juce::var& r){return p->request({r});},root);check(server.ready(),"CLI mailbox ready");
  juce::ChildProcess child;juce::StringArray command{AION_CLI_PYTHON,AION_ARP_CLI_TEST,root.getFullPathName(),server.id()};check(child.start(command),"external Arp CLI starts");
  const auto deadline=juce::Time::getMillisecondCounterHiRes()+30000;
  struct Pump final:juce::Timer{std::function<void()> tick;void timerCallback()override{tick();}} pump;
  pump.tick=[&]{process();if(!child.isRunning()||juce::Time::getMillisecondCounterHiRes()>=deadline)juce::MessageManager::getInstance()->stopDispatchLoop();};
  pump.startTimer(3);juce::MessageManager::getInstance()->runDispatchLoop();pump.stopTimer();
  if(child.isRunning()){child.kill();check(false,"external Arp CLI timed out");}
  std::cout<<child.readAllProcessOutput();check(child.getExitCode()==0,"external Arp CLI acceptance");
 }check(root.deleteFile(),"empty fixture CLI root removed");
 check(allocations==0,"zero production callback new/delete");
 std::cout<<"PASS production Arp: original precision, preset, async enable, strict atomic saved state, legacy, generation restore / "<<block<<'\n';
}
