// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <numbers>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
namespace {
thread_local bool watched=false;thread_local unsigned operations=0;
void check(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
struct AudioScope {AudioScope(){operations=0;watched=true;}~AudioScope(){watched=false;check(!operations,"heap operation in actual Engine Delay PCM path");}};
}
void* operator new(std::size_t n){if(watched)++operations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watched&&p)++operations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace aion;
static void run(double rate,unsigned block){
 auto dry=std::make_unique<Engine>(),wet=std::make_unique<Engine>();
 std::array<float,128> dl{},dr{},wl{},wr{};float* d[]{dl.data(),dr.data()},*w[]{wl.data(),wr.data()};
 for(auto* engine:{dry.get(),wet.get()}){engine->prepare(rate,16384);engine->setMonitoring(false);engine->setAutoPlay(false);
  check(engine->enqueue({0,1})&&engine->enqueueLoop({LoopAction::record,0,1,engine->generation()}),"actual PCM capture");}
 const auto render=[&](bool capture){const auto begin=dry->clock();
  for(unsigned i=0;i<block;++i)dl[i]=dr[i]=wl[i]=wr[i]=capture?static_cast<float>(.08*std::sin((begin+i)*.071)+.02*std::cos((begin+i)*.019)):0;
  {AudioScope scope;dry->process(d,2,block);wet->process(w,2,block);}
  check(dry->clock()==wet->clock(),"paired Engine sample clocks");return begin;
 };
 for(unsigned n=0;n<4096/block;++n)render(true);
 for(auto* engine:{dry.get(),wet.get()})check(engine->enqueueLoop({LoopAction::finish,0,2,engine->generation()}),"finish capture");
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 do{render(false);check(std::chrono::steady_clock::now()<deadline,"real worker finalization timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 while(dry->inspectAudioThread().track.state!=LoopState::stopped||wet->inspectAudioThread().track.state!=LoopState::stopped);
 for(auto* engine:{dry.get(),wet.get()})check(engine->enqueueLoop({LoopAction::play,0,3,engine->generation()}),"play captured loop");
 for(unsigned n=0;n<512/block;++n)render(false);
 DelayStorage oracle;check(oracle.prepareStopped(rate),"independent Delay storage");
 DelaySettings settings;settings.enabled=true;settings.values[0]=.01;settings.values[3]=.4;
 double error=0,audible=0;unsigned token=3;std::uint64_t transaction=0;
 const auto command=[&]{const auto info=wet->inspectAudioThread().track;
  LoopCommand c{LoopAction::delay,0,++token,wet->generation(),wet->clock()+17};
  c.delay=settings;c.delayClearEpoch=info.clearEpoch;c.delaySettingsRevision=info.delaySettingsRevision;return c;
 };
 const auto compare=[&](std::uint64_t begin,std::uint64_t editAt,bool live,bool disable){
  for(unsigned i=0;i<block;++i){
   if(live&&begin+i==editAt){auto runtime=settings;
    if(disable){runtime=oracle.graph(0)->settings();runtime.enabled=false;}
    AudioScope scope;check(oracle.graph(0)->set(runtime,120),"independently scheduled live target");
   }
   constexpr float centre=.7071067811865475f;float l=dl[i]/centre,r=dr[i]/centre;
   {AudioScope scope;oracle.tick(0,l,r,l,r);}
   const float expected=(l+r)*.5f*centre;
   error=std::max({error,std::abs(double(wl[i])-expected),std::abs(double(wr[i])-expected)});
   audible=std::max(audible,std::abs(double(wl[i])-dl[i]));
  }
  const auto a=dry->inspectAudioThread().track,b=wet->inspectAudioThread().track;
  check(a.length==b.length&&a.position==b.position&&a.state==b.state&&a.undoCount==b.undoCount&&a.waveformId==b.waveformId,"Delay command preserves PCM/transport/history");
 };
 const auto prepared=[&]{const auto c=command();
  check(oracle.request(0,settings,120),"reference mode admission");oracle.serviceWorker();
  check(wet->enqueueDelayEdit(c,transaction),"ordered prepared Delay command");
  bool ready=false;const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  for(unsigned n=0;!ready||n<512/block;++n){const auto begin=render(false);DelayEditResult result;
   if(wet->popDelayEdit(result)){
    check(!ready&&result.status==DelayEditStatus::ready&&result.transaction==transaction&&result.requestId==c.requestId
     &&result.frame==begin&&result.frame>c.frame,"prepared success only at a later callback adoption boundary");
    {AudioScope scope;check(oracle.poll().installed==1,"reference adoption matches actual receipt boundary");}
    ready=true;
   }
   compare(begin,0,false,false);check(std::chrono::steady_clock::now()<until,"prepared Engine PCM deadline");
   if(!ready)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  // Let a freshly adopted fade finish even if worker preparation took longer
  // than the minimum render prefix above.
  for(unsigned n=0;n<static_cast<unsigned>(rate*.025)/block+1;++n){const auto begin=render(false);compare(begin,0,false,false);}
 };
 prepared();
 settings.values[3]=.8;auto live=command();const auto liveAt=live.frame;
 check(wet->enqueueDelayEdit(live,transaction),"sample-offset live mix");bool liveReady=false;
 for(unsigned n=0;n<512/block;++n){const auto begin=render(false);DelayEditResult result;
  if(wet->popDelayEdit(result)){check(result.status==DelayEditStatus::ready&&result.transaction==transaction&&result.frame==liveAt,"live audible edit exact sample receipt");liveReady=true;}
  compare(begin,liveAt,true,false);
 }check(liveReady,"live receipt delivered");
 check(setDelayRepeats(settings,DelayRepeatMode::exact,3),"exact-count mode");prepared();
 // Disabled metadata may choose another mode/count/time without reallocating
 // or reinterpreting the audible ring. The old runtime fades to exact bypass.
 settings.enabled=false;settings.values[0]=.02;check(setDelayRepeats(settings,DelayRepeatMode::feedback,1),"disabled mode metadata");
 auto disable=command();check(wet->enqueueDelayEdit(disable,transaction),"disabled mode edit");bool disabled=false;
 for(unsigned n=0;n<static_cast<unsigned>(rate*.4)/block+1;++n){const auto begin=render(false);DelayEditResult result;
  if(wet->popDelayEdit(result)){check(result.status==DelayEditStatus::ready&&result.transaction==transaction&&result.frame==disable.frame,"disabled mode needs no worker allocation");disabled=true;}
  compare(begin,disable.frame,true,true);
 }
 check(disabled&&wet->inspectAudioThread().track.delay==settings,"disabled metadata committed");
 for(unsigned i=0;i<block;++i)check(wl[i]==dl[i]&&wr[i]==dr[i],"settled disabled mode exact Engine bypass");
 check(error<2.e-6&&audible>1.e-3,"nonzero actual Engine audio matches independently scheduled Delay");
 std::cout<<"PASS Engine Delay PCM "<<rate<<" Hz / "<<block<<", maximum error "<<error<<", audible change "<<audible<<'\n';
}
// Actual Engine callback stereo oracle, independent of Delay/DelayStorage and
// panStereo. The companion LoopBank fixture retains all five loop-pan positions.
static void stereo(double rate,unsigned block,bool exact){
 auto left=std::make_unique<Engine>(),right=std::make_unique<Engine>(),wet=std::make_unique<Engine>();
 std::array<float,128> ll{},lr{},rl{},rr{},wl{},wr{};
 float* l[]{ll.data(),lr.data()},*r[]{rl.data(),rr.data()},*w[]{wl.data(),wr.data()};
 for(auto* engine:{left.get(),right.get(),wet.get()}){engine->prepare(rate,16384);engine->setMonitoring(false);engine->setAutoPlay(false);check(engine->enqueue({0,1})&&engine->enqueueLoop({LoopAction::record,0,1,engine->generation()}),"stereo Engine capture");}
 const auto render=[&](bool capture){const auto begin=wet->clock();
  for(unsigned i=0;i<block;++i){const auto n=begin+i;
   const float a=capture?static_cast<float>(.08*std::sin(n*.031)+.03*std::cos(n*.071)):0;
   const float b=capture?static_cast<float>(.05*std::cos(n*.047)-.02*std::sin(n*.083)):0;
   ll[i]=lr[i]=wl[i]=a;rl[i]=rr[i]=wr[i]=b;
  }
  {AudioScope scope;left->process(l,2,block);right->process(r,2,block);wet->process(w,2,block);}
  check(left->clock()==wet->clock()&&right->clock()==wet->clock(),"stereo Engine clocks");return begin;
 };
 for(unsigned n=0;n<4096/block;++n)render(true);
 for(auto* engine:{left.get(),right.get(),wet.get()})check(engine->enqueueLoop({LoopAction::finish,0,2,engine->generation()}),"stereo Engine finish");
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
 do{render(false);check(std::chrono::steady_clock::now()<deadline,"stereo Engine finalization deadline");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 while(left->inspectAudioThread().track.state!=LoopState::stopped||right->inspectAudioThread().track.state!=LoopState::stopped||wet->inspectAudioThread().track.state!=LoopState::stopped);
 DelaySettings settings;settings.enabled=true;settings.values={.01017,0,3000,.63,exact?3.7:5.,exact?.7:1.,0};
 if(exact)check(setDelayRepeats(settings,DelayRepeatMode::exact,1),"stereo Engine exact1");
 const auto before=wet->inspectAudioThread().track;
 LoopCommand command{LoopAction::delay,0,3,wet->generation(),wet->clock()+17};command.delay=settings;command.delayClearEpoch=before.clearEpoch;command.delaySettingsRevision=before.delaySettingsRevision;
 std::uint64_t transaction=0;check(wet->enqueueDelayEdit(command,transaction),"stereo Engine D request");
 std::uint64_t adopted=0;
 while(!adopted){render(false);DelayEditResult result;
  if(wet->popDelayEdit(result)){check(result.status==DelayEditStatus::ready&&result.transaction==transaction,"stereo Engine exact adoption receipt");adopted=result.frame;}
  check(std::chrono::steady_clock::now()<deadline,"stereo Engine adoption deadline");std::this_thread::sleep_for(std::chrono::milliseconds(1));
 }
 const float pan=exact?8.f:2.f;
 check(wet->enqueueLoop({LoopAction::pan,0,4,wet->generation(),0,pan}),"stereo Engine pan");
 for(unsigned n=0;n<static_cast<unsigned>(rate*.1)/block+1;++n)render(false);
 for(auto* engine:{left.get(),right.get(),wet.get()})check(engine->enqueueLoop({LoopAction::play,0,5,engine->generation()}),"stereo Engine play");
 std::vector<Stereo> source(16384);double maximum=0,audible=0;
 constexpr float centre=.7071067811865475f;
 const double delay=settings.values[0]*rate,loopAngle=pan*std::numbers::pi/20;
 const unsigned whole=static_cast<unsigned>(delay);const double fraction=delay-whole;
 for(unsigned base=0;base<source.size();base+=block){const auto begin=render(false);AudioScope scope;
  for(unsigned i=0;i<block;++i){const unsigned n=base+i;source[n]={ll[i]/centre,rl[i]/centre};
   check(std::isfinite(wl[i])&&std::isfinite(wr[i]),"finite stereo Engine PCM (NaN cannot evade maximum-error reduction)");
   const auto a=n>=whole?source[n-whole]:Stereo{},b=n>whole?source[n-whole-1]:Stereo{};
   const double dl=a.left+(b.left-a.left)*fraction,dr=a.right+(b.right-a.right)*fraction;
   const double position=std::sin(2*std::numbers::pi*settings.values[4]*static_cast<double>(begin+i-adopted)/rate)*settings.values[5];
   const double angle=(position<=0?position+1:position)*std::numbers::pi/2;
   const double pl=position<=0?dl+dr*std::cos(angle):dl*std::cos(angle);
   const double pr=position<=0?dr*std::sin(angle):dr+dl*std::sin(angle);
   const double mono=((1-settings.values[3])*(source[n].left+source[n].right)+settings.values[3]*(pl+pr))*.5;
   maximum=std::max({maximum,std::abs(wl[i]-mono*std::cos(loopAngle)),std::abs(wr[i]-mono*std::sin(loopAngle))});
   audible=std::max(audible,std::max(std::abs(double(wl[i])),std::abs(double(wr[i]))));
  }
 }
 check(maximum<2.e-6&&audible>.01,"independent actual Engine stereo echo/auto-pan/loop-pan PCM");
 const auto after=wet->inspectAudioThread().track;check(after.length==before.length&&after.waveformId==before.waveformId&&after.undoCount==before.undoCount,"stereo Engine PCM/history immutable");
 std::cout<<"PASS Engine stereo D "<<rate<<" Hz / "<<block<<" / "<<(exact?"exact1":"feedback0")<<", max error "<<maximum<<'\n';
}
int main(int argc,char** argv){
 if(argc==4&&std::string_view(argv[1])=="--stereo"){
  const double rate=std::stod(argv[2]);const auto block=static_cast<unsigned>(std::stoul(argv[3]));
  check((rate==8000||rate==48000)&&(block==64||block==128),"stereo Engine fixture rate/block");
  for(bool exact:{false,true})stereo(rate,block,exact);
  return 0;
 }
 check(argc==1,"Engine fixture arguments");
 for(double rate:{8000.,48000.})for(unsigned block:{64u,128u})run(rate,block);
}
