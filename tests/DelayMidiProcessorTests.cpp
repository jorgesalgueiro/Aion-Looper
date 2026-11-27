// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <cstdlib>
#include <iostream>
#include <thread>
static thread_local bool watch=false;static unsigned operations=0;
void* operator new(std::size_t n){if(watch)++operations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete(void* p)noexcept{if(watch&&p)++operations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
static void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;const int block=argc==2?juce::String(argv[1]).getIntValue():64;check(block==64||block==128,"64/128");
 auto p=std::make_unique<AionProcessor>();p->setPlayConfigDetails(2,2,8000,block);p->prepareToPlay(8000,block);juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;midi.ensureSize(4096);
 auto pump=[&]{audio.clear();watch=true;p->processBlock(audio,midi);watch=false;midi.clear();check(!operations,"zero Delay MIDI callback new/delete");};
 auto snapshot=[&]{return p->request({juce::JSON::parse(R"({"operation":"snapshot"})")});};pump();auto s=snapshot();
 auto request=[&](const char* json){auto r=juce::JSON::parse(json);r.getDynamicObject()->setProperty("generation",s["generation"]);r.getDynamicObject()->setProperty("expiresAt",static_cast<double>(juce::Time::currentTimeMillis()+4000));return p->request({r});};
 auto cc=[&](int channel,int controller,int value,int offset=17){midi.addEvent(juce::MidiMessage::controllerEvent(channel,controller,value),offset);};
 auto settle=[&]{for(unsigned n=0;n<1000;++n){pump();s=snapshot();if(!static_cast<bool>(s["tracks"][19]["delayBusy"]))return;std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(false,"Delay MIDI restore deadline");};
 check(request(R"({"operation":"midiConfig","action":"enable","enabled":true})")["ok"],"MIDI enabled");check(request(R"({"operation":"midiConfig","action":"eqTrack","track":19})")["ok"],"selected expanded loop");
 check(request(R"({"operation":"midiConfig","action":"learn","target":147})")["ok"],"Delay Time learn");pump();
 cc(1,20,0,17);cc(16,20,64,18);cc(1,20,127,19);pump();
 juce::MemoryBlock immediate;p->getStateInformation(immediate);auto x=p->getXmlFromBinary(immediate.getData(),static_cast<int>(immediate.getSize()));
 check(x->getChildByName("LoopDelays")->getChildElement(19)->getDoubleAttribute("time")==4,"same-block learned CC applies after consumed first packet, immediate exact save");s=snapshot();pump();s=snapshot();
 check(static_cast<int>(s["midi"]["bindings"][147])==aion::midiBinding(2,20),"learn binding accepted");
 // Install six distinct omni CCs via the public versioned state, then exercise
 // every value/channel and nonmonotonic endpoints with the actual MidiBuffer.
 juce::MemoryBlock saved;p->getStateInformation(saved);x=p->getXmlFromBinary(saved.getData(),static_cast<int>(saved.getSize()));auto* mappings=x->getChildByName("MidiSettings");check(mappings->getIntAttribute("version")==10,"schema v10");
 for(unsigned j=0;j<6;++j)mappings->setAttribute("binding"+juce::String(147+j),static_cast<int>(aion::midiBinding(2,20+j)));
 p->copyXmlToBinary(*x,saved);p->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));settle();
 for(unsigned channel=1;channel<=16;++channel)for(unsigned j=0;j<6;++j)for(unsigned value=0;value<128;++value){cc(static_cast<int>(channel),20+static_cast<int>(j),static_cast<int>(value));pump();
  check(p->inspectAudioThread(19).track.delay.values[j]==aion::delayMidiValue(j,value),"all CC values/channels reach exact native D setting");s=snapshot();
  check(static_cast<double>(s["tracks"][19]["delay"]["values"].getProperty(juce::Identifier(juce::String::fromUTF8(aion::delayControls[j].name.data(),static_cast<int>(aion::delayControls[j].name.size()))),{}))==aion::delayMidiValue(j,value),"sole settings owner consumes exact receipts");
 }
 check(p->inspectAudioThread(0).track.delay==aion::DelaySettings{},"unselected track unaffected");
 // Full receipt queue must reject before mutation; drain recovers ownership.
 const auto rejectedBefore=static_cast<int>(s["midi"]["rejected"]);for(unsigned n=0;n<3;++n){for(unsigned i=0;i<200;++i)cc(1,23,static_cast<int>(i%128),static_cast<int>(i%static_cast<unsigned>(block)));pump();}
 const auto beforeDrain=p->inspectAudioThread(19).track.delay;s=snapshot();check(static_cast<int>(s["midi"]["rejected"])>rejectedBefore,"receipt saturation rejects bounded overflow");
 check(static_cast<double>(s["tracks"][19]["delay"]["values"]["mix"])==beforeDrain.values[3],"saturation never loses an accepted original value");cc(1,23,0);pump();s=snapshot();check(p->inspectAudioThread(19).track.delay.values[3]==0,"receipt drain recovers new CC");
 // Accepted host loads suspend D edits until exact joint proof is complete.
 p->getStateInformation(saved);const auto old=p->inspectAudioThread(19).track.delay;p->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));cc(1,20,0);pump();check(p->inspectAudioThread(19).track.delay==old,"MIDI cannot cross unsubmitted restore proof");settle();cc(1,20,64);pump();s=snapshot();check(p->inspectAudioThread(19).track.delay.values[0]==aion::delayMidiValue(0,64),"MIDI resumes after joint confirmation");
 p->getStateInformation(saved);x=p->getXmlFromBinary(saved.getData(),static_cast<int>(saved.getSize()));x->getChildByName("MidiSettings")->setAttribute("binding150",static_cast<int>(aion::midiBinding(2,23,16)));p->copyXmlToBinary(*x,saved);p->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));settle();
 const auto filtered=p->inspectAudioThread(19).track.delay;cc(1,23,127);pump();s=snapshot();check(p->inspectAudioThread(19).track.delay==filtered,"channel-specific Delay binding ignores other channels");cc(16,23,127);pump();s=snapshot();check(p->inspectAudioThread(19).track.delay.values[3]==1,"channel-specific Delay binding accepts selected channel");
 cc(1,21,63);pump();p->prepareToPlay(48000,block);settle();check(p->inspectAudioThread(19).track.delay.values[1]==aion::delayMidiValue(1,63),"changed-rate prepare preserves unread exact MIDI original");cc(1,25,0);pump();s=snapshot();check(p->inspectAudioThread(19).track.delay.values[5]==0,"MIDI owner resumes in new prepared generation");
 p->getStateInformation(saved);x=p->getXmlFromBinary(saved.getData(),static_cast<int>(saved.getSize()));mappings=x->getChildByName("MidiSettings");mappings->setAttribute("version",7);for(unsigned j=147;j<153;++j)mappings->removeAttribute("binding"+juce::String(j));
 mappings->setAttribute("binding19",static_cast<int>(aion::midiBinding(3,0,2)));mappings->setAttribute("binding33",static_cast<int>(aion::midiBinding(2,42,16)));mappings->setAttribute("binding146",static_cast<int>(aion::midiBinding(2,40,7)));
 p->copyXmlToBinary(*x,saved);p->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));settle();s=snapshot();
 check(static_cast<int>(s["midi"]["bindings"][19])==aion::midiBinding(3,0,2)&&static_cast<int>(s["midi"]["bindings"][33])==aion::midiBinding(2,42,16)&&static_cast<int>(s["midi"]["bindings"][146])==aion::midiBinding(2,40,7),"v7 retains PC0/mixer/final drive IDs and channel filters");
 for(int j=147;j<153;++j)check(static_cast<int>(s["midi"]["bindings"][j])==0,"v7 initializes appended targets unassigned");
 // Two concurrently merged controllers may reuse CC/note/PC numbers on
 // distinct channels. Learn must leave the other controller running, including
 // provisional movement before the message owner acknowledges the binding.
 check(request(R"({"operation":"midiConfig","action":"learn","target":147,"channel":1})")["ok"],"channel 1 Learn admitted");pump();
 cc(16,99,0);pump();s=snapshot();check(static_cast<int>(s["midi"]["learnTarget"])==147&&static_cast<int>(s["midi"]["learnChannel"])==1,"wrong-channel packet cannot capture Learn");
 cc(1,99,0,17);cc(16,99,127,18);cc(1,99,64,19);pump();s=snapshot();pump();s=snapshot();
 check(static_cast<int>(s["midi"]["bindings"][147])==aion::midiBinding(2,99,1)&&p->inspectAudioThread(19).track.delay.values[0]==aion::delayMidiValue(0,64),"exact-channel provisional CC ignores foreign stream");
 check(request(R"({"operation":"midiConfig","action":"learn","target":150,"channel":16})")["ok"],"channel 16 Learn admitted");pump();
 cc(1,99,32);pump();s=snapshot();check(static_cast<int>(s["midi"]["learnTarget"])==150&&p->inspectAudioThread(19).track.delay.values[0]==aion::delayMidiValue(0,32),"other assigned stream keeps working while Learn waits");
 cc(16,99,0,17);cc(1,99,127,18);cc(16,99,64,19);pump();s=snapshot();pump();s=snapshot();
 check(static_cast<int>(s["midi"]["bindings"][150])==aion::midiBinding(2,99,16)&&p->inspectAudioThread(19).track.delay.values[0]==4&&p->inspectAudioThread(19).track.delay.values[3]==64/127.0,"disjoint channel assignments and same-block movement are independent");
 check(request(R"({"operation":"midiConfig","action":"learn","target":148})")["ok"],"legacy omitted channel keeps omni");pump();cc(2,99,12);pump();s=snapshot();pump();s=snapshot();check(static_cast<int>(s["midi"]["bindings"][148])==0&&static_cast<int>(s["midi"]["learnTarget"])==-1,"omni conflicts with either exact-channel binding");
 for(const char* invalid:{R"({"operation":"midiConfig","action":"learn","target":148,"channel":17})",R"({"operation":"midiConfig","action":"learn","target":148,"channel":-1})",R"({"operation":"midiConfig","action":"learn","target":148,"channel":"1"})",R"({"operation":"midiConfig","action":"learn","target":148,"channel":1.5})",R"({"operation":"midiConfig","action":"learn","target":148,"channel":null})"})check(!static_cast<bool>(request(invalid)["ok"]),"malformed filter rejected atomically");
 // Notes and PC pulses share this filter rather than a second Learn path.
 for(int kind:{1,3})for(int channel:{1,16}){
  const int target=73+(kind==3?2:0)+(channel==16?1:0);auto r=juce::JSON::parse(R"({"operation":"midiConfig","action":"learn"})");r.getDynamicObject()->setProperty("target",target);r.getDynamicObject()->setProperty("channel",channel);check(p->request({r})["ok"],"button channel Learn");pump();
  auto message=kind==1?juce::MidiMessage::noteOn(channel==1?16:1,99,static_cast<juce::uint8>(100)):juce::MidiMessage::programChange(channel==1?16:1,99);midi.addEvent(message,17);pump();s=snapshot();check(static_cast<int>(s["midi"]["learnTarget"])==target,"wrong-channel Note/PC leaves Learn armed");
  message=kind==1?juce::MidiMessage::noteOn(channel,99,static_cast<juce::uint8>(100)):juce::MidiMessage::programChange(channel,99);midi.addEvent(message,17);pump();s=snapshot();pump();s=snapshot();check(static_cast<int>(s["midi"]["bindings"][target])==aion::midiBinding(static_cast<unsigned>(kind),99,static_cast<unsigned>(channel)),"Note/PC captures requested channel");
 }
 p->getStateInformation(saved);x=p->getXmlFromBinary(saved.getData(),static_cast<int>(saved.getSize()));check(x->getChildByName("MidiSettings")->getIntAttribute("version")==10,"channel Learn retains schema and IDs");p->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));settle();s=snapshot();
 check(static_cast<int>(s["midi"]["bindings"][147])==aion::midiBinding(2,99,1)&&static_cast<int>(s["midi"]["bindings"][150])==aion::midiBinding(2,99,16),"channel assignments survive host save/restore");
 cc(1,99,0,17);cc(16,99,127,18);pump();s=snapshot();check(p->inspectAudioThread(19).track.delay.values[0]==aion::delayMidiValue(0,0)&&p->inspectAudioThread(19).track.delay.values[3]==1,"restored concurrent controls remain independent");
 check(p->getActiveEditor()==nullptr,"MIDI and persistence work without editor");std::cout<<"PASS native Delay MIDI "<<block<<": all CC/channel originals, same-block learn, bounded receipt saturation/recovery, joint gate, v7 migration; zero callback heap\n";
}
