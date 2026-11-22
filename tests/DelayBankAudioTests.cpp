// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>
namespace {
thread_local bool watched=false;thread_local unsigned operations=0;
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct AudioScope {AudioScope(){operations=0;watched=true;}~AudioScope(){watched=false;check(!operations,"heap operation in bank Engine PCM callback");}};
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
 for(auto* engine:{dry.get(),wet.get()}){engine->prepare(rate,32768);engine->setMonitoring(false);engine->setAutoPlay(false);check(engine->enqueue({0,1}),"unity master");}
 std::array<float,128> dl{},dr{},wl{},wr{};float* d[]{dl.data(),dr.data()},*w[]{wl.data(),wr.data()};
 EngineView dv{},wv{},snapshot{};
 const auto render=[&](bool capture,unsigned track=0){const auto begin=dry->clock();
  for(unsigned i=0;i<block;++i)dl[i]=dr[i]=wl[i]=wr[i]=capture?static_cast<float>(.07*std::sin((begin+i)*(.031+track*.001))+.02*std::cos((begin+i)*.071)):0;
  {AudioScope guard;dry->process(d,2,block);wet->process(w,2,block);}
  while(dry->popView(snapshot))dv=snapshot;while(wet->popView(snapshot))wv=snapshot;
  check(dry->clock()==wet->clock()&&dv.frame==wv.frame,"bank paired callback clocks");return begin;
 };
 unsigned ordinary=0;
 for(unsigned track:{0u,19u}){
  for(auto* engine:{dry.get(),wet.get()})check(engine->enqueueLoop({LoopAction::record,track,++ordinary,engine->generation()}),"record two real native tracks");
  for(unsigned n=0;n<4096/block;++n)render(true,track);
  for(auto* engine:{dry.get(),wet.get()})check(engine->enqueueLoop({LoopAction::finish,track,++ordinary,engine->generation()}),"finish track");
  const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  do{render(false);check(std::chrono::steady_clock::now()<until,"native bank clip finalization");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  while(dv.tracks[track].state!=LoopState::stopped||wv.tracks[track].state!=LoopState::stopped);
  for(auto* engine:{dry.get(),wet.get()})check(engine->enqueueLoop({LoopAction::play,track,++ordinary,engine->generation()}),"play both source tracks");
  for(unsigned n=0;n<512/block;++n)render(false);
 }
 DelayStorage oracle;check(oracle.prepareStopped(rate),"bank audio oracle prepare");
 LoopBank::DelayBank accepted{};double maximum=0,audible=0;unsigned requestId=100;
 const auto restore=[&](const LoopBank::DelayBank& targets){
  DelayBankRequest request;request.settings=targets;request.requestId=++requestId;request.generation=wet->generation();request.frame=wet->clock()+17;
  for(unsigned id=0;id<20;++id){request.clearEpochs[id]=wv.tracks[id].clearEpoch;request.settingsRevisions[id]=wv.tracks[id].delaySettingsRevision;}
  check(oracle.requestBank(targets,120),"prepare independent bank");for(const auto& settings:targets)if(settings.enabled)oracle.serviceWorker();
  std::uint64_t transaction=0;check(wet->enqueueDelayBank(request,transaction),"ordered actual bank request");
  bool ready=false;unsigned settled=0;const unsigned settleBlocks=static_cast<unsigned>(std::ceil(rate*.025/block))+1;
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(!ready||settled<settleBlocks){const auto begin=render(false);DelayBankResult result;
   if(wet->popDelayBank(result)){
    check(!ready&&result.status==DelayEditStatus::ready&&result.transaction==transaction&&result.requestId==request.requestId
      &&result.frame==begin&&result.frame>request.frame,"whole bank receipt at one callback boundary");
    {AudioScope guard;check(oracle.poll(120).bankInstalled,"reference adopts complete bank at same sample");}
    for(unsigned id=0;id<20;++id)check(result.settingsRevisions[id]==request.settingsRevisions[id]+1&&result.clearEpochs[id]==request.clearEpochs[id],"receipt owns all twenty target revisions");
    ready=true;
   }
   bool old=true,next=true;
   for(unsigned id=0;id<20;++id){old&=wv.tracks[id].delay==accepted[id];next&=wv.tracks[id].delay==targets[id];}
   check(old||next,"audio snapshot never contains a partially committed bank");
   check(ready?next:old,"target visibility agrees with exact bank receipt");
   std::array<std::array<float,MeterWindow::size>,2> raw{};
   {AudioScope guard;check(dry->copyLoopMeterWindow(0,raw[0])&&dry->copyLoopMeterWindow(19,raw[1]),"real dry per-track callback PCM");
    for(unsigned i=0;i<block;++i){constexpr float centre=.7071067811865475f;float expected=0;
     for(unsigned k=0;k<2;++k){const float mono=raw[k][MeterWindow::size-block+i]/centre;float l,r;
      oracle.tick(k?19:0,mono,mono,l,r);expected+=(l+r)*.5f*centre;
     }
     maximum=std::max({maximum,std::abs(double(wl[i])-expected),std::abs(double(wr[i])-expected)});
     audible=std::max(audible,std::abs(double(wl[i])-dl[i]));
    }
   }
   for(unsigned id:{0u,19u})check(dv.tracks[id].waveformId==wv.tracks[id].waveformId&&dv.tracks[id].length==wv.tracks[id].length
    &&dv.tracks[id].position==wv.tracks[id].position&&dv.tracks[id].state==wv.tracks[id].state&&dv.tracks[id].undoCount==wv.tracks[id].undoCount,"bank restore leaves both source clips/transport/history untouched");
   if(ready)++settled;else std::this_thread::sleep_for(std::chrono::milliseconds(1));
   check(std::chrono::steady_clock::now()<deadline,"continuous bank rendering deadline");
  }
  accepted=targets;
 };
 LoopBank::DelayBank first{};first[0].enabled=true;first[0].values[0]=.01;first[0].values[3]=.7;
 first[19].enabled=true;first[19].values[0]=.025;first[19].values[3]=.6;check(setDelayRepeats(first[19],DelayRepeatMode::exact,3),"first bank exact count");restore(first);
 auto second=first;second[0].values[0]=.0113;check(setDelayRepeats(second[0],DelayRepeatMode::exact,2),"second bank exact count");second[19].enabled=false;restore(second);
 auto off=second;for(auto& settings:off)settings.enabled=false;restore(off);
 for(unsigned i=0;i<block;++i)check(wl[i]==dl[i]&&wr[i]==dr[i],"all-off bank settles to exact two-track dry output");
 check(maximum<2.e-6&&audible>1.e-3,"nonzero multi-track Engine bank PCM oracle");
 std::cout<<"PASS atomic bank Engine PCM "<<rate<<" Hz / "<<block<<", maximum error "<<maximum<<", audible bank change "<<audible<<'\n';
}
int main(){for(double rate:{8000.,48000.})for(unsigned block:{64u,128u})run(rate,block);}
