// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include "plugin/LocalCommandServer.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <cstdlib>
#include <iostream>
#include <thread>
namespace {thread_local bool watching=false;unsigned allocations=0;void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}}
void* operator new(std::size_t n){if(watching)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{if(watching)++allocations;std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
int main(int argc,char** argv){
 juce::ScopedJuceInitialiser_GUI gui;const int block=argc==2?juce::String(argv[1]).getIntValue():64;check(block==64||block==128,"Block size");
 {
 auto p=std::make_unique<AionProcessor>();p->setPlayConfigDetails(2,2,48000,block);p->prepareToPlay(48000,block);
 juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;midi.ensureSize(2048);aion::DriveBank rendered;
 auto snapshot=[&]{return p->request({juce::JSON::parse(R"({"operation":"snapshot"})")});};
 auto process=[&]{audio.clear();watching=true;p->processBlock(audio,midi);rendered=p->inspectAudioThread(19).track.drives;watching=false;midi.clear();return snapshot();};
 auto state=process();
 auto request=[&](juce::String json){auto r=juce::JSON::parse(json);auto* o=r.getDynamicObject();o->setProperty("generation",state["generation"]);o->setProperty("expiresAt",static_cast<double>(juce::Time::currentTimeMillis()+4000));return p->request({r});};
 auto drive=[&](unsigned k,const juce::String& action,const juce::String& rest=juce::String()){
  return request("{\"operation\":\"drive\",\"track\":19,\"effect\":\""+juce::String(aion::driveNames[k].data())+"\",\"action\":\""+action+"\""+rest+"}");
 };
 for(unsigned k=0;k<3;++k){check(drive(k,"get")["schema"]==juce::var(1),"Bounded native catalog");check(!rendered[k].enabled,"Legacy default off");
  const auto name=aion::drivePresets(static_cast<aion::DriveKind>(k))[1].name;check(drive(k,"preset",",\"preset\":\""+juce::String(name.data())+"\"")["ok"],"Preset accepted on expanded track");check(drive(k,"enable",",\"enabled\":true")["ok"],"Enable accepted");
 }
 juce::MemoryBlock queued;p->getStateInformation(queued);state=process();juce::MemoryBlock saved;p->getStateInformation(saved);check(queued==saved,"Accepted intent survives save before callback");const auto wanted=rendered;
 for(unsigned k=0;k<3;++k)check(rendered[k].enabled&&rendered[k].preset==2,"All native processors acknowledge preset/enable");
 check(!drive(0,"set",",\"values\":{\"amount\":5,\"tone\":-1}")["ok"],"Malformed batch rejected atomically");juce::MemoryBlock after;p->getStateInformation(after);check(after==saved,"Invalid batch preserves complete state");
 check(request(R"({"operation":"loop","action":"clear","track":19})")["ok"],"DEL admitted");check(!drive(0,"toggle")["ok"],"DEL barrier rejects older control work");state=process();check(rendered==aion::defaultDriveBank(),"DEL resets all three effects");
 p->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));for(unsigned n=0;n<1000&&rendered!=wanted;++n){state=process();std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(rendered==wanted,"Atomic full effects-bank restore reaches audio");
 // A malformed bank may not partially replace either effects or APVTS.
 auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),static_cast<int>(saved.getSize()));auto* bank=xml->getChildByName("LoopDrives");check(bank!=nullptr,"Versioned drive bank stored");
 bank->getFirstChildElement()->getChildByName("distortion")->setAttribute("amount",1000);
 juce::MemoryBlock bad;juce::AudioProcessor::copyXmlToBinary(*xml,bad);juce::MemoryBlock beforeBad;p->getStateInformation(beforeBad);p->setStateInformation(bad.getData(),static_cast<int>(bad.getSize()));p->getStateInformation(after);check(beforeBad==after,"Malformed bank cannot partially restore state");
 check(request(R"({"operation":"midiConfig","action":"eqTrack","track":19})")["ok"],"Selected loop MIDI routing");
 for(unsigned i=0;i<aion::driveMidiParameters.size();++i){
  check(request("{\"operation\":\"midiConfig\",\"action\":\"learn\",\"target\":"+juce::String(aion::MidiSettings::driveBase+i)+"}")["ok"],"Drive CC learn accepted");state=process();
  midi.addEvent(juce::MidiMessage::controllerEvent(1,static_cast<int>(11+i),0),17);midi.addEvent(juce::MidiMessage::controllerEvent(16,static_cast<int>(11+i),127),18);state=process();
  const auto pair=aion::driveMidiParameters[i];check(rendered[pair[0]].values[pair[1]]==aion::driveControls(static_cast<aion::DriveKind>(pair[0]))[pair[1]].maximum,"Learn consumes first CC then applies same-block maximum");state=process();
  check(static_cast<int>(state["midi"]["bindings"][static_cast<int>(aion::MidiSettings::driveBase+i)])==aion::midiBinding(2,11+i),"Learned binding persists");
 }
 // v6's 133 slots remain unchanged; v7 appends fourteen empty bindings.
 p->getStateInformation(saved);xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),static_cast<int>(saved.getSize()));auto* mappings=xml->getChildByName("MidiSettings");check(mappings->getIntAttribute("version")==7,"MIDI schema v7");mappings->setAttribute("version",6);
 for(unsigned i=aion::MidiSettings::driveBase;i<aion::MidiSettings::bindingCount;++i)mappings->removeAttribute("binding"+juce::String(i));juce::AudioProcessor::copyXmlToBinary(*xml,bad);p->setStateInformation(bad.getData(),static_cast<int>(bad.getSize()));
 for(unsigned n=0;n<100;++n){state=process();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 for(unsigned i=aion::MidiSettings::driveBase;i<aion::MidiSettings::bindingCount;++i)check(static_cast<int>(state["midi"]["bindings"][static_cast<int>(i)])==0,"v6 appends unassigned drive mappings");
 // Shared CLI talks to this exact processor; it never creates another engine.
 const auto root=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("drive-cli-"+juce::Uuid().toString());
 {aion::LocalCommandServer server([&](const juce::var& r){return p->request({r});},root);check(server.ready(),"CLI mailbox");juce::ChildProcess child;juce::StringArray args{AION_CLI_PYTHON,AION_DRIVE_CLI_TEST,root.getFullPathName(),server.id()};check(child.start(args),"External CLI starts");
  const auto deadline=juce::Time::getMillisecondCounterHiRes()+30000;struct Pump:juce::Timer{std::function<void()> tick;void timerCallback()override{tick();}} pump;
  pump.tick=[&]{state=process();if(!child.isRunning()||juce::Time::getMillisecondCounterHiRes()>=deadline)juce::MessageManager::getInstance()->stopDispatchLoop();};pump.startTimer(3);juce::MessageManager::getInstance()->runDispatchLoop();pump.stopTimer();if(child.isRunning()){child.kill();check(false,"CLI deadline");}std::cout<<child.readAllProcessOutput();check(child.getExitCode()==0,"Shared external drive CLI");
 }check(root.deleteFile(),"Empty test mailbox removed");
 for(double rate:{48000.,22050.,32000.,96000.}){p->prepareToPlay(rate,block);state=process();check(rendered[2].preset==2&&!rendered[2].enabled,"Device/rate change retains saved fuzz preset and bypass");}
 }check(!allocations,"No production callback new/delete");std::cout<<"PASS drive processor/CLI/MIDI/state "<<block<<"\n";
}
