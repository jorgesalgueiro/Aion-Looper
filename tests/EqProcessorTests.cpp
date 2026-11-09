// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include "plugin/LocalCommandServer.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <cstdlib>
#include <iostream>
#include <thread>
static thread_local bool watching=false;
static unsigned allocations=0;
void* operator new(std::size_t n){if(watching)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{if(watching)++allocations;std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
static void check(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
int main(int argc,char** argv){
    juce::ScopedJuceInitialiser_GUI gui;
    // JUCE's dispatch loop is stopped once per process; it cannot be restarted
    // for a second fixture. Run the two block sizes in independent invocations.
    const int chosen=argc==2?juce::String(argv[1]).getIntValue():64;
    check((argc==1||argc==2)&&(chosen==64||chosen==128),"Usage: aion_eq_processor_tests [64|128]");
    for(int block:{chosen}){
        auto p=std::make_unique<AionProcessor>();p->setPlayConfigDetails(2,2,48000,block);p->prepareToPlay(48000,block);
        juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;midi.ensureSize(1024);
        auto snapshot=[&]{return p->request({juce::JSON::parse(R"({"operation":"snapshot"})")});};
        aion::EqSettings renderedEq;
        auto process=[&]{audio.clear();watching=true;p->processBlock(audio,midi);renderedEq=p->inspectAudioThread(19).track.equalizer;watching=false;midi.clear();return snapshot();};
        auto state=process();const auto generation=static_cast<int>(state["generation"]);
        auto request=[&](const char* json){auto v=juce::JSON::parse(json);auto* o=v.getDynamicObject();o->setProperty("generation",generation);o->setProperty("expiresAt",static_cast<double>(juce::Time::currentTimeMillis()+4000));return p->request({v});};
        check(request(R"({"operation":"equalizer","action":"preset","track":19,"preset":"Radio"})")["ok"],"expanded-track preset accepted");
        check(request(R"({"operation":"equalizer","action":"enable","track":19,"enabled":true})")["ok"],"EQ enabled");
        juce::MemoryBlock queued;p->getStateInformation(queued); // Save accepted intent before any next callback.
        state=process();check(state["tracks"][19]["equalizer"]["preset"].toString()=="Radio"&&static_cast<bool>(state["tracks"][19]["equalizer"]["enabled"]),"production audio acknowledged settings");
        juce::MemoryBlock saved;p->getStateInformation(saved);check(saved==queued,"queued state serializes same complete intent as audio acknowledgement");
        const auto invalid=request(R"({"operation":"equalizer","action":"set","track":19,"values":{"p1Gain":3,"p2Q":0}})");check(!static_cast<bool>(invalid["ok"]),"partial invalid batch rejected");
        juce::MemoryBlock after;p->getStateInformation(after);check(after==saved,"invalid batch preserves complete state");
        check(request(R"({"operation":"equalizer","action":"set","track":19,"values":{"p1Q":0.1,"p1Freq":1111,"p1Gain":-3}})")["ok"],"atomic XY and exact .1 Q endpoint accepted");
        state=process();check(state["tracks"][19]["equalizer"]["preset"].toString().isEmpty(),"manual edit clears preset");
        check(request(R"({"operation":"equalizer","action":"curve","track":19})")["points"].size()==256,"bounded independently owned curve");
        std::thread restore([&]{p->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));});restore.join();
        snapshot();state=process();check(state["tracks"][19]["equalizer"]["preset"].toString()=="Radio","host-thread restore is marshalled through sole message producer");
        p->getStateInformation(after);check(after==saved,"state roundtrip exact");
        for(const char* bad:{"nan","1e100","3garbage","-99"}){
            auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),static_cast<int>(saved.getSize()));xml->getChildByName("LoopEqualizers")->getChildElement(19)->setAttribute("p1Gain",bad);
            juce::MemoryBlock malformed;juce::AudioProcessor::copyXmlToBinary(*xml,malformed);p->setStateInformation(malformed.getData(),static_cast<int>(malformed.getSize()));p->getStateInformation(after);check(after==saved,"malformed EQ state rolls back APVTS/MIDI/EQ together");
        }
        const auto savedSyncMode=static_cast<int>(state["syncMode"]);
        check(request(R"({"operation":"setParameter","id":"syncMode","value":1})")["ok"],"static arm mode");process();
        for(const auto* cancelAction:{"stop","finish","stopAll"}){
            check(request(R"({"operation":"loop","action":"record","track":19})")["ok"],"arm with configured EQ");state=process();
            check(state["tracks"][19]["state"].toString()=="armed","static recording awaits grid");
            auto cancel=juce::JSON::parse(R"({"operation":"loop","action":"stop","track":19})");cancel.getDynamicObject()->setProperty("action",cancelAction);
            check(request(juce::JSON::toString(cancel,true).toRawUTF8())["ok"],"cancel armed recording");state=process();
            check(state["tracks"][19]["state"].toString()=="empty"&&renderedEq.enabled&&renderedEq.preset==8&&static_cast<bool>(state["tracks"][19]["equalizer"]["enabled"]),"ARM CANCEL must preserve configured EQ in actual audio AND UI, as original stopRecording does");
        }
        auto restoreMode=juce::JSON::parse(R"({"operation":"setParameter","id":"syncMode","value":0})");restoreMode.getDynamicObject()->setProperty("value",savedSyncMode);
        check(p->request({restoreMode})["ok"],"restore original sync mode");process();
        // Full performance queue: failed edits preserve intent. Restore waits
        // until a complete twenty-command batch fits, never publishing a prefix.
        unsigned accepted=0;for(unsigned i=0;i<300;++i)if(request(R"({"operation":"loop","action":"volume","track":0,"value":1})")["ok"])++accepted;
        check(accepted==255,"bounded production command queue");
        check(!static_cast<bool>(request(R"({"operation":"equalizer","action":"toggle","track":19})")["ok"]),"full queue rejects EQ toggle");
        p->getStateInformation(after);check(after==saved,"rejected toggle preserves persisted intent");
        auto flat=juce::AudioProcessor::getXmlFromBinary(saved.getData(),static_cast<int>(saved.getSize()));flat->removeChildElement(flat->getChildByName("LoopEqualizers"),true);
        juce::MemoryBlock legacy;juce::AudioProcessor::copyXmlToBinary(*flat,legacy);p->setStateInformation(legacy.getData(),static_cast<int>(legacy.getSize()));
        for(unsigned n=0;n<5;++n)state=process();
        check(!static_cast<bool>(state["tracks"][19]["equalizer"]["enabled"]),"legacy preset loads default EQ after full queue drains");
        // MIDI panic discards an EQ edit beyond this callback's 128-command
        // budget. Intent must reconcile to the real audio outcome, not revive.
        check(request(R"({"operation":"midiConfig","action":"enable","enabled":true})")["ok"],"MIDI enabled for panic routing");process();
        for(unsigned i=0;i<200;++i)check(request(R"({"operation":"loop","action":"volume","track":0,"value":1})")["ok"],"panic backlog");
        check(request(R"({"operation":"equalizer","action":"toggle","track":19})")["ok"],"queued EQ before MIDI panic");midi.addEvent(juce::MidiMessage::midiStop(),0);state=process();
        check(!static_cast<bool>(state["tracks"][19]["equalizer"]["enabled"]),"MIDI panic cannot revive discarded EQ intent");
        check(request(R"({"operation":"equalizer","action":"toggle","track":19})")["ok"],"fresh toggle recovers");state=process();
        check(request(R"({"operation":"loop","action":"clear","track":19})")["ok"],"DEL accepted");
        check(!static_cast<bool>(request(R"({"operation":"equalizer","action":"toggle","track":19})")["ok"]),"EQ waits behind unacknowledged DEL barrier");
        state=process();check(!static_cast<bool>(state["tracks"][19]["equalizer"]["enabled"]),"DEL resets EQ");
        check(request(R"({"operation":"midiConfig","action":"eqTrack","track":19})")["ok"],"EQ MIDI selected tab configured");
        check(request(R"({"operation":"midiConfig","action":"learn","target":97})")["ok"],"EQ slider MIDI learn starts");process();
        midi.addEvent(juce::MidiMessage::controllerEvent(1,11,0),17);midi.addEvent(juce::MidiMessage::controllerEvent(16,11,127),18);state=process();
        check(renderedEq.values[4]==24&&renderedEq.preset==0,"actual MidiBuffer learn consumes zero then immediately applies following EQ CC before message acknowledgement");
        state=process();check(static_cast<int>(state["midi"]["bindings"][97])==aion::midiBinding(2,11),"accepted EQ binding reaches the following audio configuration acknowledgement");
        check(request(R"({"operation":"midiConfig","action":"learn","target":98})")["ok"],"overlapping EQ learn starts");process();
        midi.addEvent(juce::MidiMessage::controllerEvent(1,11,0),19);state=process();
        check(static_cast<int>(state["midi"]["bindings"][98])==0&&static_cast<int>(state["midi"]["bindings"][97])==aion::midiBinding(2,11),"conflicting EQ binding rejected without stealing prior control");
        juce::MemoryBlock midiSaved;p->getStateInformation(midiSaved);
        for(const auto* bad:{"20","-1","nan","18446744073709551616"}){
            auto xml=juce::AudioProcessor::getXmlFromBinary(midiSaved.getData(),static_cast<int>(midiSaved.getSize()));xml->getChildByName("MidiSettings")->setAttribute("eqTrack",bad);
            juce::MemoryBlock malformed;p->copyXmlToBinary(*xml,malformed);p->setStateInformation(malformed.getData(),static_cast<int>(malformed.getSize()));p->getStateInformation(after);check(after==midiSaved,"malformed EQ MIDI tab rolls back complete state");
        }
        auto oldMidi=juce::AudioProcessor::getXmlFromBinary(midiSaved.getData(),static_cast<int>(midiSaved.getSize()));auto* oldMapping=oldMidi->getChildByName("MidiSettings");oldMapping->setAttribute("version",3);oldMapping->removeAttribute("eqTrack");
        for(unsigned i=93;i<aion::MidiSettings::bindingCount;++i)oldMapping->removeAttribute("binding"+juce::String(i));
        juce::MemoryBlock oldBytes;p->copyXmlToBinary(*oldMidi,oldBytes);p->setStateInformation(oldBytes.getData(),static_cast<int>(oldBytes.getSize()));state=process();
        check(static_cast<int>(state["midi"]["bindings"][97])==0&&static_cast<int>(state["midi"]["eqTrack"])==0,"version 3 preserves old bindings and defaults appended EQ mappings/tab");
        p->setStateInformation(midiSaved.getData(),static_cast<int>(midiSaved.getSize()));state=process();
        check(static_cast<int>(state["midi"]["bindings"][97])==aion::midiBinding(2,11)&&static_cast<int>(state["midi"]["eqTrack"])==19,"EQ MIDI bindings and selected tab survive state restore without editor");
        // A real external CLI process controls this production processor. The
        // local server is an explicit fixture, not another audio engine.
        const auto root=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("eq-cli-"+juce::Uuid().toString());
        {aion::LocalCommandServer server([&](const juce::var& r){return p->request({r});},root);check(server.ready(),"CLI mailbox ready");
            juce::ChildProcess child;juce::StringArray command{AION_CLI_PYTHON,AION_EQ_CLI_TEST,root.getFullPathName(),server.id()};check(child.start(command),"external CLI starts");
            const auto deadline=juce::Time::getMillisecondCounterHiRes()+30000;
            struct Pump final:juce::Timer {std::function<void()> tick;void timerCallback()override{tick();}} pump;
            pump.tick=[&]{process();if(!child.isRunning()||juce::Time::getMillisecondCounterHiRes()>=deadline)juce::MessageManager::getInstance()->stopDispatchLoop();};
            pump.startTimer(3);juce::MessageManager::getInstance()->runDispatchLoop();pump.stopTimer();
            if(child.isRunning()){child.kill();check(false,"CLI timed out");}
            const auto output=child.readAllProcessOutput();std::cout<<output;check(child.getExitCode()==0,"external CLI EQ acceptance");
        }check(root.deleteFile(),"empty fixture CLI root removed");
        check(request(R"({"operation":"equalizer","action":"preset","track":19,"preset":"Radio"})")["ok"],"settings before reprepare");process();
        check(request(R"({"operation":"equalizer","action":"enable","track":19,"enabled":true})")["ok"],"enable before reprepare");process();
        for(double rate:{48000.,22050.,32000.,96000.}){p->prepareToPlay(rate,block);state=process();check(static_cast<double>(state["sampleRate"])==rate,"sample-rate lifecycle");check(state["tracks"][19]["equalizer"]["preset"].toString()=="Radio"&&static_cast<bool>(state["tracks"][19]["equalizer"]["enabled"]),"same/changed sample rate preserves requested EQ values and enable state");}
        std::cout<<"PASS production EQ state/validation/restore/cancel/CLI at "<<block<<" samples\n";
    }
    check(allocations==0,"no processor callback allocation/free");
}
