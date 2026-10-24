#include "plugin/Processor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <cstdlib>
#include <iostream>
#include <new>
#include <limits>
#include <cmath>
namespace {thread_local bool audioWatch=false;std::atomic<int> audioAllocations{0};void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}}
void* operator new(std::size_t n){if(audioWatch)++audioAllocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {if(audioWatch)++audioAllocations;std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
class TestPlayHead final : public juce::AudioPlayHead {
public:
 juce::Optional<PositionInfo> position;
 mutable unsigned calls=0,outsideAudio=0;
 juce::Optional<PositionInfo> getPosition() const override {++calls;if(!audioWatch)++outsideAudio;return position;}
};
class StateHostListener final : public juce::AudioProcessorListener {
public:
 unsigned changes=0; bool rescan=false,dirty=false;
 void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {}
 void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails& details) override {
  ++changes;rescan=details.parameterInfoChanged;dirty=details.nonParameterStateChanged;
 }
};
int main(){
 juce::ScopedJuceInitialiser_GUI gui;
 {
  auto p=std::make_unique<AionProcessor>();
  const auto routing=p->request({juce::JSON::parse(R"({"operation":"midiDevices"})")});
  check(routing.getProperty("ok",false)&&!static_cast<bool>(routing["standalone"])&&!static_cast<bool>(routing["deviceWatchRequested"])&&routing["devices"].size()==0,"Host-routed processor must not probe/watch OS MIDI devices");
  juce::MemoryBlock saved;p->getStateInformation(saved);
  auto* gain=p->parameters.getParameter("masterGain");check(gain!=nullptr,"Master parameter exists");
  gain->setValueNotifyingHost(0.75f);
  StateHostListener listener;p->addListener(&listener);
  p->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
  check(listener.changes==1&&listener.rescan&&!listener.dirty,"Accepted state restore must request one host parameter rescan without marking a new edit");
  check(gain->getValue()==0.0f,"State restore recovers original master value");
  juce::MemoryBlock restored;p->getStateInformation(restored);check(saved==restored,"Host notification does not mutate saved state");
  listener.changes=0;
  const char invalid[]="invalid state";p->setStateInformation(invalid,sizeof(invalid));p->setStateInformation(nullptr,0);
  check(listener.changes==0,"Rejected state must not notify host");
  p->getStateInformation(restored);check(saved==restored,"Random/empty state rejection preserves all saved bytes");
  // Paused audio: accepted state updates fill the bounded MIDI settings queue.
  for(unsigned n=0;n<100;++n)p->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
  check(listener.changes>0&&listener.changes<100,"Only queued state restores notify the host");
  listener.changes=0;gain->setValueNotifyingHost(0.5f);
  p->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
  check(listener.changes==0&&gain->getValue()==0.5f,"Queue rejection must preserve current parameters without reporting a successful restore");
  p->removeListener(&listener);
  std::cout<<"PASS: accepted state host rescan, invalid/queue rejection, exact saved-state preservation\n";
 }
 // The production processor must forward host offline mode, including changes
 // after prepare, and commit within the same callback without UI polling sleeps.
 for(int block:{64,128}){
  auto p=std::make_unique<AionProcessor>();p->setPlayConfigDetails(2,2,8000,block);p->prepareToPlay(8000,block);p->setNonRealtime(true);
  const auto rpc=[&](const char* json){auto reply=p->request({juce::JSON::parse(json)});check(reply.getProperty("ok",false),json);return reply;};
  rpc(R"({"operation":"midiConfig","action":"enable","enabled":true})");rpc(R"({"operation":"setParameter","id":"syncMode","value":0})");rpc(R"({"operation":"setParameter","id":"masterGain","value":1})");
  juce::AudioBuffer<float> buffer(2,block);juce::MidiBuffer packets;packets.ensureSize(4096);unsigned frame=0;
  const auto process=[&]{for(int i=0;i<block;++i){buffer.setSample(0,i,static_cast<float>(std::sin((frame+i)*.071)*.3));buffer.setSample(1,i,static_cast<float>(std::cos((frame+i)*.053)*.2));}audioWatch=true;p->processBlock(buffer,packets);audioWatch=false;packets.clear();frame+=block;return rpc(R"({"operation":"snapshot"})");};
  packets.addEvent(juce::MidiMessage::noteOn(1,36,juce::uint8{100}),17);process();packets.addEvent(juce::MidiMessage::noteOff(1,36),0);process();for(int i=0;i<8;++i)process();
  const auto before=rpc(R"({"operation":"snapshot"})");const int length=static_cast<int>(before["tracks"][0]["length"])+17;
  packets.addEvent(juce::MidiMessage::noteOn(1,36,juce::uint8{100}),17);auto state=process();
  check(state["tracks"][0]["state"].toString()=="playing"&&static_cast<int>(state["tracks"][0]["length"])==length,"Host offline mode must finish exact-offset recording within the callback");
  check(static_cast<juce::int64>(state["tracks"][0]["waveformId"])>0,"Offline processor committed an immutable PCM clip");
  for(int i=0;i<4;++i)process();check(buffer.getMagnitude(0,block)>0.01f,"Offline production processor renders actual nonzero recorded PCM");
  p->setNonRealtime(false);process();p->setNonRealtime(true);process();check(static_cast<int>(rpc(R"({"operation":"snapshot"})")["tracks"][0]["length"])==length,"Live/offline switches preserve the committed clip");
 }
 // VST3 program overflow is an atomic block rejection, not synthetic MIDI.
 for(const int block:{64,128}){
  auto p=std::make_unique<AionProcessor>();p->setPlayConfigDetails(2,2,8000,block);p->prepareToPlay(8000,block);
  const auto rpc=[&](const juce::String& text){auto v=p->request({juce::JSON::parse(text)});check(v.getProperty("ok",false),text.toRawUTF8());return v;};
  rpc(R"({"operation":"midiConfig","action":"enable","enabled":true})");rpc(R"({"operation":"setParameter","id":"syncMode","value":0})");
  for(auto* raw:p->getParameters())if(auto* parameter=dynamic_cast<juce::AudioProcessorParameterWithID*>(raw))check(!aion::vst3::programId(juce::VST3ClientExtensions::convertJuceParameterId(parameter->paramID,true)),"Wrapper routing IDs must not collide with existing APVTS parameters");
  check(p->getNumPrograms()==1,"MIDI routing does not fabricate application presets");
  juce::MemoryBlock before,after;p->getStateInformation(before);p->setCurrentProgram(127);p->getStateInformation(after);check(before==after,"Program routing never invokes application preset state changes");
  juce::AudioBuffer<float> audio(2,block);audio.clear();juce::MidiBuffer midi;midi.ensureSize(4096);midi.addEvent(juce::MidiMessage::noteOn(1,36,juce::uint8{100}),17);
  audioWatch=true;p->programInputOverflow();p->processBlock(audio,midi);audioWatch=false;
  auto state=rpc(R"({"operation":"snapshot"})");check(static_cast<int>(state["midi"]["overflows"])==1&&state["tracks"][0]["state"].toString()=="empty"&&midi.getNumEvents()==1&&state["midi"]["recent"].size()==0&&static_cast<int>(state["midi"]["messages"])==0,"Wrapper overflow drops all actions without fabricating/altering raw MIDI");
  audioWatch=true;p->processBlock(audio,midi);audioWatch=false;state=rpc(R"({"operation":"snapshot"})");check(static_cast<int>(state["midi"]["overflows"])==1&&state["tracks"][0]["state"].toString()=="recording"&&static_cast<int>(state["tracks"][0]["length"])==block-17,"Fresh callback recovers after wrapper overflow with exact sample offset");
 }
 std::cout<<"PASS: wrapper routing IDs/APVTS/preset isolation, atomic overflow block rejection/recovery, unchanged raw MIDI and no callback allocations at 64/128\n";
 // Version 3 appends MUTE bindings; old versions retain every existing pedal/CC assignment.
 for(const int version:{1,2,3}){
  auto p=std::make_unique<AionProcessor>();p->setPlayConfigDetails(2,2,8000,64);p->prepareToPlay(8000,64);juce::MemoryBlock original;p->getStateInformation(original);
  auto xml=juce::AudioProcessor::getXmlFromBinary(original.getData(),static_cast<int>(original.getSize()));auto* stored=xml->getChildByName("MidiSettings");stored->setAttribute("version",version);stored->setAttribute("enabled",1);stored->setAttribute("binding19",static_cast<int>(aion::midiBinding(3,0)));
  const unsigned count=version==1?33u:version==2?73u:93u;for(unsigned i=count;i<aion::MidiSettings::bindingCount;++i)stored->removeAttribute("binding"+juce::String(i));
  if(version>=2){stored->setAttribute("binding33",static_cast<int>(aion::midiBinding(2,11)));stored->setAttribute("binding72",static_cast<int>(aion::midiBinding(2,12,16)));}if(version==3)stored->setAttribute("binding73",static_cast<int>(aion::midiBinding(1,60)));
  juce::MemoryBlock bytes;juce::AudioProcessor::copyXmlToBinary(*xml,bytes);p->setStateInformation(bytes.getData(),static_cast<int>(bytes.getSize()));juce::MemoryBlock saved;p->getStateInformation(saved);auto migrated=juce::AudioProcessor::getXmlFromBinary(saved.getData(),static_cast<int>(saved.getSize()));const auto* midiState=migrated->getChildByName("MidiSettings");
  check(midiState->getIntAttribute("version")==3&&midiState->getIntAttribute("binding19")==aion::midiBinding(3,0),"Native MUTE schema preserves legacy PC0 and writes version 3");check(midiState->getIntAttribute("binding33",-1)==(version>=2?aion::midiBinding(2,11):0)&&midiState->getIntAttribute("binding72",-1)==(version>=2?aion::midiBinding(2,12,16):0),"Native MUTE schema preserves both ends of legacy continuous targets");
  for(unsigned i=73;i<aion::MidiSettings::bindingCount;++i)check(midiState->getIntAttribute("binding"+juce::String(i),-1)==(version==3&&i==73?aion::midiBinding(1,60):0),"Old states initialise MUTE mappings unassigned");
  juce::AudioBuffer<float> buffer(2,64);buffer.clear();juce::MidiBuffer midi;midi.ensureSize(4096);midi.addEvent(juce::MidiMessage::noteOn(1,60,juce::uint8{100}),17);midi.addEvent(juce::MidiMessage::noteOn(1,60,juce::uint8{127}),18);midi.addEvent(juce::MidiMessage::noteOff(1,60),19);audioWatch=true;p->processBlock(buffer,midi);audioWatch=false;
  const auto state=p->request({juce::JSON::parse(R"({"operation":"snapshot"})")});check(state["midi"]["bindings"].size()==93&&static_cast<bool>(state["tracks"][0]["muted"])==(version==3)&&state["tracks"][0]["state"].toString()=="empty"&&midi.getNumEvents()==3,"Restored MUTE controls work without an editor, preserve repeats/release and raw MIDI pass-through");
  auto future=juce::AudioProcessor::getXmlFromBinary(saved.getData(),static_cast<int>(saved.getSize()));future->getChildByName("MidiSettings")->setAttribute("version",4);juce::MemoryBlock invalid;juce::AudioProcessor::copyXmlToBinary(*future,invalid);p->setStateInformation(invalid.getData(),static_cast<int>(invalid.getSize()));juce::MemoryBlock after;p->getStateInformation(after);check(after==saved,"Unknown future MIDI schema rejects the entire preset without clearing mappings");
 }
 std::cout<<"PASS: MIDI versions 1/2/3 preserve all legacy pedal/CC mappings; persisted MUTE works without editor, repeat/NoteOff isolation, future-schema rejection and no callback allocations\n";
 for(const int block:{64,128}){
  // Real MidiBuffer offsets: panic followed by fresh NoteOn in the same block.
  for(const int channel:{1,16}){
   auto p=std::make_unique<AionProcessor>();p->setPlayConfigDetails(2,2,8000,block);p->prepareToPlay(8000,block);
   const auto rpc=[&](const juce::String& json){auto state=p->request({juce::JSON::parse(json)});check(state.getProperty("ok",false),json.toRawUTF8());return state;};
   rpc(R"({"operation":"midiConfig","action":"enable","enabled":true})");rpc(R"({"operation":"setParameter","id":"syncMode","value":0})");rpc(R"({"operation":"setParameter","id":"inputMonitor","value":0})");rpc(R"({"operation":"setParameter","id":"masterGain","value":1})");
   juce::AudioBuffer<float> buffer(2,block);juce::MidiBuffer packets;packets.ensureSize(4096);unsigned sample=0;
   const auto run=[&]{for(int c=0;c<2;++c)for(int i=0;i<block;++i)buffer.setSample(c,i,static_cast<float>((c?0.3:0.4)*std::sin((sample+i)*(c?0.053:0.071))));sample+=block;const auto count=packets.getNumEvents();audioWatch=true;p->processBlock(buffer,packets);audioWatch=false;check(packets.getNumEvents()==count,"Panic recovery preserves raw MidiBuffer pass-through");packets.clear();return rpc(R"({"operation":"snapshot"})");};
   packets.addEvent(juce::MidiMessage::noteOn(1,48,juce::uint8{100}),0);packets.addEvent(juce::MidiMessage::noteOn(channel,36,juce::uint8{100}),17);run();run();run();packets.addEvent(juce::MidiMessage::noteOff(channel,36),17);auto state=run();
   for(unsigned n=0;n<10000&&state["tracks"][0]["state"].toString()=="finalizing";++n){std::this_thread::sleep_for(std::chrono::milliseconds(1));state=run();}
   check(state["tracks"][0]["state"].toString()=="playing","Recovery processor fixture records real input");const auto waveform=state["tracks"][0]["waveformId"];const auto length=state["tracks"][0]["length"];const auto history=state["tracks"][0]["undoCount"];
   packets.addEvent(juce::MidiMessage::noteOn(1,48,juce::uint8{100}),0);run(); // leave SUS
   for(const int panic:{252,120,123}){
    packets.addEvent(juce::MidiMessage::noteOff(channel,36),0);packets.addEvent(juce::MidiMessage::midiStop(),1);state=run();check(state["tracks"][0]["state"].toString()=="stopped","Recovery processor baseline stopped");
    packets.addEvent(juce::MidiMessage::noteOn(channel,36,juce::uint8{100}),17);state=run();check(state["tracks"][0]["state"].toString()=="playing","Held processor note starts playback");
    packets.addEvent(panic==252?juce::MidiMessage::midiStop():juce::MidiMessage::controllerEvent(16,panic,0),18);packets.addEvent(juce::MidiMessage::noteOn(channel,36,juce::uint8{100}),19);packets.addEvent(juce::MidiMessage::noteOn(channel,36,juce::uint8{127}),20);packets.addEvent(juce::MidiMessage::noteOff(channel,36),21);state=run();
    const auto expected=panic==252?"playing":"stopped";check(state["tracks"][0]["state"].toString()==expected,"Raw panic/fresh/repeated NoteOn offsets must act exactly once in same block");for(int n=0;n<4;++n)state=run();
    check(panic==252?buffer.getMagnitude(0,block)>0.01f:buffer.getMagnitude(0,block)==0,"Actual recovered processor playback renders nonzero/silent PCM");check(state["tracks"][0]["waveformId"]==waveform&&state["tracks"][0]["length"]==length&&state["tracks"][0]["undoCount"]==history,"Actual panic/fresh note preserves clip/history");
   }
   // The last case stops normally; a held SUS play is cancelled and immediately reowned.
   packets.addEvent(juce::MidiMessage::noteOn(1,48,juce::uint8{100}),0);run();packets.addEvent(juce::MidiMessage::noteOn(channel,36,juce::uint8{100}),17);state=run();check(state["tracks"][0]["state"].toString()=="playing","SUS processor starts owned play");packets.addEvent(juce::MidiMessage::controllerEvent(16,120,0),18);packets.addEvent(juce::MidiMessage::noteOn(channel,36,juce::uint8{100}),19);state=run();check(state["tracks"][0]["state"].toString()=="playing","Cancelled SUS play reowns immediately");packets.addEvent(juce::MidiMessage::noteOff(channel,36),17);state=run();check(state["tracks"][0]["state"].toString()=="stopped","Fresh SUS release stops its own resumed play");
   std::cout<<"PASS: actual panic/fresh NoteOn same-block offsets, active-repeat debounce, stereo PCM and SUS ownership on channel "<<channel<<" at "<<block<<" samples\n";
  }
  for(const int zeroChannels:{0,2}){
   auto p=std::make_unique<AionProcessor>();p->setPlayConfigDetails(2,2,8000,block);p->prepareToPlay(8000,block);
   const auto rpc=[&](const char* json){auto reply=p->request({juce::JSON::parse(json)});check(reply.getProperty("ok",false),json);return reply;};
   rpc(R"({"operation":"midiConfig","action":"enable","enabled":true})");rpc(R"({"operation":"setParameter","id":"syncMode","value":0})");
   juce::AudioBuffer<float> normal(2,block),zero(zeroChannels,0);juce::MidiBuffer midi;midi.ensureSize(4096);
   const auto run=[&](bool flush){auto& buffer=flush?zero:normal;if(!flush)for(int c=0;c<2;++c)for(int i=0;i<block;++i)buffer.setSample(c,i,c?-0.1f:0.2f);const auto count=midi.getNumEvents();audioWatch=true;p->processBlock(buffer,midi);audioWatch=false;check(midi.getNumEvents()==count,"Zero callback retains MIDI pass-through");midi.clear();return rpc(R"({"operation":"snapshot"})");};
   midi.addEvent(juce::MidiMessage::noteOn(1,48,juce::uint8{100}),0);midi.addEvent(juce::MidiMessage::noteOn(1,36,juce::uint8{100}),0);auto state=run(true);
   check(state["tracks"][0]["state"].toString()=="recording"&&static_cast<bool>(state["midi"]["sustain"])&&static_cast<juce::int64>(state["frame"])==0,"Actual zero-sample MIDI starts held recording at frame zero");
   run(false);run(false);state=run(false);const auto frame=static_cast<juce::int64>(state["frame"]);check(frame==block*3,"Only nonzero callbacks advance the frame clock");
   rpc(R"({"operation":"midiConfig","action":"learn","target":33})");run(true);midi.addEvent(juce::MidiMessage::noteOff(1,99),0);const juce::uint8 malformed[]={176,120,128};midi.addEvent(malformed,3,0);state=run(true);
   check(state["tracks"][0]["state"].toString()=="recording"&&static_cast<int>(state["midi"]["learnTarget"])==33&&static_cast<int>(state["midi"]["overflows"])==0&&static_cast<juce::int64>(state["frame"])==frame,"Actual harmless/malformed zero MIDI preserves held recording, learn and clock");
   midi.addEvent(juce::MidiMessage::controllerEvent(1,11,0),0);run(true);run(true);midi.addEvent(juce::MidiMessage::controllerEvent(1,11,127),0);state=run(true);
   check(static_cast<int>(state["midi"]["bindings"][33])==aion::midiBinding(2,11)&&static_cast<double>(state["tracks"][0]["volume"])==2&&static_cast<juce::int64>(state["frame"])==frame,"Zero-sample CC learn and continuous control apply without rendering");
   midi.addEvent(juce::MidiMessage::noteOff(1,36),0);state=run(true);
   for(unsigned n=0;n<10000&&state["tracks"][0]["state"].toString()=="finalizing";++n){std::this_thread::sleep_for(std::chrono::milliseconds(1));state=run(true);}
   const auto waveform=static_cast<juce::int64>(state["tracks"][0]["waveformId"]);check(waveform>0&&state["tracks"][0]["state"].toString()=="playing"&&static_cast<int>(state["tracks"][0]["length"])==frame&&static_cast<juce::int64>(state["frame"])==frame,"Zero NoteOff commits the full take without a phantom sample");
   midi.addEvent(juce::MidiMessage::midiStop(),0);state=run(true);check(state["tracks"][0]["state"].toString()=="stopped"&&static_cast<juce::int64>(state["tracks"][0]["waveformId"])==waveform&&static_cast<juce::int64>(state["frame"])==frame,"Zero-sample MIDI Stop takes effect and preserves PCM");
   rpc(R"({"operation":"midiConfig","action":"learn","target":19})");run(true);midi.addEvent(juce::MidiMessage::programChange(1,0),0);run(true);state=run(true);check(static_cast<int>(state["midi"]["bindings"][19])==aion::midiBinding(3,0)&&state["tracks"][19]["state"].toString()=="empty","Zero-sample PC0 learns and consumes its packet");
   midi.addEvent(juce::MidiMessage::programChange(1,0),0);state=run(true);check(state["tracks"][19]["state"].toString()=="recording","Zero-sample PC main task dispatches without requiring rendered samples");
   midi.addEvent(juce::MidiMessage::midiStop(),0);run(true);midi.addEvent(juce::MidiMessage::noteOn(1,48,juce::uint8{100}),1);state=run(true);check(static_cast<int>(state["midi"]["overflows"])==1,"Positive MIDI offset remains invalid in zero callback");
   midi.addEvent(juce::MidiMessage::noteOn(1,48,juce::uint8{100}),-1);state=run(true);check(static_cast<int>(state["midi"]["overflows"])==2&&static_cast<bool>(state["midi"]["sustain"]),"Negative zero-block offset is rejected before mode dispatch");
   for(int n=0;n<257;++n)midi.addEvent(juce::MidiMessage::noteOff(1,99),0);state=run(true);check(static_cast<int>(state["midi"]["overflows"])==3&&static_cast<juce::int64>(state["frame"])==frame,"Zero-sample event cap remains bounded without clock drift");
   midi.addEvent(juce::MidiMessage::noteOn(1,48,juce::uint8{100}),block);state=run(false);check(static_cast<int>(state["midi"]["overflows"])==4&&static_cast<bool>(state["midi"]["sustain"])&&static_cast<juce::int64>(state["frame"])==frame+block,"Ordinary nonzero callback retains exclusive end-offset validation and normal sample clock");
   std::cout<<"PASS: actual zero-sample MidiBuffer held capture/CC learn/PC0/Stop and frame stability with "<<zeroChannels<<" channels between "<<block<<"-sample blocks\n";
  }

  {
   auto p=std::make_unique<AionProcessor>();p->setPlayConfigDetails(2,2,8000,block);p->prepareToPlay(8000,block);
   const auto rpc=[&](const char* json){auto reply=p->request({juce::JSON::parse(json)});check(reply.getProperty("ok",false),json);return reply;};
   rpc(R"({"operation":"midiConfig","action":"enable","enabled":true})");rpc(R"({"operation":"setParameter","id":"syncMode","value":0})");
   juce::AudioBuffer<float> buffer(2,block);juce::MidiBuffer packets;packets.ensureSize(4096);
   const auto run=[&]{for(int c=0;c<2;++c)for(int i=0;i<block;++i)buffer.setSample(c,i,c?-0.1f:0.2f);const auto count=packets.getNumEvents();audioWatch=true;p->processBlock(buffer,packets);audioWatch=false;check(packets.getNumEvents()==count,"Malformed panic preserves incoming raw MIDI");packets.clear();return rpc(R"({"operation":"snapshot"})");};
   packets.addEvent(juce::MidiMessage::noteOn(1,48,juce::uint8{100}),1);packets.addEvent(juce::MidiMessage::noteOn(1,36,juce::uint8{100}),17);run();run();run();
   rpc(R"({"operation":"midiConfig","action":"learn","target":33})");run();
   for(unsigned channel=0;channel<16;++channel)for(unsigned cc:{120u,123u})for(unsigned bad:{128u,255u}){const juce::uint8 bytes[]={static_cast<juce::uint8>(176+channel),static_cast<juce::uint8>(cc),static_cast<juce::uint8>(bad)};packets.addEvent(bytes,3,17);}
   auto state=run();check(state["tracks"][0]["state"].toString()=="recording"&&static_cast<int>(state["midi"]["learnTarget"])==33,"Actual malformed MidiBuffer packets preserve SUS capture and slider learn on every channel");
   rpc(R"({"operation":"midiConfig","action":"cancelLearn"})");run();packets.addEvent(juce::MidiMessage::noteOff(1,36),17);state=run();
   for(unsigned n=0;n<10000&&state["tracks"][0]["state"].toString()=="finalizing";++n){std::this_thread::sleep_for(std::chrono::milliseconds(1));state=run();}
   check(state["tracks"][0]["state"].toString()=="playing"&&static_cast<int>(state["tracks"][0]["length"])>=128,"Real release still commits malformed-packet test audio");
   for(int cc:{120,123}){packets.addEvent(juce::MidiMessage::noteOn(1,36,juce::uint8{100}),17);state=run();check(static_cast<bool>(state["tracks"][0]["muted"]),"Valid panic control starts held mute");packets.addEvent(juce::MidiMessage::controllerEvent(16,cc,0),17);state=run();check(!static_cast<bool>(state["tracks"][0]["muted"]),"Actual valid panic retains cancellation");packets.addEvent(juce::MidiMessage::noteOff(1,36),17);run();}
   std::cout<<"PASS: actual malformed CC120/123 preserves SUS/learn; valid panic still releases at "<<block<<" samples\n";
  }

  {
   TestPlayHead head;auto p=std::make_unique<AionProcessor>();p->setPlayConfigDetails(2,2,48000,block);p->prepareToPlay(48000,block);p->setPlayHead(&head);
   juce::AudioBuffer<float> buffer(2,block);buffer.clear();juce::MidiBuffer midi;midi.ensureSize(256);
   const auto snapshot=[&]{return p->request({juce::JSON::parse(R"({"operation":"snapshot"})")});};
   const auto run=[&]{audioWatch=true;p->processBlock(buffer,midi);audioWatch=false;return snapshot();};
   auto state=run(),host=state["hostTransport"];check(host.isObject(),"Host transport snapshot must be published");
   check(!static_cast<bool>(host["available"])&&host["bpm"].isVoid()&&host["samplePosition"].isVoid(),"Missing host does not invent timing");
   juce::AudioPlayHead::PositionInfo position;position.setBpm(137.3);position.setPpqPosition(-1.25);position.setTimeInSamples(-12000);position.setTimeInSeconds(-0.25);
   position.setTimeSignature(juce::AudioPlayHead::TimeSignature{7,8});position.setLoopPoints(juce::AudioPlayHead::LoopPoints{-8,6});position.setPpqPositionOfLastBarStart(-3.5);
   position.setIsPlaying(true);position.setIsRecording(true);position.setIsLooping(true);head.position=position;
   state=run();host=state["hostTransport"];
   check(static_cast<bool>(host["available"])&&static_cast<bool>(host["playing"])&&static_cast<bool>(host["recording"])&&static_cast<bool>(host["looping"]),"Host playback/record/cycle state");
   check(static_cast<double>(host["bpm"])==137.3&&static_cast<double>(host["ppq"])==-1.25&&static_cast<juce::int64>(host["samplePosition"])==-12000&&static_cast<double>(host["seconds"])==-0.25,"Fractional tempo and negative preroll remain exact");
   check(static_cast<int>(host["numerator"])==7&&static_cast<int>(host["denominator"])==8&&static_cast<double>(host["loopStartPpq"])==-8&&static_cast<double>(host["loopEndPpq"])==6&&static_cast<double>(host["barStartPpq"])==-3.5,"Host meter/bar/cycle markers");
   check(static_cast<juce::int64>(host["callbackFrame"])==static_cast<juce::int64>(state["frame"])-block,"Host position describes block start, not end");
   check(static_cast<double>(state["syncBpm"])==120,"Observing host tempo never overwrites internal sync settings");
   buffer.setSize(2,0);state=run();host=state["hostTransport"];check(static_cast<juce::int64>(host["callbackFrame"])==static_cast<juce::int64>(state["frame"])&&static_cast<double>(host["bpm"])==137.3,"Zero-sample host callbacks publish position without advancing local time");buffer.setSize(2,block);buffer.clear();
   const auto calls=head.calls;juce::MemoryBlock saved;p->getStateInformation(saved);snapshot();check(head.calls==calls&&head.outsideAudio==0,"Host is queried only once per audio callback, never IPC/state save");
   for(const double invalid:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}){
    position.setBpm(invalid);position.setPpqPosition(invalid);position.setTimeInSeconds(invalid);position.setPpqPositionOfLastBarStart(invalid);position.setLoopPoints(juce::AudioPlayHead::LoopPoints{0,invalid});position.setTimeSignature(juce::AudioPlayHead::TimeSignature{0,0});head.position=position;
    state=run();host=state["hostTransport"];for(const auto* key:{"bpm","ppq","seconds","barStartPpq","loopStartPpq","loopEndPpq","numerator","denominator"})check(host[key].isVoid(),"Invalid optional host data clears previous valid field");
    check(!juce::JSON::toString(host).contains("nan")&&!juce::JSON::toString(host).contains("inf"),"IPC has no nonfinite host numbers");
   }
   for(const double bpm:{0.0,-1.0}){position.setBpm(bpm);head.position=position;check(run()["hostTransport"]["bpm"].isVoid(),"Nonpositive host tempo is unavailable");}
   for(const auto points:{juce::AudioPlayHead::LoopPoints{8,2},juce::AudioPlayHead::LoopPoints{4,4}}){position.setLoopPoints(points);head.position=position;check(run()["hostTransport"]["loopStartPpq"].isVoid(),"Reversed/empty host loop interval is unavailable");}
   position={};position.setTimeSignature(juce::AudioPlayHead::TimeSignature{3,4});position.setLoopPoints(juce::AudioPlayHead::LoopPoints{2,8});position.setPpqPosition(7.9);position.setTimeInSamples(24000);head.position=position;run();
   position.setPpqPosition(2);position.setTimeInSamples(0);head.position=position;host=run()["hostTransport"];check(static_cast<double>(host["ppq"])==2&&static_cast<juce::int64>(host["samplePosition"])==0&&!static_cast<bool>(host["playing"]),"Backward seek/cycle wrap and stopped host remain observable");
   head.position=juce::AudioPlayHead::PositionInfo{};host=run()["hostTransport"];check(static_cast<bool>(host["available"])&&host["ppq"].isVoid()&&host["numerator"].isVoid()&&host["loopStartPpq"].isVoid(),"Empty PositionInfo clears every optional field");
   head.position.reset();check(!static_cast<bool>(run()["hostTransport"]["available"]),"Missing PositionInfo clears availability");p->setPlayHead(nullptr);check(!static_cast<bool>(run()["hostTransport"]["available"]),"Detached host cannot leave old playing state");
   check(head.outsideAudio==0,"Every host read stayed on audio");
   auto reference=std::make_unique<AionProcessor>();reference->setPlayConfigDetails(2,2,48000,block);reference->prepareToPlay(48000,block);p->setPlayHead(&head);
   for(auto* processor:{p.get(),reference.get()})for(const auto* json:{R"({"operation":"setParameter","id":"masterGain","value":1})",R"({"operation":"setParameter","id":"inputMonitor","value":1})",R"({"operation":"setParameter","id":"syncMode","value":0})"})check(processor->request({juce::JSON::parse(json)}).getProperty("ok",false),"Configure monitored PCM control");
   juce::AudioBuffer<float> expected(2,block);bool heard=false;
   for(int n=0;n<40;++n){
    position={};position.setBpm(90.25+n);position.setPpqPosition(n%3==0?-4.0:double(n%8));position.setTimeInSamples(n%3==0?-24000:n*block);position.setIsPlaying(n%2==0);position.setIsLooping(n%3==0);position.setTimeSignature(juce::AudioPlayHead::TimeSignature{n%2?7:3,n%2?8:4});position.setLoopPoints(juce::AudioPlayHead::LoopPoints{-4,12});head.position=n%5?juce::Optional<juce::AudioPlayHead::PositionInfo>{position}:juce::Optional<juce::AudioPlayHead::PositionInfo>{};
    for(int c=0;c<2;++c)for(int i=0;i<block;++i){const auto sample=float((n*block+i)%101-50)/150*(c?0.7f:1.0f);buffer.setSample(c,i,sample);expected.setSample(c,i,sample);}
    audioWatch=true;p->processBlock(buffer,midi);reference->processBlock(expected,midi);audioWatch=false;
    for(int c=0;c<2;++c)for(int i=0;i<block;++i){check(buffer.getSample(c,i)==expected.getSample(c,i),"Host tempo/play/meter/loop/seek observation leaves actual PCM bit-identical");heard=heard||std::abs(buffer.getSample(c,i))>0.01f;}
    state=snapshot();check(static_cast<double>(state["syncBpm"])==120&&static_cast<int>(state["syncMode"])==0,"Host observation does not change internal user controls");
   }
   check(heard&&head.outsideAudio==0,"Host comparison checks nonzero PCM with audio-only reads");p->setPlayHead(nullptr);
   std::cout<<"PASS: production host optional/malformed/seek/meter/cycle snapshots and identical monitored PCM at "<<block<<" samples\n";

  }

  {
   auto retry=std::make_unique<AionProcessor>();retry->setPlayConfigDetails(2,2,8000,block);retry->prepareToPlay(8000,block);
   const auto request=[&](const juce::String& json){auto result=retry->request({juce::JSON::parse(json)});check(result.getProperty("ok",false),json.toRawUTF8());return result;};
   juce::AudioBuffer<float> buffer(2,block);buffer.clear();juce::MidiBuffer packets;packets.ensureSize(256);
   const auto run=[&]{const auto count=packets.getNumEvents();audioWatch=true;retry->processBlock(buffer,packets);audioWatch=false;check(packets.getNumEvents()==count,"Recovered note preserves host MIDI");packets.clear();};
   request(R"({"operation":"setParameter","id":"syncMode","value":0})");request(R"({"operation":"midiConfig","action":"enable","enabled":true})");packets.addEvent(juce::MidiMessage::noteOn(1,36,juce::uint8{100}),17);run();
   for(int offset=block;offset<80000;offset+=block)run();
   packets.addEvent(juce::MidiMessage::noteOn(1,36,juce::uint8{100}),17);packets.addEvent(juce::MidiMessage::noteOn(1,36,juce::uint8{100}),18);run();request(R"({"operation":"snapshot"})");run();
   const auto state=request(R"({"operation":"snapshot"})");check(state["tracks"][0]["state"].toString()!="recording"&&static_cast<int>(state["tracks"][0]["length"])==80001,"Production NoteOn recovers lost release strictly after ten seconds at its sample offset");
  }
  {
   auto modes=std::make_unique<AionProcessor>();modes->setPlayConfigDetails(2,2,8000,block);modes->prepareToPlay(8000,block);
   check(modes->request({juce::JSON::parse(R"({"operation":"midiConfig","action":"enable","enabled":true})")}).getProperty("ok",false),"Enable no-editor mode-note fixture");
   juce::AudioBuffer<float> buffer(2,block);buffer.clear();juce::MidiBuffer packets;packets.ensureSize(1024);
   const auto run=[&]{const auto count=packets.getNumEvents();audioWatch=true;modes->processBlock(buffer,packets);audioWatch=false;check(packets.getNumEvents()==count,"Mode toggles preserve host MIDI");packets.clear();return modes->request({juce::JSON::parse(R"({"operation":"snapshot"})")});};
   for(const int note:{46,47,48}){
    packets.addEvent(juce::MidiMessage::noteOn(1,note,juce::uint8{1}),17);auto state=run();check(note==48?static_cast<bool>(state["midi"]["sustain"]):static_cast<int>(state["midi"]["mode"])==note-45,"First mode NoteOn toggles on at offset 17");
    packets.addEvent(juce::MidiMessage::noteOn(1,note,juce::uint8{127}),18);state=run();check(static_cast<int>(state["midi"]["mode"])==0&&!static_cast<bool>(state["midi"]["sustain"]),"Second mode NoteOn toggles off without NoteOff");
    packets.addEvent(juce::MidiMessage::noteOff(1,note),19);packets.addEvent(juce::MidiMessage::noteOn(1,note,juce::uint8{0}),20);state=run();check(static_cast<int>(state["midi"]["mode"])==0&&!static_cast<bool>(state["midi"]["sustain"]),"Mode releases never toggle");
   }
   for(int i=0;i<80;++i)packets.addEvent(juce::MidiMessage::noteOn(1,48,juce::uint8{100}),i%2?18:17);
   const auto state=run();check(!static_cast<bool>(state["midi"]["sustain"])&&static_cast<int>(state["midi"]["overflows"])==0,"Repeated mode pulses never exhaust held-press storage");
   packets.addEvent(juce::MidiMessage::noteOn(1,36,juce::uint8{100}),17);run();
   const auto config=[&](const char* json){check(modes->request({juce::JSON::parse(json)}).getProperty("ok",false),"Reassigned mode configuration accepted");};
   config(R"({"operation":"midiConfig","action":"clear","target":0})");config(R"({"operation":"midiConfig","action":"learn","target":30})");run();
   packets.addEvent(juce::MidiMessage::noteOn(1,36,juce::uint8{127}),18);run();run();
   packets.addEvent(juce::MidiMessage::noteOn(1,36,juce::uint8{1}),19);auto reassigned=run();check(static_cast<int>(reassigned["midi"]["mode"])==1&&reassigned["tracks"][0]["state"].toString()=="recording","Learned global NoteOn bypasses its former held loop press without touching capture");
   packets.addEvent(juce::MidiMessage::noteOn(1,36,juce::uint8{127}),20);packets.addEvent(juce::MidiMessage::noteOff(1,36),21);reassigned=run();check(static_cast<int>(reassigned["midi"]["mode"])==0&&reassigned["tracks"][0]["state"].toString()=="recording","Reassigned mode repeat/release preserves former recording");
  }
  for(const int cancellation:{252,120,123,300})for(const bool staleViews:{false,true}){
   auto cancelled=std::make_unique<AionProcessor>();cancelled->setPlayConfigDetails(2,2,8000,block);cancelled->prepareToPlay(8000,block);
   const auto request=[&](const juce::String& json){auto result=cancelled->request({juce::JSON::parse(json)});check(result.getProperty("ok",false),json.toRawUTF8());return result;};
   juce::AudioBuffer<float> buffer(2,block);buffer.clear();juce::MidiBuffer packets;packets.ensureSize(8192);
   const auto run=[&]{audioWatch=true;cancelled->processBlock(buffer,packets);audioWatch=false;packets.clear();};
   request(R"({"operation":"midiConfig","action":"learn","target":19})");run();
   if(staleViews)for(int n=0;n<10;++n)run(); // Saturate telemetry before learn/panic.
   packets.addEvent(juce::MidiMessage::programChange(1,3),17);
   if(cancellation==252)packets.addEvent(juce::MidiMessage::midiStop(),18);
   else if(cancellation!=300)packets.addEvent(juce::MidiMessage::controllerEvent(1,cancellation,0),18);
   run();
   if(cancellation==300){for(int n=0;n<300;++n)packets.addEvent(juce::MidiMessage::noteOn(1,40,juce::uint8{100}),0);run();}
   auto state=request(R"({"operation":"snapshot"})");check(static_cast<int>(state["midi"]["bindings"][19])==0,"Panic/overflow after learn capture must invalidate unacknowledged binding");
   juce::MemoryBlock saved;cancelled->getStateInformation(saved);auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),static_cast<int>(saved.getSize()));
   check(xml->getChildByName("MidiSettings")->getIntAttribute("binding19")==0,"Cancelled learn must not persist a stale binding before audio acknowledges it");
   run();state=request(R"({"operation":"snapshot"})");check(static_cast<int>(state["midi"]["bindings"][19])==0,"Cancelled binding cannot revive on the next callback");
   request(R"({"operation":"midiConfig","action":"learn","target":19})");run();packets.addEvent(juce::MidiMessage::programChange(1,5),17);run();request(R"({"operation":"snapshot"})");run();
   state=request(R"({"operation":"snapshot"})");check(static_cast<int>(state["midi"]["bindings"][19])==aion::midiBinding(3,5)&&state["tracks"][19]["state"].toString()=="empty","Fresh learning recovers after panic invalidates previous capture");
  }
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
  for(const auto* operation:{"hello","not-supported"}){
   const auto request=juce::JSON::parse("{\"operation\":\""+juce::String(operation)+"\",\"_aionSession\":\"current-document\"}");const auto reply=p->request({request});check(reply["_aionSession"].toString()=="current-document","Native success/error replies echo document correlation token");
  }
  for(const auto* token:{"null","17","\"\"","[]","{}"}){const auto reply=p->request({juce::JSON::parse("{\"operation\":\"hello\",\"_aionSession\":"+juce::String(token)+"}")});
   // Void/null represents a legacy request with no correlation envelope.
   if(juce::String(token)!="null")check(!reply.getProperty("ok",false),"Invalid native bridge session rejected");}
  auto longSession=juce::JSON::parse(R"({"operation":"hello"})");longSession.getDynamicObject()->setProperty("_aionSession",juce::String::repeatedString("x",129));check(!p->request({longSession}).getProperty("ok",false),"Oversized bridge token rejected");
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
  rpc(R"({"operation":"midiConfig","action":"learn","target":17})");
  rpc(R"({"operation":"midiConfig","action":"cancelLearn","target":16})");check(static_cast<int>(rpc(R"({"operation":"snapshot"})")["midi"]["learnTarget"])==17,"Blur of old mapping cannot cancel newer field learn before callbacks resume");
  rpc(R"({"operation":"midiConfig","action":"cancelLearn","target":17})");check(static_cast<int>(rpc(R"({"operation":"snapshot"})")["midi"]["learnTarget"])==-1,"Matching field blur cancels learn immediately");process();
  // Pause callbacks and fill all 31 usable MIDI configuration entries.
  for(const bool scoped:{false,true}){
  rpc(R"({"operation":"midiConfig","action":"learn","target":18})");
  for(int i=0;i<30;++i)rpc("{\"operation\":\"midiConfig\",\"action\":\"mode\",\"mode\":"+juce::String(i%3)+"}");
  const auto cancelled=p->request({juce::JSON::parse(scoped?R"({"operation":"midiConfig","action":"cancelLearn","target":18})":R"({"operation":"midiConfig","action":"cancelLearn"})")});check(cancelled.getProperty("ok",false),"Field/Escape cancellation must succeed when audio is paused and MIDI configuration queue is full");
  process();midi.addEvent(juce::MidiMessage::programChange(1,3),0);process();state=rpc(R"({"operation":"snapshot"})");check(static_cast<int>(state["midi"]["learnTarget"])==-1&&static_cast<int>(state["midi"]["bindings"][18])==0,"Cancelled full-queue learn cannot revive after callbacks resume");
  }
  rpc(R"({"operation":"midiConfig","action":"learn","target":18})");process();midi.addEvent(juce::MidiMessage::programChange(1,5),0);process();rpc(R"({"operation":"snapshot"})");process();state=rpc(R"({"operation":"snapshot"})");check(static_cast<int>(state["midi"]["bindings"][18])==aion::midiBinding(3,5)&&state["tracks"][18]["state"].toString()=="empty","Fresh learn works after full-queue cancellation and consumes its packet");
  midi.clear();for(int i=0;i<300;++i)midi.addEvent(juce::MidiMessage::noteOn(1,40,juce::uint8{100}),0);process();state=rpc(R"({"operation":"snapshot"})");check(static_cast<int>(state["midi"]["overflows"])>0,"Bounded MidiBuffer overflow reported and held actions cancelled");
  for(const auto* request:{R"({"operation":"midiConfig","action":"learn","target":20})",R"({"operation":"midiConfig","action":"mode","mode":7})",R"({"operation":"midiConfig","action":"enable","enabled":1})",R"({"operation":"midiConfig","action":"cancelLearn","target":20})",R"({"operation":"midiConfig","action":"cancelLearn","target":"17"})"})check(!p->request({juce::JSON::parse(request)}).getProperty("ok",false),"Invalid MIDI IPC rejected");
 }
 check(audioAllocations.load()==0,"Production processBlock MIDI path must perform zero new/delete");
 std::cout<<"PASS: actual juce::MidiBuffer offsets, PC0 learn/consume, state roundtrip/invalid rollback, cancellation/overflow/IPC and zero processBlock new/delete at 64/128\n";
}
