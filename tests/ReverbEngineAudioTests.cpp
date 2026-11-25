// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>
#include <vector>
namespace {
thread_local bool watched=false;thread_local unsigned operations=0;
void check(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
struct AudioScope {AudioScope(){operations=0;watched=true;}~AudioScope(){watched=false;check(!operations,"heap operation in actual Engine Reverb PCM path");}};
}
void* operator new(std::size_t n){if(watched)++operations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watched&&p)++operations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace aion;
// Actual Engine callback PCM oracle: the independent ReverbStorage graph is
// moved to the exact same target at the exact same sample as the ordered
// command, so worker adoption, sample-offset live edits and disabled metadata
// are all compared sample-for-sample. B remains gated in the product surface.
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
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
 do{render(false);check(std::chrono::steady_clock::now()<deadline,"real worker finalization timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 while(dry->inspectAudioThread().track.state!=LoopState::stopped||wet->inspectAudioThread().track.state!=LoopState::stopped);
 for(auto* engine:{dry.get(),wet.get()})check(engine->enqueueLoop({LoopAction::play,0,3,engine->generation()}),"play captured loop");
 for(unsigned n=0;n<512/block;++n)render(false);
 ReverbStorage oracle;check(oracle.prepareStopped(rate),"independent Reverb storage");
 ReverbSettings settings;settings.enabled=true;settings.values[2]=.1;settings.values[1]=.5;
 double error=0,audible=0;unsigned token=3;std::uint64_t transaction=0;
 const auto command=[&]{const auto info=wet->inspectAudioThread().track;
  LoopCommand c{LoopAction::reverb,0,++token,wet->generation(),wet->clock()+17};
  c.reverb=settings;c.reverbClearEpoch=info.clearEpoch;c.reverbSettingsRevision=info.reverbSettingsRevision;return c;
 };
 const auto compare=[&](std::uint64_t begin,bool live,std::uint64_t editAt,bool disable){
  for(unsigned i=0;i<block;++i){
   if(live&&begin+i==editAt){auto runtime=settings;
    if(disable){runtime=oracle.graph(0)->settings();runtime.enabled=false;}
    AudioScope scope;check(oracle.set(0,runtime),"independently scheduled live target");
   }
   constexpr float centre=.7071067811865475f;float l=dl[i]/centre,r=dr[i]/centre;
   {AudioScope scope;oracle.tick(0,l,r,l,r);}
   const float expected=(l+r)*.5f*centre;
   error=std::max({error,std::abs(double(wl[i])-expected),std::abs(double(wr[i])-expected)});
   audible=std::max(audible,std::abs(double(wl[i])-dl[i]));
  }
  const auto a=dry->inspectAudioThread().track,b=wet->inspectAudioThread().track;
  check(a.length==b.length&&a.position==b.position&&a.state==b.state&&a.undoCount==b.undoCount&&a.waveformId==b.waveformId,"Reverb command preserves PCM/transport/history");
 };
 const auto prepared=[&]{
  const auto c=command();
  check(oracle.request(0,settings,0),"reference graph admission");oracle.serviceWorker();
  check(wet->enqueueReverbEdit(c,transaction),"ordered prepared Reverb command");
  bool ready=false;const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);
  for(unsigned n=0;!ready||n<512/block;++n){const auto begin=render(false);ReverbEditResult result;
   if(wet->popReverbEdit(result)){
    check(!ready&&result.status==ReverbEditStatus::ready&&result.transaction==transaction&&result.requestId==c.requestId
     &&result.frame==begin&&result.frame>c.frame,"prepared success only at a later callback adoption boundary");
    {AudioScope scope;check(oracle.poll().installed==1,"reference adoption matches actual receipt boundary");}
    ready=true;
   }
   compare(begin,false,0,false);check(std::chrono::steady_clock::now()<until,"prepared Engine PCM deadline");
   if(!ready)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  // Let a freshly adopted fade finish even if worker preparation took longer.
  for(unsigned n=0;n<static_cast<unsigned>(rate*.025)/block+1;++n){const auto begin=render(false);compare(begin,false,0,false);}
 };
 prepared();
 // A same-shape live edit must land on its exact sample deadline.
 settings.values[1]=.8;auto live=command();const auto liveAt=live.frame;
 check(wet->enqueueReverbEdit(live,transaction),"sample-offset live mix");bool liveReady=false;
 for(unsigned n=0;n<512/block+1;++n){const auto begin=render(false);ReverbEditResult result;
  if(wet->popReverbEdit(result)){check(result.status==ReverbEditStatus::ready&&result.transaction==transaction&&result.frame==liveAt,"live audible edit exact sample receipt");liveReady=true;}
  compare(begin,true,liveAt,false);
 }check(liveReady,"live receipt delivered");
 // Disabled metadata may select a new shape without allocating; the old runtime
 // fades to exact bypass and the committed metadata keeps the requested shape.
 settings.enabled=false;settings.values[2]=.2;
 auto disable=command();check(wet->enqueueReverbEdit(disable,transaction),"disabled shape edit");bool disabled=false;
 for(unsigned n=0;n<static_cast<unsigned>(rate*.4)/block+1;++n){const auto begin=render(false);ReverbEditResult result;
  if(wet->popReverbEdit(result)){check(result.status==ReverbEditStatus::ready&&result.transaction==transaction&&result.frame==disable.frame,"disabled shape needs no worker allocation");disabled=true;}
  compare(begin,true,disable.frame,true);
 }
 check(disabled&&wet->inspectAudioThread().track.reverb==settings,"disabled metadata committed");
 for(unsigned i=0;i<block;++i)check(wl[i]==dl[i]&&wr[i]==dr[i],"settled disabled B is exact Engine bypass");
 // Enabling that shape requires (and adopts) a prepared graph.
 settings.enabled=true;
 prepared();
 check(error<2.e-6&&audible>1.e-3,"nonzero actual Engine audio matches independently scheduled Reverb");
 std::cout<<"PASS Engine Reverb PCM "<<rate<<" Hz / "<<block<<", maximum error "<<error<<", audible change "<<audible<<'\n';
}
int main(){
 check(true,"fixture");
 for(double rate:{8000.,48000.})for(unsigned block:{64u,128u})run(rate,block);
}
