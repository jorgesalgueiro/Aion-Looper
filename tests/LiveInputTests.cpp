#include "core/Engine.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>
extern thread_local bool watch;
extern std::atomic<int> allocations;
namespace {void check(bool x,const char* why){if(!x){std::cerr<<why<<'\n';std::exit(1);}}}
void liveInputTests(){
 using namespace aion;
 auto monitor=std::make_unique<LiveInput>();LiveInputEvent event{};
 watch=true;monitor->start(2,3,123);for(unsigned i=0;i<17;++i)monitor->push(i*0.01f,-static_cast<float>(i)*0.005f);check(monitor->publish(140)&&monitor->pop(event),"live first window");
 check(event.track==2&&event.generation==3&&event.startFrame==123&&event.frame==140,"live capture tags");
 for(unsigned i=0;i<495;++i)check(event.samples[i]==0,"live zero-filled prefix");
 for(unsigned i=0;i<17;++i)check(event.samples[495+i]==(i*0.01f)*0.5f+(-static_cast<float>(i)*0.005f)*0.5f,"live chronological stereo mix");
 monitor->start(1,4,200);for(unsigned i=0;i<700;++i)monitor->push(i*0.001f,i*0.001f);monitor->publish(900);monitor->pop(event);for(unsigned i=0;i<512;++i)check(event.samples[i]==(188+i)*0.001f,"live wrap retains latest 512 samples");
 for(unsigned i=0;i<3;++i)check(monitor->publish(901+i),"bounded live queue accepts slots");check(!monitor->publish(904),"full live queue drops telemetry");while(monitor->pop(event)){}
 monitor->start(0,5,1000);monitor->push(std::numeric_limits<float>::infinity(),0.2f);monitor->publish(1001);monitor->pop(event);check(event.samples.back()==0.1f&&event.samples.front()==0,"new capture clears old window and sanitizes invalid input");monitor->cancel();check(!monitor->publish(1002),"cancel suppresses publication");watch=false;
 std::vector<float> baseline;
 for(const auto block:{64,128}){
  auto engine=std::make_unique<Engine>();engine->prepare(8000,81920);engine->setMonitoring(false);engine->setMetronome(true,0.5f);engine->configureSync({SyncMode::dynamic,120,1,4,4,false});
  engine->enqueueLoop({LoopAction::record,0,1,engine->generation(),128,0});
  std::vector<float> left(2048),right(2048),outL(2048),outR(2048);for(unsigned i=0;i<2048;++i){left[i]=0.4f*std::sin(i*0.13f);right[i]=0.2f*std::cos(i*0.07f);}outL=left;outR=right;
  for(unsigned offset=0;offset<2048;offset+=block){float* channels[]={outL.data()+offset,outR.data()+offset};watch=true;engine->process(channels,2,block);watch=false;
   while(engine->popLiveInput(event)){
    check(event.startFrame==128&&event.track==0&&event.generation==engine->generation()&&event.frame==offset+block,"sample-clock capture tags at both blocks");
    for(unsigned i=0;i<512;++i){const auto frame=static_cast<long long>(event.frame)-512+i;const float want=frame<128?0:left[static_cast<unsigned>(frame)]*0.5f+right[static_cast<unsigned>(frame)]*0.5f;check(event.samples[i]==want,"actual Engine window matches input, excludes metronome and MASTER");}
   }
  }
  if(baseline.empty())baseline.assign(event.samples.begin(),event.samples.end());else check(std::equal(baseline.begin(),baseline.end(),event.samples.begin()),"exact 64/128 live windows");
  engine->enqueueLoop({LoopAction::stopAll,0,2,engine->generation(),0,0});float zero=0,*channels[]={&zero};watch=true;engine->process(channels,1,1);watch=false;check(!engine->popLiveInput(event),"panic suppresses stale live telemetry");
  const auto oldGeneration=engine->generation();engine->prepare(16000,81920);check(engine->generation()!=oldGeneration,"reprepare invalidates live epoch");watch=true;engine->process(nullptr,0,0);watch=false;check(!engine->popLiveInput(event),"no live publication after reprepare");
 }
 check(allocations.load()==0,"live input has zero callback new/delete");std::cout<<"PASS: live input 512-sample chronological mono mix, prefix/wrap/reset, bounded overflow, Engine capture tags, click/MASTER exclusion, panic/reset, exact 64/128 and zero callback allocations\n";
}
