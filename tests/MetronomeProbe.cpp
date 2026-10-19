#include "core/Engine.h"
#include <vector>
#include <iostream>
#include <iomanip>
#include <string>
int main(){
 aion::Metronome metro;aion::Engine engine;std::string op;std::cout<<std::setprecision(9);
 while(std::cin>>op){
  if(op=="prepare"){double rate;if(!(std::cin>>rate))return 2;metro.prepare(rate);engine.prepare(rate,8192);engine.setMonitoring(false);}
  else if(op=="render"){
   std::uint64_t frame;unsigned count,meter,playing;float bpm,origin,volume;
   if(!(std::cin>>frame>>count>>bpm>>origin>>meter>>volume>>playing)||count>2000000)return 2;
   metro.configure(bpm,origin,meter);metro.setVolume(volume);metro.setEnabled(playing!=0);
   for(unsigned i=0;i<count;++i)std::cout<<metro.tick(frame+i)<<' ';
   std::cout<<'\n';
  }else if(op=="engine"){
   unsigned count,meter,playing,block,mode;float bpm,volume;
   if(!(std::cin>>count>>bpm>>meter>>volume>>playing>>block>>mode)||!block||block>4096||count>2000000||mode>2)return 2;
   engine.configureSync({static_cast<aion::SyncMode>(mode),bpm,1,meter,4,false});engine.setMetronome(playing!=0,volume);
   std::vector<float> left(block),right(block);
   for(unsigned offset=0;offset<count;offset+=block){
    const auto n=std::min(block,count-offset);std::fill(left.begin(),left.end(),0);std::fill(right.begin(),right.end(),0);
    float* channels[]={left.data(),right.data()};const auto start=engine.clock();engine.process(channels,2,static_cast<int>(n));
    aion::EngineView view{},latest{};while(engine.popView(view))latest=view;
    std::cout<<std::setprecision(17)<<start<<' '<<n<<' '<<latest.sync.settings.bpm<<' '<<latest.sync.origin/latest.sampleRate<<' '<<meter<<' '<<volume<<' '<<playing;
    std::cout<<std::setprecision(9);for(unsigned i=0;i<n;++i){if(left[i]!=right[i])return 3;std::cout<<' '<<left[i];}std::cout<<'\n';
   }
  }else return 2;
 }
}
