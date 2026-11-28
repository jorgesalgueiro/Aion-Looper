// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <cstdlib>
#include <iostream>
#include <thread>
static void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;const int block=argc==2?juce::String(argv[1]).getIntValue():64;check(block==64||block==128,"64/128");
 auto p=std::make_unique<AionProcessor>();p->setPlayConfigDetails(2,2,8000,block);p->prepareToPlay(8000,block);
 juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;
 auto pump=[&]{audio.clear();p->processBlock(audio,midi);midi.clear();};
 auto snapshot=[&]{return p->request({juce::JSON::parse(R"({"operation":"snapshot"})")});};pump();auto s=snapshot();
 auto request=[&](const char* json){auto r=juce::JSON::parse(json);r.getDynamicObject()->setProperty("generation",s["generation"]);r.getDynamicObject()->setProperty("expiresAt",static_cast<double>(juce::Time::currentTimeMillis()+4000));return p->request({r});};
 auto settle=[&]{for(unsigned n=0;n<3000;++n){pump();s=snapshot();if(!static_cast<bool>(s["tracks"][0]["reverbBusy"]))return;std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(false,"engine settle deadline");};
 check(request(R"({"operation":"reverb","track":0,"action":"engine","engine":"machine"})")["queued"],"machine engine accepted");settle();
 s=snapshot();check(s["tracks"][0]["reverb"]["engine"].toString()=="machine","snapshot engine is machine");
 check(static_cast<double>(s["tracks"][0]["reverb"]["plate"]["decay"])==2.5,"plate defaults installed");
 check(request(R"({"operation":"midiConfig","action":"enable","enabled":true})")["ok"],"machine MIDI enabled");
 check(request(R"({"operation":"midiConfig","action":"learn","target":153})")["ok"],"machine decay learn");
 midi.addEvent(juce::MidiMessage::controllerEvent(1,20,0),17);midi.addEvent(juce::MidiMessage::controllerEvent(1,20,64),18);pump();
 for(unsigned n=0;n<1000;++n){pump();s=snapshot();if(static_cast<double>(s["tracks"][0]["reverb"]["plate"]["decay"])==aion::plateMidiValue(0,64))break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 check(static_cast<double>(s["tracks"][0]["reverb"]["plate"]["decay"])==aion::plateMidiValue(0,64),"machine MIDI drives the plate decay live");
 check(request(R"({"operation":"reverb","track":0,"action":"set","parameter":"decay","value":4})")["queued"],"plate decay accepted");settle();
 s=snapshot();check(static_cast<double>(s["tracks"][0]["reverb"]["plate"]["decay"])==4,"plate decay applied");
 check(request(R"({"operation":"reverb","track":0,"action":"preset","preset":"vocal-plate"})")["queued"],"plate preset accepted");settle();
 s=snapshot();check(s["tracks"][0]["reverb"]["platePreset"].toString()=="vocal-plate","plate preset identity"); 
 juce::MemoryBlock saved;p->getStateInformation(saved);auto xml=p->getXmlFromBinary(saved.getData(),static_cast<int>(saved.getSize()));
 auto* bank=xml?xml->getChildByName("LoopReverbs"):nullptr;check(bank&&bank->getIntAttribute("version")==2,"state v2");
 auto* row=bank?bank->getChildElement(0):nullptr;check(row&&row->getStringAttribute("engine")=="machine","state stores machine engine");
 auto q=std::make_unique<AionProcessor>();q->setPlayConfigDetails(2,2,8000,block);q->prepareToPlay(8000,block);
 q->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
 juce::AudioBuffer<float> qa(2,block);juce::MidiBuffer qm;auto qs=[&]{return q->request({juce::JSON::parse(R"({"operation":"snapshot"})")});};
 for(unsigned n=0;n<3000;++n){qa.clear();q->processBlock(qa,qm);qm.clear();s=qs();if(!static_cast<bool>(s["tracks"][0]["reverbBusy"]))break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 s=qs();check(s["tracks"][0]["reverb"]["engine"].toString()=="machine"&&s["tracks"][0]["reverb"]["platePreset"].toString()=="vocal-plate","restored machine engine and preset");
 check(request(R"({"operation":"reverb","track":0,"action":"engine","engine":"convolution"})")["queued"],"convolution switch accepted");settle();
 s=snapshot();check(s["tracks"][0]["reverb"]["engine"].toString()=="convolution","switch back to convolution");
 std::cout<<"PASS Machine engine: command switch, plate set/preset, v2 save/restore and switch back at "<<block<<"\n";
}
