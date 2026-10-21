#include "plugin/Processor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <cstdlib>
#include <iostream>
#include <new>
namespace {thread_local bool audioWatch=false;std::atomic<int> audioAllocations{0};void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}}
void* operator new(std::size_t n){if(audioWatch)++audioAllocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {if(audioWatch)++audioAllocations;std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
int main(){
 juce::ScopedJuceInitialiser_GUI gui;
 for(const int block:{64,128}){
  {
   auto cc=std::make_unique<AionProcessor>();cc->setPlayConfigDetails(2,2,8000,block);cc->prepareToPlay(8000,block);
   const auto request=[&](const juce::String& json){auto result=cc->request({juce::JSON::parse(json)});check(result.getProperty("ok",false),json.toRawUTF8());return result;};
   juce::AudioBuffer<float> buffer(2,block);buffer.clear();juce::MidiBuffer packets;packets.ensureSize(8192);
   const auto run=[&]{audioWatch=true;cc->processBlock(buffer,packets);audioWatch=false;packets.clear();};
   request(R"({"operation":"setParameter","id":"syncMode","value":0})");request(R"({"operation":"midiConfig","action":"learn","target":0})");run();
   packets.addEvent(juce::MidiMessage::controllerEvent(1,20,127),17);run();request(R"({"operation":"snapshot"})");run();
   packets.addEvent(juce::MidiMessage::noteOn(1,48,juce::uint8{100}),0);packets.addEvent(juce::MidiMessage::noteOff(1,48),1);packets.addEvent(juce::MidiMessage::controllerEvent(1,20,127),17);run();
   check(request(R"({"operation":"snapshot"})")["tracks"][0]["state"].toString()=="recording","Production CC SUS recording fixture");
   request(R"({"operation":"midiConfig","action":"learn","target":34})");run();packets.addEvent(juce::MidiMessage::controllerEvent(1,20,0),17);run();
   auto state=request(R"({"operation":"snapshot"})");check(state["tracks"][0]["state"].toString()!="recording","Learn-consumed CC zero still finishes production SUS recording");
   check(static_cast<int>(state["midi"]["bindings"][0])==aion::midiBinding(2,20)&&static_cast<int>(state["midi"]["bindings"][34])==0&&state["midi"]["status"].toString().contains("already assigned"),"Conflicting release does not steal transport binding");
   for(int i=0;i<1000&&state["tracks"][0]["state"].toString()!="playing";++i){juce::Thread::sleep(1);run();state=request(R"({"operation":"snapshot"})");}
   check(state["tracks"][0]["state"].toString()=="playing","CC release commits playable PCM");packets.addEvent(juce::MidiMessage::controllerEvent(1,20,127),17);run();check(static_cast<bool>(request(R"({"operation":"snapshot"})")["tracks"][0]["muted"]),"CC rearmed after rejected learn");
   request(R"({"operation":"midiConfig","action":"learn","target":54})");run();packets.addEvent(juce::MidiMessage::controllerEvent(1,20,0),17);run();
   check(!static_cast<bool>(request(R"({"operation":"snapshot"})")["tracks"][0]["muted"]),"Learn-consumed CC zero restores production momentary mute");
  }
  {
   auto held=std::make_unique<AionProcessor>();held->setPlayConfigDetails(2,2,8000,block);held->prepareToPlay(8000,block);
   const auto request=[&](const juce::String& json){auto result=held->request({juce::JSON::parse(json)});check(result.getProperty("ok",false),json.toRawUTF8());return result;};
   juce::AudioBuffer<float> buffer(2,block);buffer.clear();juce::MidiBuffer packets;packets.ensureSize(8192);
   const auto run=[&]{audioWatch=true;held->processBlock(buffer,packets);audioWatch=false;packets.clear();};
   request(R"({"operation":"setParameter","id":"syncMode","value":0})");
   request(R"({"operation":"midiConfig","action":"enable","enabled":true})");
   packets.addEvent(juce::MidiMessage::noteOn(1,48,juce::uint8{100}),0);packets.addEvent(juce::MidiMessage::noteOff(1,48),1);packets.addEvent(juce::MidiMessage::noteOn(1,36,juce::uint8{100}),2);run();
   check(request(R"({"operation":"snapshot"})")["tracks"][0]["state"].toString()=="recording","Held SUS processor fixture records");
   request(R"({"operation":"midiConfig","action":"learn","target":34})");run();
   check(request(R"({"operation":"snapshot"})")["tracks"][0]["state"].toString()=="recording","Production slider learn preserves held recording");
   packets.addEvent(juce::MidiMessage::controllerEvent(1,13,0),17);run();request(R"({"operation":"snapshot"})");run();
   check(request(R"({"operation":"snapshot"})")["tracks"][0]["state"].toString()=="recording","Production learned binding acknowledgement preserves held recording");
   request(R"({"operation":"midiConfig","action":"learn","target":54})");
   for(int i=0;i<30;++i)request("{\"operation\":\"midiConfig\",\"action\":\"mode\",\"mode\":"+juce::String(i%3)+"}");
   request(R"({"operation":"midiConfig","action":"cancelLearn"})");run();
   auto state=request(R"({"operation":"snapshot"})");check(state["tracks"][0]["state"].toString()=="recording"&&static_cast<int>(state["midi"]["learnTarget"])==-1,"Full-queue learn cancellation preserves held SUS recording while clearing learn");
   packets.addEvent(juce::MidiMessage::noteOff(1,36),17);run();
   check(request(R"({"operation":"snapshot"})")["tracks"][0]["state"].toString()!="recording","Owned NoteOff still finishes SUS recording after full-queue cancellation");
  }
  auto p=std::make_unique<AionProcessor>();p->setPlayConfigDetails(2,2,8000,block);p->prepareToPlay(8000,block);
  const auto rpc=[&](const juce::String& json){auto result=p->request({juce::JSON::parse(json)});check(result.getProperty("ok",false),json.toRawUTF8());return result;};
  juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;midi.ensureSize(8192);
  const auto process=[&]{const int events=midi.getNumEvents();for(int c=0;c<2;++c)for(int i=0;i<block;++i)audio.setSample(c,i,c?0.17f:0.25f);audioWatch=true;p->processBlock(audio,midi);audioWatch=false;check(midi.getNumEvents()==events,"Incoming host MIDI remains available for pass-through");midi.clear();};
  rpc(R"({"operation":"midiConfig","action":"enable","enabled":true})");process();
  const auto storedEnabled=[&]{juce::MemoryBlock bytes;p->getStateInformation(bytes);const auto xml=juce::AudioProcessor::getXmlFromBinary(bytes.getData(),static_cast<int>(bytes.getSize()));return xml->getChildByName("MidiSettings")->getBoolAttribute("enabled");};
  rpc(R"({"operation":"midiConfig","action":"toggle"})");check(!storedEnabled(),"First toggle updates desired state before next audio block");
  rpc(R"({"operation":"midiConfig","action":"toggle"})");check(storedEnabled(),"Second toggle uses latest accepted state, not stale telemetry");process();check(static_cast<bool>(rpc(R"({"operation":"snapshot"})")["midi"]["enabled"]),"Two queued MIDI toggles preserve final enabled state");

  midi.addEvent(juce::MidiMessage::noteOn(1,36,juce::uint8{100}),17);process();
  auto state=rpc(R"({"operation":"snapshot"})");check(state["tracks"][0]["state"].toString()=="recording"&&static_cast<int>(state["tracks"][0]["length"])==block-17,"Actual MidiBuffer sample offset reaches native recorder");
  // Learning consumes a two-byte program with data1==0; it must not finish the
  // recording through the old mapping or assume a nonexistent velocity byte.
  rpc(R"({"operation":"midiConfig","action":"learn","target":19})");process();midi.addEvent(juce::MidiMessage::programChange(16,0),0);process();rpc(R"({"operation":"snapshot"})");process();
  state=rpc(R"({"operation":"snapshot"})");check(static_cast<int>(state["midi"]["bindings"][19])==aion::midiBinding(3,0),"Processor MIDI learn bridge commits PC zero");
  rpc(R"({"operation":"midiConfig","action":"learn","target":33})");process();midi.addEvent(juce::MidiMessage::controllerEvent(1,11,0),17);process();rpc(R"({"operation":"snapshot"})");process();
  state=rpc(R"({"operation":"snapshot"})");check(static_cast<int>(state["midi"]["bindings"][33])==aion::midiBinding(2,11)&&static_cast<double>(state["tracks"][0]["volume"])==1,"CC zero learns volume without moving it");
  midi.addEvent(juce::MidiMessage::controllerEvent(16,11,127),17);process();state=rpc(R"({"operation":"snapshot"})");check(static_cast<double>(state["tracks"][0]["volume"])==2&&state["tracks"][0]["state"].toString()=="recording","Actual CC reaches volume, never finishes capture");
  rpc(R"({"operation":"midiConfig","action":"learn","target":53})");process();midi.addEvent(juce::MidiMessage::controllerEvent(1,12,0),17);midi.addEvent(juce::MidiMessage::controllerEvent(1,12,127),18);process();
  state=rpc(R"({"operation":"snapshot"})");check(static_cast<int>(state["tracks"][0]["pan"])==10,"Following expression in same MidiBuffer applies before message-thread learn acknowledgement");process();
  juce::MemoryBlock saved;p->getStateInformation(saved);
  auto restored=std::make_unique<AionProcessor>();restored->setPlayConfigDetails(2,2,8000,block);restored->prepareToPlay(8000,block);restored->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
  juce::MemoryBlock roundtrip;restored->getStateInformation(roundtrip);check(saved==roundtrip,"MIDI mappings persist immediately, including before resumed audio callbacks");
  juce::AudioBuffer<float> restoredAudio(2,block);restoredAudio.clear();juce::MidiBuffer restoredMidi;restoredMidi.ensureSize(256);restoredMidi.addEvent(juce::MidiMessage::programChange(1,0),0);
  audioWatch=true;restored->processBlock(restoredAudio,restoredMidi);audioWatch=false;
  const auto restoredView=restored->request({juce::JSON::parse(R"({"operation":"snapshot"})")});check(restoredView["tracks"][19]["state"].toString()=="recording","Saved learned mapping performs with no editor or WebView");
  restoredMidi.clear();restoredMidi.addEvent(juce::MidiMessage::controllerEvent(1,11,0),17);audioWatch=true;restored->processBlock(restoredAudio,restoredMidi);audioWatch=false;
  const auto expressionView=restored->request({juce::JSON::parse(R"({"operation":"snapshot"})")});check(static_cast<double>(expressionView["tracks"][0]["volume"])==0&&expressionView["tracks"][19]["state"].toString()=="recording","Persisted expression works with editor closed");
  auto legacyXml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),static_cast<int>(saved.getSize()));auto* legacyMidi=legacyXml->getChildByName("MidiSettings");legacyMidi->removeAttribute("version");for(unsigned i=33;i<aion::MidiSettings::bindingCount;++i)legacyMidi->removeAttribute("binding"+juce::String(i));
  juce::MemoryBlock legacyBytes;juce::AudioProcessor::copyXmlToBinary(*legacyXml,legacyBytes);auto legacy=std::make_unique<AionProcessor>();legacy->setStateInformation(legacyBytes.getData(),static_cast<int>(legacyBytes.getSize()));juce::MemoryBlock migrated;legacy->getStateInformation(migrated);auto migratedXml=juce::AudioProcessor::getXmlFromBinary(migrated.getData(),static_cast<int>(migrated.getSize()));
  check(migratedXml->getChildByName("MidiSettings")->getIntAttribute("binding19")==aion::midiBinding(3,0)&&migratedXml->getChildByName("MidiSettings")->getIntAttribute("binding33",-1)==0,"Legacy 33-slot MIDI state migrates without losing pedal mappings");
  auto overflowingXml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),static_cast<int>(saved.getSize()));overflowingXml->getChildByName("MidiSettings")->setAttribute("binding19","18446744073709551616");
  juce::MemoryBlock overflowingBytes;juce::AudioProcessor::copyXmlToBinary(*overflowingXml,overflowingBytes);restored->setStateInformation(overflowingBytes.getData(),static_cast<int>(overflowingBytes.getSize()));juce::MemoryBlock afterOverflow;restored->getStateInformation(afterOverflow);
  check(afterOverflow==roundtrip,"Overflowing MIDI binding decimal must reject entire state instead of wrapping and clearing pedal mapping");
  auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),static_cast<int>(saved.getSize()));check(xml!=nullptr,"Native state XML");auto* stored=xml->getChildByName("MidiSettings");check(stored!=nullptr,"MIDI state subtree");stored->setAttribute("binding19",static_cast<int>(aion::midiBinding(1,36)));juce::MemoryBlock invalid;juce::AudioProcessor::copyXmlToBinary(*xml,invalid);
  restored->setStateInformation(invalid.getData(),static_cast<int>(invalid.getSize()));juce::MemoryBlock preserved;restored->getStateInformation(preserved);check(preserved==roundtrip,"Invalid duplicate mapping preserves complete prior state");
  rpc(R"({"operation":"midiConfig","action":"learn","target":18})");process();p->cancelMidiLearn();process();midi.addEvent(juce::MidiMessage::programChange(1,3),0);process();state=rpc(R"({"operation":"snapshot"})");check(static_cast<int>(state["midi"]["bindings"][18])==0,"Editor cancellation leaves old binding intact");
  rpc(R"({"operation":"midiConfig","action":"learn","target":17})");process();state=rpc(R"({"operation":"snapshot"})");check(static_cast<int>(state["midi"]["learnTarget"])==17,"Learn indicator armed before pause");
  rpc(R"({"operation":"midiConfig","action":"cancelLearn"})");state=rpc(R"({"operation":"snapshot"})");check(static_cast<int>(state["midi"]["learnTarget"])==-1,"Cancelled learn indicator must clear before another audio callback");process();
  // Pause callbacks and fill all 31 usable MIDI configuration entries.
  rpc(R"({"operation":"midiConfig","action":"learn","target":18})");
  for(int i=0;i<30;++i)rpc("{\"operation\":\"midiConfig\",\"action\":\"mode\",\"mode\":"+juce::String(i%3)+"}");
  const auto cancelled=p->request({juce::JSON::parse(R"({"operation":"midiConfig","action":"cancelLearn"})")});check(cancelled.getProperty("ok",false),"Escape cancellation must succeed when audio is paused and MIDI configuration queue is full");
  process();midi.addEvent(juce::MidiMessage::programChange(1,3),0);process();state=rpc(R"({"operation":"snapshot"})");check(static_cast<int>(state["midi"]["learnTarget"])==-1&&static_cast<int>(state["midi"]["bindings"][18])==0,"Cancelled full-queue learn cannot revive after callbacks resume");
  rpc(R"({"operation":"midiConfig","action":"learn","target":18})");process();midi.addEvent(juce::MidiMessage::programChange(1,5),0);process();rpc(R"({"operation":"snapshot"})");process();state=rpc(R"({"operation":"snapshot"})");check(static_cast<int>(state["midi"]["bindings"][18])==aion::midiBinding(3,5)&&state["tracks"][18]["state"].toString()=="empty","Fresh learn works after full-queue cancellation and consumes its packet");
  midi.clear();for(int i=0;i<300;++i)midi.addEvent(juce::MidiMessage::noteOn(1,40,juce::uint8{100}),0);process();state=rpc(R"({"operation":"snapshot"})");check(static_cast<int>(state["midi"]["overflows"])>0,"Bounded MidiBuffer overflow reported and held actions cancelled");
  for(const auto* request:{R"({"operation":"midiConfig","action":"learn","target":20})",R"({"operation":"midiConfig","action":"mode","mode":7})",R"({"operation":"midiConfig","action":"enable","enabled":1})"})check(!p->request({juce::JSON::parse(request)}).getProperty("ok",false),"Invalid MIDI IPC rejected");
 }
 check(audioAllocations.load()==0,"Production processBlock MIDI path must perform zero new/delete");
 std::cout<<"PASS: actual juce::MidiBuffer offsets, PC0 learn/consume, state roundtrip/invalid rollback, cancellation/overflow/IPC and zero processBlock new/delete at 64/128\n";
}
