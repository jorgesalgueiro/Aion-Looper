// SPDX-License-Identifier: AGPL-3.0-only
#include "core/ReverbSettingsOwner.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>
static thread_local bool watch=false;static unsigned heap=0;
void* operator new(std::size_t n){if(watch)++heap;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete(void* p)noexcept{if(watch&&p)++heap;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
static void check(bool b,const char* why){if(!b){std::cerr<<why<<'\n';std::exit(1);}}
using namespace aion;
static void run(double rate,unsigned block,bool machine=false){
 const unsigned controls=machine?6:4;
 const auto parameter=[&](unsigned p){return machine?p:reverbMidiControls[p];};
 const auto midiValue=[&](unsigned p,unsigned cc){return machine?plateMidiValue(p,cc):reverbMidiValue(p,cc);};
 auto direct=std::make_unique<Engine>(),midi=std::make_unique<Engine>();ReverbSettingsOwner d(*direct),m(*midi);
 auto dv=std::make_unique<EngineView>(),mv=std::make_unique<EngineView>();
 for(auto* e:{direct.get(),midi.get()}){e->prepare(rate,16384);e->setMonitoring(false);e->setAutoPlay(false);check(e->enqueue({0,1})&&e->enqueueLoop({LoopAction::record,0,1,e->generation()}),"paired capture");}
 MidiSettings bindings;bindings.enabled=true;for(unsigned p=0;p<controls;++p)bindings.bindings[MidiSettings::reverbBase+p]=midiBinding(2,20+p);check(midi->configureMidi(bindings),"paired MIDI configuration");
 std::array<float,128> dl{},dr{},ml{},mr{};float* da[]{dl.data(),dr.data()},*ma[]{ml.data(),mr.data()};
 unsigned stage=0;auto render=[&](bool capture,std::span<const MidiEvent> events={},bool compare=false){const auto frame=direct->clock();
  for(unsigned i=0;i<block;++i){dl[i]=ml[i]=capture?static_cast<float>(.12*std::sin((frame+i)*.071)):0;dr[i]=mr[i]=capture?static_cast<float>(.08*std::cos((frame+i)*.039)):0;}
  watch=true;direct->process(da,2,block);midi->process(ma,2,block,events);watch=false;check(!heap,"paired MIDI audio callback heap");
  while(direct->popView(*dv)){}while(midi->popView(*mv)){}d.service(*dv);m.service(*mv);check(direct->clock()==midi->clock(),"paired sample clocks");
  double peak=0,error=0;for(unsigned i=0;i<block;++i){peak=std::max(peak,std::max(std::abs(double(dl[i])),std::abs(double(dr[i]))));error=std::max(error,std::max(std::abs(double(dl[i])-ml[i]),std::abs(double(dr[i])-mr[i])));}
  if(compare&&error>1e-6)std::cerr<<"rate="<<rate<<" block="<<block<<" stage="<<stage<<" frame="<<frame<<" PCM error="<<error<<" dRev="<<direct->inspectAudioThread().track.reverbSettingsRevision<<" mRev="<<midi->inspectAudioThread().track.reverbSettingsRevision<<'\n';
  if(compare)check(error<=1e-6,"sample-offset MIDI equals scheduled shared-owner stereo PCM");return peak;
 };
 render(true);check(d.bind(*dv)&&m.bind(*mv),"paired sole owner bind");for(unsigned n=1;n<4096/block;++n)render(true);
 for(auto* e:{direct.get(),midi.get()})check(e->enqueueLoop({LoopAction::finish,0,2,e->generation()}),"paired finish");
 for(unsigned n=0;;++n){render(false);if(dv->tracks[0].state==LoopState::stopped&&mv->tracks[0].state==LoopState::stopped)break;check(n<5000,"paired finalization deadline");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 auto settle=[&]{for(unsigned n=0;n<5000;++n){render(false);if(!d.track(0)->pending&&!m.track(0)->pending){check(d.track(0)->status==ReverbEditStatus::ready&&m.track(0)->status==ReverbEditStatus::ready,"paired accepted owner edits");return;}std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(false,"paired edit timeout");};
 if(machine){ReverbSettings plate;plate.engine=ReverbEngine::machine;check(d.assign(0,plate,4)&&m.assign(0,plate,4),"paired Machine owner settings");}
 else check(d.set(0,2,.1,4)&&m.set(0,2,.1,4),"short paired reverb impulse");
 settle();check(d.enable(0,true,5)&&m.enable(0,true,5),"paired worker enable");settle();
 for(unsigned n=0;n<static_cast<unsigned>(.1*rate)/block+2;++n)render(false);
 if(machine){
  // Independent workers can adopt on different callbacks. Machine's persistent
  // modulation clocks then have different legitimate ages even before PLAY.
  // Use the real chain remove/reinsert reset to establish a common initial
  // phase; never change DSP or relax PCM tolerance to align test fixtures.
  for(const auto chain:{normalizedSignalChain("QCATFODVKZG"),SignalChain{}}){
   for(auto* e:{direct.get(),midi.get()}){LoopCommand command{LoopAction::signalChain,0,6,e->generation()};command.signalChain=chain;check(e->enqueueLoop(command),"paired production chain reset");}
   render(false);
  }
 }
 for(auto* e:{direct.get(),midi.get()})check(e->enqueueLoop({LoopAction::play,0,3,e->generation()}),"paired playback after both empty-history owners are ready");render(false);
 double signal=0;for(unsigned p=0;p<controls;++p){stage=p;const unsigned cc=p==0?1:64;const auto value=midiValue(p,cc);
  check(d.set(0,parameter(p),value,10+p,direct->clock()+17),"scheduled direct owner edit");const MidiEvent event{17,static_cast<std::uint8_t>(p%2?191:176),static_cast<std::uint8_t>(20+p),static_cast<std::uint8_t>(cc),3};
  signal=std::max(signal,render(false,{&event,1},true));
  for(unsigned n=0;n<static_cast<unsigned>(.12*rate)/block+2;++n)signal=std::max(signal,render(false,{},true));
  check(d.track(0)->committed==m.track(0)->committed&&d.bound()&&m.bound(),"paired exact originals/owner proof");
 }
 // Learn consumes the first CC; the next packet in the same block must already
 // drive the provisional target. Compare it with a scheduled ordinary edit.
 for(unsigned p=0;p<controls;++p){stage=10+p;bindings.bindings[MidiSettings::reverbBase+p]=0;bindings.learnTarget=static_cast<int>(MidiSettings::reverbBase+p);bindings.learnGeneration=midi->generation();++bindings.learnToken;
  check(midi->configureMidi(bindings),"audible provisional MIDI learn admitted");
  const auto value=midiValue(p,127);
  check(d.set(0,parameter(p),value,100+p,direct->clock()+17),"scheduled learned-control oracle");
  const std::array<MidiEvent,2> burst{{{0,176,static_cast<std::uint8_t>(20+p),0,3},{17,176,static_cast<std::uint8_t>(20+p),127,3}}};
  signal=std::max(signal,render(false,burst,true));
  MidiLearnResult learned;check(midi->popMidiLearn(learned)&&learned.target==static_cast<int>(MidiSettings::reverbBase+p)&&midi->midiLearnCurrent(learned),"exact provisional learn receipt");
  bindings.bindings[MidiSettings::reverbBase+p]=midiBinding(2,20+p);bindings.learnTarget=-1;
  check(midi->configureMidi(bindings),"accept learned binding");
  for(unsigned n=0;n<static_cast<unsigned>(.03*rate)/block+2;++n)render(false,{},true);
  check(d.bound()&&m.bound()&&d.track(0)->committed==m.track(0)->committed,"learned control retains exact owner proof");
 }
 // Exercise both edges of a callback and a nonmonotonic CC sequence after learn.
 unsigned serial=200;
 for(unsigned p=0;p<controls;++p)for(unsigned cc:{0u,127u,32u,64u}){
  const unsigned offset=serial%3==0?0:serial%3==1?17:block-1;
  check(d.set(0,parameter(p),midiValue(p,cc),serial++,direct->clock()+offset),"scheduled boundary control");
  const MidiEvent event{offset,191,static_cast<std::uint8_t>(20+p),static_cast<std::uint8_t>(cc),3};
  render(false,{&event,1},true);
  for(unsigned n=0;n<static_cast<unsigned>(.01*rate)/block+2;++n)render(false,{},true);
  check(d.bound()&&m.bound()&&d.track(0)->committed==m.track(0)->committed,"boundary control preserves originals");
 }
 check(signal>.001,"comparison includes nonzero native stereo audio");
 // Keep one accepted MIDI receipt unread while a later clear retires the
 // source. The epoch reset must win; a fresh post-clear edit must still work.
 const MidiEvent later{17,176,21,127,3};watch=true;midi->process(ma,2,block,{&later,1});direct->process(da,2,block);watch=false;
 for(auto* e:{direct.get(),midi.get()})check(e->enqueueLoop({LoopAction::clear,0,30,e->generation()}),"clear after unread MIDI");render(false);
 check(m.bound()&&m.track(0)->committed==ReverbSettings{}&&mv->tracks[0].length==0,"later clear discards old MIDI originals without resurrecting PCM");
 render(false,{&later,1});check(m.track(0)->committed.values[1]==1&&!m.track(0)->committed.enabled,"fresh post-clear MIDI owns reset epoch");
 m.suspendMidi(true);const MidiEvent zero{17,176,21,0,3};render(false,{&zero,1});check(m.track(0)->committed.values[1]==1,"suspended owner rejects mutation");m.suspendMidi(false);render(false,{&zero,1});check(m.track(0)->committed.values[1]==0,"resume restores fresh ownership");
 check(!heap,"no callback allocation/deallocation during MIDI clear/recovery");
 std::cout<<"PASS "<<(machine?"Machine":"Convolution")<<" Reverb MIDI PCM "<<rate<<'/'<<block<<": "<<controls<<" mapped and same-block learned controls, offsets 0/17/last, both channels <=1e-6, exact originals, clear/suspend recovery, zero callback heap\n";
}
int main(){for(bool machine:{false,true})for(double rate:{8000.,48000.})for(unsigned block:{64u,128u})run(rate,block,machine);}
