#include "core/Engine.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>
extern thread_local bool watch;extern std::atomic<int> allocations;
namespace {
using namespace aion;using A=LoopAction;using S=LoopState;
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct Fixture {
 std::unique_ptr<Engine> engine=std::make_unique<Engine>();unsigned block;std::uint32_t revision=0,request=0;EngineView latest{};
 explicit Fixture(unsigned n):block(n){engine->prepare(8000,131072);engine->setMonitoring(false);engine->setTarget(1);MidiSettings settings;settings.enabled=true;settings.revision=++revision;check(engine->configureMidi(settings),"configure MIDI");process(0);}
 void process(unsigned frames,std::span<const MidiEvent> events={}){float l[128],r[128];std::fill_n(l,128,0.2f);std::fill_n(r,128,-0.1f);float* channels[]={l,r};watch=true;engine->process(channels,2,static_cast<int>(frames),events);watch=false;EngineView view;while(engine->popView(view))latest=view;}
 void advance(unsigned frames){while(frames){const auto n=std::min(frames,block);process(n);frames-=n;}}
 void send(unsigned status,unsigned a,unsigned b,unsigned size=3){const MidiEvent e{0,static_cast<std::uint8_t>(status),static_cast<std::uint8_t>(a),static_cast<std::uint8_t>(b),static_cast<std::uint8_t>(size)};process(1,{&e,1});}
 void command(A action,unsigned id=0,float value=0){check(engine->enqueueLoop({action,id,++request,engine->generation(),0,value}),"MIDI fixture command");process(1);}
 void wait(){for(unsigned n=0;n<10000;++n){process(0);if(!engine->finalizationPending())return;std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(false,"MIDI fixture worker timeout");}
 void tap(unsigned note){send(144,note,100);send(128,note,0);}
};
}
void midiTests(){
 using namespace aion;using A=LoopAction;using S=LoopState;
 MidiSettings config;check(MidiController::valid(config),"original MIDI defaults valid");config.bindings[10]=midiBinding(1,36);check(!MidiController::valid(config),"overlapping omni mappings rejected");config.bindings[10]=midiBinding(3,0,16);check(MidiController::valid(config),"channel program binding valid");config.bindings[11]=midiBinding(2,123);check(!MidiController::valid(config),"panic controllers cannot be bound");
 for(const unsigned block:{64u,128u}){
  Fixture f(block);
  // Offset within a callback must delimit input capture exactly.
  const MidiEvent start{17,144,36,99,3};f.process(64,{&start,1});check(f.latest.tracks[0].state==S::recording&&f.latest.tracks[0].length==47,"sample-offset MIDI record");
  f.send(144,36,127);check(f.latest.tracks[0].state==S::recording,"repeated note-on debounced");f.send(144,36,0);f.advance(128);f.tap(36);f.wait();check(f.latest.tracks[0].state==S::playing&&f.latest.tracks[0].waveformId,"MIDI finish commits playable PCM");
  const auto length=f.latest.tracks[0].length;f.tap(36);check(f.latest.tracks[0].state==S::stopped&&f.latest.tracks[0].length==length,"note tap stops without deleting");
  f.tap(36);check(f.latest.tracks[0].state==S::playing,"note tap plays");f.send(144,36,100);f.advance(31998);check(f.latest.tracks[0].length==length,"STOP held less than four seconds keeps PCM");f.send(128,36,0);check(f.latest.tracks[0].length==length,"3999-sample-boundary release keeps PCM");
  f.tap(36);f.send(144,36,100);f.advance(31999);check(f.latest.tracks[0].length==length,"before exact deadline");f.send(128,36,0);check(f.latest.tracks[0].state==S::empty,"exact four-second release deletes only STOP origin");
  f.tap(36);f.advance(128);f.tap(36);f.wait();check(f.latest.tracks[0].state==S::playing,"rerecord after MIDI delete");
  f.send(144,36,100);f.advance(1000);f.send(252,0,0,1);f.advance(40000);check(f.latest.tracks[0].length>0&&f.latest.tracks[0].state==S::stopped,"MIDI Stop cancels pending delete and playback");f.send(128,36,0);
  f.tap(46);check(f.latest.midi.mode==1,"OD note mode");f.tap(47);check(f.latest.midi.mode==2,"SUB mode excludes OD");f.tap(47);check(f.latest.midi.mode==0,"SUB toggles off");
  f.tap(48);check(f.latest.midi.sustain,"SUS mode enabled");f.send(144,36,100);check(f.latest.tracks[0].state==S::playing,"SUS press plays");f.send(128,36,0);check(f.latest.tracks[0].state==S::stopped,"SUS release stops");f.tap(48);
  f.tap(36);f.tap(48);f.send(144,36,100);check(f.latest.tracks[0].muted,"SUS press momentarily mutes a playing loop");
  MidiSettings disabled;disabled.revision=++f.revision;check(f.engine->configureMidi(disabled),"disable held sustain");f.process(0);check(!f.latest.tracks[0].muted,"Disabling MIDI restores momentary mute without deleting PCM");
  disabled.enabled=true;disabled.revision=++f.revision;check(f.engine->configureMidi(disabled),"reenable after held note");f.process(0);f.send(128,36,0);f.tap(48);
  f.send(144,38,100);f.advance(40000);check(f.latest.tracks[2].state==S::recording&&f.latest.tracks[2].length>32000,"Long RECORD-origin note never cuts/deletes take");f.send(128,38,0);f.tap(38);f.wait();check(f.latest.tracks[2].state==S::playing,"Long record can finish normally");
  f.engine->setMidiInputAvailable(false);f.tap(39);check(f.latest.tracks[3].state==S::empty,"MIDI cannot record absent/muted native input");f.engine->setMidiInputAvailable(true);
  check(f.engine->enqueueLoop({A::play,0,++f.request,f.engine->generation(),f.engine->clock()+1000,0}),"future GUI play before MIDI Stop");
  f.send(252,0,0,1);f.advance(2000);check(f.latest.tracks[0].state==S::stopped&&f.latest.lastRequestId==f.request&&f.latest.rejected,"MIDI Stop rejects/acknowledges future queued GUI actions without resurrection");
  MidiSettings mapped;mapped.enabled=true;mapped.revision=++f.revision;mapped.bindings[0]=midiBinding(3,0);mapped.bindings[1]=midiBinding(2,20);check(f.engine->configureMidi(mapped),"FCB program and CC mappings");f.process(0);
  f.send(192,0,0,2);check(f.latest.tracks[0].state==S::playing,"program pulse plays");f.advance(40000);check(f.latest.tracks[0].length>0&&f.latest.tracks[0].state==S::playing,"program has no synthetic hold/delete");
  f.send(176,20,127);f.advance(40000);check(f.latest.tracks[1].state==S::recording,"positive CC edge records without destructive hold");f.send(176,20,64);check(f.latest.tracks[1].state==S::recording,"expression repeats do not retrigger button");f.send(176,20,0);f.send(176,20,127);f.wait();check(f.latest.tracks[1].state==S::playing,"CC release rearms next positive edge");
  mapped.learnTarget=19;mapped.learnToken=7;mapped.learnGeneration=f.engine->generation();mapped.revision=++f.revision;check(f.engine->configureMidi(mapped),"learn starts");f.process(0);f.send(191,21,0);MidiLearnResult learned;check(!f.engine->popMidiLearn(learned),"CC zero never learns");f.send(207,12,0,2);check(f.engine->popMidiLearn(learned)&&learned.target==19&&learned.binding==midiBinding(3,12)&&learned.generation==f.engine->generation(),"zero-valued program learns on channel 16");
  f.command(A::stopAll);f.advance(40000);check(f.latest.tracks[0].length>0&&f.latest.tracks[1].length>0,"panic retains clips");
  mapped.learnTarget=-1;mapped.enabled=false;mapped.revision=++f.revision;check(f.engine->configureMidi(mapped),"disable MIDI");f.process(0);f.send(192,0,0,2);check(f.latest.tracks[0].state==S::stopped,"disabled MIDI ignores input");
 }
 for(const unsigned block:{64u,128u})for(const auto syncMode:{SyncMode::fixed,SyncMode::dynamic}){
  Fixture f(block);f.command(A::record);f.advance(1000);f.command(A::finish);f.wait();f.process(0);
  check(f.latest.tracks[0].state==S::playing,"sync PC fixture plays");
  MidiSettings settings;settings.enabled=true;settings.bindings[0]=midiBinding(3,0);settings.revision=++f.revision;check(f.engine->configureMidi(settings),"sync PC settings");
  f.engine->configureSync({syncMode,120,2,4,4,false});f.send(192,0,0,2);
  check(f.latest.tracks[0].state==S::stopping,"one synced PC pulse schedules STOP, never PLAY");
  f.send(192,0,0,2);f.advance(8);f.send(192,0,0,2);
  check(f.latest.tracks[0].state==S::stopping&&f.latest.midi.actions==1,"physical triple-PC burst stops once, never requeues playback");
  check(f.latest.midi.observed==3&&f.latest.midi.recent[0].event.status==192&&f.latest.midi.recent[0].event.size==2,"bounded raw MIDI observation");
  f.advance(32001);check(f.latest.tracks[0].state==S::stopped,"synced PC stop settles after grid");
  f.send(192,0,0,2);check(f.latest.tracks[0].state==S::queued,"synced PC on stopped queues play");
  f.advance(32001);check(f.latest.tracks[0].state==S::playing,"synced PC play settles after grid");
 }
 for(const unsigned block:{64u,128u}){
  Fixture f(block);MidiSettings settings;settings.enabled=true;settings.bindings[0]=midiBinding(3,0);settings.revision=++f.revision;check(f.engine->configureMidi(settings),"program burst config");
  const std::array<MidiEvent,3> burst{{{0,192,0,0,2},{0,192,0,0,2},{8,192,0,0,2}}};f.process(16,burst);
  check(f.latest.tracks[0].state==S::recording&&f.latest.midi.messages==3&&f.latest.midi.actions==1,"physical same-block PC burst starts one recording");
  // Boundaries stay sample accurate: 159 is suppressed, 160 is a separate tap.
  f.advance(143);f.send(192,0,0,2);check(f.latest.tracks[0].state==S::recording,"19.875ms duplicate suppressed");
  f.send(192,0,0,2);f.wait();f.process(0);check(f.latest.tracks[0].state==S::playing,"20ms distinct PC tap accepted without extending debounce");
  f.advance(160);f.process(16,burst);check(f.latest.tracks[0].state==S::stopped,"PC burst stops once with sync off");
 }
 std::vector<std::uint8_t> reference;
 for(const unsigned block:{64u,128u})for(const bool fromMidi:{false,true}){
  auto engine=std::make_unique<Engine>();engine->prepare(8000,8192);engine->setMonitoring(false);engine->setTarget(1);
  MidiSettings settings;settings.enabled=fromMidi;settings.revision=1;check(engine->configureMidi(settings),"MIDI/audio comparison config");
  if(!fromMidi){check(engine->enqueueLoop({A::record,0,1,engine->generation(),17,0})&&engine->enqueueLoop({A::finish,0,2,engine->generation(),900,0}),"sample-scheduled direct record/finish");}
  float l[128],r[128];float* input[]={l,r};
  for(unsigned offset=0;offset<1024;offset+=block){
   std::array<MidiEvent,4> events{};std::size_t count=0;
   for(const auto& e:std::array<MidiEvent,4>{{{17,144,36,100,3},{18,128,36,0,3},{900,144,36,100,3},{901,128,36,0,3}}})if(fromMidi&&e.offset>=offset&&e.offset<offset+block){auto local=e;local.offset-=offset;events[count++]=local;}
   for(unsigned i=0;i<block;++i){l[i]=0.4f*std::sin((offset+i)*0.11f);r[i]=0.2f*std::cos((offset+i)*0.093f);}
   watch=true;engine->process(input,2,static_cast<int>(block),{events.data(),count});watch=false;
  }
  EngineView latest{},view{};for(unsigned n=0;n<10000;++n){watch=true;engine->process(nullptr,0,0);watch=false;while(engine->popView(view))latest=view;if(!engine->finalizationPending()&&latest.tracks[0].state==S::playing)break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  check(latest.tracks[0].length==883,"MIDI/direct sample-identical capture span");
  check(engine->enqueueExport({0,engine->generation(),latest.tracks[0].waveformId}),"MIDI comparison stored PCM");watch=true;engine->process(nullptr,0,0);watch=false;WavExport output;bool ready=false;
  for(unsigned n=0;n<10000&&!ready;++n){ready=engine->popExport(output);if(!ready)std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(ready&&!output.bytes.empty(),"MIDI stored PCM completed");
  if(reference.empty())reference=output.bytes;else check(reference==output.bytes,"MIDI and direct gestures capture exact stereo PCM at 64/128");
 }
 check(allocations.load()==0,"MIDI callback zero new/delete");std::cout<<"PASS: MIDI sample timing, Note/CC/PC mappings, debounce, four-second origin/release, sustain, panic and no callback new/delete\n";
}
