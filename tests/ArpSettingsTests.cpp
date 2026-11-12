// SPDX-License-Identifier: AGPL-3.0-only
#include "core/ArpSettings.h"
#include "core/Engine.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <new>
#include <string_view>
#include <thread>
namespace {
thread_local bool watched=false;thread_local unsigned operations=0;
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct Guard {Guard(){operations=0;watched=true;}~Guard(){watched=false;check(!operations,"settings/compiler callback new/delete");}};
}
void* operator new(std::size_t n){if(watched)++operations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watched&&p)++operations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace aion;
template<class Values>void printValues(const Values& v){std::cout<<'[';for(unsigned p=0;p<v.size();++p){if(p)std::cout<<',';std::cout<<v[p];}std::cout<<']';}
static void catalog(){
 std::cout<<"{\"controls\":[";for(unsigned p=0;p<arpControls.size();++p){const auto& d=arpControls[p];if(p)std::cout<<',';
  std::cout<<"{\"name\":\""<<d.name<<"\",\"initial\":"<<d.initial<<",\"min\":"<<d.minimum<<",\"max\":"<<d.maximum<<",\"widgetMax\":"<<d.widgetMaximum<<",\"index\":"<<d.renderIndex<<'}';}
 std::cout<<"],\"presets\":[";for(unsigned id=1;id<=arpPresetNames.size();++id){ArpSettings s;check(applyArpPreset(s,id),"catalog preset");if(id>1)std::cout<<',';std::cout<<"{\"name\":\""<<arpPresetNames[id-1]<<"\",\"values\":";printValues(s.values);std::cout<<'}';}
 std::cout<<"],\"scales\":[";for(unsigned p=0;p<arpScaleNames.size();++p){if(p)std::cout<<',';std::cout<<'"'<<arpScaleNames[p]<<'"';}std::cout<<"]}\n";
}
static void compileProbe(){
 double bpm;std::uint32_t seed,mask;unsigned enabled,preset;
 while(std::cin>>bpm>>seed>>mask>>enabled>>preset){ArpSettings s;s.enabled=enabled!=0;if(preset>255)std::exit(2);s.preset=static_cast<std::uint8_t>(preset);
  for(auto& v:s.values)if(!(std::cin>>v))std::exit(2);
  ArpRenderSettings result;std::uint32_t renderMask=0;
  if(!compileArpSettings(s,bpm,seed,result)||!arpRenderMask(mask,renderMask)){std::cout<<"{\"ok\":false}\n";continue;}
  std::cout<<"{\"ok\":true,\"mask\":"<<renderMask<<",\"seed\":"<<result.seed<<",\"enabled\":"<<(result.enabled?"true":"false")<<",\"values\":";printValues(result.values);std::cout<<"}\n";
 }
}
static void unit(){
 ArpSettings s;check(validArpSettings(s)&&!s.enabled&&!s.preset,"disabled reference defaults");
 ArpRenderSettings out;out.seed=19;
 s.values[3]=s.values[5]=-0.;check(compileArpSettings(s,120,9,out)&&!std::signbit(out.values[3])&&!std::signbit(out.values[5]),"original factory truthiness normalizes checkbox negative zero");s={};
 {Guard guard;check(compileArpSettings(s,137,99,out)&&out.seed==99&&!out.enabled&&out.values[1]==static_cast<float>(2./11.1)&&out.values[6]==137,"reference double-to-float boundary");}
 for(unsigned id=1;id<=8;++id){s.enabled=id%2;const bool enabled=s.enabled;
  {Guard guard;check(applyArpPreset(s,id)&&s.enabled==enabled&&s.preset==id&&validArpSettings(s),"preset preserves enable and exact metadata");}
  auto old=s;check(!applyArpPreset(s,0)&&!applyArpPreset(s,9)&&s==old,"unknown presets preserve state");
  auto bad=s;bad.values[2]=.123;check(!validArpSettings(bad),"preset metadata cannot describe unrelated values");
  {Guard guard;check(setArpControl(s,2,s.values[2])&&!s.preset,"equal-value manual gesture clears preset to Custom");}
  for(unsigned p=0;p<11;++p){const auto& d=arpControls[p];old=s;
   {Guard guard;check(!setArpControl(s,p,d.minimum-1)&&!setArpControl(s,p,d.widgetMaximum+1)&&!setArpControl(s,p,NAN)&&s==old,"invalid edits atomic");
    check(setArpControl(s,p,d.minimum)&&setArpControl(s,p,d.widgetMaximum),"all widget endpoints accepted");}
  }
 }
 s={};s.values[0]=4;s.values[6]=2;check(validArpSettings(s)&&!setArpControl(s,0,4)&&!setArpControl(s,6,2),"stored/DSP bounds wider than widgets; no clamping of stored state");
 s={};s.values[3]=.5;check(!validArpSettings(s),"checkbox domain is boolean");s={};s.values[5]=.5;check(!validArpSettings(s),"sync checkbox domain is boolean");
 s={};s.values[4]=3.5;check(validArpSettings(s),"fractional stored scale retains original DSP floor behavior");
 s={};s.values[1]=6.123456789;
 check(compileArpSettings(s,999,8,out)&&out.values[1]==static_cast<float>(s.values[1]/11.1)&&out.values[6]==300,"decimal repetitions and nominal BPM upper clamp");
 s.values[1]=9.9;check(compileArpSettings(s,120,8,out)&&out.values[1]!=static_cast<float>(static_cast<float>(9.9)/11.1),"9.9 repetitions catches premature Float32 storage");
 check(compileArpSettings(s,1,8,out)&&out.values[6]==30,"nominal BPM lower clamp");
 const auto old=out;check(!compileArpSettings(s,NAN,1,out)&&!compileArpSettings(s,INFINITY,1,out)&&!compileArpSettings(s,std::numeric_limits<double>::max(),1,out)&&out==old,"invalid BPM leaves complete output intact");
 s.values[1]=INFINITY;check(!compileArpSettings(s,120,1,out)&&out==old,"invalid control leaves output intact");
 std::uint32_t mask=99;check(!arpRenderMask(1u<<11,mask)&&mask==99,"bad control mask atomic");
 check(arpRenderMask(0,mask)&&mask==0&&arpRenderMask(arpAllControls,mask)&&mask==arpAllParameters,"toggle versus complete parameter mask");
 for(unsigned p=0;p<11;++p)check(arpRenderMask(1u<<p,mask)&&mask==((1u<<arpControls[p].renderIndex)|(1u<<6)),"each original edit also retargets BPM");
}
static void engineContract(unsigned block){
 auto engine=std::make_unique<Engine>();engine->prepare(8000,32768);std::array<float,128> l{},r{};float* audio[]{l.data(),r.data()};
 auto process=[&]{Guard guard;engine->process(audio,2,block);};
 for(unsigned id=1;id<=8;++id){
  ArpSettings settings;settings.enabled=true;check(applyArpPreset(settings,id),"engine preset");ArpRenderSettings compiled;
  check(compileArpSettings(settings,137,99,compiled),"engine typed controls compilation");const auto before=engine->inspectAudioThread().track;
  ArpPreparationRequest request;request.requestId=id;request.generation=engine->generation();request.values=compiled.values;request.seed=compiled.seed;request.activate=true;request.clearEpoch=before.clearEpoch;request.settingsRevision=before.arpSettingsRevision;
  check(engine->enqueueArpPreparation(request),"compiled settings prepare");ArpPreparationResult result;const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(!engine->popArpPreparation(result)){process();check(std::chrono::steady_clock::now()<until,"compiled settings adoption timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  check(result.status==ArpPreparationStatus::ready&&engine->inspectAudioThread().track.arp==compiled,"factory settings commit with matching storage");
  for(unsigned p=0;p<11;++p){const auto view=engine->inspectAudioThread().track;const auto value=(p==3||p==5)?1-settings.values[p]:(arpControls[p].minimum+arpControls[p].widgetMaximum)/2;
   check(setArpControl(settings,p,value)&&compileArpSettings(settings,173,99,compiled),"original control edit compilation");
   LoopCommand command{LoopAction::arpDelay,0,100+id*11+p,engine->generation()};command.arp=compiled;command.arpClearEpoch=view.clearEpoch;command.arpSettingsRevision=view.arpSettingsRevision;
   check(arpRenderMask(1u<<p,command.arpMask)&&engine->enqueueLoop(command),"compiled edit enqueue");process();
   check(engine->inspectAudioThread().track.arp==compiled&&engine->inspectPreparedArpAudioThread(0).targets==compiled.values,"compiled edit and BPM reach existing Engine owner together");
  }
 }
 std::cout<<"PASS original settings/presets -> existing Engine prepare/edit targets / "<<block<<", no callback new/delete\n";
}
int main(int argc,char** argv){std::cout<<std::setprecision(17);
 if(argc==2&&std::string_view(argv[1])=="catalog"){catalog();return 0;}
 if(argc==2&&std::string_view(argv[1])=="compile"){compileProbe();return 0;}
 check(argc==1,"settings test arguments");unit();for(unsigned block:{64u,128u})engineContract(block);
}
