// SPDX-License-Identifier: AGPL-3.0-only
// Manual warmed Release callback observation, never a hard real-time guarantee.
#include "core/Engine.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>
int main(){
 using namespace aion;
 constexpr unsigned warm=512,measured=2048;constexpr double rate=48000;
 for(unsigned block:{64u,128u})for(bool automated:{false,true}){
  auto engine=std::make_unique<Engine>();engine->prepare(rate,262144);engine->setMonitoring(false);
  std::array<float,128> left{},right{};float* audio[]{left.data(),right.data()};
  const auto render=[&](bool input,bool offline){for(unsigned i=0;i<block;++i){left[i]=input?static_cast<float>(.13*std::sin(i*.13)):0;right[i]=input?static_cast<float>(.07*std::cos(i*.17)):0;}engine->process(audio,2,block,{},offline);};
  for(unsigned track=0;track<20;++track){
   if(!engine->enqueueLoop({LoopAction::record,track,1,engine->generation()}))return 2;
   for(unsigned n=0;n<1024/block;++n)render(true,true);
   if(!engine->enqueueLoop({LoopAction::finish,track,2,engine->generation()}))return 3;
   render(false,true);
   if(!engine->enqueueLoop({LoopAction::play,track,3,engine->generation()}))return 4;
  }
  std::array<EqSettings,20> eq{};std::array<CompressorSettings,20> comp{};
  for(unsigned i=0;i<20;++i){eq[i]={eqPresetValues(8),true,8};comp[i]={compressorPresetValues(3),true,3};}
  if(!engine->enqueueEffectsBank(eq,comp,100))return 5;
  std::vector<double> times(measured);double checksum=0;
  for(unsigned n=0;n<warm+measured;++n){
   if(automated&&!(n%16)){
    for(unsigned i=0;i<20;++i){eq[i].preset=0;eq[i].values[4]=n%32?-6:6;comp[i].preset=0;comp[i].values[0]=n%32?-12:-30;}
    if(!engine->enqueueEffectsBank(eq,comp,100))return 6;
   }
   left.fill(0);right.fill(0);const auto start=std::chrono::steady_clock::now();
   engine->process(audio,2,block,{},false);
   const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
   if(n>=warm)times[n-warm]=us;checksum+=left[block-1]+right[block-1];
  }
  for(unsigned i=0;i<20;++i)if(engine->inspectAudioThread(i).track.state!=LoopState::playing){std::cerr<<"Fixture track "<<i<<" state "<<static_cast<unsigned>(engine->inspectAudioThread(i).track.state)<<'\n';return 7;}
  std::sort(times.begin(),times.end());double sum=0;unsigned over=0;const double deadline=block/rate*1e6;
  for(double t:times){sum+=t;over+=t>deadline;}
  std::cout<<"{\"scope\":\"Engine process: 20 playing stereo loops, EQ + compressor, bounded telemetry; excludes device/UI/roundtrip\",\"rate\":"<<rate<<",\"block\":"<<block
   <<",\"automated\":"<<(automated?"true":"false")<<",\"warmCallbacks\":"<<warm<<",\"measuredCallbacks\":"<<measured<<",\"meanUs\":"<<sum/measured
   <<",\"p99Us\":"<<times[measured*99/100]<<",\"maximumUs\":"<<times.back()<<",\"deadlineUs\":"<<deadline<<",\"observedOverruns\":"<<over<<",\"checksum\":"<<checksum<<"}\n";
 }
}
