// SPDX-License-Identifier: AGPL-3.0-only
// Manual warmed Release Engine observation. No timing assertion/latency claim.
#include "core/Engine.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <vector>
#ifdef _WIN32
#include <malloc.h>
#endif
namespace {
thread_local bool audioWatch=false;
std::atomic<unsigned> heapCalls{0};
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
void allocation(){if(audioWatch)++heapCalls;}
}
void* operator new(std::size_t n){allocation();if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{allocation();std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
void* operator new(std::size_t n,std::align_val_t a){
 allocation();void* p=nullptr;
#ifdef _WIN32
 p=_aligned_malloc(n?n:1,static_cast<std::size_t>(a));
#else
 if(posix_memalign(&p,static_cast<std::size_t>(a),n?n:1))p=nullptr;
#endif
 if(p)return p;throw std::bad_alloc();
}
void operator delete(void* p,std::align_val_t)noexcept{allocation();
#ifdef _WIN32
 _aligned_free(p);
#else
 std::free(p);
#endif
}
void* operator new[](std::size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete[](void* p,std::align_val_t a)noexcept{::operator delete(p,a);}
void operator delete(void* p,std::size_t,std::align_val_t a)noexcept{::operator delete(p,a);}
void operator delete[](void* p,std::size_t,std::align_val_t a)noexcept{::operator delete(p,a);}

int driveEngineBenchmark(){
 using namespace aion;constexpr unsigned warm=256,measured=1024;
 for(double rate:{48000.,96000.})for(unsigned block:{64u,128u})for(unsigned tracks:{10u,20u}){
  auto engine=std::make_unique<Engine>();engine->prepare(rate,262144);engine->setMonitoring(false);
  check(engine->enqueue({0,1}),"Benchmark MASTER enabled");
  std::array<float,128> left{},right{};float* audio[]{left.data(),right.data()};unsigned request=0;
  auto view=std::make_unique<EngineView>();
  const auto process=[&](bool input,bool offline){
   for(unsigned i=0;i<block;++i){left[i]=input?static_cast<float>(.13*std::sin((engine->clock()+i)*.13)):0;right[i]=input?static_cast<float>(.07*std::cos((engine->clock()+i)*.17)):0;}
   audioWatch=true;engine->process(audio,2,block,{},offline);audioWatch=false;
  };
  for(unsigned track=0;track<tracks;++track){
   check(engine->enqueueLoop({LoopAction::record,track,++request,engine->generation()}),"Benchmark record");
   for(unsigned n=0;n<1024/block;++n)process(true,true);
   check(engine->enqueueLoop({LoopAction::finish,track,++request,engine->generation()}),"Benchmark finish");process(false,true);
   check(engine->enqueueLoop({LoopAction::play,track,++request,engine->generation()}),"Benchmark play");
   check(engine->enqueueLoop({LoopAction::volume,track,++request,engine->generation(),0,.5/tracks}),"Benchmark non-clipping mixer");
  }
  process(false,true);std::array<std::uint64_t,20> clips{};
  for(unsigned id=0;id<tracks;++id){const auto t=engine->inspectAudioThread(id).track;check(t.state==LoopState::playing&&t.length==1024,"Every benchmark loop really plays");clips[id]=t.waveformId;}
  auto drive=defaultDriveBank();
  const auto edit=[&](bool enabled,bool automated,unsigned iteration){
   for(unsigned k=0;k<3;++k){drive[k]=driveDefaults(static_cast<DriveKind>(k));drive[k].enabled=enabled;
    if(automated){drive[k].values[2]=iteration%32?1800:8500;drive[k].values[4]=iteration%32?.3:.9;}}
   for(unsigned id=0;id<tracks;++id){LoopCommand c{LoopAction::drive,id,++request,engine->generation()};c.drives=drive;check(engine->enqueueLoop(c),"Bounded benchmark automation admitted");}
  };
  // Retain the old benchmark's Q/C matrix via the default executable mode.
  // This additional matrix reports new drives alone and with existing Q/C.
  for(unsigned scenario=0;scenario<5;++scenario){const bool enabled=scenario!=0,automated=scenario==2||scenario==4,qc=scenario>=3;
   for(unsigned id=0;id<tracks;++id){
    LoopCommand chain{LoopAction::signalChain,id,++request,engine->generation()};chain.signalChain=normalizedSignalChain("QCTFO");check(engine->enqueueLoop(chain),"Benchmark chain");
    LoopCommand q{LoopAction::equalizer,id,++request,engine->generation()};q.equalizer={eqPresetValues(8),qc,8};check(engine->enqueueLoop(q),"Benchmark EQ");
    LoopCommand c{LoopAction::compressor,id,++request,engine->generation()};c.compressor={compressorPresetValues(3),qc,3};check(engine->enqueueLoop(c),"Benchmark compressor");
   }edit(enabled,false,0);
   std::vector<double> times(measured);double checksum=0;
   for(unsigned n=0;n<warm+measured;++n){
    if(automated&&n%16==0)edit(true,true,n);
    left.fill(0);right.fill(0);const auto start=std::chrono::steady_clock::now();
    audioWatch=true;engine->process(audio,2,block,{},false);audioWatch=false;
    const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
    if(n>=warm)times[n-warm]=us;
    if(n%16==0)while(engine->popView(*view)){} // Off audio; bounded producer/consumer telemetry pressure.
    for(unsigned i=0;i<block;++i){check(std::isfinite(left[i])&&std::isfinite(right[i]),"Finite workspace PCM");if(n>=warm)checksum+=std::abs(left[i])+std::abs(right[i]);}
   }
   check(checksum>1,"Never report a silent fixture");
   for(unsigned id=0;id<tracks;++id){const auto t=engine->inspectAudioThread(id).track;check(t.state==LoopState::playing&&t.waveformId==clips[id]&&t.length==1024&&t.drives==drive,"Measured effects preserve clips, playback and accepted settings");}
   check(heapCalls.load()==0,"No watched callback new/delete, including aligned storage");
   std::sort(times.begin(),times.end());double sum=0;unsigned over=0;const double deadline=block/rate*1e6;
   for(double t:times){sum+=t;over+=t>deadline;}
   std::cout<<"{\"scope\":\"Engine playing loops, bounded telemetry; excludes Arp, synth/sampler, JUCE/device/UI/roundtrip\",\"rate\":"<<rate<<",\"block\":"<<block<<",\"tracks\":"<<tracks
    <<",\"drives\":"<<(enabled?"true":"false")<<",\"eqCompressor\":"<<(qc?"true":"false")<<",\"automated\":"<<(automated?"true":"false")
    <<",\"warmCallbacks\":"<<warm<<",\"measuredCallbacks\":"<<measured<<",\"meanUs\":"<<sum/measured<<",\"p99Us\":"<<times[measured*99/100]
    <<",\"maximumUs\":"<<times.back()<<",\"deadlineUs\":"<<deadline<<",\"observedOverruns\":"<<over<<",\"callbackHeapCalls\":"<<heapCalls.load()<<",\"checksum\":"<<checksum<<"}"<<std::endl;
  }
 }
 return 0;
}
