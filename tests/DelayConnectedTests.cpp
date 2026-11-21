// SPDX-License-Identifier: AGPL-3.0-only
#include "core/LoopBank.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
namespace {
thread_local bool watched=false;thread_local unsigned operations=0;
void check(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
struct AudioScope {AudioScope(){operations=0;watched=true;}~AudioScope(){watched=false;check(!operations,"allocation/deallocation on connected audio owner");}};
}
void* operator new(std::size_t n){if(watched)++operations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watched&&p)++operations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace aion;
static void run(double rate,unsigned block){
 auto dry=std::make_unique<LoopBank>(),wet=std::make_unique<LoopBank>();
 for(auto* bank:{dry.get(),wet.get()}){bank->prepare(32768,0,rate);bank->setAutoPlay(false);check(bank->apply(LoopAction::record,0),"seed record");}
 for(unsigned n=0;n<4096;++n){const float input=static_cast<float>(.08*std::sin(n*.031)+.03*std::cos(n*.071));
  for(auto* bank:{dry.get(),wet.get()})bank->tick({input,input});}
 for(auto* bank:{dry.get(),wet.get()}){check(bank->apply(LoopAction::finish,0),"seed finish");bank->completeOfflineEdits();bank->beginBlock();}
 DelaySettings settings;settings.enabled=true;settings.values[0]=.01;settings.values[3]=.7;settings.values[6]=1;
 double currentBpm=120;
 DelayStorage oracle;check(oracle.prepareStopped(rate),"oracle storage");
 const auto request=[&](unsigned token){const auto view=wet->view()[0];AudioScope scope;
  check(wet->requestDelay(0,settings,view.clearEpoch,view.delaySettingsRevision,token)&&oracle.request(0,settings,currentBpm),"paired prepared request");};
 const auto adopt=[&](unsigned token){oracle.serviceWorker();
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(wet->view()[0].delayRequest!=token&&std::chrono::steady_clock::now()<deadline){
   {AudioScope scope;wet->beginBlock();}std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  check(wet->view()[0].delayRequest==token,"worker installation acknowledgement");
  {AudioScope scope;check(oracle.poll().installed==1,"oracle adoption");
   check(oracle.graph(0)->set(settings,currentBpm),"adoption uses latest tempo");oracle.graph(0)->reset();}
 };
 request(1);adopt(1);
 auto info=wet->view()[0];check(info.delay==settings&&info.delaySettingsRevision==1,"settings commit only at matching adoption");
 {AudioScope scope;check(!wet->setDelay(0,settings,info.clearEpoch+1,info.delaySettingsRevision,2)
  &&!wet->setDelay(0,settings,info.clearEpoch,0,2),"stale epoch/revision rejected");}
 for(auto* bank:{dry.get(),wet.get()})check(bank->apply(LoopAction::play,0),"seed play");
 check(wet->setSignalChain(0,normalizedSignalChain("DT"),3,info.clearEpoch),"D chain with initially bypassed T");
 Drive distortion;auto drives=defaultDriveBank();check(distortion.prepare(rate,DriveKind::distortion,drives[0]),"drive oracle");
 bool driveEnabled=false,driveFirst=false;double error=0,difference=0;
 const auto render=[&](unsigned frames){
  for(unsigned base=0;base<frames;base+=block){AudioScope scope;dry->beginBlock();wet->beginBlock();
   for(unsigned i=0;i<block;++i){const auto input=dry->tick({});const auto actual=wet->tick({});
    constexpr float centre=.7071067811865475f;float l=input.left/centre,r=input.right/centre;
    if(driveEnabled&&driveFirst){const auto d=distortion.tick(l,r);l=d[0];r=d[1];}
    oracle.tick(0,l,r,l,r);
    if(driveEnabled&&!driveFirst){const auto d=distortion.tick(l,r);l=d[0];r=d[1];}
    const float expected=(l+r)*.5f*centre;
    error=std::max({error,std::abs(double(actual.left)-expected),std::abs(double(actual.right)-expected)});
    difference=std::max(difference,std::abs(double(actual.left)-input.left));
   }
  }
  const auto a=dry->view()[0],b=wet->view()[0];
  check(a.length==b.length&&a.position==b.position&&a.state==b.state&&a.waveformId==b.waveformId&&a.undoCount==b.undoCount,"D does not mutate source/history/transport");
 };
 render(4096);
 check(setDelayRepeats(settings,DelayRepeatMode::exact,3),"exact replacement settings");request(4);
 currentBpm=90;SyncSettings sync;sync.bpm=currentBpm;
 {AudioScope scope;dry->configureSync(sync);wet->configureSync(sync);check(oracle.setTempo(0,currentBpm),"old owner tempo before replacement");}
 adopt(4);
 // Change BPM again while both old/new graphs are pinned in the crossfade.
 currentBpm=137;sync.bpm=currentBpm;
 {AudioScope scope;dry->configureSync(sync);wet->configureSync(sync);check(oracle.setTempo(0,currentBpm),"both transition-owner tempos");}
 render(4096);
 info=wet->view()[0];settings.values[3]=.45;
 {AudioScope scope;check(wet->setDelay(0,settings,info.clearEpoch,info.delaySettingsRevision,5),"live exact edit");
  check(oracle.graph(0)->set(settings,currentBpm),"live oracle edit");}
 render(2048);
 {AudioScope scope;dry->resetTimeline();wet->resetTimeline();currentBpm=120;
  check(oracle.setTempo(0,currentBpm),"timeline default tempo");oracle.reset(0);}
 render(2048);
 drives[0].enabled=true;driveEnabled=true;info=wet->view()[0];
 {AudioScope scope;check(wet->setDrives(0,drives,6,info.clearEpoch)&&distortion.set(drives[0]),"live distortion");
  check(wet->setSignalChain(0,normalizedSignalChain("DT"),7,info.clearEpoch),"D then T");}
 render(2048);driveFirst=true;
 {AudioScope scope;check(wet->setSignalChain(0,normalizedSignalChain("TD"),8,info.clearEpoch),"T then D live reorder");}
 render(2048);
 check(error<2.e-6&&difference>1.e-3,"connected D/mode replacement/user order PCM oracle");
 // Remove D while preparing a new mode. The hidden transition must finish
 // without leaking delayed audio or retaining its old allocation indefinitely.
 info=wet->view()[0];
 {AudioScope scope;check(wet->setSignalChain(0,normalizedSignalChain("T"),9,info.clearEpoch),"remove D");}
 check(setDelayRepeats(settings,DelayRepeatMode::feedback,1),"off-chain mode settings");request(10);adopt(10);
 const unsigned hiddenFrames=((static_cast<unsigned>(std::ceil(.02*rate))+block-1)/block+1)*block;
 {AudioScope scope;for(unsigned base=0;base<hiddenFrames;base+=block){dry->beginBlock();wet->beginBlock();
  for(unsigned n=0;n<block;++n){const auto input=dry->tick({});const auto actual=wet->tick({});
   constexpr float centre=.7071067811865475f;const auto d=distortion.tick(input.left/centre,input.right/centre);
   const float expected=(d[0]+d[1])*.5f*centre;
   check(std::abs(actual.left-expected)<2.e-6&&std::abs(actual.right-expected)<2.e-6,"removed D cannot leak transition PCM");
   if(oracle.view(0).transitioning){float l,r;oracle.tick(0,0,0,l,r);}
  }
 }}
 check(!wet->delayStorageView(0).transitioning&&!oracle.view(0).transitioning,"off-chain transition cannot strand owner");
 info=wet->view()[0];
 {AudioScope scope;check(wet->setSignalChain(0,normalizedSignalChain("TD"),11,info.clearEpoch),"reinsert D");oracle.reset(0,true);}
 render(2048);check(error<2.e-6,"reinserted D resets history with shared fade-in");
 // A fresh value invalidates an older not-yet-adopted worker replacement.
 info=wet->view()[0];auto pending=settings;check(setDelayRepeats(pending,DelayRepeatMode::exact,10),"large-count settings");
 {AudioScope scope;check(wet->requestDelay(0,pending,info.clearEpoch,info.delaySettingsRevision,12),"pending replacement");
  settings.values[3]=.25;check(wet->setDelay(0,settings,info.clearEpoch,info.delaySettingsRevision,13),"newer edit cancels pending replacement");}
 for(unsigned n=0;n<32;++n){{AudioScope scope;wet->beginBlock();}std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 check(wet->view()[0].delayRequest==13&&wet->view()[0].delay==settings&&!wet->delayStorageView(0).pending,"stale completion cannot overwrite live edit");
 {AudioScope scope;check(wet->apply(LoopAction::stopAll,0),"panic");
  for(unsigned n=0;n<2048;++n){const auto out=wet->tick({});check(out.left==0&&out.right==0,"STOP source gate no delayed resurrection");}}
 info=wet->view()[0];check(info.delay==settings,"panic keeps settings");
 {AudioScope scope;check(wet->requestDelay(0,pending,info.clearEpoch,info.delaySettingsRevision,14),"pre-clear worker request");
  check(wet->apply(LoopAction::clear,0),"clear");}
 for(unsigned n=0;n<32;++n){{AudioScope scope;wet->beginBlock();}std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 info=wet->view()[0];check(info.delay==DelaySettings{}&&info.clearEpoch!=0&&!wet->delayStorageView(0).active&&!wet->delayStorageView(0).pending,"DEL invalidates prepared work and resets D");
 std::cout<<"PASS LoopBank D "<<rate<<" Hz / "<<block<<" frames, max PCM error "<<error<<'\n';
}
int main(){for(double rate:{8000.,48000.})for(unsigned block:{64u,128u})run(rate,block);}
