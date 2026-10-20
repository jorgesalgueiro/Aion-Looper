#include "core/Engine.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
extern thread_local bool watch;
extern std::atomic<int> allocations;
namespace {
void check(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
void wait(aion::LoopBank& bank){for(unsigned n=0;bank.finalizationPending()&&n<10000;++n){watch=true;bank.beginBlock();watch=false;std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(!bank.finalizationPending(),"waveform worker timeout");}
}
void waveformTests(){
 using namespace aion;using A=LoopAction;
 auto bank=std::make_unique<LoopBank>();bank->prepare(8192,0,8000);
 watch=true;check(bank->apply(A::record,0),"waveform record");for(unsigned i=0;i<4000;++i)bank->tick({0.1f+0.3f*std::sin(i*0.07f),0.7f*std::cos(i*0.13f)});check(bank->apply(A::finish,0),"waveform finish");watch=false;wait(*bank);
 std::array<float,LoopBank::waveformWidth> original{},normalized{},restored{};
 watch=true;const auto first=bank->view()[0].waveformId;check(first&&bank->copyWaveform(0,first,original),"worker publishes waveform identity");
 check(!bank->copyWaveform(20,first,restored)&&!bank->copyWaveform(0,first+1,restored),"stale/invalid waveform copy refused");
 check(bank->apply(A::normalize,0),"waveform normalize");watch=false;wait(*bank);watch=true;
 const auto norm=bank->view()[0].waveformId;check(norm!=first&&bank->copyWaveform(0,norm,normalized)&&normalized!=original,"NORM replaces waveform with PCM");
 check(!bank->apply(A::undo,0),"NORM creates no history, matching JS");
 check(bank->apply(A::overdub,0),"waveform edit");for(unsigned i=0;i<257;++i)bank->tick({0.2f*std::sin(i*0.3f),0.1f});check(bank->apply(A::finish,0),"waveform edit finish");watch=false;wait(*bank);watch=true;
 const auto edited=bank->view()[0].waveformId;check(edited!=norm,"overdub publishes waveform");
 check(bank->apply(A::undo,0)&&bank->view()[0].waveformId==norm&&bank->copyWaveform(0,norm,restored)&&restored==normalized,"Undo restores exact waveform identity and values");
 check(bank->apply(A::redo,0)&&bank->view()[0].waveformId==edited,"Redo restores edited cache");
 bank->apply(A::reverse,0,1);check(bank->view()[0].waveformId==edited,"Reverse retains raw recorded waveform like JS");
 check(bank->apply(A::multiply,0,0),"waveform stopped/no-input double");watch=false;wait(*bank);watch=true;
 const auto doubled=bank->view()[0].waveformId;check(doubled!=norm&&bank->view()[0].length==8000,"Multiply publishes its own waveform");
 check(!bank->apply(A::multiply,0,0)&&bank->view()[0].waveformId==doubled,"failed growth preserves waveform");
 bank->apply(A::clear,0);check(!bank->view()[0].waveformId&&!bank->copyWaveform(0,doubled,restored),"DEL invalidates cache");watch=false;
 // Fill the producer queue without a consumer, then recover all 20 tracks.
 auto engine=std::make_unique<Engine>();engine->prepare(8000,81920);EngineView latest{},view{};unsigned request=0;
 for(unsigned id=0;id<20;++id){
  check(engine->enqueueLoop({A::record,id,++request,engine->generation(),0,0}),"queue waveform record");
  float left[128]{},right[128]{};for(unsigned i=0;i<128;++i){left[i]=0.3f*std::sin(i*0.1f);right[i]=0.6f*std::cos(i*0.09f);}float* channels[]={left,right};
  watch=true;engine->process(channels,2,128);watch=false;
  check(engine->enqueueLoop({A::finish,id,++request,engine->generation(),0,0}),"queue waveform finish");watch=true;engine->process(channels,2,1);watch=false;
  for(unsigned n=0;n<10000;++n){watch=true;engine->process(nullptr,0,0);watch=false;while(engine->popView(view))latest=view;if(latest.tracks[id].state==LoopState::playing)break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  check(latest.tracks[id].state==LoopState::playing,"waveform Engine capture committed");
 }
 std::array<std::uint64_t,20> ids{};WaveformEvent event{};
 while(engine->popWaveform(event))ids[event.track]=event.id;
 for(unsigned n=0;n<30;++n){watch=true;engine->process(nullptr,0,0);watch=false;unsigned count=0;while(engine->popWaveform(event)){check(event.generation==engine->generation()&&event.id==latest.tracks[event.track].waveformId,"coherent event track/generation/clip");ids[event.track]=event.id;++count;}check(count<=2,"at most two fixed waveform copies per block");while(engine->popView(view))latest=view;}
 for(unsigned id=0;id<20;++id)check(ids[id]&&ids[id]==latest.tracks[id].waveformId,"slow editor recovers every current waveform");
 const auto generation=engine->generation();engine->prepare(16000,81920);check(engine->generation()!=generation,"waveform generation advances on prepare");watch=true;engine->process(nullptr,0,0);watch=false;while(engine->popView(view))latest=view;for(const auto& track:latest.tracks)check(!track.waveformId,"sample-rate reset clears waveform identities");
 check(allocations.load()==0,"waveform telemetry has zero callback new/delete");
 std::cout<<"PASS: worker waveform lifecycle, NORM/history/reverse/Multiply/DEL/OOM, slow-editor recovery across 20 tracks, reset and bounded zero-allocation telemetry\n";
}
