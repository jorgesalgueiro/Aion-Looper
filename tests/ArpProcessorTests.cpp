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
 unsigned stage=0;auto settle=[&]{++stage;for(unsigned n=0;n<2000;++n){state=process();bool pending=false;for(const auto& t:*state["tracks"].getArray())pending|=static_cast<bool>(t["arpBusy"]);if(!pending)return;std::this_thread::sleep_for(std::chrono::milliseconds(1));}std::cerr<<"stage "<<stage<<" generation "<<state["generation"].toString()<<" track19 "<<juce::JSON::toString(state["tracks"][19],true)<<'\n';check(false,"bounded Arp preparation completion");};
 juce::MemoryBlock original;p->getStateInformation(original);
 check(request(R"({"operation":"arp","action":"set","track":19,"parameter":"time","value":0.12345678901234566})")["ok"],"original double edit admission");
 juce::MemoryBlock queued;p->getStateInformation(queued);settle();juce::MemoryBlock committed;p->getStateInformation(committed);check(queued==committed,"accepted edit serializes complete original precision before and after receipt");
 check(static_cast<double>(state["tracks"][19]["arp"]["values"]["time"])==0.12345678901234566,"no Float32 reconstruction");
 check(request(R"({"operation":"arp","action":"preset","track":19,"preset":"sync-trip"})")["ok"],"preset admission");settle();
 check(state["tracks"][19]["arp"]["preset"].toString()=="sync-trip","original named preset retained");
 check(request(R"({"operation":"arp","action":"enable","track":19,"enabled":true})")["ok"],"async enable admission");
 check(request(R"({"operation":"setParameter","id":"syncBpm","value":159})")["ok"],"Master change while first enable is unacknowledged");
 juce::MemoryBlock enablingTempo;p->getStateInformation(enablingTempo);auto enablingXml=p->getXmlFromBinary(enablingTempo.getData(),static_cast<int>(enablingTempo.getSize()));
 check(enablingXml->getChildByName("LoopArps")->getChildElement(19)->getDoubleAttribute("bpm")==p->parameters.getRawParameterValue("syncBpm")->load(),"save includes tempo waiting behind first enable");settle();
 check(static_cast<double>(state["tracks"][19]["arp"]["bpm"])==p->parameters.getRawParameterValue("syncBpm")->load(),"first enable cannot consume pending tempo before its graph exists");
 check(p->inspectAudioThread(19).track.arp.enabled&&static_cast<bool>(state["tracks"][19]["arpReady"]),"worker-prepared Arp activates on exact receipt");
 const auto enableRequest=state["tracks"][19]["arpRequestId"];
 check(request(R"({"operation":"setParameter","id":"syncBpm","value":173.25})")["ok"],"shared GUI/CLI Master BPM gesture");
 const double bpm=p->parameters.getRawParameterValue("syncBpm")->load();
 juce::MemoryBlock pendingTempo;p->getStateInformation(pendingTempo);auto pendingXml=p->getXmlFromBinary(pendingTempo.getData(),static_cast<int>(pendingTempo.getSize()));
 check(pendingXml->getChildByName("LoopArps")->getChildElement(19)->getDoubleAttribute("bpm")==bpm,"immediate save includes accepted automatic BPM intent");
 auto maintenance=snapshot();check(maintenance["tracks"][19]["arpRequestId"]==enableRequest&&!static_cast<bool>(maintenance["tracks"][19]["arpPending"])&&static_cast<bool>(maintenance["tracks"][19]["arpBusy"]),"maintenance cannot steal GUI/CLI acknowledgement");
 settle();check(static_cast<double>(state["tracks"][19]["arp"]["bpm"])==bpm&&p->inspectAudioThread(19).track.arp.values[6]==static_cast<float>(bpm),"automatic BPM reaches exact original owner and native graph");
 check(static_cast<double>(state["tracks"][0]["arp"]["bpm"])==120&&state["tracks"][19]["arp"]["preset"].toString()=="sync-trip","absent graph BPM and active preset identity preserved");
 const auto revision=p->inspectAudioThread(19).track.arpSettingsRevision;
 check(request(R"({"operation":"setParameter","id":"syncBars","value":3})")["ok"],"original timing settings also issue immediate BPM gesture");settle();
 check(p->inspectAudioThread(19).track.arpSettingsRevision==revision+1,"equal-BPM timing gesture is not elided");
 auto manual=request(R"({"operation":"arp","action":"set","track":19,"parameter":"humanize","value":0.63})");check(manual["ok"],"manual control before tempo");
 check(request(R"({"operation":"setParameter","id":"syncBpm","value":95})")["ok"],"tempo retained behind manual edit");process();maintenance=snapshot();
 check(maintenance["tracks"][19]["arpRequestId"]==manual["requestId"]&&static_cast<bool>(maintenance["tracks"][19]["arpReady"]),"accepted manual command remains confirmed during later maintenance");settle();
 check(static_cast<double>(state["tracks"][19]["arp"]["values"]["humanize"])==.63,"tempo cannot overwrite a pending original control edit");
 for(int value:{60,120,181}){auto v=juce::JSON::parse(R"({"operation":"setParameter","id":"syncBpm"})");v.getDynamicObject()->setProperty("value",value);check(p->request({v})["ok"],"rapid timing gesture");}settle();
 check(static_cast<double>(state["tracks"][19]["arp"]["bpm"])==p->parameters.getRawParameterValue("syncBpm")->load(),"bounded latest timing intent settles after in-flight older gesture");
 auto* automated=p->parameters.getParameter("syncBpm");automated->setValueNotifyingHost(automated->convertTo0to1(207.5f));settle();
 check(static_cast<double>(state["tracks"][19]["arp"]["bpm"])==p->parameters.getRawParameterValue("syncBpm")->load(),"message service observes direct APVTS change without editor or setParameter RPC");
 check(request(R"({"operation":"setParameter","id":"syncBpm","value":117})")["ok"],"tempo admitted before prepare");
 p->prepareToPlay(8000,block);settle();
 check(static_cast<double>(state["tracks"][19]["arp"]["bpm"])==p->parameters.getRawParameterValue("syncBpm")->load()&&p->inspectAudioThread(19).track.arp.enabled,"prepare retires old tempo receipt and restores accepted original intent");
 check(request(R"({"operation":"midiConfig","action":"enable","enabled":true})")["ok"],"Arp MIDI enable");
 check(request(R"({"operation":"midiConfig","action":"eqTrack","track":19})")["ok"],"Arp MIDI selected track");
 check(request(R"({"operation":"midiConfig","action":"learn","target":125})")["ok"],"Arp repetitions CC learn");process();
 midi.addEvent(juce::MidiMessage::controllerEvent(1,11,0),17);midi.addEvent(juce::MidiMessage::controllerEvent(16,11,64),18);
 watch=true;p->processBlock(audio,midi);watch=false;midi.clear();
 juce::MemoryBlock midiSaved;p->getStateInformation(midiSaved);auto midiXml=p->getXmlFromBinary(midiSaved.getData(),static_cast<int>(midiSaved.getSize()));
 const double repeats=(64/127.)*10;
 check(midiXml->getChildByName("LoopArps")->getChildElement(19)->getDoubleAttribute("repetitions")==repeats,"immediate save consumes exact MIDI originals before Timer/snapshot");
 state=snapshot(); // Message owner accepts captured learn; the next callback acknowledges its configuration.
 state=process();check(static_cast<int>(state["midi"]["bindings"][125])==aion::midiBinding(2,11)&&static_cast<double>(state["tracks"][19]["arp"]["values"]["repetitions"])==repeats,"learn zero consumed; following same-block CC applies exact original value without editor");
 check(p->inspectAudioThread(19).track.arp.values[1]==static_cast<float>(repeats/11.1),"MIDI repetitions divide in double before render Float32");
 midi.addEvent(juce::MidiMessage::controllerEvent(1,11,63),31);watch=true;p->processBlock(audio,midi);watch=false;midi.clear();
 p->prepareToPlay(8000,block);settle();
 check(static_cast<double>(state["tracks"][19]["arp"]["values"]["repetitions"])==(63/127.)*10,"prepare retains unread exact MIDI intent before message telemetry service");
 check(request(R"({"operation":"loop","action":"clear","track":19})")["ok"],"DEL before same-block fresh Arp CC");
 midi.addEvent(juce::MidiMessage::controllerEvent(1,11,64),17);state=process();midi.clear();
 check(!static_cast<bool>(state["tracks"][19]["arp"]["enabled"])&&static_cast<double>(state["tracks"][19]["arp"]["values"]["repetitions"])==repeats,"DEL then fresh CC retains exact new inactive controls without graph revival");
 check(request(R"({"operation":"arp","action":"enable","track":19,"enabled":true})")["ok"],"original owner remains bound after clear/CC");settle();
 check(request(R"({"operation":"arp","action":"preset","track":19,"preset":"sync-trip"})")["ok"],"restore named preset for remaining legacy/state regressions");settle();
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
 check(request(R"({"operation":"setParameter","id":"syncBpm","value":89})")["ok"],"tempo admitted before newer session restore");
 load(original);check(!p->inspectAudioThread(19).track.arp.enabled,"all-off original bank restores after older tempo maintenance");
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
  if(child.isRunning()){child.kill();std::cerr<<child.readAllProcessOutput();check(false,"external Arp CLI timed out");}
  std::cout<<child.readAllProcessOutput();check(child.getExitCode()==0,"external Arp CLI acceptance");
 }check(root.deleteFile(),"empty fixture CLI root removed");
 check(allocations==0,"zero production callback new/delete");
 std::cout<<"PASS production Arp: original precision, preset, async enable, strict atomic saved state, legacy, generation restore / "<<block<<'\n';
}
