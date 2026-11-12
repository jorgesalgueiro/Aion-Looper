// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>
namespace {
thread_local bool watched=false;thread_local unsigned operations=0;
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct Guard {Guard(){operations=0;watched=true;}~Guard(){watched=false;check(!operations,"preset callback new/delete");}};
}
void* operator new(std::size_t n){if(watched)++operations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{if(watched&&p)++operations;std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace aion;
struct Fixture {
 std::unique_ptr<Engine> engine=std::make_unique<Engine>();unsigned block;std::array<float,128> l{},r{};
 Fixture(unsigned frames,double rate):block(frames){engine->prepare(rate,32768);engine->setTarget(1);engine->setMonitoring(false);engine->setAutoPlay(true);}
 void pump(bool zero=false,bool offline=false){float* audio[]{l.data(),r.data()};Guard guard;engine->process(zero?nullptr:audio,zero?0:2,zero?0:static_cast<int>(block),{},offline);}
 void command(LoopAction action){check(engine->enqueueLoop({action,0,1,engine->generation()}),"fixture command");pump(false,true);}
 void install(const ArpRenderSettings& s){const auto info=engine->inspectAudioThread().track;ArpPreparationRequest r;r.requestId=2;r.generation=engine->generation();r.seed=s.seed;r.values=s.values;r.activate=true;r.clearEpoch=info.clearEpoch;r.settingsRevision=info.arpSettingsRevision;
  check(engine->enqueueArpPreparation(r),"fixture Arp install");ArpPreparationResult result;const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(!engine->popArpPreparation(result)){pump(true);check(std::chrono::steady_clock::now()<end,"preset fixture prepare timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(result.status==ArpPreparationStatus::ready,"fixture ready");
 }
 LoopCommand edit(unsigned id,std::uint64_t at=0){const auto info=engine->inspectAudioThread().track;LoopCommand c{LoopAction::arpDelay,0,id,engine->generation(),at};c.arp=info.arp;c.arpClearEpoch=info.clearEpoch;c.arpSettingsRevision=info.arpSettingsRevision;return c;}
};
static void connected(unsigned block,double rate){
 Fixture actual(block,rate),reference(block,rate);
 for(auto* f:{&actual,&reference})f->command(LoopAction::record);
 for(unsigned n=0;n<4096/block;++n)for(auto* f:{&actual,&reference}){for(unsigned i=0;i<block;++i){f->l[i]=static_cast<float>(.16*std::sin((n*block+i)*.117));f->r[i]=static_cast<float>(.09*std::cos((n*block+i)*.037));}f->pump();}
 for(auto* f:{&actual,&reference})f->command(LoopAction::finish);
 ArpSettings controls;controls.enabled=true;controls.values[0]=.012;controls.values[7]=2;controls.values[8]=.7;controls.values[9]=1.3;ArpRenderSettings initial;check(compileArpSettings(controls,137,123456789,initial),"initial compiled controls");
 actual.install(initial);reference.install(initial);
 auto render=[&]{for(auto* f:{&actual,&reference}){f->l.fill(0);f->r.fill(0);f->pump();}double energy=0;
  for(unsigned i=0;i<block;++i){check(actual.l[i]==reference.l[i]&&actual.r[i]==reference.r[i],"atomic preset PCM must equal ordered original control gestures exactly");energy+=std::abs(actual.l[i])+std::abs(actual.r[i]);}
  const auto a=actual.engine->inspectPreparedArpAudioThread(0),b=reference.engine->inspectPreparedArpAudioThread(0);check(a.state==b.state&&a.frame==b.frame&&a.targets==b.targets,"persistent preset delay/STAY/LFO state and clock");return energy;
 };
 double energy=0;for(unsigned n=0;n<4096/block;++n)energy+=render();
 constexpr std::array<unsigned,11> indices{0,1,2,3,4,5,7,8,9,10,11};
 for(unsigned preset=1;preset<=8;++preset){
  ArpSettings next=controls;check(applyArpPreset(next,preset),"preset selection");const double bpm=preset%2?173.123456789:137.123456789;ArpRenderSettings target;check(compileArpSettings(next,bpm,initial.seed,target),"preset target compile");
  const auto at=actual.engine->clock()+13;auto command=actual.edit(100+preset,at);const auto revision=command.arpSettingsRevision;
  // 127 older due commands consume all but one callback dispatch slot. The
  // complete preset must still happen at sample 13, never continue next block.
  for(unsigned i=0;i<127;++i)check(actual.engine->enqueueLoop({LoopAction::volume,19,10,actual.engine->generation(),0,1}),"dispatch-budget fixture");
  check(actual.engine->enqueueArpPreset(command,next,bpm),"one-slot typed preset admission");
  auto expected=reference.edit(200,at);auto intermediate=expected.arp;
  for(unsigned p=0;p<indices.size();++p){const auto index=indices[p];intermediate.values[index]=target.values[index];intermediate.values[6]=target.values[6];expected.arp=intermediate;expected.arpMask=(1u<<index)|(1u<<6);expected.arpSettingsRevision=reference.engine->inspectAudioThread().track.arpSettingsRevision+p;expected.requestId=200+p;check(reference.engine->enqueueLoop(expected),"ordered reference gesture admission");}
  energy+=render();check(actual.engine->inspectAudioThread().track.arp==target&&actual.engine->inspectAudioThread().track.arpSettingsRevision==revision+1,"one logical preset revision and complete target at deadline");
  for(unsigned n=0;n<32768/block;++n)energy+=render();controls=next;
 }
 check(energy>1,"nonzero stereo preset comparison");
 // Invalid/stale commands must leave every automation lane untouched, not just
 // reject final metadata after part of the preset has reached the graph.
 for(unsigned failure=0;failure<4;++failure){ArpSettings next=controls;check(applyArpPreset(next,1),"stale preset");auto command=actual.edit(500+failure);if(failure==0)++command.arpClearEpoch;else if(failure==1)++command.arpSettingsRevision;else if(failure==2)++command.arp.seed;else next.enabled=false;
  check(actual.engine->enqueueArpPreset(command,next,299),"audio validation owns stale preset rejection");energy+=render();check(actual.engine->inspectAudioThread().track.arp==reference.engine->inspectAudioThread().track.arp,"stale preset metadata unchanged");for(unsigned n=0;n<1024/block;++n)energy+=render();}
 std::cout<<"PASS original ordered preset Engine PCM/state / "<<rate<<" / "<<block<<"; signal "<<energy<<'\n';
}
static void admission(){
 Fixture f(64,8000);ArpSettings settings;check(applyArpPreset(settings,1),"admission preset");auto command=f.edit(1);
 auto bad=settings;bad.preset=0;check(!f.engine->enqueueArpPreset(command,bad,120),"Custom is not a named preset");bad=settings;bad.values[2]=NAN;check(!f.engine->enqueueArpPreset(command,bad,120),"bad preset values rejected");
 check(!f.engine->enqueueArpPreset(command,settings,NAN),"bad preset BPM rejected");auto wrong=command;wrong.action=LoopAction::clear;check(!f.engine->enqueueArpPreset(wrong,settings,120),"preset cannot turn another action into Arp");wrong=command;wrong.requestId=0;check(!f.engine->enqueueArpPreset(wrong,settings,120),"preset correlation required");
 wrong=command;wrong.arpPreset=true;wrong.arpMask=1u<<2;check(!f.engine->enqueueLoop(wrong),"partial preset mask cannot reach audio");
 // Exactly one remaining slot is sufficient; no room rejects the complete
 // gesture without advancing source or settings, including disabled presets.
 for(unsigned n=0;n<254;++n)check(f.engine->enqueueLoop({LoopAction::volume,19,10,f.engine->generation(),UINT64_MAX,1}),"fill 254 queue slots");
 check(f.engine->enqueueArpPreset(command,settings,120),"preset fits one remaining queue slot");check(!f.engine->enqueueArpPreset(command,settings,120),"full queue rejects whole preset");f.pump(true);check(!f.engine->inspectAudioThread().track.arp.enabled&&!f.engine->inspectAudioThread().track.arpSettingsRevision,"rejected/undispatched preset cannot mutate settings");
 f.engine->prepare(8000,32768);f.pump();check(!f.engine->inspectAudioThread().track.arpSettingsRevision,"prepare discards old-generation preset with entire FIFO");
 auto after=f.edit(2);check(f.engine->enqueueArpPreset(after,settings,120),"fresh disabled preset");f.pump();ArpRenderSettings compiled;check(compileArpSettings(settings,120,after.arp.seed,compiled)&&f.engine->inspectAudioThread().track.arp==compiled,"disabled preset stores controls without enabling or allocating a graph");
}
static void repeatedBpmBoundary(){
 // At this timestamp JS seconds -> frames lands just above an integer. The
 // retained automation model defers the new anchor one sample. Repeating BPM
 // in the same preset changes that sample; collapsing to a final target loses it.
 constexpr unsigned frame=14;check((double(frame)/48000)*48000>frame,"deferred boundary fixture");
 ArpModulation ordered,collapsed;const auto initial=arpFactoryTargets();check(ordered.prepare(48000,initial,frame)&&collapsed.prepare(48000,initial,frame),"ordered modulation fixture");
 ArpSettings s;ArpRenderSettings target;check(applyArpPreset(s,5)&&compileArpSettings(s,173,1,target),"boundary preset compile");
 {Guard guard;for(const auto& write:arpPresetWrites(target))check(ordered.setTarget(write.parameter,write.value),"original ordered modulation write");
  for(unsigned p=0;p<12;++p)check(collapsed.setTarget(p,target.values[p]),"collapsed comparison write");
  ArpControlFrame a,b;check(ordered.next(a)&&collapsed.next(b)&&a.parameters[6]!=b.parameters[6],"same-time repeated BPM must not be replaced by one final-value retarget");}
}
int main(){admission();repeatedBpmBoundary();for(unsigned block:{64u,128u})for(double rate:{8000.,48000.})connected(block,rate);}
