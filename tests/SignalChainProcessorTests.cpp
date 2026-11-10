// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include "plugin/LocalCommandServer.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <cstdlib>
#include <iostream>
static thread_local bool watching=false;
static unsigned operations=0;
void* operator new(std::size_t n){if(watching)++operations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watching)++operations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
static void check(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
int main(int argc,char** argv){
 juce::ScopedJuceInitialiser_GUI gui;const int block=argc==2?juce::String(argv[1]).getIntValue():64;check(block==64||block==128,"64/128 only");
 auto p=std::make_unique<AionProcessor>();p->setPlayConfigDetails(2,2,48000,block);p->prepareToPlay(48000,block);
 juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;midi.ensureSize(1024);
 auto snapshot=[&]{return p->request({juce::JSON::parse(R"({"operation":"snapshot"})")});};
 auto process=[&]{audio.clear();watching=true;p->processBlock(audio,midi);watching=false;return snapshot();};
 auto state=process();auto generation=static_cast<int>(state["generation"]);
 auto request=[&](const char* json){auto v=juce::JSON::parse(json);auto* o=v.getDynamicObject();o->setProperty("generation",generation);o->setProperty("expiresAt",static_cast<double>(juce::Time::currentTimeMillis()+4000));return p->request({v});};
 check(request(R"({"operation":"signalChain","action":"set","track":19,"order":" c q a c ! "})")["ok"],"expanded track chain normalized");
 juce::MemoryBlock saved;p->getStateInformation(saved); // Pending intent saved without an audio callback.
 state=process();check(state["tracks"][19]["signalChain"].toString()=="CQA"&&p->inspectAudioThread(19).track.signalChain.text()=="CQA","metadata and audio owner agree");
 check(state["tracks"][0]["signalChain"].toString()=="QCATFODBVKZG","track isolation");
 for(const char* bad:{R"({"operation":"signalChain","action":"set","track":20,"order":"Q"})",R"({"operation":"signalChain","action":"set","track":19,"order":123})",R"({"operation":"signalChain","action":"wat","track":19})"})check(!static_cast<bool>(request(bad)["ok"]),"malformed request rejected");
 check(request(R"({"operation":"signalChain","action":"reset","track":19})")["ok"],"reset accepted");process();
 p->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));snapshot();state=process();
 check(state["tracks"][19]["signalChain"].toString()=="CQA"&&p->inspectAudioThread(19).track.signalChain.text()=="CQA","versioned restore follows audio acknowledgement");
 auto load=[&](juce::XmlElement& xml){juce::MemoryBlock bytes;p->copyXmlToBinary(xml,bytes);p->setStateInformation(bytes.getData(),static_cast<int>(bytes.getSize()));snapshot();return process();};
 for(unsigned corruption=0;corruption<5;++corruption){
  auto xml=p->getXmlFromBinary(saved.getData(),static_cast<int>(saved.getSize()));check(xml!=nullptr,"saved XML");auto* bank=xml->getChildByName("LoopSignalChains");check(bank!=nullptr,"versioned chain bank exists");
  if(corruption==0)bank->setAttribute("version",999);
  if(corruption==1)bank->getFirstChildElement()->setAttribute("order","CCQ");
  if(corruption==2)bank->getFirstChildElement()->setAttribute("track",19);
  if(corruption==3)bank->deleteAllChildElements();
  if(corruption==4)xml->addChildElement(new juce::XmlElement(*bank));
  state=load(*xml);check(state["tracks"][19]["signalChain"].toString()=="CQA"&&p->inspectAudioThread(19).track.signalChain.text()=="CQA","malformed bank preserves complete old state");
 }
 auto legacy=p->getXmlFromBinary(saved.getData(),static_cast<int>(saved.getSize()));legacy->removeChildElement(legacy->getChildByName("LoopSignalChains"),true);state=load(*legacy);check(state["tracks"][19]["signalChain"].toString()=="QCATFODBVKZG","legacy sessions default chain");
 p->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));snapshot();process();
 for(double rate:{48000.,32000.,96000.}){p->prepareToPlay(rate,block);state=process();generation=static_cast<int>(state["generation"]);check(p->inspectAudioThread(19).track.signalChain.text()=="CQA","rate change retains native order");}
 check(request(R"({"operation":"loop","action":"clear","track":19})")["ok"],"DEL admitted");process();check(p->inspectAudioThread(19).track.signalChain==aion::SignalChain{},"DEL restores default");
 const auto root=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("chain-cli-"+juce::Uuid().toString());
 {aion::LocalCommandServer server([&](const juce::var& r){return p->request({r});},root);check(server.ready(),"CLI mailbox");
  juce::ChildProcess child;juce::StringArray command{AION_CLI_PYTHON,AION_CHAIN_CLI_TEST,root.getFullPathName(),server.id()};check(child.start(command),"external CLI");
  const auto deadline=juce::Time::getMillisecondCounterHiRes()+30000;
  struct Pump final:juce::Timer{std::function<void()> tick;void timerCallback()override{tick();}} pump;
  pump.tick=[&]{process();if(!child.isRunning()||juce::Time::getMillisecondCounterHiRes()>=deadline)juce::MessageManager::getInstance()->stopDispatchLoop();};pump.startTimer(3);juce::MessageManager::getInstance()->runDispatchLoop();pump.stopTimer();
  if(child.isRunning()){child.kill();check(false,"CLI timeout");}std::cout<<child.readAllProcessOutput();check(child.getExitCode()==0,"CLI results");
 }check(root.deleteFile(),"empty fixture mailbox removed");
 check(operations==0,"zero processor callback allocation/free");std::cout<<"PASS production chain state/validation/legacy/clear/rates/CLI at "<<block<<'\n';
}
