// SPDX-License-Identifier: AGPL-3.0-only
#include "core/LoopBank.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <numbers>
#include <string_view>
#include <thread>
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
// Connected B slice: prepared worker ownership, adoption acknowledgement, live
// edits, replacement crossfades, off-chain transition servicing, panic/clear.
// The oracle is an independent ReverbStorage graph fed the recovered source.
static void run(double rate,unsigned block){
 auto dry=std::make_unique<LoopBank>(),wet=std::make_unique<LoopBank>();
 for(auto* bank:{dry.get(),wet.get()}){bank->prepare(32768,0,rate);bank->setAutoPlay(false);check(bank->apply(LoopAction::record,0),"seed record");}
 for(unsigned n=0;n<4096;++n){const float input=static_cast<float>(.08*std::sin(n*.031)+.03*std::cos(n*.071));
  for(auto* bank:{dry.get(),wet.get()})bank->tick({input,input});}
 for(auto* bank:{dry.get(),wet.get()}){check(bank->apply(LoopAction::finish,0),"seed finish");bank->completeOfflineEdits();bank->beginBlock();}
 ReverbSettings settings;settings.enabled=true;settings.values[2]=.1;settings.values[1]=.7;
 ReverbStorage oracle;check(oracle.prepareStopped(rate),"oracle storage");
 const auto request=[&](unsigned token){const auto view=wet->view()[0];AudioScope scope;
  check(wet->requestReverb(0,settings,view.clearEpoch,view.reverbSettingsRevision,token)&&oracle.request(0,settings,0),"paired prepared request");};
 const auto adopt=[&](unsigned token){oracle.serviceWorker();
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(wet->view()[0].reverbRequest!=token&&std::chrono::steady_clock::now()<deadline){
   {AudioScope scope;wet->beginBlock();}std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  check(wet->view()[0].reverbRequest==token,"worker installation acknowledgement");
  {AudioScope scope;check(oracle.poll().installed==1,"oracle adoption");}
 };
 // Mirrors LoopBank's disabled-metadata rule: a live set keeps the active
 // graph's prepared shape and only ramps its enabled flag to bypass.
 const auto oracleLive=[&](const ReverbSettings& s){
  auto runtime=s;
  if(!s.enabled&&oracle.graph(0)){runtime=oracle.graph(0)->settings();runtime.enabled=false;}
  return oracle.set(0,runtime);
 };
 // Prepared requests validate settings and ownership identity before any work.
 {const auto view=wet->view()[0];auto off=settings;off.enabled=false;AudioScope scope;
  check(!wet->requestReverb(0,off,view.clearEpoch,view.reverbSettingsRevision,98),"disabled prepared request rejected");
  check(!wet->requestReverb(0,settings,view.clearEpoch+1,view.reverbSettingsRevision,98),"stale prepared epoch rejected");
  check(!wet->requestReverb(0,settings,view.clearEpoch,view.reverbSettingsRevision+1,98),"stale prepared revision rejected");
  check(!wet->reverbStorageView(0).active&&!wet->reverbStorageView(0).pending,"rejected request allocates nothing");}
 request(1);adopt(1);
 auto info=wet->view()[0];check(info.reverb==settings&&info.reverbSettingsRevision==1,"settings commit only at matching adoption");
 {AudioScope scope;check(!wet->setReverb(0,settings,info.clearEpoch+1,info.reverbSettingsRevision,2)
  &&!wet->setReverb(0,settings,info.clearEpoch,0,2),"stale epoch/revision rejected");}
 for(auto* bank:{dry.get(),wet.get()})check(bank->apply(LoopAction::play,0),"seed play");
 // The default chain already contains B, so this is a live narrowing without a
 // runtime reset. The oracle therefore keeps its adoption fade untouched.
 check(wet->setSignalChain(0,normalizedSignalChain("B"),3,info.clearEpoch),"B chain");
 constexpr float centre=.7071067811865475f;
 double error=0,difference=0;
 const auto render=[&](unsigned frames,bool bypass=false){
  for(unsigned base=0;base<frames;base+=block){AudioScope scope;dry->beginBlock();wet->beginBlock();
   for(unsigned i=0;i<block;++i){const auto input=dry->tick({});const auto actual=wet->tick({});
    // The fixture records identical stereo channels, so the dry downmix/pan can
    // be inverted exactly to recover the chain input the wet bank reverb sees.
    float l=input.left/centre,r=input.right/centre,ol=0,or_=0;oracle.tick(0,l,r,ol,or_);
    const float expected=bypass?input.left:(ol+or_)*.5f*centre;
    const float expectedRight=bypass?input.right:(ol+or_)*.5f*centre;
    error=std::max({error,std::abs(double(actual.left)-expected),std::abs(double(actual.right)-expectedRight)});
    difference=std::max(difference,std::abs(double(actual.left)-input.left));
   }
  }
  const auto a=dry->view()[0],b=wet->view()[0];
  check(a.length==b.length&&a.position==b.position&&a.state==b.state&&a.waveformId==b.waveformId&&a.undoCount==b.undoCount,"B does not mutate source/history/transport");
 };
 render(4096);
 info=wet->view()[0];settings.values[1]=.45;
 {AudioScope scope;check(wet->setReverb(0,settings,info.clearEpoch,info.reverbSettingsRevision,4),"live mix edit");
  check(oracle.set(0,settings),"live oracle mix edit");}
 render(2048);
 // A shape replacement allocates on the worker and crossfades from the old graph.
 settings.values[2]=.25;request(5);adopt(5);
 info=wet->view()[0];check(info.reverb==settings,"replacement settings adopted");
 render(4096);
 {AudioScope scope;dry->resetTimeline();wet->resetTimeline();oracle.reset(0);}
 render(2048);
 // Disable smoothing snaps to exact bypass; then a re-enable ramps back in.
 info=wet->view()[0];settings.enabled=false;
 {AudioScope scope;check(wet->setReverb(0,settings,info.clearEpoch,info.reverbSettingsRevision,6),"live disable");
  check(oracle.set(0,settings),"oracle disable");}
 const auto settle=((static_cast<unsigned>(std::ceil(.01*rate*28))+block-1)/block)*block;
 render(settle);render(4096,true);
 settings.enabled=true;
 {AudioScope scope;check(wet->setReverb(0,settings,info.clearEpoch,wet->view()[0].reverbSettingsRevision,7),"live enable");
  check(oracle.set(0,settings),"oracle enable");}
 render(2048);
 // Remove B while a replacement crossfade is still running. The hidden
 // transition must finish without leaking reverb PCM or stranding its owner.
 settings.values[2]=.4;request(10);adopt(10);
 render(block);
 check(wet->reverbStorageView(0).transitioning,"removal fixture keeps a live transition");
 info=wet->view()[0];
 {AudioScope scope;check(wet->setSignalChain(0,normalizedSignalChain("T"),8,info.clearEpoch),"remove B");}
 const auto fade=static_cast<unsigned>(std::ceil(.02*rate));
 const unsigned hiddenFrames=((fade+block-1)/block+1)*block;
 {AudioScope scope;for(unsigned base=0;base<hiddenFrames;base+=block){dry->beginBlock();wet->beginBlock();
  for(unsigned n=0;n<block;++n){const auto input=dry->tick({});const auto actual=wet->tick({});
   check(std::abs(actual.left-input.left)<2.e-6&&std::abs(actual.right-input.right)<2.e-6,"removed B cannot leak transition PCM");
   if(oracle.view(0).transitioning){float l=0,r=0;oracle.tick(0,0,0,l,r);}
  }
 }}
 check(!wet->reverbStorageView(0).transitioning&&!oracle.view(0).transitioning,"off-chain transition cannot strand owner");
 info=wet->view()[0];
 {AudioScope scope;check(wet->setSignalChain(0,normalizedSignalChain("TB"),9,info.clearEpoch),"reinsert B");oracle.reset(0);}
 render(2048);
 // A live shape edit needs worker preparation; disabled metadata may select a
 // shape without allocating, but a later enable still needs that preparation.
 info=wet->view()[0];auto shaped=settings;shaped.values[2]=.55;
 {AudioScope scope;
  check(!wet->setReverb(0,shaped,info.clearEpoch,info.reverbSettingsRevision,15),"live shape edit requires preparation");
  check(wet->view()[0].reverb==settings,"rejected live shape preserves metadata");
  shaped.enabled=false;
  check(wet->setReverb(0,shaped,info.clearEpoch,info.reverbSettingsRevision,16),"disabled shape metadata accepted");
  check(wet->view()[0].reverb==shaped,"disabled shape metadata committed");
  check(oracleLive(shaped),"oracle disabled shape metadata");
  auto enabled=shaped;enabled.enabled=true;
  check(!wet->setReverb(0,enabled,info.clearEpoch,wet->view()[0].reverbSettingsRevision,17),"enabling a changed shape requires preparation");
  check(wet->view()[0].reverb==shaped,"rejected enable preserves disabled metadata");}
 // A prepared request is the only way to deliver the changed shape; once
 // installed, live editing continues against it.
 settings=shaped;settings.enabled=true;
 request(18);adopt(18);
 info=wet->view()[0];check(info.reverb==settings,"prepared request delivers the shape live editing refused");
 render(4096);
 check(error<2.e-6&&difference>1.e-3,"connected B PCM oracle and audible wet difference");
 // A fresh value invalidates an older not-yet-adopted worker replacement.
 info=wet->view()[0];auto pending=settings;pending.enabled=true;pending.values[2]=.6;
 {AudioScope scope;check(wet->requestReverb(0,pending,info.clearEpoch,info.reverbSettingsRevision,12)&&oracle.request(0,pending,0),"pending replacement");
  settings.values[1]=.25;check(wet->setReverb(0,settings,info.clearEpoch,info.reverbSettingsRevision,13),"newer edit cancels pending replacement");
  check(oracleLive(settings),"oracle live edit");oracle.cancel(0);}
 for(unsigned n=0;n<32;++n){{AudioScope scope;wet->beginBlock();}std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 check(wet->view()[0].reverbRequest==13&&wet->view()[0].reverb==settings&&!wet->reverbStorageView(0).pending,"stale completion cannot overwrite live edit");
 {AudioScope scope;check(wet->apply(LoopAction::stopAll,0),"panic");
  for(unsigned n=0;n<2048;++n){const auto out=wet->tick({});check(out.left==0&&out.right==0,"STOP source gate no reverb resurrection");}}
 info=wet->view()[0];check(info.reverb==settings,"panic keeps settings");
 check(wet->reverbStorageView(0).active&&!wet->reverbStorageView(0).transitioning,"panic retains the prepared owner without a live fade");
 {AudioScope scope;check(wet->requestReverb(0,pending,info.clearEpoch,info.reverbSettingsRevision,14),"pre-clear worker request");
  check(wet->apply(LoopAction::clear,0),"clear");}
 for(unsigned n=0;n<32;++n){{AudioScope scope;wet->beginBlock();}std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 info=wet->view()[0];check(info.reverb==ReverbSettings{}&&info.clearEpoch!=0&&!wet->reverbStorageView(0).active&&!wet->reverbStorageView(0).pending,"DEL invalidates prepared work and resets B");
 std::cout<<"PASS LoopBank B "<<rate<<" Hz / "<<block<<" frames, max PCM error "<<error<<'\n';
}
// Independent stereo oracle. Two mono captures recover each conditioned source
// channel before the loop's stereo->mono merger, so a swapped/misrouted B
// channel shows up as a nonzero PCM difference at non-centre loop pans. The
// ReverbImpulse IRs differ per channel, so duplicate-channel feeding is caught.
static void stereo(double rate,unsigned block){
 auto left=std::make_unique<LoopBank>(),right=std::make_unique<LoopBank>(),wet=std::make_unique<LoopBank>();
 for(auto* bank:{left.get(),right.get(),wet.get()}){bank->prepare(16384,0,rate);bank->setAutoPlay(false);check(bank->apply(LoopAction::record,0),"stereo capture");}
 for(unsigned n=0;n<4096;++n){
  const float l=static_cast<float>(.08*std::sin(n*.031)+.03*std::cos(n*.071));
  const float r=static_cast<float>(.05*std::cos(n*.047)-.02*std::sin(n*.083));
  left->tick({l,l});right->tick({r,r});wet->tick({l,r});
 }
 for(auto* bank:{left.get(),right.get(),wet.get()}){check(bank->apply(LoopAction::finish,0),"stereo finish");bank->completeOfflineEdits();bank->beginBlock();}
 ReverbSettings settings;settings.enabled=true;settings.values[2]=.1;settings.values[1]=.6;settings.values[5]=.011;
 const auto before=wet->view()[0];
 ReverbStorage oracle;check(oracle.prepareStopped(rate),"stereo oracle storage");
 check(wet->requestReverb(0,settings,before.clearEpoch,before.reverbSettingsRevision,1)&&oracle.request(0,settings,0),"stereo prepared request");
 oracle.serviceWorker();
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 while(wet->view()[0].reverbRequest!=1){
  {AudioScope scope;wet->beginBlock();}check(std::chrono::steady_clock::now()<deadline,"stereo owner deadline");
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
 }
 {AudioScope scope;check(oracle.poll().installed==1,"stereo oracle adoption");}
 // Public timeline reset gives a known source/history origin and ends the
 // prepared-owner fade; no private DSP state or fixture snapshot is injected.
 std::vector<Stereo> input(16384);double maximum=0,audible=0;
 constexpr float centre=.7071067811865475f;
 for(float pan:{0.f,2.f,5.f,8.f,10.f}){
  {AudioScope scope;for(auto* bank:{left.get(),right.get(),wet.get()}){bank->resetTimeline();check(bank->apply(LoopAction::play,0),"stereo playback");}
   check(wet->apply(LoopAction::pan,0,pan),"stereo loop pan");oracle.reset(0);}
  const double angle=pan*std::numbers::pi/20;
  for(unsigned base=0;base<input.size();base+=block){AudioScope scope;left->beginBlock();right->beginBlock();wet->beginBlock();
   for(unsigned i=0;i<block;++i){const unsigned n=base+i;
    // Recover each raw pre-merger channel from its own mono capture.
    input[n]={left->tick({}).left/centre,right->tick({}).left/centre};
    float ol=0,or_=0;oracle.tick(0,input[n].left,input[n].right,ol,or_);
    const double mono=(ol+or_)*.5;
    const auto actual=wet->tick({});
    const double expectedLeft=pan==10?0:mono*std::cos(angle),expectedRight=pan==0?0:mono*std::sin(angle);
    maximum=std::max({maximum,std::abs(actual.left-expectedLeft),std::abs(actual.right-expectedRight)});
    audible=std::max(audible,std::abs(mono));
    check(std::isfinite(actual.left)&&std::isfinite(actual.right),"finite stereo B output");
    if(pan==0)check(actual.right==0,"hard-left loop pan remains silent on right");
    if(pan==10)check(actual.left==0,"hard-right loop pan remains silent on left");
   }
  }
  const auto after=wet->view()[0];check(after.length==before.length&&after.waveformId==before.waveformId&&after.undoCount==before.undoCount,"stereo B preserves immutable PCM/history");
  {AudioScope scope;for(auto* bank:{left.get(),right.get(),wet.get()})check(bank->apply(LoopAction::stopImmediate,0),"stereo stop");}
 }
 check(maximum<2.e-6&&audible>.01,"independent stereo B/loop-pan PCM");
 std::cout<<"PASS stereo LoopBank B "<<rate<<" Hz / "<<block<<", max error "<<maximum<<'\n';
}
int main(int argc,char** argv){
 if(argc==4&&std::string_view(argv[1])=="--stereo"){
  const double rate=std::stod(argv[2]);const auto block=static_cast<unsigned>(std::stoul(argv[3]));
  check((rate==8000||rate==48000)&&(block==64||block==128),"stereo fixture rate/block");
  stereo(rate,block);return 0;
 }
 check(argc==1,"connected fixture arguments");
 for(double rate:{8000.,48000.})for(unsigned block:{64u,128u})run(rate,block);
}
