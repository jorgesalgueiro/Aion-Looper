#include "core/Engine.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <vector>
namespace {
thread_local bool watching=false;
unsigned allocations=0;
void check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
struct Render {std::vector<float> pcm;std::array<aion::LoopView,20> tracks;};
Render render(unsigned block,unsigned channels){
 using namespace aion;
 auto engine=std::make_unique<Engine>();engine->prepare(8000,131072);engine->setTarget(1);engine->setMonitoring(false);
 unsigned request=0;
 auto command=[&](LoopAction action,unsigned track,unsigned frame,double value=0){check(engine->enqueueLoop({action,track,++request,engine->generation(),frame,value}),"Queue offline command");};
 command(LoopAction::record,0,17);command(LoopAction::finish,0,8209);
 // Another capture at precisely the finish sample must see the committed take.
 command(LoopAction::record,1,8209);command(LoopAction::finish,1,9001);
 command(LoopAction::overdub,0,10000);command(LoopAction::finish,0,11027);
 command(LoopAction::normalize,0,12000);command(LoopAction::reverse,0,13000);
 command(LoopAction::undo,0,14000);command(LoopAction::redo,0,15000);
 command(LoopAction::multiply,0,16000);command(LoopAction::undo,0,17000);
 command(LoopAction::merge,1,18000,0);command(LoopAction::substitute,0,19000);
 command(LoopAction::finish,0,20000);command(LoopAction::stopAll,0,21000);
 Render result;result.pcm.reserve(22000*2);
 for(unsigned frame=0;frame<22000;){
  const auto n=std::min(block,22000-frame);float left[128]{},right[128]{};
  for(unsigned i=0;i<n;++i){left[i]=static_cast<float>(0.3*std::sin((frame+i)*0.071));right[i]=static_cast<float>(0.2*std::cos((frame+i)*0.053));}
  float* data[]={left,right};watching=true;engine->process(data,static_cast<int>(channels),static_cast<int>(n),{},true);watching=false;
  check(!engine->finalizationPending(),"Offline callback cannot leave a pending worker commit");
  EngineView view{};while(engine->popView(view)){check(!view.rejected,"Offline ordered edits must all be accepted");result.tracks=view.tracks;}
  for(unsigned i=0;i<n;++i){check(std::isfinite(left[i])&&std::isfinite(right[i]),"Finite offline PCM");result.pcm.push_back(left[i]);result.pcm.push_back(channels==1?left[i]:right[i]);}
  frame+=n;
 }
 check(result.tracks[0].length==8192&&result.tracks[1].length==792,"Same-sample capture handoff preserves both exact lengths");
 check(result.tracks[0].state==LoopState::stopped&&result.tracks[1].state==LoopState::stopped,"Offline panic stops committed clips");
 check(result.tracks[0].undoCount>0&&result.tracks[1].undoCount==1,"Offline OD/SUB/merge retain immutable history");
 float peak=0;for(unsigned i=16600;i<17400;++i)peak=std::max(peak,std::abs(result.pcm[i]));check(peak>0.01f,"Committed recording produces real nonzero playback before next edit");
 return result;
}
}
void* operator new(std::size_t n){if(watching)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {if(watching)++allocations;std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
int main(){
 using namespace aion;
 for(unsigned channels:{1u,2u}){
  const auto reference=render(1,channels);
  for(unsigned block:{64u,128u}){const auto actual=render(block,channels);check(actual.pcm==reference.pcm,"Offline complete PCM must be exactly block-partition invariant");}
 }
 for(unsigned block:{64u,128u}){
  auto engine=std::make_unique<Engine>();engine->prepare(8000,65536);engine->setMonitoring(false);engine->setTarget(1);
  MidiSettings settings;settings.enabled=true;check(engine->configureMidi(settings),"Enable offline MIDI");
  struct Packet {unsigned frame;std::uint8_t status,key;};
  const std::array<Packet,8> source{{{17,144,36},{18,128,36},{4097,144,36},{4097,144,37},{4098,128,36},{4098,128,37},{8193,144,37},{8194,128,37}}};
  EngineView latest{};
  for(unsigned frame=0;frame<8400;){const auto count=std::min(block,8400-frame);float left[128]{},right[128]{};for(unsigned i=0;i<count;++i){left[i]=static_cast<float>(std::sin((frame+i)*.071)*.3);right[i]=static_cast<float>(std::cos((frame+i)*.053)*.2);}float* data[]={left,right};
   std::array<MidiEvent,8> events{};unsigned n=0;for(const auto& packet:source)if(packet.frame>=frame&&packet.frame<frame+count)events[n++]={packet.frame-frame,packet.status,packet.key,100,3};
   watching=true;engine->process(data,2,static_cast<int>(count),std::span(events.data(),n),true);watching=false;
   EngineView next{};while(engine->popView(next))latest=next;frame+=count;
  }
  check(latest.tracks[0].length==4080&&latest.tracks[1].length==4096,"Offline MIDI FIFO must preserve same-sample finish/next-track recording handoff");
  check(latest.tracks[0].state==LoopState::playing&&latest.tracks[1].state==LoopState::playing&&latest.outputPeak>0.01f,"Both offline MIDI captures commit and play actual PCM");
 }
 {
  auto engine=std::make_unique<Engine>();engine->prepare(8000,8192);MidiSettings settings;settings.enabled=true;check(engine->configureMidi(settings),"Enable zero-sample MIDI fixture");
  const MidiEvent press{0,144,36,100,3},release{0,128,36,0,3};
  engine->process(nullptr,0,0,{&press,1},true);float left[513]{},right[513]{};for(unsigned i=0;i<513;++i){left[i]=static_cast<float>(std::sin(i*.071)*.3);right[i]=.2f;}float* data[]={left,right};engine->process(data,2,513,{},true);
  watching=true;engine->process(nullptr,0,0,{&release,1},true);engine->process(nullptr,0,0,{&press,1},true);watching=false;
  const auto state=engine->inspectAudioThread();check(state.frame==513&&state.track.length==513&&state.track.state==LoopState::playing&&state.track.waveformId&&!engine->finalizationPending(),"Zero-sample MIDI finish commits without advancing time or rendering PCM");
 }
 for(unsigned length:{0u,1u,127u,128u,8191u}){
  auto small=std::make_unique<LoopBank>();small->prepare(8192,0,8000);check(small->apply(LoopAction::record,0),"Start edge-length offline take");for(unsigned i=0;i<length;++i)small->tick({.1f,.2f},true);
  check(small->apply(LoopAction::finish,0),"Finish edge-length offline take");watching=true;small->completeOfflineEdits();watching=false;
  const auto view=small->trackView(0);check(!small->finalizationPending()&&view.length==length&&view.state==(length?LoopState::playing:LoopState::empty),"Empty/short/near-capacity offline takes preserve exact length and state");
 }
 for(double rate:{8000.0,48000.0,96000.0}){
  // Scheduled finishes and pool exhaustion happen inside LoopBank::tick, not
  // in an explicit command. Both must finish before rendering that sample.
  auto bank=std::make_unique<LoopBank>();bank->prepare(131072,0,rate);bank->configureSync({SyncMode::fixed,240,1,1,4,false,false});
  check(bank->apply(LoopAction::record,0),"Arm static recording");
  for(unsigned i=0;i<static_cast<unsigned>(rate);++i)bank->tick({static_cast<float>(std::sin(i*.071)*.3),static_cast<float>(std::cos(i*.053)*.2)},true);
  check(!bank->finalizationPending()&&bank->trackView(0).state==LoopState::playing&&bank->trackView(0).length==static_cast<unsigned>(rate/4),"Static auto-finish commits at offline sample boundary");
 }
 auto bank=std::make_unique<LoopBank>();bank->prepare(4096,0,8000);check(bank->apply(LoopAction::record,0),"Record with limited pool");
 for(unsigned i=0;i<4097;++i)bank->tick({static_cast<float>(std::sin(i*.071)*.3),0.2f},true);
 check(!bank->finalizationPending()&&bank->trackView(0).state==LoopState::playing&&bank->trackView(0).length==4096,"Pool-triggered finish commits offline without time gap");
 // A live-origin job cancelled before the mode switch cannot resurrect a clip.
 bank->prepare(8192,0,8000);check(bank->apply(LoopAction::record,0),"Record cancelled take");for(unsigned i=0;i<513;++i)bank->tick({.1f,.2f});
 check(bank->apply(LoopAction::finish,0)&&bank->apply(LoopAction::clear,0),"Cancel live-origin worker job");watching=true;bank->completeOfflineEdits();watching=false;
 check(!bank->finalizationPending()&&bank->trackView(0).state==LoopState::empty&&!bank->trackView(0).waveformId,"Offline transition respects cancellation and stale-result ownership");
 check(allocations==0,"Zero tracked new/delete on offline render owner; worker retains allocation ownership");
 std::cout<<"PASS: offline PCM exact at 1/64/128 mono/stereo, ordered same-sample capture/edit/history/merge, scheduled and pool finishes, cancellation, zero render-owner new/delete\n";
}
