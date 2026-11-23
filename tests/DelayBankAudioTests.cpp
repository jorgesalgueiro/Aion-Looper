// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"
#include "core/ArpSettings.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>
namespace {
thread_local bool watched=false;thread_local unsigned operations=0;
std::atomic<bool> failNext{false};
std::atomic<unsigned> pauseAt{0};std::atomic<bool> pauseEntered{false},releasePause{false};
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct AudioScope {AudioScope(){operations=0;watched=true;}~AudioScope(){watched=false;check(!operations,"heap operation in bank Engine PCM callback");}};
}
void* operator new(std::size_t n){if(watched)++operations;if(failNext.exchange(false))throw std::bad_alloc();
 if(pauseAt.load()&&pauseAt.fetch_sub(1)==1){pauseEntered.store(true,std::memory_order_release);while(!releasePause.load(std::memory_order_acquire))std::this_thread::yield();}
 if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watched&&p)++operations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace aion;
static void run(double rate,unsigned block,bool coordinated=false){
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
 ArpDelayStorage arpOracle;check(arpOracle.prepareStopped(rate),"coordinated Arp audio oracle prepare");
 LoopBank::ArpBank acceptedArps{};std::array<SignalChain,20> acceptedChains{};
 for(unsigned id=0;id<20;++id){acceptedArps[id]=wv.tracks[id].arp;acceptedChains[id]=wv.tracks[id].signalChain;}
 LoopBank::DelayBank accepted{};double maximum=0,audible=0;unsigned requestId=100;
 const auto audit=[&](const std::array<SignalChain,20>& routes){
  std::array<std::array<float,MeterWindow::size>,2> raw{};
  AudioScope guard;check(dry->copyLoopMeterWindow(0,raw[0])&&dry->copyLoopMeterWindow(19,raw[1]),"real dry per-track callback PCM");
  for(unsigned i=0;i<block;++i){constexpr float centre=.7071067811865475f;float expected=0;
   for(unsigned k=0;k<2;++k){const unsigned id=k?19:0;const float mono=raw[k][MeterWindow::size-block+i]/centre;float l=mono,r=mono;
    if(coordinated){for(char code:routes[id].text()){
     if(code=='D')oracle.tick(id,l,r,l,r);
     else if(code=='A')if(auto* graph=arpOracle.graph(id))check(graph->process(&l,&r,2,&l,&r,1),"independent coordinated Arp PCM");
    }}else oracle.tick(id,mono,mono,l,r);
    // Existing loop routing downmixes the completed chain before loop pan.
    expected+=(l+r)*.5f*centre;
   }
   maximum=std::max({maximum,std::abs(double(wl[i])-expected),std::abs(double(wr[i])-expected)});
   audible=std::max(audible,std::abs(double(wl[i])-dl[i]));
  }
 };
 const auto restore=[&](const LoopBank::DelayBank& targets){
  DelayBankRequest request;request.settings=targets;request.requestId=++requestId;request.generation=wet->generation();request.frame=wet->clock()+17;
  for(unsigned id=0;id<20;++id){request.clearEpochs[id]=wv.tracks[id].clearEpoch;request.settingsRevisions[id]=wv.tracks[id].delaySettingsRevision;}
  check(oracle.requestBank(targets,120),"prepare independent bank");for(const auto& settings:targets)if(settings.enabled)oracle.serviceWorker();
  ArpBankRequest joint;joint.requestId=request.requestId;joint.generation=request.generation;joint.restoreEffects=true;joint.effects.restoreDelays=true;joint.effects.delays=targets;
  for(unsigned id=0;id<20;++id){joint.clearEpochs[id]=wv.tracks[id].clearEpoch;joint.settingsRevisions[id]=wv.tracks[id].arpSettingsRevision;joint.effects.delayRevisions[id]=request.settingsRevisions[id];
   ArpSettings controls;controls.enabled=targets[id].enabled||(coordinated&&request.requestId==102&&id==19);controls.values[0]=.01;controls.values[2]=.35;controls.values[9]=0;
   check(compileArpSettings(controls,120,request.requestId+id,joint.settings[id]),"coordinated audio Arp settings");
   joint.effects.chains[id]=normalizedSignalChain(request.requestId%2?"AD":"DA");
  }
  std::uint64_t transaction=0;
  const bool hold=coordinated&&request.requestId==102;unsigned heldCallbacks=0;bool released=false;
  if(coordinated){check(arpOracle.requestBank(joint.settings),"prepare independent Arp bank");for(const auto& settings:joint.settings)if(settings.enabled)arpOracle.serviceWorker();
   if(hold){pauseEntered=false;releasePause=false;pauseAt=3;} // A0, D0 ready; hold A19 allocation.
   check(wet->enqueueArpBank(joint),"coordinated actual bank request");}
  else check(wet->enqueueDelayBank(request,transaction),"ordered actual bank request");
  bool ready=false;unsigned settled=0;const unsigned settleBlocks=static_cast<unsigned>(std::ceil(rate*.025/block))+1;
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(!ready||settled<settleBlocks){const auto begin=render(false);DelayBankResult result;
   bool committed=false;
   if(coordinated){ArpBankResult receipt;if(wet->popArpBank(receipt)){
    check(!ready&&receipt.status==ArpPreparationStatus::ready&&receipt.includesDelays&&receipt.requestId==request.requestId&&receipt.callbackSerial==wv.callbackClock.serial,"coordinated receipt belongs to exact callback");
    for(unsigned id=0;id<20;++id)check(receipt.settingsRevisions[id]==joint.settingsRevisions[id]+1&&receipt.delaySettingsRevisions[id]==joint.effects.delayRevisions[id]+1&&receipt.clearEpochs[id]==joint.clearEpochs[id],"coordinated receipt owns both full banks");
    {AudioScope guard;check(arpOracle.poll(begin).bankInstalled,"independent Arp adopts at shared sample");}committed=true;
   }}else if(wet->popDelayBank(result)){
    check(!ready&&result.status==DelayEditStatus::ready&&result.transaction==transaction&&result.requestId==request.requestId
      &&result.frame==begin&&result.frame>request.frame,"whole bank receipt at one callback boundary");
    for(unsigned id=0;id<20;++id)check(result.settingsRevisions[id]==request.settingsRevisions[id]+1&&result.clearEpochs[id]==request.clearEpochs[id],"receipt owns all twenty target revisions");
    committed=true;
   }
   if(committed){{AudioScope guard;check(oracle.poll(120).bankInstalled,"reference adopts complete bank at same sample");}ready=true;}
   bool old=true,next=true;
   for(unsigned id=0;id<20;++id){old&=wv.tracks[id].delay==accepted[id];next&=wv.tracks[id].delay==targets[id];
    if(coordinated){old&=wv.tracks[id].arp==acceptedArps[id]&&wv.tracks[id].signalChain==acceptedChains[id];next&=wv.tracks[id].arp==joint.settings[id]&&wv.tracks[id].signalChain==joint.effects.chains[id];}
   }
   check(old||next,"audio snapshot never contains a partially committed bank");
   check(ready?next:old,"target visibility agrees with exact bank receipt");
   audit(coordinated&&ready?joint.effects.chains:acceptedChains);
   if(hold&&pauseEntered.load(std::memory_order_acquire)&&!released){
    check(!ready,"ready D cannot commit while final Arp allocation is paused");
    if(++heldCallbacks==20){releasePause.store(true,std::memory_order_release);released=true;}
   }
   for(unsigned id:{0u,19u})check(dv.tracks[id].waveformId==wv.tracks[id].waveformId&&dv.tracks[id].length==wv.tracks[id].length
    &&dv.tracks[id].position==wv.tracks[id].position&&dv.tracks[id].state==wv.tracks[id].state&&dv.tracks[id].undoCount==wv.tracks[id].undoCount,"bank restore leaves both source clips/transport/history untouched");
   if(ready)++settled;else std::this_thread::sleep_for(std::chrono::milliseconds(1));
   check(std::chrono::steady_clock::now()<deadline,"continuous bank rendering deadline");
  }
  accepted=targets;
  if(hold)check(released&&heldCallbacks==20,"twenty audible old-owner callbacks while D is ready and Arp waits");
  if(coordinated){acceptedArps=joint.settings;acceptedChains=joint.effects.chains;}
 };
 LoopBank::DelayBank first{};first[0].enabled=true;first[0].values[0]=.01;first[0].values[3]=.7;
 first[19].enabled=true;first[19].values[0]=.025;first[19].values[3]=.6;check(setDelayRepeats(first[19],DelayRepeatMode::exact,3),"first bank exact count");restore(first);
 if(coordinated){
  ArpBankRequest failed;failed.requestId=9000;failed.generation=wet->generation();failed.settings=acceptedArps;failed.restoreEffects=true;failed.effects.restoreDelays=true;failed.effects.delays=accepted;
  for(unsigned id=0;id<20;++id){failed.clearEpochs[id]=wv.tracks[id].clearEpoch;failed.settingsRevisions[id]=wv.tracks[id].arpSettingsRevision;failed.effects.delayRevisions[id]=wv.tracks[id].delaySettingsRevision;
   failed.settings[id].seed+=17;failed.effects.chains[id]=normalizedSignalChain("QCADTFO");failed.effects.equalizers[id].enabled=true;failed.effects.equalizers[id].values[4]=12;
   failed.effects.compressors[id].enabled=true;failed.effects.compressors[id].values[0]=-40;failed.effects.drives[id][2].enabled=true;
  }
  check(wet->enqueueArpBank(failed),"audible rollback candidate");failNext=true;
  bool terminal=false;unsigned after=0;const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(!terminal||after<8){render(false);ArpBankResult receipt;if(wet->popArpBank(receipt)){
    check(!terminal&&receipt.status==ArpPreparationStatus::failed&&receipt.includesDelays&&receipt.requestId==9000,"audible coordinated allocation failure");terminal=true;
    for(unsigned id=0;id<20;++id)check(receipt.settingsRevisions[id]==failed.settingsRevisions[id]&&receipt.delaySettingsRevisions[id]==failed.effects.delayRevisions[id],"failure receipt preserves both old revision banks");
   }
   for(unsigned id=0;id<20;++id){const auto& info=wv.tracks[id];check(info.arp==acceptedArps[id]&&info.delay==accepted[id]&&info.signalChain==acceptedChains[id]&&info.equalizer==EqSettings{}&&info.compressor==CompressorSettings{}&&info.drives==defaultDriveBank(),"failed allocation cannot partially apply any effect or chain");}
   audit(acceptedChains);if(terminal)++after;else std::this_thread::sleep_for(std::chrono::milliseconds(1));check(std::chrono::steady_clock::now()<deadline,"audible failure completion");
  }
  check(!failNext,"audible rollback exercised real worker allocation failure");
 }
 auto second=first;second[0].values[0]=.0113;check(setDelayRepeats(second[0],DelayRepeatMode::exact,2),"second bank exact count");second[19].enabled=false;restore(second);
 auto off=second;for(auto& settings:off)settings.enabled=false;restore(off);
 for(unsigned i=0;i<block;++i)check(wl[i]==dl[i]&&wr[i]==dr[i],"all-off bank settles to exact two-track dry output");
 if(maximum>=2.e-6||audible<=1.e-3)std::cerr<<"bank PCM coordinated="<<coordinated<<" rate="<<rate<<" block="<<block<<" maximum="<<maximum<<" audible="<<audible<<'\n';
 check(maximum<2.e-6&&audible>1.e-3,"nonzero multi-track Engine bank PCM oracle");
 std::cout<<"PASS "<<(coordinated?"coordinated Arp/D":"atomic D")<<" bank Engine PCM "<<rate<<" Hz / "<<block<<", maximum error "<<maximum<<", audible bank change "<<audible<<'\n';
}
int main(){for(double rate:{8000.,48000.})for(unsigned block:{64u,128u}){run(rate,block);run(rate,block,true);}}
