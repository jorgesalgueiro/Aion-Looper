// SPDX-License-Identifier: AGPL-3.0-only
#include "core/ArpDelay.h"
#include "core/ArpModulation.h"
#include "core/ArpDelayGraph.h"
#include "core/ArpSettings.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
int main(){
 aion::ArpDelay dsp;aion::ArpModulation mod;aion::ArpDelayGraph graph;std::string operation;std::cout<<std::setprecision(17);
 while(std::cin>>operation){
  if(operation=="prepare"){
   double rate;std::uint32_t seed;aion::ArpValues values{};if(!(std::cin>>rate>>seed))return 2;for(auto& value:values)if(!(std::cin>>value))return 2;
   std::cout<<(dsp.prepare(rate,values,seed)?"ok":"rejected")<<'\n';
  }else if(operation=="values"){
   aion::ArpValues values{};for(auto& value:values)if(!(std::cin>>value))return 2;std::cout<<(dsp.setValues(values)?"ok":"rejected")<<'\n';
  }else if(operation=="reset"){
   std::uint32_t seed;if(!(std::cin>>seed))return 2;std::cout<<(dsp.reset(seed)?"ok":"rejected")<<'\n';
  }else if(operation=="state"||operation=="graphstate"){
   const auto s=operation=="state"?dsp.state():graph.state();std::cout<<"{\"write\":"<<s.write<<",\"clock\":"<<s.clock<<",\"sequenceIndex\":"<<s.sequenceIndex<<",\"step\":"<<s.step<<",\"direction\":"<<s.direction
    <<",\"phase\":"<<s.phase<<",\"semitone\":"<<s.semitone<<",\"grainRate\":"<<s.grainRate<<",\"grainTarget\":"<<s.grainTarget<<",\"envelope\":"<<s.envelope<<",\"tail\":"<<s.tail<<",\"randomDraws\":"<<s.randomDraws<<",\"sequence\":[";
   for(unsigned i=0;i<16;++i){if(i)std::cout<<',';std::cout<<s.sequence[i];}std::cout<<"]}\n";
  }else if(operation=="render"||operation=="graphrender"){
   const bool combined=operation=="graphrender";
   unsigned frames,channels,block;if(!(std::cin>>frames>>channels>>block)||frames>2000000||channels>2||!block||block>(combined?4096u:128u))return 2;
   std::vector<float> l(frames),r(frames),ol(frames),orr(frames);
   for(unsigned i=0;i<frames;++i){if(channels&&!(std::cin>>l[i]))return 2;if(channels>1&&!(std::cin>>r[i]))return 2;}
   for(unsigned i=0;i<frames;i+=block){const auto count=std::min(block,frames-i);
    if(combined?!graph.process(channels?l.data()+i:nullptr,channels>1?r.data()+i:nullptr,channels,ol.data()+i,orr.data()+i,count):
                 !dsp.process(channels?l.data()+i:nullptr,channels>1?r.data()+i:nullptr,channels,ol.data()+i,orr.data()+i,count))return 3;}
   std::cout<<"pcm";for(unsigned i=0;i<frames;++i)std::cout<<' '<<ol[i]<<' '<<orr[i];std::cout<<'\n';
  }else if(operation=="graphprepare"){
   double rate;std::uint32_t seed;aion::ArpModValues values{};if(!(std::cin>>rate>>seed))return 2;for(auto& v:values)if(!(std::cin>>v))return 2;
   std::cout<<(graph.prepare(rate,values,seed)?"ok":"rejected")<<'\n';
  }else if(operation=="graphbpm"){
   float value;if(!(std::cin>>value))return 2;std::cout<<(graph.setBpmImmediate(value)?"ok":"rejected")<<'\n';
  }else if(operation=="graphtarget"){
   unsigned p;float value;if(!(std::cin>>p>>value))return 2;std::cout<<(graph.setTarget(p,value)?"ok":"rejected")<<'\n';
  }else if(operation=="graphpreset"){
   aion::ArpRenderSettings settings;for(auto& v:settings.values)if(!(std::cin>>v))return 2;
   if(!aion::validArpRenderSettings(settings)){std::cout<<"rejected\n";continue;}
   bool ok=true;for(const auto& write:aion::arpPresetWrites(settings))ok=graph.setTarget(write.parameter,write.value)&&ok;
   std::cout<<(ok?"ok":"rejected")<<'\n';
  }else if(operation=="graphreset"){
   std::uint32_t seed;std::uint64_t frame;if(!(std::cin>>seed>>frame))return 2;std::cout<<(graph.reset(seed,frame)?"ok":"rejected")<<'\n';
  }else if(operation=="modprepare"){
   double rate;aion::ArpModValues values{};if(!(std::cin>>rate))return 2;for(auto& v:values)if(!(std::cin>>v))return 2;
   std::cout<<(mod.prepare(rate,values)?"ok":"rejected")<<'\n';
  }else if(operation=="modtarget"){
   unsigned p;float value;if(!(std::cin>>p>>value))return 2;std::cout<<(mod.setTarget(p,value)?"ok":"rejected")<<'\n';
  }else if(operation=="modrender"){
   unsigned frames,block;if(!(std::cin>>frames>>block)||frames>2000000||!block||block>128)return 2;
   std::vector<float> l(frames),r(frames);for(unsigned i=0;i<frames;++i)if(!(std::cin>>l[i]>>r[i]))return 2;
   std::cout<<"mod";
   for(unsigned begin=0;begin<frames;begin+=block)for(unsigned i=begin;i<std::min(begin+block,frames);++i){
    aion::ArpControlFrame controls;if(!mod.next(controls))return 3;float ol,orr;
    mod.panStereo(l[i],r[i],controls.pan,ol,orr);for(float v:controls.parameters)std::cout<<' '<<v;std::cout<<' '<<ol<<' '<<orr;
   }std::cout<<'\n';
  }else return 2;
 }
}
