// SPDX-License-Identifier: AGPL-3.0-only
#include "core/ArpDelayGraph.h"
#include "core/Wav.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <string>
#include <vector>
namespace {
thread_local bool watch=false;
unsigned operations=0;
void check(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
struct AudioScope {AudioScope(){watch=true;}~AudioScope(){watch=false;check(!operations,"organic callback new/delete");}};
}
void* operator new(std::size_t n){if(watch)++operations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{if(watch&&p)++operations;std::free(p);}
void operator delete[](void* p) noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept{::operator delete(p);}
using namespace aion;
static void motion(){
 ArpOrganicMotion motion;motion.reset(42,1000,48000);
 std::int64_t elapsed=0;double low=2,high=0,left=0,right=0;
 for(unsigned n=0;n<100000;++n){
  AudioScope audio;motion.note(1000,1);const auto state=motion.state();elapsed+=state.interval;
  check(state.interval>=500&&state.interval<=1500,"bounded loose intervals");
  check(std::abs(elapsed-static_cast<std::int64_t>(n+1)*1000)<=250,"anchored onset offsets cannot random-walk");
  check(elapsed-static_cast<std::int64_t>(n+1)*1000==state.offset,"onset grid identity");
  check(state.gain>=.35&&state.gain<=1.25&&std::abs(state.targetPan)<=.85,"bounded dynamics and pan");
  low=std::min(low,state.gain);high=std::max(high,state.gain);left=std::min(left,state.targetPan);right=std::max(right,state.targetPan);
  const double before=state.pan,after=motion.nextPan();check(std::abs(after-before)<.00045,"80ms pan slew");
 }
 check(low<.36&&high>1.24&&left<-.84&&right>.84,"loose motion exercises both sides and wide dynamics");
 {AudioScope audio;motion.retime(32);check(motion.state().interval==32&&motion.state().offset==0,"tempo jump reanchors next note");
  motion.note(32,0);check(motion.state().interval==32&&motion.state().gain==1&&motion.state().targetPan==0,"Feel zero identity");}
}
static std::vector<float> render(ArpDelayGraph& graph,unsigned block,unsigned frames,unsigned channels=2,bool edits=true,bool inPlace=false){
 std::vector<float> output(frames*2);std::array<float,128> l{},r{},ol{},orr{};
 // Split at event samples independently of callback size. Live tempo, minimum
 // delay, STAY, Feel and extreme feedback exercise the same streaming kernel.
 constexpr std::array<unsigned,5> times{4097,6013,8191,10003,12001};
 unsigned event=0;
 for(unsigned at=0;at<frames;){
  if(edits&&event<times.size()&&at==times[event]){AudioScope audio;
   if(event==0)check(graph.setTarget(0,.01f),"live minimum delay");
   if(event==1)check(graph.setTarget(3,1),"STAY");
   if(event==2)check(graph.setTarget(9,1)&&graph.setTarget(1,.98f),"Feel/feedback extremes");
   if(event==3)check(graph.setBpmImmediate(300)&&graph.setTarget(5,1),"tempo/sync jump");
   if(event==4)check(graph.setTarget(3,0)&&graph.setTarget(6,30),"STAY release/tempo smoothing");
   ++event;
  }
  unsigned count=std::min(block,frames-at);if(edits&&event<times.size())count=std::min(count,times[event]-at);
  for(unsigned i=0;i<count;++i){const auto f=at+i;const bool active=f<5000||f>=11000;
   l[i]=active?static_cast<float>(.2*std::sin(f*.071)+.1*std::cos(f*.023)):0;
   r[i]=active?static_cast<float>(.17*std::sin(f*.091)-.07*std::cos(f*.031)):0;}
  {AudioScope audio;check(graph.process(channels?l.data():nullptr,channels>1?r.data():nullptr,channels,
      inPlace?l.data():ol.data(),inPlace?r.data():orr.data(),count),"organic streaming render");}
  for(unsigned i=0;i<count;++i){const float a=inPlace?l[i]:ol[i],b=inPlace?r[i]:orr[i];
   check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a)<100&&std::abs(b)<100,"finite bounded stressed PCM");
   output[2*(at+i)]=a;output[2*(at+i)+1]=b;}
  at+=count;
 }
 return output;
}
static void streaming(){
 {
  ArpDelay kernel;auto values=arpDefaults();values[0]=.01f;values[9]=1;
  check(kernel.prepare(8000,values,42,0,ArpVoicing::loose),"onset trace prepare");
  const unsigned delay=static_cast<unsigned>(std::floor(double(values[0])*8000));unsigned expected=delay,notes=0;float input=.1f,l=0,r=0;
  for(unsigned frame=1;notes<200;++frame){
   {AudioScope audio;check(kernel.processCausal(&input,&input,2,&l,&r,1),"single-sample onset trace");}
   const auto state=kernel.organicState();
   if(state.notes!=notes){++notes;check(frame==expected,"actual kernel uses the scheduled loose onset");
    check(std::abs(static_cast<std::int64_t>(frame)-static_cast<std::int64_t>(notes)*delay)<=20,"actual onset remains near steady grid");expected+=state.interval;}
   check(frame<20000,"onset trace progresses");
  }
 }
 for(double rate:{8000.,22050.,32000.,44100.,48000.,96000.,192000.}){
  auto values=arpModDefaults();values[0]=.017f;values[2]=.8f;values[9]=.8f;
  ArpDelayGraph canonical;check(canonical.prepare(rate,values,42,0,ArpVoicing::loose),"prepare loose graph");
  const auto expected=render(canonical,128,16384);const auto state=canonical.organicState();
  check(state.notes>0,"actual graph drives organic note clock");
  for(unsigned block:{1u,37u,64u}){ArpDelayGraph split;check(split.prepare(rate,values,42,0,ArpVoicing::loose),"prepare split graph");
   check(render(split,block,16384)==expected&&split.organicState()==state,"exact loose PCM/state across callback sizes");}
  {AudioScope audio;check(canonical.reset(42),"realtime reset");}
  ArpDelayGraph resetReference;check(resetReference.prepare(rate,canonical.targets(),42,0,ArpVoicing::loose),"reset reference retains current targets");
  check(render(canonical,64,4096,2,false)==render(resetReference,128,4096,2,false),"same-seed reset exact PCM with retained controls");
  ArpDelayGraph other;check(other.prepare(rate,values,43,0,ArpVoicing::loose),"different seed prepare");
  check(render(other,128,16384)!=expected,"different seeds produce distinct audio");
  for(unsigned channels:{0u,1u,2u}){
   ArpDelayGraph separate,inPlace;check(separate.prepare(rate,values,42,0,ArpVoicing::loose)&&inPlace.prepare(rate,values,42,0,ArpVoicing::loose),"in-place prepare");
   check(render(separate,64,4096,channels,false)==render(inPlace,128,4096,channels,false,true),"tails/mono/stereo in-place identity");
  }
  // Zero Feel is an exact opt-out, not merely perceptually close.
  values[9]=0;ArpDelayGraph reference,zero;
  check(reference.prepare(rate,values,42)&&zero.prepare(rate,values,42,0,ArpVoicing::loose),"zero Feel prepare");
  check(render(reference,64,16384,2,false)==render(zero,128,16384,2,false),"Feel zero exact original PCM");
 }
 ArpDelayGraph graph;auto values=arpModDefaults();values[0]=.01f;
 check(graph.prepare(8000,values,99,0,ArpVoicing::loose),"idle/reset prepare");
 std::array<float,128> l{},r{};
 for(unsigned i=0;i<256;++i){AudioScope audio;check(graph.process(nullptr,nullptr,0,l.data(),r.data(),128),"silence");}
 const auto idle=graph.organicState();
 for(unsigned i=0;i<256;++i){AudioScope audio;check(graph.process(nullptr,nullptr,0,l.data(),r.data(),128),"idle silence");}
 check(graph.organicState()==idle,"idle suspends organic note/pan state");
 check(!graph.prepare(8000,values,99,0,static_cast<ArpVoicing>(255))&&graph.organicState()==idle,"invalid mode preserves old graph");
 {AudioScope audio;check(graph.reset(99),"idle reset");}
 check(graph.organicState().notes==0&&graph.organicState().pan==0,"reset clears motion history");
}
static void audition(const std::string& prefix){
 constexpr unsigned rate=48000,frames=rate*12;std::vector<float> dry(frames),l(frames),r(frames);
 for(unsigned i=0;i<frames;++i){const unsigned beat=i%(rate/2);if(i<rate*8)
  dry[i]=static_cast<float>(.19*std::exp(-double(beat)/(rate*.09))*(std::sin(i*6.283185307179586*220/rate)+.5*std::sin(i*6.283185307179586*330/rate)));}
 auto write=[&](const std::string& suffix,const std::vector<float>& a,const std::vector<float>& b){
  const auto bytes=encodeWav(a,b,rate);std::ofstream file(prefix+suffix,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));check(bool(file),"write audition WAV");};
 write("-dry.wav",dry,dry);
 for(auto mode:{ArpVoicing::original,ArpVoicing::loose}){ArpDelayGraph graph;auto values=arpModDefaults();values[0]=.18f;values[1]=.55f;values[2]=.65f;
  check(graph.prepare(rate,values,42,0,mode),"audition prepare");
  for(unsigned at=0;at<frames;at+=128){AudioScope audio;check(graph.process(dry.data()+at,dry.data()+at,2,l.data()+at,r.data()+at,std::min(128u,frames-at)),"audition render");}
  write(mode==ArpVoicing::original?"-original.wav":"-loose.wav",l,r);
 }
 std::cout<<"12-second dry/original/loose WAVs: "<<prefix<<"-*.wav (same input, seed, controls; Feel 0.35)\n";
}
static void benchmark(){
 for(unsigned block:{64u,128u})for(auto mode:{ArpVoicing::original,ArpVoicing::loose}){
  std::array<ArpDelayGraph,20> graphs;auto values=arpModDefaults();values[0]=.04f;
  for(unsigned n=0;n<graphs.size();++n)check(graphs[n].prepare(48000,values,n+42,0,mode),"benchmark graph prepare");
  std::array<float,128> l{},r{},ol{},orr{};std::array<double,1024> samples{};
  for(unsigned n=0;n<1536;++n){for(unsigned i=0;i<block;++i){l[i]=static_cast<float>(.15*std::sin((n*block+i)*.031));r[i]=static_cast<float>(.13*std::cos((n*block+i)*.023));}
   const auto start=std::chrono::steady_clock::now();
   {AudioScope audio;for(auto& graph:graphs)check(graph.process(l.data(),r.data(),2,ol.data(),orr.data(),block),"benchmark render");}
   if(n>=512)samples[n-512]=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
  }
  double sum=0;unsigned overruns=0;const double deadline=block*1e6/48000;
  for(double value:samples){sum+=value;overruns+=value>deadline;}std::sort(samples.begin(),samples.end());
  std::cout<<"{\"mode\":\""<<(mode==ArpVoicing::original?"original":"loose")<<"\",\"block\":"<<block
   <<",\"graphs\":20,\"mean_us\":"<<sum/samples.size()<<",\"p99_us\":"<<samples[1013]
   <<",\"max_us\":"<<samples.back()<<",\"wall_overruns\":"<<overruns<<"}\n";
 }
}
int main(int argc,char** argv){
 if(argc==2&&std::string(argv[1])=="--benchmark"){benchmark();return 0;}
 if(argc==3&&std::string(argv[1])=="--audition"){audition(argv[2]);return 0;}
 if(argc!=1)return 2;
 motion();streaming();std::cout<<"organic motion/streaming: passed; product defaults remain original\n";
}
