#include "core/Engine.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <thread>
#include <vector>
extern thread_local bool watch;
extern std::atomic<int> allocations;
namespace {void check(bool ok,const char* what){if(!ok){std::cerr<<what<<'\n';std::exit(1);}}}
void countInTests(){
 using namespace aion;
 CountIn cue;cue.prepare(48000);
 watch=true;cue.schedule(0,2,120,4,true,true);check(cue.tick(0)==0&&cue.flash(0)==0,"count-in skips past/50ms cues");
 check(cue.tick(24000)==0&&cue.tick(24001)>0&&cue.flash(24000)==1,"count-in sample boundary and ordinary flash");
 check(cue.flash(72000)==2&&cue.tick(72001)>cue.tick(24001),"count-in final accent");
 check(cue.tick(76800)==0&&cue.flash(79200)==0,"count-in 100ms sound and 150ms visual duration");
 cue.cancel();check(cue.tick(24001)==0&&cue.flash(24000)==0,"cancel removes all pending cues");
 cue.schedule(0,2,std::numeric_limits<double>::quiet_NaN(),4,true,true);check(cue.tick(24001)==0,"invalid count-in scheduling is silent");watch=false;
 std::vector<float> reference;
 for(const auto block:{64,128}){
  auto engine=std::make_unique<Engine>();engine->prepare(8000,16384);engine->setMonitoring(false);
  engine->configureSync({SyncMode::fixed,999,1,4,4,false,true});
  check(engine->enqueueLoop({LoopAction::record,0,1,engine->generation(),0,0}),"arm SOUND-only recording");
  std::vector<float> left(8192),right(8192);bool tailInCapture=false;EngineView latest{},view{};
  for(unsigned offset=0;offset<8192;offset+=block){float* channels[]={left.data()+offset,right.data()+offset};watch=true;engine->process(channels,2,block);watch=false;while(engine->popView(view))latest=view;
   check(latest.masterPeak==0,"count-in bypasses MASTER meter");if(latest.tracks[0].state==LoopState::recording)for(unsigned i=0;i<static_cast<unsigned>(block);++i)tailInCapture|=std::abs(left[offset+i])>0.01f;
  }
  check(left==right,"count-in stereo equality");if(reference.empty())reference=left;else check(reference==left,"count-in exact 64/128 invariance");
  check(tailInCapture,"fast-tempo cue tail overlaps recording for capture-exclusion test");
  for(unsigned i=0;i<10000&&latest.tracks[0].state!=LoopState::playing;++i){float zeros[64]{},*channels[]={zeros};watch=true;engine->process(channels,1,64);watch=false;while(engine->popView(view))latest=view;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  check(latest.tracks[0].state==LoopState::playing&&latest.tracks[0].length==1922,"static SOUND take commits with original length");
  engine->setTarget(1);
  for(unsigned i=0;i<4096;i+=64){float zeros[64]{},*channels[]={zeros};watch=true;engine->process(channels,1,64);watch=false;for(float value:zeros)check(value==0,"SOUND never enters committed loop PCM");}
 }
 // Every user cancellation barrier cancels the same audio-owned cue schedule.
 for(const auto action:{LoopAction::finish,LoopAction::stop,LoopAction::stopImmediate,LoopAction::clear,LoopAction::clearAll,LoopAction::stopAll}){
  auto engine=std::make_unique<Engine>();engine->prepare(8000,16384);engine->setMonitoring(false);engine->configureSync({SyncMode::fixed,999,1,4,4,true,true});
  check(engine->enqueueLoop({LoopAction::record,0,1,engine->generation(),0,0}),"arm cancellation case");float zeros[128]{},*channels[]={zeros};watch=true;engine->process(channels,1,128);watch=false;
  check(engine->enqueueLoop({action,0,2,engine->generation(),0,0}),"queue count-in cancellation");
  for(unsigned i=0;i<4096;i+=128){std::fill_n(zeros,128,0);watch=true;engine->process(channels,1,128);watch=false;for(float sample:zeros)check(sample==0,"cancellation prevents delayed SOUND");EngineView view{};while(engine->popView(view))check(view.countInFlash==0,"cancellation prevents delayed visual flash");}
 }
 // Accepted schedules latch their timing and flags, like the original scheduled
 // oscillators. Unrelated track actions and rejected gestures cannot steal them.
 {
  auto engine=std::make_unique<Engine>();engine->prepare(8000,16384);engine->setMonitoring(false);engine->configureSync({SyncMode::fixed,120,1,4,4,true,true});
  check(engine->enqueueLoop({LoopAction::record,0,1,engine->generation(),0,0}),"arm latched cues");float zeros[64]{},*channels[]={zeros};watch=true;engine->process(channels,1,64);watch=false;
  engine->configureSync({SyncMode::off,600,1,1,4,false,false});
  check(engine->enqueueLoop({LoopAction::stop,1,2,engine->generation(),0,0})&&engine->enqueueLoop({LoopAction::record,1,3,engine->generation(),0,0}),"unrelated/rejected actions queued");
  bool heard=false,flashed=false;
  for(unsigned i=0;i<8192;i+=64){std::fill_n(zeros,64,0);watch=true;engine->process(channels,1,64);watch=false;for(float sample:zeros)heard|=std::abs(sample)>0.01f;EngineView view{};while(engine->popView(view))flashed|=view.countInFlash!=0;}
  check(heard&&flashed,"tempo/mode/checkbox edits and unrelated/rejected actions preserve latched cues");
  engine->prepare(96000,16384);bool resetSilent=true;
  for(unsigned i=0;i<16384;i+=64){std::fill_n(zeros,64,0);watch=true;engine->process(channels,1,64);watch=false;for(float sample:zeros)resetSilent&=sample==0;}
  check(resetSilent,"prepare/reset removes old cues");
 }
 for(const auto mode:{SyncMode::off,SyncMode::dynamic}){
  auto engine=std::make_unique<Engine>();engine->prepare(8000,8192);engine->setMonitoring(false);engine->configureSync({mode,999,1,4,4,true,true});
  engine->enqueueLoop({LoopAction::record,0,1,engine->generation(),0,0});
  for(unsigned i=0;i<4096;i+=64){float zeros[64]{},*channels[]={zeros};watch=true;engine->process(channels,1,64);watch=false;for(float sample:zeros)check(sample==0,"OFF/DYNAMIC do not schedule count-in");}
 }
 check(allocations.load()==0,"count-in process/schedule/cancel must not allocate/free");
 std::cout<<"PASS: SOUND-only lead-in, exact 64/128 stereo, accents/flash, overlapping cue capture exclusion, all cancellation barriers; zero callback new/delete\n";
}
