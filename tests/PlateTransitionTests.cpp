// SPDX-License-Identifier: AGPL-3.0-only
#include "core/PlateReverb.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>
static bool watched=false;static unsigned heap=0;
void* operator new(std::size_t n){if(watched)++heap;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watched&&p)++heap;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
static void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
// Independent event-time envelope: each edit starts from the value at that
// absolute sample, reaches its target 20 ms later, and never restarts on a
// repeated target. No dependency on the DSP's ramp representation.
struct Envelope {
 double start=0,target=0;std::size_t origin=0,length=1;
 double at(std::size_t n)const {return start+(target-start)*std::min(1.,double(n-origin)/double(length));}
 void edit(double value,std::size_t n){if(value!=target){start=at(n);origin=n;target=value;}}
};
static void run(std::string_view mode,double rate,unsigned block){
 const auto ramp=static_cast<std::size_t>(std::ceil(.02*rate));
 const auto warm=static_cast<std::size_t>(rate*.2),frames=warm+5*ramp+block;
 std::vector<float> l(frames),r(frames),wetL(frames),wetR(frames),outL(frames),outR(frames);
 for(std::size_t n=0;n<frames;++n){l[n]=float(.19*std::sin(.071*double(n))+.07);r[n]=float(.13*std::cos(.049*double(n))-.04);}
 aion::PlateSettings s;s.enabled=true;s.unmask=0;s.mix=mode=="mix"?0.f:.6f;
 auto oracleSettings=s;oracleSettings.mix=mode=="mix"?1.f:s.mix;
 aion::PlateReverb actual,oracle;check(actual.prepare(rate,s)&&oracle.prepare(rate,oracleSettings),"prepare transition pair");
 Envelope envelope;envelope.start=envelope.target=mode=="mix"?0.:1.;envelope.length=ramp;
 struct Edit {std::size_t frame;double value;};
 const std::array<Edit,5> edits=mode=="mix"?std::array<Edit,5>{{{warm+17,1},{warm+ramp/3,.25},{warm+ramp/2,.25},{warm+ramp,0},{warm+2*ramp,1}}}:
  std::array<Edit,5>{{{warm+17,0},{warm+ramp/3,1},{warm+ramp/2,1},{warm+ramp,0},{warm+3*ramp,1}}};
 unsigned event=0;double energy=0;
 for(std::size_t n=0;n<frames;){
  if(event<edits.size()&&edits[event].frame==n){
   const auto value=edits[event].value;
   if(mode=="recovery"&&event==4){oracle.reset();envelope.start=envelope.target=0;envelope.origin=n;}
   envelope.edit(value,n);
   if(mode=="mix")s.mix=static_cast<float>(value);else s.enabled=value!=0;
   watched=true;check(actual.set(s),"live transition accepted");watched=false;
   check(actual.settings().enabled==s.enabled&&actual.settings().mix==s.mix,"requested state remains immediate and exact");
   auto bad=s;bad.mix=std::numeric_limits<float>::quiet_NaN();
   watched=true;check(!actual.set(bad),"invalid edit rejected during ramp");
   check(actual.process({},{},{},{}),"empty callback accepted without advancing ramp");watched=false;
   ++event;
  }
  // For the rapid-bypass case stop before the first completed bypass; only
  // recovery compares the intentionally reset engine after a full bypass.
  if(mode=="enable"&&n>=warm+2*ramp)break;
  const auto next=event<edits.size()?edits[event].frame:frames;
  auto count=std::min<std::size_t>(block,frames-n);count=std::min(count,next-n);
  if(mode=="enable")count=std::min(count,warm+2*ramp-n);
  // Exercise irregular spans as well as 64/128, including one-sample calls.
  if((n%7)==0)count=std::min<std::size_t>(count,1);
  watched=true;
  check(actual.process(std::span(l).subspan(n,count),std::span(r).subspan(n,count),std::span(outL).subspan(n,count),std::span(outR).subspan(n,count)),"actual transition block");
  check(oracle.process(std::span(l).subspan(n,count),std::span(r).subspan(n,count),std::span(wetL).subspan(n,count),std::span(wetR).subspan(n,count)),"independent static wet block");
  watched=false;check(!heap,"transitions have zero callback heap operations");
  for(std::size_t i=n;i<n+count;++i){
   const double e=envelope.at(i);
   const double expectedL=double(l[i])+(double(wetL[i])-double(l[i]))*e;
   const double expectedR=double(r[i])+(double(wetR[i])-double(r[i]))*e;
   if(std::abs(double(outL[i])-expectedL)>2e-6||std::abs(double(outR[i])-expectedR)>2e-6){
    std::cerr<<mode<<" rate="<<rate<<" block="<<block<<" frame="<<i<<" envelope="<<e<<" expected="<<expectedL<<" actual="<<outL[i]<<'\n';check(false,"stereo transition PCM follows independent 20ms event-time envelope");}
   if(mode!="mix"&&e==0)check(outL[i]==l[i]&&outR[i]==r[i],"settled bypass is bit-exact dry");
   energy+=std::abs(double(wetL[i])-l[i])+std::abs(double(wetR[i])-r[i]);
  }
  n+=count;
 }
 check(energy>1,"transition oracle contains nonzero stereo effect PCM");
 // A destructive reset must latch the requested controls without continuing
 // a previous ramp. Existing reset tests retain full dirty-vs-fresh coverage.
 s.mix=.83f;s.enabled=true;watched=true;check(actual.set(s),"begin new ramp immediately before reset");actual.reset();watched=false;
 aion::PlateReverb fresh;check(fresh.prepare(rate,s),"prepare reset oracle");
 std::array<float,128> input{},a{},b{},c{},d{};
 for(unsigned n=0;n<static_cast<unsigned>(rate*.15)/block+1;++n){
  for(unsigned i=0;i<block;++i)input[i]=float(.2*std::sin(double(n*block+i)*.091));
  watched=true;
  actual.process(std::span(input).first(block),std::span(input).first(block),std::span(a).first(block),std::span(b).first(block));
  fresh.process(std::span(input).first(block),std::span(input).first(block),std::span(c).first(block),std::span(d).first(block));watched=false;
  check(a==c&&b==d&&!heap,"reset cancels ramps without heap work or stale audio");
 }
}
int main(int argc,char** argv){check(argc==3,"MODE BLOCK required");const std::string_view mode=argv[1];const auto block=static_cast<unsigned>(std::atoi(argv[2]));
 check((mode=="mix"||mode=="enable"||mode=="recovery")&&(block==64||block==128),"valid mode and block");
 for(double rate:{8000.,48000.,384000.})run(mode,rate,block);
 std::cout<<"PASS plate "<<mode<<": independent stereo PCM ramps, rapid/repeated edits, exact dry, reset and zero callback heap at "<<block<<" / 8,48,384 kHz\n";
}
