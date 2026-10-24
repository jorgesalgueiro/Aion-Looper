#include "plugin/Vst3ProgramRouting.h"
#include "core/Engine.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>
namespace {thread_local bool watch=false;unsigned allocations=0;void check(bool v,const char* why){if(!v){std::cerr<<why<<'\n';std::exit(1);}}}
void* operator new(std::size_t n){if(watch)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {if(watch)++allocations;std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
int main(){
 using namespace aion;using namespace aion::vst3;
 for(unsigned id:{0u,programBase-1,programBase+16,0xffffffffu})check(!decode(id,0,0),"Non-routing IDs rejected");
 for(double v:{-0.01,1.01,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()})check(!decode(programBase,17,v),"Malformed host point rejected");
 check(!decode(programBase,-1,0),"Negative host offset rejected");
 PointBudget budget;for(unsigned i=0;i<256;++i)check(budget.take(),"All 256 valid point slots retained");for(unsigned i=0;i<10000;++i)check(!budget.take()&&budget.overflow&&budget.used==256,"Program point overflow remains bounded and latched");
 unsigned cases=0;
 for(unsigned block:{64u,128u})for(unsigned channel=1;channel<=16;++channel){
  auto e=std::make_unique<Engine>();e->prepare(8000,65536);e->setMonitoring(false);e->setTarget(1);EngineView view{};std::uint32_t request=0;
  const auto process=[&](unsigned n,std::span<const MidiEvent> events=std::span<const MidiEvent>{}){float l[128]{},r[128]{};for(unsigned i=0;i<n;++i){l[i]=static_cast<float>(std::sin((e->clock()+i)*0.071)*0.3);r[i]=static_cast<float>(std::cos((e->clock()+i)*0.053)*0.2);}float* data[]={l,r};watch=true;e->process(data,2,static_cast<int>(n),events);watch=false;EngineView next;while(e->popView(next))view=next;};
  const auto command=[&](LoopAction a){check(e->enqueueLoop({a,0,++request,e->generation(),0,0}),"Queue fixture recording command");process(1);};
  command(LoopAction::record);for(unsigned i=0;i<4;++i)process(block);command(LoopAction::finish);
  for(unsigned i=0;e->finalizationPending()&&i<10000;++i){process(0);std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(!e->finalizationPending(),"Worker finalizes fixture");process(0);
  const auto clip=view.tracks[0].waveformId;const auto length=view.tracks[0].length;check(clip&&view.tracks[0].state==LoopState::playing,"Real PCM fixture plays");
  MidiSettings s;s.enabled=true;s.bindings.fill(0);
  for(unsigned program=0;program<128;++program){
   s.bindings[73]=midiBinding(3,program,channel);++s.revision;check(e->configureMidi(s),"Specific channel/program binding");process(0);
   const auto packet=[&](unsigned ch,int offset){watch=true;const auto decoded=decode(programBase+ch-1,offset,program/127.0);watch=false;check(decoded&&decoded->channel==static_cast<int>(ch)&&decoded->number==static_cast<int>(program)&&decoded->offset==offset,"Roundtrip every host program/channel/sample offset");return MidiEvent{static_cast<unsigned>(decoded->offset),static_cast<std::uint8_t>(0xc0+decoded->channel-1),static_cast<std::uint8_t>(decoded->number),0,2};};
   auto wrong=packet(channel%16+1,17);process(block,{&wrong,1});check(!view.tracks[0].muted,"Different MIDI channel cannot mute the clip");
   const std::array<MidiEvent,3> burst{packet(channel,17),packet(channel,18),packet(channel,19)};process(block,burst);check(view.tracks[0].muted,"Host program points reach the real mute action once per burst");
   for(unsigned n=0;n<4;++n)process(block);check(view.outputPeak==0,"Routed program renders actual silence");
   auto again=packet(channel,31);process(block,{&again,1});for(unsigned n=0;n<4;++n)process(block);check(!view.tracks[0].muted&&view.outputPeak>0.01,"Repeated identical PC restores actual loop PCM");
   check(view.tracks[0].waveformId==clip&&view.tracks[0].length==length&&view.tracks[0].undoCount==0&&view.midi.overflows==0,"Program routing preserves clip/history and bounded MIDI state");++cases;
  }
 }
 check(allocations==0,"Routing/processing performs zero tracked callback new/delete");
 std::cout<<"PASS: "<<cases<<" real-PCM host program routes (all 16 channels/128 programs, 64/128), wrong-channel isolation, repeated pulses, offsets, overflow bounds and zero callback new/delete\n";
}
