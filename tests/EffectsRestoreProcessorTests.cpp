// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include <juce_gui_extra/juce_gui_extra.h>
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
 auto p=std::make_unique<AionProcessor>();p->setPlayConfigDetails(2,2,48000,block);p->prepareToPlay(48000,block);
 juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;midi.ensureSize(2048);
 auto process=[&](bool zero=false){if(zero)audio.setSize(2,0,false,false,true);audio.clear();watch=true;p->processBlock(audio,midi);watch=false;if(zero)audio.setSize(2,block,false,false,true);};
 auto snapshot=[&]{return p->request({juce::JSON::parse(R"({"operation":"snapshot"})")});};
 process();auto state=snapshot();const auto generation=static_cast<int>(state["generation"]);
 juce::MemoryBlock original;p->getStateInformation(original);
 auto saved=[&](int gain,int threshold,const char* order){auto xml=p->getXmlFromBinary(original.getData(),static_cast<int>(original.getSize()));
  for(auto* child:xml->getChildByName("LoopEqualizers")->getChildIterator()){child->setAttribute("enabled",1);child->setAttribute("preset",0);child->setAttribute("p1Gain",gain);}
  for(auto* child:xml->getChildByName("LoopCompressors")->getChildIterator()){child->setAttribute("enabled",1);child->setAttribute("preset",0);child->setAttribute("threshold",threshold);}
  for(auto* child:xml->getChildByName("LoopSignalChains")->getChildIterator())child->setAttribute("order",order);
  juce::MemoryBlock bytes;p->copyXmlToBinary(*xml,bytes);return bytes;
 };
 const auto a=saved(3,-30,"CQ"),b=saved(8,-24,"QC"),c=saved(-3,-12,"CQA");
 auto load=[&](const juce::MemoryBlock& bytes){p->setStateInformation(bytes.getData(),static_cast<int>(bytes.getSize()));};
 auto matches=[&](int gain,int threshold,const char* order){
  for(unsigned id=0;id<20;++id){const auto info=p->inspectAudioThread(id).track;
   check(info.equalizer.enabled&&info.equalizer.values[4]==gain&&info.compressor.enabled&&info.compressor.values[0]==threshold&&info.signalChain.text()==order,"all twenty native FX records match");
   check(info.eqRequest==info.compressorRequest&&info.eqRequest==info.chainRequest&&info.eqRequest==info.arpRequest,"one exact bank identity across Q/C/chain/A");}
 };
 // Ordinary commands accepted before a state load must settle first, even when
 // the host invokes setStateInformation on a different non-audio thread.
 auto edit=juce::JSON::parse(R"({"operation":"signalChain","action":"set","track":19,"order":"Q"})");edit.getDynamicObject()->setProperty("generation",generation);edit.getDynamicObject()->setProperty("expiresAt",static_cast<double>(juce::Time::currentTimeMillis()+4000));
 check(p->request({edit})["ok"],"older accepted FIFO edit");
 std::thread host([&]{load(a);});host.join();snapshot();
 process();check(p->inspectAudioThread(19).track.signalChain.text()=="Q","older FIFO action settles before restore");snapshot();process();snapshot();matches(3,-30,"CQ");
 // A second accepted load supersedes an admitted but unprocessed first load.
 load(b);snapshot();load(c);snapshot();process();snapshot();process();snapshot();matches(-3,-12,"CQA");
 // Unread success, saturated telemetry, then a newer intent. Saved state must
 // contain the newest accepted intent, never the obsolete completion.
 load(b);snapshot();for(unsigned n=0;n<32;++n)process();load(a);
 juce::MemoryBlock newest;p->getStateInformation(newest);auto xml=p->getXmlFromBinary(newest.getData(),static_cast<int>(newest.getSize()));
 check(xml->getChildByName("LoopSignalChains")->getFirstChildElement()->getStringAttribute("order")=="CQ","pending newest intent serialized immediately");
 snapshot();process();snapshot();matches(3,-30,"CQ");
 // Repeated loads with no ordinary command in between must not wait forever
 // for a global acknowledgement of an ID owned only by the bank protocol.
 load(b);snapshot();process();snapshot();matches(8,-24,"QC");load(a);snapshot();process();snapshot();matches(3,-30,"CQ");
 // Zero-frame callbacks have distinct serials despite identical sample clocks.
 load(c);snapshot();process(true);snapshot();matches(-3,-12,"CQA");
 // prepare applies accepted pending state on its stopped owner. An obsolete
 // unread result cannot overwrite it when message servicing resumes.
 load(b);snapshot();process();load(a);p->prepareToPlay(32000,block);process();snapshot();matches(3,-30,"CQ");
 // No editor or snapshot polling: only the production Timer drains receipts
 // and telemetry, permitting a later host load to progress.
 load(b);bool second=false;unsigned ticks=0;
 struct Pump final:juce::Timer{std::function<void()> tick;void timerCallback()override{tick();}} pump;
 pump.tick=[&]{process();if(++ticks==20){load(c);second=true;}if(ticks==60)juce::MessageManager::getInstance()->stopDispatchLoop();};
 pump.startTimer(3);juce::MessageManager::getInstance()->runDispatchLoop();pump.stopTimer();check(second,"second editor-closed host intent");matches(-3,-12,"CQA");
 check(allocations==0,"zero production callback new/delete");
 std::cout<<"PASS production FX restore: exact bank, older FIFO ordering, off-message host intent, latest wins, unread/saturated telemetry, zero-frame serials, prepare and editor-closed Timer / "<<block<<"\n";
}
