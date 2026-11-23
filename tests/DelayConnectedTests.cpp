// SPDX-License-Identifier: AGPL-3.0-only
#include "core/LoopBank.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>
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
// Independent first-echo oracle. Neither Delay/DelayStorage nor the production
// panner is called here. Feedback=0 (or exact count=1) deliberately isolates
// fractional reads, wet-only stereo panning and the original loop downmix/pan.
static Stereo stereoPan(Stereo input,double pan){
 if(pan<=0){const double angle=(pan+1)*std::numbers::pi/2;
  return {static_cast<float>(input.left+input.right*std::cos(angle)),static_cast<float>(input.right*std::sin(angle))};}
 const double angle=pan*std::numbers::pi/2;
 return {static_cast<float>(input.left*std::cos(angle)),static_cast<float>(input.right+input.left*std::sin(angle))};
}
static Stereo echo(const std::vector<Stereo>& input,unsigned frame,double delay){
 const auto whole=static_cast<unsigned>(delay);const double fraction=delay-whole;
 const auto get=[&](unsigned age){return frame>=age?input[frame-age]:Stereo{};};
 const auto a=get(whole),b=get(whole+1);
 return {static_cast<float>(a.left+(b.left-a.left)*fraction),static_cast<float>(a.right+(b.right-a.right)*fraction)};
}
static void stereo(double rate,unsigned block,bool exact){
 auto left=std::make_unique<LoopBank>(),right=std::make_unique<LoopBank>(),wet=std::make_unique<LoopBank>();
 for(auto* bank:{left.get(),right.get(),wet.get()}){bank->prepare(16384,0,rate);bank->setAutoPlay(false);check(bank->apply(LoopAction::record,0),"stereo capture");}
 for(unsigned n=0;n<4096;++n){
  const float l=static_cast<float>(.08*std::sin(n*.031)+.03*std::cos(n*.071));
  const float r=static_cast<float>(.05*std::cos(n*.047)-.02*std::sin(n*.083));
  left->tick({l,l});right->tick({r,r});wet->tick({l,r});
 }
 for(auto* bank:{left.get(),right.get(),wet.get()}){check(bank->apply(LoopAction::finish,0),"stereo finish");bank->completeOfflineEdits();bank->beginBlock();}
 DelaySettings settings;settings.enabled=true;settings.values={.01017,0,3000,.63,5,1,0};
 if(exact)check(setDelayRepeats(settings,DelayRepeatMode::exact,1),"stereo exact one echo");
 const auto before=wet->view()[0];
 check(wet->requestDelay(0,settings,before.clearEpoch,before.delaySettingsRevision,1),"stereo prepared request");
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 while(wet->view()[0].delayRequest!=1){
  {AudioScope scope;wet->beginBlock();}check(std::chrono::steady_clock::now()<deadline,"stereo owner deadline");std::this_thread::sleep_for(std::chrono::milliseconds(1));
 }
 // Public timeline reset gives a known oscillator/history origin and ends the
 // prepared-owner fade; no private DSP state or fixture snapshot is injected.
 std::vector<Stereo> input(16384);double maximum=0,audible=0;
 constexpr float centre=.7071067811865475f;
 for(float pan:{0.f,2.f,5.f,8.f,10.f}){
  {AudioScope scope;for(auto* bank:{left.get(),right.get(),wet.get()}){bank->resetTimeline();check(bank->apply(LoopAction::play,0),"stereo playback");}check(wet->apply(LoopAction::pan,0,pan),"stereo loop pan");}
  const double angle=pan*std::numbers::pi/20;
  for(unsigned base=0;base<input.size();base+=block){AudioScope scope;left->beginBlock();right->beginBlock();wet->beginBlock();
   for(unsigned i=0;i<block;++i){const unsigned n=base+i;
    // Two separate mono captures recover each conditioned source channel before
    // the loop's final stereo->mono merger. Unequal/anti-correlated input is not
    // reconstructible from a single already-downmixed dry render.
    input[n]={left->tick({}).left/centre,right->tick({}).left/centre};
    const auto delayed=echo(input,n,settings.values[0]*rate);
    const auto panned=stereoPan(delayed,std::sin(2*std::numbers::pi*settings.values[4]*n/rate)*settings.values[5]);
    const double mono=((1-settings.values[3])*(input[n].left+input[n].right)+settings.values[3]*(panned.left+panned.right))*.5;
    const auto actual=wet->tick({});
    const double expectedLeft=pan==10?0:mono*std::cos(angle),expectedRight=pan==0?0:mono*std::sin(angle);
    maximum=std::max({maximum,std::abs(actual.left-expectedLeft),std::abs(actual.right-expectedRight)});
    audible=std::max(audible,std::abs(mono));
    check(std::isfinite(actual.left)&&std::isfinite(actual.right),"finite stereo Delay output");
    if(pan==0)check(actual.right==0,"hard-left loop pan remains silent on right");
    if(pan==10)check(actual.left==0,"hard-right loop pan remains silent on left");
   }
  }
  const auto after=wet->view()[0];check(after.length==before.length&&after.waveformId==before.waveformId&&after.undoCount==before.undoCount,"stereo processing preserves immutable PCM/history");
  {AudioScope scope;for(auto* bank:{left.get(),right.get(),wet.get()})check(bank->apply(LoopAction::stopImmediate,0),"stereo stop");}
 }
 check(maximum<2.e-6&&audible>.01,"independent moving wet-pan/fractional-echo/loop-pan PCM");
 std::cout<<"PASS stereo LoopBank D "<<rate<<" Hz / "<<block<<" / "<<(exact?"exact1":"feedback0")<<", max error "<<maximum<<'\n';
}
int main(int argc,char** argv){
 if(argc==4&&std::string_view(argv[1])=="--stereo"){
  const double rate=std::stod(argv[2]);const auto block=static_cast<unsigned>(std::stoul(argv[3]));
  check((rate==8000||rate==48000)&&(block==64||block==128),"stereo fixture rate/block");
  for(bool exact:{false,true})stereo(rate,block,exact);
  return 0;
 }
 check(argc==1,"connected fixture arguments");
 for(double rate:{8000.,48000.})for(unsigned block:{64u,128u})run(rate,block);
}
