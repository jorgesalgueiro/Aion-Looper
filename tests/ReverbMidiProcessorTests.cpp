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
 auto pump=[&]{audio.clear();watch=true;p->processBlock(audio,midi);watch=false;midi.clear();check(!operations,"zero Reverb MIDI callback new/delete");};
 auto snapshot=[&]{return p->request({juce::JSON::parse(R"({"operation":"snapshot"})")});};pump();auto s=snapshot();
 auto request=[&](const char* json){auto r=juce::JSON::parse(json);r.getDynamicObject()->setProperty("generation",s["generation"]);r.getDynamicObject()->setProperty("expiresAt",static_cast<double>(juce::Time::currentTimeMillis()+4000));return p->request({r});};
 auto cc=[&](int channel,int controller,int value,int offset=17){midi.addEvent(juce::MidiMessage::controllerEvent(channel,controller,value),offset);};
 auto settle=[&]{for(unsigned n=0;n<2000;++n){pump();s=snapshot();if(!static_cast<bool>(s["tracks"][19]["reverbBusy"]))return;std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(false,"Reverb MIDI restore deadline");};
 check(aion::MidiSettings::bindingCount==157&&aion::MidiSettings::reverbBase==153,"appended reverb targets 153..156 keep the 93-slot schema base");
 check(request(R"({"operation":"midiConfig","action":"enable","enabled":true})")["ok"],"MIDI enabled");
 check(request(R"({"operation":"midiConfig","action":"eqTrack","track":19})")["ok"],"selected expanded loop");
 check(request(R"({"operation":"midiConfig","action":"learn","target":153})")["ok"],"Reverb Volume learn");pump();
 cc(1,20,0,17);cc(16,20,64,18);cc(1,20,127,19);pump();s=snapshot();pump();s=snapshot();
 {const int learned=static_cast<int>(s["midi"]["bindings"][153]);if(learned!=aion::midiBinding(2,20))std::cerr<<"DEBUG binding="<<learned<<" expected="<<aion::midiBinding(2,20)<<" learnTarget="<<static_cast<int>(s["midi"]["learnTarget"])<<" messages="<<static_cast<int>(s["midi"]["messages"])<<" rejected="<<static_cast<int>(s["midi"]["rejected"])<<'\n';check(learned==aion::midiBinding(2,20),"Reverb Volume CC learn accepted");}
 check(p->inspectAudioThread(19).track.reverb.values[0]==aion::reverbMidiValue(0,127),"same-block learned CC applies after the consumed first packet");
 juce::MemoryBlock saved;p->getStateInformation(saved);auto x=p->getXmlFromBinary(saved.getData(),static_cast<int>(saved.getSize()));
 auto* mappings=x->getChildByName("MidiSettings");check(mappings&&mappings->getIntAttribute("version")==9,"schema v9");
 for(unsigned j=0;j<4;++j)mappings->setAttribute("binding"+juce::String(153+j),static_cast<int>(aion::midiBinding(2,20+j)));
 p->copyXmlToBinary(*x,saved);p->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));settle();
 for(unsigned channel=1;channel<=16;++channel)for(unsigned j=0;j<4;++j)for(unsigned value:{0u,32u,64u,127u}){cc(static_cast<int>(channel),20+static_cast<int>(j),static_cast<int>(value));pump();
  check(p->inspectAudioThread(19).track.reverb.values[aion::reverbMidiControls[j]]==aion::reverbMidiValue(j,value),"all four live B controls accept exact CC values/channels");
  s=snapshot();const auto control=aion::reverbControls[aion::reverbMidiControls[j]].name;const juce::String controlName(control.data(),static_cast<int>(control.size()));
  check(static_cast<double>(s["tracks"][19]["reverb"]["values"][juce::Identifier(controlName)])==aion::reverbMidiValue(j,value),"snapshot exposes the B MIDI target");}
 // Full 0..127 sweep on Mix proves the documented value mapping without shape edits.
 for(unsigned value=0;value<128;++value){cc(1,21,static_cast<int>(value));pump();check(p->inspectAudioThread(19).track.reverb.values[1]==aion::reverbMidiValue(1,value),"full Mix CC sweep");}
 check(!request(R"({"operation":"midiConfig","action":"learn","target":157})")["ok"],"duration/decay and absent slots stay unmapped");
 for(unsigned t=0;t<20;++t)if(t!=19)check(p->inspectAudioThread(t).track.reverb==aion::ReverbSettings{},"unselected tracks never receive the MIDI edit");
 // Bounded receipt saturation: 600 same-block controls exceed the 512-result
 // queue; service drains it, then a fresh control applies exactly.
 juce::MemoryBlock beforeSaturation;p->getStateInformation(beforeSaturation);
 for(unsigned n=0;n<3;++n){for(unsigned i=0;i<200;++i)cc(1,22,static_cast<int>((i*7)%128),static_cast<int>(i%static_cast<unsigned>(block)));pump();}
 s=snapshot();check(static_cast<int>(s["midi"]["rejected"])>0,"saturated B receipts reject excess controls");
 for(unsigned n=0;n<40;++n){pump();s=snapshot();}cc(1,22,127);pump();s=snapshot();
 check(p->inspectAudioThread(19).track.reverb.values[4]==aion::reverbMidiValue(2,127),"fresh control recovers after saturation drain");
 // A whole-state load suspends MIDI ownership until its exact B receipt; a
 // packet during that window cannot change the old/native owner.
 p->setStateInformation(beforeSaturation.getData(),static_cast<int>(beforeSaturation.getSize()));const auto restored=p->inspectAudioThread(19).track.reverb;
 cc(1,20,127);pump();check(p->inspectAudioThread(19).track.reverb==restored,"MIDI is suspended during joint B restore");
 settle();cc(1,20,127);pump();check(p->inspectAudioThread(19).track.reverb.values[0]==aion::reverbMidiValue(0,127),"MIDI resumes after the joint receipt");
 // v8 states carry 153 bindings; the four new slots stay unassigned.
 juce::MemoryBlock v9;p->getStateInformation(v9);auto old=p->getXmlFromBinary(v9.getData(),static_cast<int>(v9.getSize()));auto* oldMappings=old->getChildByName("MidiSettings");
 oldMappings->setAttribute("version",8);oldMappings->setAttribute("binding152",static_cast<int>(aion::midiBinding(2,25)));for(unsigned j=0;j<4;++j)oldMappings->removeAttribute("binding"+juce::String(153+j));
 juce::MemoryBlock oldBytes;p->copyXmlToBinary(*old,oldBytes);p->setStateInformation(oldBytes.getData(),static_cast<int>(oldBytes.getSize()));settle();
 juce::MemoryBlock migrated;p->getStateInformation(migrated);auto mx=p->getXmlFromBinary(migrated.getData(),static_cast<int>(migrated.getSize()));auto* mm=mx->getChildByName("MidiSettings");
 check(mm->getIntAttribute("version")==9,"v8 migrates to v9");for(unsigned j=0;j<4;++j)check(mm->getIntAttribute("binding"+juce::String(153+j),-1)==0,"new B slots start unassigned in v8 states");
 check(mm->getIntAttribute("binding152",-1)==aion::midiBinding(2,25),"legacy Delay binding survives migration");
 check(p->getActiveEditor()==nullptr,"Reverb MIDI works without an editor");
 std::cout<<"PASS native Reverb MIDI "<<block<<": four live targets, all CC/channel values, learn, saturation/recovery, joint restore gate and v8 migration; zero callback heap\n";
}
