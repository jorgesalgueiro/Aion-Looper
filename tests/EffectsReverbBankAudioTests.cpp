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
std::atomic<unsigned> failAt{0};
void check(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
struct AudioScope {AudioScope(){operations=0;watched=true;}~AudioScope(){watched=false;check(!operations,"heap operation in coordinated B PCM path");}};
}
void* operator new(std::size_t n){if(watched)++operations;
 if(failAt.load()&&failAt.fetch_sub(1)==1)throw std::bad_alloc();
 if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
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
  check(engine->enqueue({0,1})&&engine->enqueueLoop({LoopAction::record,0,1,engine->generation()}),"joint B PCM capture");}
 const auto render=[&](bool capture){const auto begin=dry->clock();
  for(unsigned i=0;i<block;++i)dl[i]=dr[i]=wl[i]=wr[i]=capture?static_cast<float>(.08*std::sin((begin+i)*.071)+.02*std::cos((begin+i)*.019)):0;
  {AudioScope scope;dry->process(d,2,block);wet->process(w,2,block);}
  check(dry->clock()==wet->clock(),"paired B Engine sample clocks");return begin;
 };
 for(unsigned n=0;n<4096/block;++n)render(true);
 for(auto* engine:{dry.get(),wet.get()})check(engine->enqueueLoop({LoopAction::finish,0,2,engine->generation()}),"finish B capture");
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
 do{render(false);check(std::chrono::steady_clock::now()<deadline,"B clip finalization timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 while(dry->inspectAudioThread().track.state!=LoopState::stopped||wet->inspectAudioThread().track.state!=LoopState::stopped);
 for(auto* engine:{dry.get(),wet.get()})check(engine->enqueueLoop({LoopAction::play,0,3,engine->generation()}),"play captured B loop");
 for(unsigned n=0;n<512/block;++n)render(false);
 ReverbStorage oracle;check(oracle.prepareStopped(rate),"independent Reverb storage");
 const auto snapshotView=[&]{EngineView current{},next{};while(wet->popView(next))current=next;return current;};
 const auto target=[&](unsigned variant,bool on){ReverbSettings s;s.enabled=on;s.values[1]=.5;s.values[2]=.1+.1*variant;s.values[3]=1.2+.1*variant;s.values[4]=8000;s.values[5]=.02;return s;};
 const auto joint=[&](unsigned id,const ReverbSettings& s){ArpBankRequest q;q.requestId=id;q.generation=wet->generation();q.restoreEffects=true;q.effects.restoreReverbs=true;
  const auto view=snapshotView();
  for(unsigned t=0;t<20;++t){const auto& v=view.tracks[t];
   q.clearEpochs[t]=v.clearEpoch;q.settingsRevisions[t]=v.arpSettingsRevision;q.effects.reverbRevisions[t]=v.reverbSettingsRevision;
   ArpSettings a;a.enabled=false;a.values[0]=.01;check(compileArpSettings(a,120,id+t,q.settings[t]),"B joint Arp descriptor");
   q.effects.equalizers[t]=v.equalizer;q.effects.compressors[t]=v.compressor;q.effects.chains[t]=v.signalChain;
   q.effects.reverbs[t]=(t==0?s:ReverbSettings{});
  }return q;};
 double error=0,audible=0;bool active=false;
 const auto compare=[&](std::uint64_t begin){
  for(unsigned i=0;i<block;++i){constexpr float centre=.7071067811865475f;float l=dl[i]/centre,r=dr[i]/centre;
   {AudioScope scope;check(oracle.tick(0,l,r,l,r),"independent B tick");}
   const float expected=(l+r)*.5f*centre;
   error=std::max({error,std::abs(double(wl[i])-expected),std::abs(double(wr[i])-expected)});
   audible=std::max(audible,std::abs(double(wl[i])-dl[i]));
  }
  const auto a=dry->inspectAudioThread().track,b=wet->inspectAudioThread().track;
  check(a.length==b.length&&a.position==b.position&&a.state==b.state&&a.undoCount==b.undoCount&&a.waveformId==b.waveformId,"joint B commit preserves PCM/transport/history");
  (void)begin;
 };
 const auto applyBank=[&](unsigned id,const ReverbSettings& s){
  ReverbSettings targetSettings=s;ReverbStorage::Bank bank{};for(unsigned t=0;t<20;++t)bank[t]=(t==0?targetSettings:ReverbSettings{});
  check(oracle.requestBank(bank),"prepare independent B bank");
  for(unsigned n=0;n<64&&oracle.bankStatus()!=ReverbStorage::BankStatus::ready;++n)oracle.serviceWorker();
  check(oracle.bankStatus()==ReverbStorage::BankStatus::ready,"independent B bank ready");
  const auto q=joint(id,s);check(wet->enqueueArpBank(q),"coordinated B bank request");
  bool sawReceipt=false;unsigned settled=0;const unsigned settleBlocks=static_cast<unsigned>(rate*.03)/block+1;
  const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);
  for(unsigned n=0;!sawReceipt||settled<settleBlocks;++n){
   const auto begin=render(false);ArpBankResult receipt;
   if(wet->popArpBank(receipt)){
    check(!sawReceipt&&receipt.status==ArpPreparationStatus::ready&&receipt.includesReverbs&&receipt.requestId==id,"coordinated B receipt");
    for(unsigned t=0;t<20;++t)check(receipt.reverbSettingsRevisions[t]==q.effects.reverbRevisions[t]+1,"B receipt owns all revisions");
    {AudioScope scope;const auto completion=oracle.poll();check(completion.bankInstalled,"independent B adopts complete bank");}
    active=true;sawReceipt=true;
   }
   compare(begin);
   if(sawReceipt)++settled;
   check(std::chrono::steady_clock::now()<until,"B bank PCM deadline");
   if(!sawReceipt)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return targetSettings;
 };
 applyBank(11,target(1,true));
 applyBank(12,target(2,true));
 // Worker OOM must fail the joint bank and leave the accepted B owner and audio.
 ReverbStorage::Bank pending{};check(oracle.requestBank(pending),"prepare failing independent bank");
 for(unsigned n=0;n<64&&oracle.bankStatus()!=ReverbStorage::BankStatus::ready;++n)oracle.serviceWorker();
 const auto failed=joint(9000,target(3,true));failAt=1;check(wet->enqueueArpBank(failed),"audible B rollback candidate");
 bool terminal=false;const auto deadline2=std::chrono::steady_clock::now()+std::chrono::seconds(10);
 for(unsigned n=0;!terminal;++n){const auto begin=render(false);ArpBankResult receipt;
  if(wet->popArpBank(receipt)){check(receipt.status==ArpPreparationStatus::failed&&receipt.requestId==9000,"audible B allocation failure");terminal=true;}
  compare(begin);check(std::chrono::steady_clock::now()<deadline2,"B rollback deadline");
  if(!terminal)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 }
 check(failAt.exchange(0)==0,"B worker allocation failure consumed");
 {AudioScope scope;oracle.cancelBank();}
 check(wet->inspectAudioThread(0).track.reverb==target(2,true),"failed joint bank retains committed B");
 check(error<2.e-6&&audible>1.e-3,"actual coordinated B Engine audio matches the independent Reverb bank");
 std::cout<<"PASS coordinated B PCM "<<rate<<" Hz / "<<block<<", maximum error "<<error<<", audible change "<<audible<<'\n';
}
int main(){for(double rate:{8000.,48000.})for(unsigned block:{64u,128u})run(rate,block);}
