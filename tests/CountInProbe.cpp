#include "core/Engine.h"
#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>
int main(){
 auto engine=std::make_unique<aion::Engine>();aion::CountIn cue;std::string op;std::cout<<std::setprecision(17);
 while(std::cin>>op){
  if(op=="prepare"){double rate;std::cin>>rate;cue.prepare(rate);engine->prepare(rate,1048576);engine->setMonitoring(false);}
  else if(op=="schedule"){double now,target,bpm;unsigned meter,sound,visual;std::cin>>now>>target>>bpm>>meter>>sound>>visual;cue.schedule(now,target,bpm,meter,sound!=0,visual!=0);}
  else if(op=="kernel"){std::uint64_t frame;unsigned n;std::cin>>frame>>n;if(n>2000000)return 2;for(unsigned i=0;i<n;++i)std::cout<<cue.tick(frame+i)<<' '<<cue.flash(frame+i)<<' ';std::cout<<'\n';}
  else if(op=="configure"){unsigned mode,bars,num,den,visual,sound;double bpm;std::cin>>mode>>bpm>>bars>>num>>den>>visual>>sound;engine->configureSync({static_cast<aion::SyncMode>(mode),bpm,bars,num,den,visual!=0,sound!=0});}
  else if(op=="action"){unsigned action,id;std::cin>>action>>id;if(!engine->enqueueLoop({static_cast<aion::LoopAction>(action),id,1,engine->generation(),engine->clock(),0}))return 3;}
  else if(op=="render"){
   unsigned n,block;std::cin>>n>>block;if(n>2000000||!block||block>4096)return 2;
   std::vector<float> left(block),right(block);
   for(unsigned offset=0;offset<n;offset+=block){const auto count=std::min(block,n-offset);std::fill(left.begin(),left.end(),0);std::fill(right.begin(),right.end(),0);float* channels[]={left.data(),right.data()};
    const auto start=engine->clock();engine->process(channels,2,static_cast<int>(count));aion::EngineView view{},latest{};while(engine->popView(view))latest=view;
    std::cout<<start<<' '<<latest.tracks[0].scheduledFrame<<' '<<static_cast<unsigned>(latest.tracks[0].state)<<' '<<latest.masterPeak<<' '<<latest.outputPeak<<' '<<latest.countInFlash<<' '<<count;
    for(unsigned i=0;i<count;++i){if(left[i]!=right[i])return 4;std::cout<<' '<<left[i];}std::cout<<'\n';
   }
  }else return 2;
  if(!std::cin)return 2;
 }
}
