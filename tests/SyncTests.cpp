#include "core/Engine.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
extern thread_local bool watch;
extern std::atomic<int> allocations;
extern std::atomic<bool> failWorkerAllocation;
namespace {
using A=aion::LoopAction;using S=aion::LoopState;using M=aion::SyncMode;
void check(bool ok,const char* what){if(!ok){std::cerr<<what<<'\n';std::exit(1);}}
void wait(aion::LoopBank& bank){for(int i=0;bank.finalizationPending()&&i<10000;++i){watch=true;bank.beginBlock();watch=false;std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(!bank.finalizationPending(),"sync worker completion");}
void ticks(aion::LoopBank& bank,unsigned count){watch=true;for(unsigned i=0;i<count;++i)bank.tick({0.1f+0.3f*std::sin(i*0.11f),-0.1f+0.2f*std::cos(i*0.073f)});watch=false;}
}
void syncTests(){
 auto storage=std::make_unique<aion::LoopBank>();auto& bank=*storage;
 const auto ready=[&](M mode=M::fixed){bank.prepare(8192,0,8000);bank.configureSync({mode,600,1,1,4,false});};
 ready();watch=true;check(bank.apply(A::record,0),"static arm entry");watch=false;
 check(bank.view()[0].state==S::armed&&bank.view()[0].scheduledFrame==800,"static arm targets next shared grid");
 check(!bank.apply(A::record,1)&&bank.available()==4096,"armed recording owns lane and reserves output budget");
 ticks(bank,799);check(bank.view()[0].state==S::armed&&!bank.view()[0].length,"no capture before start boundary");
 bank.apply(A::finish,0);ticks(bank,2000);check(bank.view()[0].state==S::empty&&bank.available()==8192,"armed cancel never starts later");
 ready();bank.apply(A::record,0);ticks(bank,800);check(bank.view()[0].state==S::armed,"deadline starts with the boundary sample, not before");ticks(bank,1);check(bank.view()[0].state==S::recording&&bank.view()[0].length==1,"first static capture sample exactly at grid");
 ticks(bank,959);check(bank.view()[0].length==960,"static capture includes 20ms fold tail");ticks(bank,1);wait(bank);
 check(bank.view()[0].length==800&&bank.view()[0].state==S::playing&&bank.view()[0].position==161,"static exact extraction and grid-phase autoplay after worker");
 bank.apply(A::stopImmediate,0);check(bank.apply(A::play,0)&&bank.view()[0].state==S::queued&&bank.view()[0].scheduledFrame==2400,"stopped play queues to shared grid");
 bank.apply(A::stop,0);ticks(bank,1000);check(bank.view()[0].state==S::stopped,"queued play cancellation cannot fire later");
 bank.apply(A::play,0);auto start=bank.view()[0].scheduledFrame;ticks(bank,static_cast<unsigned>(start-bank.syncView().frame+1));check(bank.view()[0].state==S::playing,"queued play begins at deadline");
 check(bank.apply(A::stop,0)&&bank.view()[0].state==S::stopping,"playing short stop is quantized");check(bank.apply(A::stop,0)&&bank.view()[0].state==S::stopped,"second stop tap cuts immediately");
 // Count-in adds one bar when a grid point is too close; no delayed resurrection on panic.
 ready();ticks(bank,760);bank.configureSync({M::fixed,600,1,1,4,true});bank.apply(A::record,0);check(bank.view()[0].scheduledFrame==1600,"count-in minimum lead matches original extra bar");bank.apply(A::stopAll,0);ticks(bank,3000);check(bank.view()[0].state==S::empty,"Stop All cancels armed countdown");
 ready(M::dynamic);bank.apply(A::record,0);check(bank.view()[0].state==S::recording,"dynamic starts immediately");ticks(bank,300);bank.apply(A::finish,0);check(bank.view()[0].stopPending&&bank.view()[0].scheduledFrame==800,"first dynamic finish waits for next bar");ticks(bank,100);bank.apply(A::finish,0);wait(bank);check(bank.view()[0].length==400&&bank.view()[0].dynamicRecorded,"second dynamic finish stops now and keeps sub-bar take");
 ready(M::dynamic);bank.apply(A::record,0);ticks(bank,300);bank.apply(A::finish,0);ticks(bank,501);wait(bank);check(bank.view()[0].length==800&&!bank.view()[0].stopPending,"dynamic scheduled stop captures exactly through bar boundary");
 ready(M::dynamic);bank.apply(A::record,0);ticks(bank,500);bank.apply(A::finish,0);wait(bank);check(bank.view()[0].length==500,"dynamic <=50ms end window stops immediately");
 ready(M::dynamic);bank.apply(A::record,0);ticks(bank,1650);bank.apply(A::finish,0);ticks(bank,50);bank.apply(A::finish,0);wait(bank);check(bank.view()[0].length==1600,"dynamic forced stop trims down only when rounded bars fit captured audio");
 ready(M::dynamic);bank.apply(A::record,0);ticks(bank,300);bank.apply(A::finish,0);bank.apply(A::stopAll,0);wait(bank);ticks(bank,1000);check(bank.view()[0].state==S::stopped&&bank.view()[0].length==300&&!bank.view()[0].stopPending,"panic cancels dynamic wait and cannot autoplay later");
 ready();bank.setAutoPlay(false);bank.apply(A::record,0);ticks(bank,801);bank.apply(A::finish,0);wait(bank);check(bank.view()[0].state==S::stopped&&bank.view()[0].length==800,"early static finish zero-pads target and respects auto-play off");bank.setAutoPlay(true);
 ready();bank.apply(A::record,0);ticks(bank,900);failWorkerAllocation.store(true);bank.apply(A::finish,0);wait(bank);check(bank.view()[0].state==S::empty&&bank.view()[0].finalizationFailed&&bank.available()==8192,"static OOM releases reservation and cannot publish partial audio");
 ready();bank.configureSync({M::fixed,10,64,32,4,false});check(!bank.apply(A::record,0)&&bank.view()[0].state==S::empty,"oversized static take rejected before arm");
 ready();bank.apply(A::record,0);ticks(bank,900);bank.apply(A::finish,0);bank.apply(A::clearAll,0);wait(bank);ticks(bank,3000);check(bank.available()==8192&&bank.view()[0].state==S::empty,"clear invalidates static result and all deadlines");
 ready();bank.apply(A::record,0);bank.prepare(4096,0,96000);ticks(bank,3000);check(bank.view()[0].state==S::empty&&bank.syncView().settings.mode==M::off,"rate reset removes old armed schedules");
 // Exact 64/128 sample capture, worker extraction and committed grid playback.
 const auto render=[](unsigned block,M mode){
  auto engine=std::make_unique<aion::Engine>();engine->prepare(8000,8192);engine->configureSync({mode,600,1,1,4,false});engine->setMonitoring(false);engine->setTarget(1);
  check(engine->enqueueLoop({A::record,0,1,engine->generation(),0,0}),"sync timestamped arm");
  if(mode==M::dynamic)check(engine->enqueueLoop({A::finish,0,2,engine->generation(),300,0}),"dynamic timestamped finish");
  const auto run=[&](unsigned length,bool input){std::vector<float> left(length),right(length);if(input)for(unsigned i=0;i<length;++i){left[i]=0.1f+0.3f*std::sin(i*0.11f);right[i]=-0.1f+0.2f*std::cos(i*0.073f);}watch=true;for(unsigned i=0;i<length;i+=block){float* channels[]={left.data()+i,right.data()+i};engine->process(channels,2,static_cast<int>(block));aion::EngineView view;while(engine->popView(view)){};}watch=false;return std::make_pair(left,right);};
  run(2048,true);for(int n=0;engine->finalizationPending()&&n<10000;++n){float* channels[]={nullptr,nullptr};watch=true;engine->process(channels,2,0);aion::EngineView view;while(engine->popView(view)){}watch=false;std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(!engine->finalizationPending(),"sync engine commit wait");run(1024,false);return run(2048,false);
 };
 for(const auto mode:{M::fixed,M::dynamic}){const auto b64=render(64,mode),b128=render(128,mode);check(b64==b128,"static/dynamic capture, extraction and grid-phase playback exact at 64/128 blocks");}
 ready(M::dynamic);bank.apply(A::record,0);ticks(bank,300);bank.configureSync({M::fixed,300,2,3,8,true});bank.apply(A::finish,0);check(bank.view()[0].scheduledFrame==800,"active capture latches its own mode/bar/origin despite new settings");bank.apply(A::stopAll,0);wait(bank);
 check(aion::SyncClock::deadline(2400.0000000000005)==2400&&aion::SyncClock::deadline(2400.001)==2401,"grid deadlines remove only floating integer noise");
 aion::SyncClock clock;clock.reset(8000);clock.configure({M::fixed,120,2,4,4,false},0);const double before=(10000-clock.origin)*clock.settings.bpm;clock.configure({M::fixed,150,2,4,8,false},10000);check(std::abs((10000-clock.origin)*clock.settings.bpm-before)<1e-6,"tempo changes preserve beat phase and reference denominator behavior");
 check(allocations.load()==0,"sync scheduling/capture/queue/cancel must not allocate/free on callback");
 std::cout<<"PASS: static arm/cancel/extract/autoplay, dynamic bar/second-tap stop, queued play/stop, panic/reset/OOM/budget, tempo phase, 64/128 and callback allocation contract\n";
}
