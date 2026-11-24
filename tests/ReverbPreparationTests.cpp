// SPDX-License-Identifier: AGPL-3.0-only
#include "core/ReverbImpulse.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <new>
#include <string_view>
namespace {
bool watched=false;unsigned heap=0;int failAfter=-1;std::atomic<bool>* cancelOnAllocate=nullptr;
void check(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
struct Guard {Guard(){heap=0;watched=true;}~Guard(){watched=false;check(!heap,"settings/early cancellation must not allocate/free");}};
}
void* operator new(std::size_t n){if(watched)++heap;if(failAfter==0)throw std::bad_alloc();if(failAfter>0)--failAfter;
 if(auto* p=std::malloc(n?n:1)){if(cancelOnAllocate)cancelOnAllocate->store(true);return p;}throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watched&&p)++heap;std::free(p);}void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace aion;
static void catalog(){
 std::cout.precision(17);std::cout<<"{\"controls\":[";
 for(unsigned p=0;p<reverbControls.size();++p){const auto& c=reverbControls[p];if(p)std::cout<<',';
  std::cout<<"{\"name\":\""<<c.name<<"\",\"initial\":"<<c.initial<<",\"min\":"<<c.minimum<<",\"max\":"<<c.maximum<<",\"step\":"<<c.step<<'}';}
 std::cout<<"],\"rooms\":[";
 for(unsigned r=0;r<reverbRooms.size();++r){if(r)std::cout<<',';const auto& room=reverbRooms[r];std::cout<<"{\"name\":\""<<room.name<<"\",\"duration\":"<<room.duration<<",\"decay\":"<<room.decay<<'}';}
 std::cout<<"],\"presets\":[";
 for(unsigned p=1;p<=reverbPresetNames.size();++p){ReverbSettings s;check(applyReverbPreset(s,p),"catalog preset");if(p>1)std::cout<<',';
  std::cout<<"{\"name\":\""<<reverbPresetNames[p-1]<<"\",\"room\":\""<<reverbRooms[static_cast<unsigned>(s.room)].name<<"\",\"values\":[";
  for(unsigned i=0;i<s.values.size();++i){if(i)std::cout<<',';std::cout<<s.values[i];}std::cout<<"]}";
 }std::cout<<"]}\n";
}
struct Stats {double energy=0,dc=0,early=0,late=0;};
static Stats stats(std::span<const float> samples){Stats result;for(std::size_t i=0;i<samples.size();++i){const double v=samples[i];check(std::isfinite(v),"finite IR coefficients");const double power=v*v;result.energy+=power;result.dc+=v;if(i<samples.size()/4)result.early+=power;if(i>=3*samples.size()/4)result.late+=power;}return result;}
int main(int argc,char** argv){
 if(argc==2&&std::string_view(argv[1])=="catalog"){catalog();return 0;}
 ReverbSettings s;check(validReverbSettings(s)&&!s.enabled&&s.room==ReverbRoom::studio,"disabled studio defaults");
 for(unsigned id=1;id<=reverbPresetNames.size();++id){s={};s.enabled=(id&1)!=0;const bool enabled=s.enabled;
  Guard guard;check(applyReverbPreset(s,id)&&s.enabled==enabled&&validReverbSettings(s),"all named presets preserve enable");
  auto previous=s;check(!applyReverbPreset(s,0)&&!applyReverbPreset(s,18)&&s==previous,"bad preset is atomic");
  check(setReverbControl(s,0,s.values[0])&&s.preset==0,"same-value manual gesture selects Custom");
  for(unsigned p=0;p<6;++p){previous=s;check(!setReverbControl(s,p,NAN)&&!setReverbControl(s,p,INFINITY)&&!setReverbControl(s,p,reverbControls[p].minimum-1)&&!setReverbControl(s,p,reverbControls[p].maximum+1)&&s==previous,"invalid numeric controls preserve settings");
   check(setReverbControl(s,p,reverbControls[p].minimum)&&setReverbControl(s,p,reverbControls[p].maximum),"all range endpoints");}
 }
 s={};check(setReverbControl(s,3,4.123456789012345)&&s.values[3]==4.123456789012345,"stored precision");
 for(unsigned r=0;r<reverbRooms.size();++r){const auto old=s;Guard guard;check(setReverbRoom(s,static_cast<ReverbRoom>(r)),"room selection");check(s.values[2]==reverbRooms[r].duration&&s.values[3]==reverbRooms[r].decay&&s.values[0]==old.values[0]&&s.values[1]==old.values[1]&&s.values[4]==old.values[4]&&s.values[5]==old.values[5],"room changes only duration/decay");}
 auto previous=s;check(!setReverbRoom(s,static_cast<ReverbRoom>(255))&&!setReverbControl(s,6,0)&&s==previous,"unknown room/control rejected atomically");
 s={};s.preset=1;check(!validReverbSettings(s),"false preset identity rejected");
 unsigned cases=0;
 for(double rate:{8000.,22050.,32000.,44100.,48000.,96000.,192000.,384000.}){
  s={};s.values[2]=.1;ReverbImpulse ir,copy;
  check(ir.prepare(rate,s,12345)&&copy.prepare(rate,s,12345),"IR preparation");
  const auto* cached=ir.channel(0).data();{Guard guard;check(ir.prepare(rate,s,12345)&&ir.channel(0).data()==cached,"identical IR key reuses prepared storage without allocation/free");}
  check(ir.channel(0).size()==static_cast<std::size_t>(std::floor(rate*.1))&&ir.channel(2).empty(),"bounded channel/frame layout");
  double correlation=0;
  for(unsigned c=0;c<2;++c){const auto a=ir.channel(c),b=copy.channel(c);check(a.front()==0&&a.back()==0,"exact smooth IR endpoints");check(std::memcmp(a.data(),b.data(),a.size_bytes())==0,"same seed/settings regenerate identical PCM on this platform");
   const auto st=stats(a);check(std::abs(st.energy-1)<2.e-7&&std::abs(st.dc)<1.e-5,"unit energy and DC rejection");
   for(unsigned block:{64u,128u}){double energy=0;Guard guard;for(std::size_t base=0;base<a.size();base+=block)for(std::size_t i=base;i<std::min(base+block,a.size());++i)energy+=static_cast<double>(a[i])*a[i];check(std::abs(energy-st.energy)<1.e-12,"immutable callback-sized reads do not allocate or change PCM");}
  }
  for(std::size_t i=0;i<ir.channel(0).size();++i)correlation+=static_cast<double>(ir.channel(0)[i])*ir.channel(1)[i];check(std::abs(correlation)<.2,"stereo IR channels decorrelated");++cases;
 }
 // The full Decay range changes the distribution rather than having a dead
 // region. Volume/mix/damp/predelay are live downstream controls, not IR keys.
 s={};s.values[2]=1;s.values[3]=.1;ReverbImpulse loose,tight,other;
 check(loose.prepare(48000,s,0),"zero seed is valid");s.values[3]=5;check(tight.prepare(48000,s,0),"tight decay");
 check(stats(tight.channel(0)).early>stats(loose.channel(0)).early&&stats(tight.channel(0)).late<stats(loose.channel(0)).late,"higher decay tightens the tail");
 auto live=s;live.enabled=true;live.values[0]=3;live.values[1]=0;live.values[4]=500;live.values[5]=.25;check(other.prepare(48000,live,0),"live control IR independence");check(std::memcmp(tight.channel(0).data(),other.channel(0).data(),tight.channel(0).size_bytes())==0,"live controls do not redefine IR PCM");
 {Guard guard;const auto* cached=tight.channel(0).data();check(tight.prepare(48000,live,0)&&tight.channel(0).data()==cached,"live gain/mix/damping/predelay/bypass edits retain cached IR");}
 check(other.prepare(48000,s,1)&&!std::equal(tight.channel(0).begin(),tight.channel(0).end(),other.channel(0).begin()),"different seed gives distinct impulse");
 const auto* old=other.channel(0).data();const auto oldRate=other.sampleRate();const auto oldSeed=other.seed();const auto oldFirst=other.channel(0)[10];
 const std::array<std::vector<float>,2> oldPcm{std::vector<float>(other.channel(0).begin(),other.channel(0).end()),std::vector<float>(other.channel(1).begin(),other.channel(1).end())};
 const auto unchanged=[&]{return other.channel(0).size()==oldPcm[0].size()&&other.channel(1).size()==oldPcm[1].size()&&other.channel(0).data()==old&&other.channel(0)[10]==oldFirst&&other.sampleRate()==oldRate&&other.seed()==oldSeed&&std::equal(oldPcm[0].begin(),oldPcm[0].end(),other.channel(0).begin())&&std::equal(oldPcm[1].begin(),oldPcm[1].end(),other.channel(1).begin());};
 for(double invalid:{0.,7999.,384001.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){Guard guard;check(!other.prepare(invalid,s,42),"invalid sample rate rejected before allocation");}
 {auto invalid=s;invalid.values[2]=NAN;Guard guard;check(!other.prepare(48000,invalid,42)&&unchanged(),"invalid settings preserve old IR without allocation");}
 for(int fail:{0,1}){failAfter=fail;const bool ok=other.prepare(44100,s,42);failAfter=-1;check(!ok&&unchanged(),"OOM at either channel preserves complete old impulse");}
 std::atomic<bool> cancelled{true};{Guard guard;check(!other.prepare(44100,s,42,&cancelled),"cancel before allocation");}
 cancelled=false;cancelOnAllocate=&cancelled;const bool ok=other.prepare(44100,s,42,&cancelled);cancelOnAllocate=nullptr;
 check(!ok&&unchanged(),"cancellation during preparation preserves complete old impulse");
 s.values[2]=5;check(ReverbImpulse::requiredBytes(384000,s)==1920000u*2*sizeof(float),"maximum raw IR byte budget");
 cancelled=false;check(other.prepare(384000,s,0,&cancelled)&&other.channel(0).size()==1920000,"maximum rate/duration preparation");check(std::abs(stats(other.channel(0)).energy-1)<2.e-7,"maximum IR remains normalized");
 std::cout<<"PASS B preparation: 17 presets, 8 rooms, 6 controls, "<<cases<<" rates, deterministic stereo/DC/energy/edges/decay, 64/128 immutable reads, OOM/cancel rollback; not connected convolution\n";
}
