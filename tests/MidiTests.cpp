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
 // Malformed high-bit CC data must never invoke the native panic path.
 for(const unsigned block:{64u,128u})for(const unsigned cc:{120u,123u}){
  std::vector<std::uint8_t> baseline;
  for(const bool inject:{false,true}){
   Fixture f(block);f.tap(48);f.send(144,36,100);f.advance(256);
   const auto start=f.latest.tracks[0].captureStartFrame;
   MidiSettings learn;learn.enabled=true;learn.learnTarget=19;learn.learnToken=1;learn.learnGeneration=f.engine->generation();learn.revision=++f.revision;check(f.engine->configureMidi(learn),"Learn during held malformed-panic test");f.process(0);
   for(unsigned channel=0;channel<16;++channel)for(unsigned bad:{128u,255u}){
    if(inject)f.send(176+channel,cc,bad);else f.process(1);
    check(f.latest.tracks[0].state==S::recording,"Malformed CC120/123 must not finish a held SUS recording");check(f.latest.midi.learnTarget==19,"Malformed panic must not cancel pending MIDI learn");
   }
   const auto length=f.latest.frame-start;f.send(128,36,0);f.wait();check(f.latest.tracks[0].length==length&&f.latest.tracks[0].state==S::playing,"Normal NoteOff finishes the entire uncorrupted take");
   check(f.engine->enqueueExport({0,f.engine->generation(),f.latest.tracks[0].waveformId}),"Export malformed-panic stereo PCM");f.process(0);WavExport wav;bool ready=false;
   for(unsigned n=0;n<10000&&!ready;++n){ready=f.engine->popExport(wav);if(!ready)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
   check(ready&&wav.bytes.size()>44&&std::any_of(wav.bytes.begin()+44,wav.bytes.end(),[](auto v){return v!=0;}),"Malformed-panic comparison contains actual stereo audio");
   if(!inject)baseline=wav.bytes;else check(wav.bytes==baseline,"Malformed CC panic packets preserve exact recorded stereo WAV at 64/128");
   learn.learnTarget=-1;++learn.learnToken;learn.revision=++f.revision;check(f.engine->configureMidi(learn),"Leave learn before valid panic controls");f.process(0);
   for(unsigned channel=0;channel<16;++channel)for(unsigned value:{0u,127u}){
    f.send(144,36,100);check(f.latest.tracks[0].muted,"Valid panic test owns a SUS mute");f.send(176+channel,cc,value);check(!f.latest.tracks[0].muted&&f.latest.tracks[0].length==length,"Valid seven-bit panic retains existing cancellation and PCM");f.send(128,36,0);
   }
  }
 }

 for(const unsigned block:{64u,128u}){
  Fixture f(block);f.tap(48);f.send(144,36,100);f.advance(128);
  MidiSettings settings;settings.enabled=true;settings.learnTarget=34;settings.learnToken=1;settings.learnGeneration=f.engine->generation();settings.revision=++f.revision;
  check(f.engine->configureMidi(settings),"learn while SUS recording");f.process(0);
  check(f.latest.tracks[0].state==S::recording,"Starting unrelated slider learn must not release held SUS recording");
  f.send(176,13,0);settings.bindings[34]=midiBinding(2,13);settings.learnTarget=-1;++settings.learnToken;settings.revision=++f.revision;
  check(f.engine->configureMidi(settings),"acknowledge slider learn while holding SUS");f.process(0);
  check(f.latest.tracks[0].state==S::recording,"Acknowledging slider learn must preserve held SUS recording");
  f.send(128,36,0);f.wait();check(f.latest.tracks[0].state==S::playing,"Original note release still finishes held recording after learn");
  f.send(144,36,100);check(f.latest.tracks[0].muted,"SUS mute fixture");
  settings.learnTarget=54;++settings.learnToken;settings.revision=++f.revision;check(f.engine->configureMidi(settings),"learn during SUS mute");f.process(0);
  check(f.latest.tracks[0].muted,"Starting learn must not restore an unrelated held mute");
  settings.learnTarget=-1;++settings.learnToken;settings.revision=++f.revision;check(f.engine->configureMidi(settings),"cancel during SUS mute");f.process(0);
  check(f.latest.tracks[0].muted,"Cancelling learn must not restore an unrelated held mute");f.send(128,36,0);check(!f.latest.tracks[0].muted,"SUS release restores mute after learn cancellation");
  f.tap(48);const auto length=f.latest.tracks[0].length;f.send(144,36,100);f.advance(16000);
  settings.learnTarget=54;++settings.learnToken;settings.revision=++f.revision;check(f.engine->configureMidi(settings),"learn during STOP hold");f.process(0);
  f.advance(15999);check(f.latest.tracks[0].length==length,"Learn preserves original four-second STOP deadline");f.send(128,36,0);check(f.latest.tracks[0].state==S::empty,"Uninterrupted STOP hold still deletes at four seconds after unrelated learn");
 }
 // A held CC button must receive its zero/release even if slider learn consumes it.
 for(const unsigned block:{64u,128u}){
  Fixture f(block);MidiSettings settings;settings.enabled=true;settings.bindings[0]=midiBinding(2,20);settings.revision=++f.revision;
  check(f.engine->configureMidi(settings),"held CC mapping");f.process(0);f.tap(48);
  f.send(176,20,127);f.advance(128);check(f.latest.tracks[0].state==S::recording,"held CC SUS record fixture");
  settings.learnTarget=34;settings.learnGeneration=f.engine->generation();++settings.learnToken;settings.revision=++f.revision;
  check(f.engine->configureMidi(settings),"slider learn while CC held");f.process(0);f.send(176,20,0);f.wait();
  check(f.latest.tracks[0].state==S::playing,"Consumed CC learn packet must still release its owned SUS recording");
  MidiLearnResult learned;check(f.engine->popMidiLearn(learned)&&learned.target==34&&learned.binding==midiBinding(2,20),"Owned release still reaches learn validation");
  f.send(176,20,127);check(!f.latest.tracks[0].muted,"Rejected provisional CC must not trigger an extra action before acknowledgement");
  settings.learnTarget=-1;++settings.learnToken;settings.revision=++f.revision;check(f.engine->configureMidi(settings),"reject overlapping CC learn");f.process(0);
  f.send(176,20,127);check(f.latest.tracks[0].muted,"Released CC rearms after rejected learn");
  settings.learnTarget=54;++settings.learnToken;settings.revision=++f.revision;check(f.engine->configureMidi(settings),"learn during held CC mute");f.process(0);
  f.send(191,20,0);check(f.latest.tracks[0].muted,"Other MIDI channel cannot release owned CC mute");
  f.send(176,20,0);check(!f.latest.tracks[0].muted,"Owned CC zero restores SUS mute even while rejected learn awaits acknowledgement");
  settings.learnTarget=-1;++settings.learnToken;settings.revision=++f.revision;check(f.engine->configureMidi(settings),"cancel CC mute learn");f.process(0);
  f.command(A::stopImmediate);f.send(176,20,127);check(f.latest.tracks[0].state==S::playing,"held CC plays stopped clip");
  settings.learnTarget=34;++settings.learnToken;settings.revision=++f.revision;check(f.engine->configureMidi(settings),"learn during held CC play");f.process(0);
  f.send(176,20,0);check(f.latest.tracks[0].state==S::stopped&&f.latest.tracks[0].length>0,"Owned CC release stops momentary playback without deleting PCM");
 }
 // Original global mode notes are pulses, unlike loop press/release gestures.
 for(const unsigned block:{64u,128u}){
  Fixture f(block);for(const unsigned note:{46u,47u,48u}){
   f.send(144,note,1);check(note==48?f.latest.midi.sustain:f.latest.midi.mode==static_cast<int>(note-45),"Mode note first press toggles on");
   f.send(144,note,127);check(f.latest.midi.mode==0&&!f.latest.midi.sustain,"Repeated global NoteOn toggles off without release");
   f.send(128,note,0);f.send(144,note,0);check(f.latest.midi.mode==0&&!f.latest.midi.sustain,"Global NoteOff and velocity-zero never toggle");
  }
  f.send(144,36,100);f.send(144,36,127);check(f.latest.tracks[0].state==S::recording&&f.latest.midi.actions==1,"Global pulse repair retains loop-note debounce");
 }
 // A formerly held loop key may be reassigned to a global mode before release.
 for(const unsigned block:{64u,128u})for(const unsigned target:{30u,31u,32u}){
  Fixture f(block);f.send(144,36,100);f.advance(128);const auto length=f.latest.tracks[0].length;
  MidiSettings settings;settings.enabled=true;settings.bindings[0]=0;settings.bindings[target]=midiBinding(1,36);settings.revision=++f.revision;check(f.engine->configureMidi(settings),"Reassign held loop note to global mode");f.process(0);
  f.send(144,36,127);check(target==32?f.latest.midi.sustain:f.latest.midi.mode==static_cast<int>(target-29),"Original global NoteOn bypasses a cancelled former loop press");
  f.send(144,36,1);check(f.latest.midi.mode==0&&!f.latest.midi.sustain,"Reassigned global note toggles again without release");
  f.send(128,36,0);check(f.latest.midi.mode==0&&!f.latest.midi.sustain&&f.latest.tracks[0].state==S::recording&&f.latest.tracks[0].length>length,"Old loop release neither toggles global mode nor finishes the take");
 }
 // Native CC/PC mode bindings retain their existing edge and burst contracts.
 for(const unsigned block:{64u,128u}){
  Fixture f(block);MidiSettings settings;settings.enabled=true;settings.bindings[30]=midiBinding(2,20);settings.bindings[31]=midiBinding(3,0);settings.revision=++f.revision;check(f.engine->configureMidi(settings),"CC/PC mode mapping");
  f.send(176,20,127);f.send(176,20,64);check(f.latest.midi.mode==1,"Mode CC positive repeats remain debounced");f.send(176,20,0);f.send(176,20,127);check(f.latest.midi.mode==0,"Mode CC zero rearms next toggle");
  f.send(192,0,0,2);f.send(192,0,0,2);check(f.latest.midi.mode==2,"Mode PC burst toggles only once");f.advance(160);f.send(192,0,0,2);check(f.latest.midi.mode==0,"Mode PC later pulse toggles again");
 }
 // Match original lost-NoteOff recovery: retry only strictly after ten seconds.
 for(const unsigned block:{64u,128u}){
  Fixture f(block);f.send(144,36,100);f.advance(79999);f.send(144,36,100);
  check(f.latest.tracks[0].state==S::recording&&f.latest.midi.actions==1,"Exact ten-second repeat remains debounced");
  f.send(144,36,100);f.wait();check(f.latest.tracks[0].state==S::playing&&f.latest.tracks[0].length==80001,"Fresh NoteOn after ten seconds recovers a lost NoteOff");
  f.send(128,36,0);f.send(144,36,100);f.send(128,36,0);check(f.latest.tracks[0].state==S::stopped,"Recovered note rearms its ordinary next tap");
  f.command(A::clear);f.send(144,37,100);f.advance(90000);check(f.latest.tracks[1].state==S::recording,"Ten-second recovery never auto-ends an intentional long recording");f.send(128,37,0);f.tap(37);f.wait();
 }
 for(const unsigned block:{64u,128u}){
  Fixture f(block);MidiSettings settings;settings.enabled=true;settings.bindings[0]=midiBinding(2,20);check(f.engine->configureMidi(settings),"CC long-edge configuration");f.send(176,20,127);f.advance(90000);f.send(176,20,64);
  check(f.latest.tracks[0].state==S::recording,"Loop-note recovery must never retrigger a held CC after ten seconds");f.send(176,20,0);f.send(176,20,127);f.wait();check(f.latest.tracks[0].state==S::playing,"CC zero still rearms after a long hold");
 }
 // Audio cancellation invalidates captured learns independently of UI telemetry.
 for(const unsigned block:{64u,128u})for(const unsigned cancellation:{0u,1u,2u,3u,4u,5u}){
  Fixture f(block);MidiSettings settings;settings.enabled=true;settings.learnTarget=19;settings.learnToken=71;settings.learnGeneration=f.engine->generation();settings.revision=++f.revision;
  check(f.engine->configureMidi(settings),"panic learn config");f.send(192,3,0,2);MidiLearnResult learned;
  check(f.engine->popMidiLearn(learned)&&f.engine->midiLearnCurrent(learned),"Captured learn current before cancellation");
  if(cancellation==0)f.send(252,0,0,1);
  else if(cancellation==1||cancellation==2)f.send(176,cancellation==1?120:123,0);
  else if(cancellation==3){f.engine->midiOverflow();f.process(0);}
  else if(cancellation==4){f.engine->cancelMidi();f.process(0);}
  else {f.engine->cancelMidiLearn();f.process(0);}
  check(!f.engine->midiLearnCurrent(learned),"Panic/overflow/device/learn cancellation invalidates already-popped result");
  settings.learnTarget=18;++settings.learnToken;settings.revision=++f.revision;check(f.engine->configureMidi(settings),"fresh learn after audio cancellation");f.send(192,5,0,2);
  check(f.engine->popMidiLearn(learned)&&f.engine->midiLearnCurrent(learned)&&learned.target==18&&learned.binding==midiBinding(3,5),"Fresh audio-owned learn valid after cancellation");
 }
 // A continuous controller may send the learn packet and movement in one block.
 for(const unsigned block:{64u,128u}){
  Fixture f(block);MidiSettings settings;settings.enabled=true;settings.learnTarget=33;settings.learnToken=17;settings.learnGeneration=f.engine->generation();settings.revision=++f.revision;check(f.engine->configureMidi(settings),"learn burst fixture");f.process(0);
  const std::array<MidiEvent,2> burst{{{17,176,13,0,3},{18,176,13,127,3}}};f.process(block,burst);
  check(f.latest.tracks[0].volume==2&&f.latest.tracks[0].state==S::empty,"CC movement immediately after learn must not wait for the editor/message thread");
  settings.modeOverride=1;settings.revision=++f.revision;check(f.engine->configureMidi(settings),"mode change during pending learn");f.send(191,13,64);
  check(f.latest.tracks[0].volume==static_cast<float>(128/127.0)&&f.latest.midi.mode==1,"mode-only configuration preserves pending learned movement");
  MidiLearnResult learned;check(f.engine->popMidiLearn(learned)&&learned.target==33&&learned.binding==midiBinding(2,13)&&!f.engine->popMidiLearn(learned),"movement does not enqueue duplicate learn results");
  f.send(252,0,0,1);f.send(176,13,0);check(f.latest.tracks[0].volume==static_cast<float>(128/127.0),"panic cancels provisional movement before persisted acknowledgement");
 }
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
 for(const unsigned block:{64u,128u}){
  Fixture f(block);MidiSettings settings;settings.enabled=true;settings.bindings[33]=midiBinding(2,11);settings.bindings[53]=midiBinding(2,12);settings.revision=++f.revision;
  check(f.engine->configureMidi(settings),"continuous MIDI config");f.process(0);
  for(unsigned v=0;v<128;++v){f.send(176,11,v);f.send(191,12,v);check(std::abs(f.latest.tracks[0].volume-static_cast<float>(2*v/127.0))<1e-7&&f.latest.tracks[0].pan==std::floor(10*v/127.0),"continuous CC all values and channels match original setters");}
  f.send(176,11,0);check(f.latest.tracks[0].volume==0&&f.latest.tracks[0].state==S::empty,"continuous zero is volume, never transport release/record");
  f.send(176,11,90);f.send(176,11,80);check(f.latest.tracks[0].volume==static_cast<float>(160/127.0),"expression positive repeats are continuous");
  settings.bindings[1]=midiBinding(2,11);check(!MidiController::valid(settings),"continuous CC conflicts with button mapping");settings.bindings[1]=midiBinding(1,37);
  settings.bindings[34]=midiBinding(1,60);check(!MidiController::valid(settings),"slider rejects non-CC bindings");settings.bindings[34]=0;
  settings.learnTarget=34;settings.learnGeneration=f.engine->generation();settings.learnToken=9;settings.revision=++f.revision;check(f.engine->configureMidi(settings),"continuous learn config");f.process(0);
  f.send(176,13,0);MidiLearnResult learned;check(f.engine->popMidiLearn(learned)&&learned.target==34&&learned.binding==midiBinding(2,13),"CC zero learns and is consumed");
  check(f.latest.tracks[1].volume==1&&f.latest.tracks[1].state==S::empty,"learning does not change control or record");
  f.send(144,36,100);check(f.latest.tracks[0].state==S::recording,"Notes retain performance during continuous CC learn");f.send(128,36,0);
  settings.learnTarget=-1;settings.enabled=false;settings.revision=++f.revision;check(f.engine->configureMidi(settings),"disable continuous");f.process(0);f.send(176,11,127);check(f.latest.tracks[0].volume==static_cast<float>(160/127.0),"disabled CC preserves value");
 }
 for(const unsigned block:{64u,128u}){
  Fixture f(block);MidiSettings settings;settings.enabled=true;settings.bindings[34]=midiBinding(2,13,16);settings.learnTarget=33;settings.learnToken=31;settings.learnGeneration=f.engine->generation();settings.revision=++f.revision;check(f.engine->configureMidi(settings),"conflicting pending learn");f.process(0);
  const std::array<MidiEvent,2> burst{{{0,191,13,0,3},{1,191,13,127,3}}};f.process(block,burst);
  check(f.latest.tracks[0].volume==1&&f.latest.tracks[1].volume==1&&f.latest.midi.actions==0,"overlapping omni learn burst cannot alter existing controls");
  settings.learnTarget=-1;++settings.learnToken;settings.revision=++f.revision;check(f.engine->configureMidi(settings),"cancel rejected learn");f.send(191,13,127);check(f.latest.tracks[1].volume==2,"prior channel-specific control survives rejected learn");
 }
 std::vector<float> expressionReference;
 for(const unsigned block:{64u,128u})for(const unsigned controlMode:{0u,1u,2u}){
  const bool midiControls=controlMode!=0;
  Fixture f(block);f.command(A::record);
  for(unsigned n=0;n<1024;n+=block){float l[128],r[128];for(unsigned i=0;i<block;++i){l[i]=0.4f*std::sin((n+i)*0.11f);r[i]=0.2f*std::cos((n+i)*0.093f);}float* input[]={l,r};watch=true;f.engine->process(input,2,block);watch=false;}
  f.command(A::finish);f.wait();
  MidiSettings settings;settings.enabled=true;settings.bindings[33]=midiBinding(2,11);settings.bindings[53]=midiBinding(2,12);if(controlMode==2){settings.bindings[33]=0;settings.learnTarget=33;settings.learnToken=37;settings.learnGeneration=f.engine->generation();}
  settings.revision=++f.revision;check(f.engine->configureMidi(settings),"CC PCM comparison config");f.process(0);
  const auto origin=f.engine->clock();
  const std::array<MidiEvent,8> sequence{{{17,176,11,0,3},{257,176,11,127,3},{402,176,12,0,3},{630,176,12,64,3},{755,176,12,127,3},{1023,176,11,70,3},{1157,176,11,60,3},{1281,176,12,33,3}}};
  if(!midiControls)for(const auto& e:sequence)check(f.engine->enqueueLoop({e.data1==11?A::volume:A::pan,0,++f.request,f.engine->generation(),origin+e.offset,e.data1==11?2*e.data2/127.0:std::floor(10*e.data2/127.0)}),"scheduled direct expression parameter");
  std::vector<float> pcm;for(unsigned n=0;n<2048;n+=block){std::array<MidiEvent,9> events{};unsigned count=0;if(controlMode==2&&n==0)events[count++]={0,176,11,0,3};if(midiControls)for(auto e:sequence)if(e.offset>=n&&e.offset<n+block){e.offset-=n;events[count++]=e;}
   float l[128]{},r[128]{};float* audio[]={l,r};watch=true;f.engine->process(audio,2,block,{events.data(),count});watch=false;for(unsigned i=0;i<block;++i){pcm.push_back(l[i]);pcm.push_back(r[i]);}}
  if(expressionReference.empty()){expressionReference=pcm;check(std::any_of(pcm.begin(),pcm.end(),[](float v){return std::abs(v)>0.01f;}),"expression PCM oracle must contain audible signal");}
  else check(expressionReference==pcm,"MIDI expression renders exact stereo PCM versus scheduled UI parameters at 64/128");
 }
 std::vector<std::uint8_t> reference;
 for(const unsigned block:{64u,128u})for(const unsigned controlMode:{0u,1u,2u,3u}){
  const bool fromMidi=controlMode!=0,heldLearn=controlMode>=2,heldCC=controlMode==3;
  auto engine=std::make_unique<Engine>();engine->prepare(8000,8192);engine->setMonitoring(false);engine->setTarget(1);
  MidiSettings settings;settings.enabled=fromMidi;settings.revision=1;if(heldCC)settings.bindings[0]=midiBinding(2,20);check(engine->configureMidi(settings),"MIDI/audio comparison config");
  if(!fromMidi){check(engine->enqueueLoop({A::record,0,1,engine->generation(),17,0})&&engine->enqueueLoop({A::finish,0,2,engine->generation(),900,0}),"sample-scheduled direct record/finish");}
  float l[128],r[128];float* input[]={l,r};
  for(unsigned offset=0;offset<1024;offset+=block){
   if(heldLearn&&(offset==256||(!heldCC&&offset==512))){
    settings.learnTarget=offset==256?34:-1;++settings.learnToken;settings.learnGeneration=engine->generation();++settings.revision;
    if(offset==512)settings.bindings[34]=midiBinding(2,13);
    check(engine->configureMidi(settings),"Held SUS PCM learn/acknowledgement config");
   }
   std::array<MidiEvent,6> events{};std::size_t count=0;
   const std::array<MidiEvent,6> heldSequence{{{0,144,48,100,3},{1,128,48,0,3},{17,144,36,100,3},{300,176,13,0,3},{301,176,13,127,3},{900,128,36,0,3}}};
   const std::array<MidiEvent,4> tapSequence{{{17,144,36,100,3},{18,128,36,0,3},{900,144,36,100,3},{901,128,36,0,3}}};
   const std::array<MidiEvent,4> ccSequence{{{0,144,48,100,3},{1,128,48,0,3},{17,176,20,127,3},{900,176,20,0,3}}};
   const auto sequence=heldCC?std::span<const MidiEvent>(ccSequence):heldLearn?std::span<const MidiEvent>(heldSequence):std::span<const MidiEvent>(tapSequence);
   for(const auto& e:sequence)if(fromMidi&&e.offset>=offset&&e.offset<offset+block){auto local=e;local.offset-=offset;events[count++]=local;}
   for(unsigned i=0;i<block;++i){l[i]=0.4f*std::sin((offset+i)*0.11f);r[i]=0.2f*std::cos((offset+i)*0.093f);}
   watch=true;engine->process(input,2,static_cast<int>(block),{events.data(),count});watch=false;
  }
  EngineView latest{},view{};for(unsigned n=0;n<10000;++n){watch=true;engine->process(nullptr,0,0);watch=false;while(engine->popView(view))latest=view;if(!engine->finalizationPending()&&latest.tracks[0].state==S::playing)break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  check(latest.tracks[0].length==883,"MIDI/direct sample-identical capture span");
  check(engine->enqueueExport({0,engine->generation(),latest.tracks[0].waveformId}),"MIDI comparison stored PCM");watch=true;engine->process(nullptr,0,0);watch=false;WavExport output;bool ready=false;
  for(unsigned n=0;n<10000&&!ready;++n){ready=engine->popExport(output);if(!ready)std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(ready&&!output.bytes.empty(),"MIDI stored PCM completed");
  if(reference.empty())reference=output.bytes;else check(reference==output.bytes,"MIDI tap/held SUS with slider learn and direct gestures capture exact stereo PCM at 64/128");
 }
 // Lost-release recovery must capture the same nonzero stereo take as direct actions.
 std::vector<std::uint8_t> recoveredPcm;
 for(const unsigned block:{64u,128u})for(const bool fromMidi:{false,true}){
  auto engine=std::make_unique<Engine>();engine->prepare(8000,131072);engine->setMonitoring(false);engine->setTarget(1);
  MidiSettings settings;settings.enabled=fromMidi;check(engine->configureMidi(settings),"lost-release PCM configuration");
  if(!fromMidi)check(engine->enqueueLoop({A::record,0,1,engine->generation(),17,0})&&engine->enqueueLoop({A::finish,0,2,engine->generation(),80018,0}),"direct ten-second capture oracle");
  for(unsigned offset=0;offset<80128;offset+=block){
   std::array<MidiEvent,4> events{};unsigned count=0;for(auto e:std::array<MidiEvent,4>{{{17,144,36,100,3},{80017,144,36,100,3},{80018,144,36,100,3},{80019,128,36,0,3}}})if(fromMidi&&e.offset>=offset&&e.offset<offset+block){e.offset-=offset;events[count++]=e;}
   float l[128],r[128];for(unsigned i=0;i<block;++i){l[i]=0.3f*std::sin((offset+i)*0.021f);r[i]=0.2f*std::cos((offset+i)*0.033f);}float* audio[]={l,r};watch=true;engine->process(audio,2,block,{events.data(),count});watch=false;
   EngineView discard;while(engine->popView(discard)){}
  }
  EngineView latest{},view{};for(unsigned n=0;n<10000;++n){watch=true;engine->process(nullptr,0,0);watch=false;while(engine->popView(view))latest=view;if(!engine->finalizationPending()&&latest.tracks[0].state==S::playing)break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  check(latest.tracks[0].length==80001&&latest.tracks[0].waveformId,"Recovered note commits exact ten-second-plus-one-sample span");check(engine->enqueueExport({0,engine->generation(),latest.tracks[0].waveformId}),"export recovered stereo take");watch=true;engine->process(nullptr,0,0);watch=false;
  WavExport output;bool ready=false;for(unsigned n=0;n<10000&&!ready;++n){ready=engine->popExport(output);if(!ready)std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(ready&&output.bytes.size()>44,"Recovered stereo WAV ready");check(std::any_of(output.bytes.begin()+44,output.bytes.end(),[](auto sample){return sample!=0;}),"Recovered stereo WAV oracle must contain signal");
  if(recoveredPcm.empty())recoveredPcm=output.bytes;else check(recoveredPcm==output.bytes,"Recovered NoteOn captures byte-identical stereo WAV versus direct commands at 64/128");
 }
 // Reassigning a held main key to each global mode must preserve recorded PCM.
 std::vector<std::uint8_t> reassignedPcm;
 for(const unsigned block:{64u,128u})for(const unsigned target:{30u,31u,32u})for(const bool fromMidi:{false,true}){
  auto engine=std::make_unique<Engine>();engine->prepare(8000,8192);engine->setMonitoring(false);engine->setTarget(1);
  MidiSettings settings;settings.enabled=fromMidi;check(engine->configureMidi(settings),"Reassigned stereo configuration");
  if(!fromMidi)check(engine->enqueueLoop({A::record,0,1,engine->generation(),17,0}),"Direct reassignment capture oracle");
  check(engine->enqueueLoop({A::finish,0,2,engine->generation(),900,0}),"Finish reassigned capture");
  for(unsigned offset=0;offset<1024;offset+=block){
   if(offset==128){settings.bindings[0]=0;settings.bindings[target]=midiBinding(1,36);++settings.revision;check(engine->configureMidi(settings),"Reassign main key during nonzero stereo capture");}
   std::array<MidiEvent,4> events{};unsigned count=0;for(auto e:std::array<MidiEvent,4>{{{17,144,36,100,3},{145,144,36,127,3},{146,144,36,1,3},{147,128,36,0,3}}})if(fromMidi&&e.offset>=offset&&e.offset<offset+block){e.offset-=offset;events[count++]=e;}
   float l[128],r[128];for(unsigned i=0;i<block;++i){l[i]=0.3f*std::sin((offset+i)*0.021f);r[i]=0.2f*std::cos((offset+i)*0.033f);}float* audio[]={l,r};watch=true;engine->process(audio,2,block,{events.data(),count});watch=false;EngineView discard;while(engine->popView(discard)){}
  }
  EngineView latest{},view{};for(unsigned n=0;n<10000;++n){watch=true;engine->process(nullptr,0,0);watch=false;while(engine->popView(view))latest=view;if(!engine->finalizationPending()&&latest.tracks[0].state==S::playing)break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  check(latest.tracks[0].length==883&&latest.midi.mode==0&&!latest.midi.sustain,"Reassigned mode repeats/release preserve exact capture span and final modes");check(engine->enqueueExport({0,engine->generation(),latest.tracks[0].waveformId}),"Export reassigned stereo take");watch=true;engine->process(nullptr,0,0);watch=false;
  WavExport output;bool ready=false;for(unsigned n=0;n<10000&&!ready;++n){ready=engine->popExport(output);if(!ready)std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(ready&&output.bytes.size()>44,"Reassigned stereo WAV ready");check(std::any_of(output.bytes.begin()+44,output.bytes.end(),[](auto sample){return sample!=0;}),"Reassigned stereo oracle contains signal");
  if(reassignedPcm.empty())reassignedPcm=output.bytes;else check(reassignedPcm==output.bytes,"Reassigned OD/SUB/SUS keys preserve byte-identical stereo WAV versus direct capture at 64/128");
 }
 check(allocations.load()==0,"MIDI callback zero new/delete");std::cout<<"PASS: MIDI sample timing, Note/CC/PC mappings, debounce, four-second origin/release, sustain, panic and no callback new/delete\n";
}
