// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
namespace {thread_local bool watching=false;unsigned allocations=0;
void check(bool ok,const char* s){if(!ok){std::cerr<<s<<'\n';std::exit(1);}}}
void* operator new(std::size_t n){if(watching)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{if(watching)++allocations;std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
void run(unsigned block){
 using namespace aion;auto dry=std::make_unique<Engine>(),wet=std::make_unique<Engine>();
 for(auto* e:{dry.get(),wet.get()}){e->prepare(48000,32768);e->setMonitoring(false);check(e->enqueue({0,1})&&e->enqueueLoop({LoopAction::record,0,1,e->generation()}),"Capture fixture");}
 std::array<float,128> dl{},dr{},wl{},wr{};float* d[]{dl.data(),dr.data()},*w[]{wl.data(),wr.data()};std::uint64_t frame=0;
 auto render=[&](bool capture){for(unsigned i=0;i<block;++i)dl[i]=dr[i]=wl[i]=wr[i]=capture?static_cast<float>(.08*std::sin((frame+i)*.131)+.03*std::cos((frame+i)*.027)):0;watching=true;dry->process(d,2,block,{},true);wet->process(w,2,block,{},true);watching=false;frame+=block;};
 for(unsigned n=0;n<8192/block;++n)render(true);
 for(auto* e:{dry.get(),wet.get()})check(e->enqueueLoop({LoopAction::finish,0,2,e->generation()}),"Finish capture");render(false);
 for(auto* e:{dry.get(),wet.get()})check(e->enqueueLoop({LoopAction::play,0,3,e->generation()}),"Play capture");
 for(unsigned n=0;n<2048/block;++n)render(false);
 auto bank=defaultDriveBank();std::array<Drive,3> independent;
 for(unsigned k=0;k<3;++k){check(independent[k].prepare(48000,static_cast<DriveKind>(k),bank[k]),"Prepare independent owner");bank[k].enabled=true;}
 const auto original=wet->inspectAudioThread().track;SignalChain previous;
 double maximum=0;
 LoopCommand enable{LoopAction::drive,0,4,wet->generation(),frame+17};enable.drives=bank;enable.driveClearEpoch=original.clearEpoch;check(wet->enqueueLoop(enable),"Sample-offset enable");const auto enabledAt=frame+17;
 for(const auto order:{"TFO","OFT","FTO","T","FO","OFT","QCATFODBVKZG"}){
  const auto next=normalizedSignalChain(order);const auto at=frame+17;LoopCommand c{LoopAction::signalChain,0,5,wet->generation(),at};c.signalChain=next;c.chainClearEpoch=original.clearEpoch;check(wet->enqueueLoop(c),"Reorder command");
  for(unsigned n=0;n<2048/block;++n){const auto begin=frame;render(false);
   for(unsigned i=0;i<block;++i){const auto now=begin+i;
    if(now==enabledAt)for(unsigned k=0;k<3;++k)check(independent[k].set(bank[k]),"Reference activation");
    if(now==at){for(unsigned k=0;k<3;++k)if(next.contains(driveCodes[k])&&!previous.contains(driveCodes[k]))check(independent[k].prepare(48000,static_cast<DriveKind>(k),bank[k]),"Reference reinsert");previous=next;}
    constexpr float centre=.7071067811865475f;float l=dl[i]/centre,r=dr[i]/centre;
    for(char code:previous.text())for(unsigned k=0;k<3;++k)if(code==driveCodes[k]){const auto y=independent[k].tick(l,r);l=y[0];r=y[1];}
    const auto expected=std::clamp((l+r)*.5f*centre,-1.f,1.f);maximum=std::max({maximum,std::abs(static_cast<double>(wl[i])-expected),std::abs(static_cast<double>(wr[i])-expected)});
   }
   const auto v=wet->inspectAudioThread().track;check(v.drives==bank&&v.waveformId==original.waveformId&&v.length==original.length&&v.undoCount==original.undoCount,"Effects preserve stored PCM/history");
  }
 }
 check(maximum<=1e-6,"Actual Engine PCM matches independent persistent ordered processors");
 // A stale GUI payload must not overwrite a different, newer control/family.
 auto first=bank;first[2].values[0]=14;auto second=bank;second[0].values[2]=3456;
 LoopCommand midiLike{LoopAction::drive,0,10,wet->generation(),frame+3};midiLike.drives=first;midiLike.driveMask=1u<<12;midiLike.driveClearEpoch=original.clearEpoch;
 LoopCommand guiLike{LoopAction::drive,0,11,wet->generation(),frame+4};guiLike.drives=second;guiLike.driveMask=1u<<2;guiLike.driveClearEpoch=original.clearEpoch;
 check(wet->enqueueLoop(midiLike)&&wet->enqueueLoop(guiLike),"Interleaved field edits");render(false);bank[2].values[0]=14;bank[0].values[2]=3456;check(wet->inspectAudioThread().track.drives==bank,"Masked edits preserve concurrent controls");
 LoopCommand stale{LoopAction::drive,0,7,wet->generation()};stale.drives=defaultDriveBank();stale.driveClearEpoch=original.clearEpoch+1;check(wet->enqueueLoop(stale),"Stale request reaches audio validation");render(false);check(wet->inspectAudioThread().track.drives==bank,"Stale request cannot overwrite settings");
 for(auto* e:{dry.get(),wet.get()})check(e->enqueueLoop({LoopAction::stopAll,0,8,e->generation()}),"Panic");
 bool originalTail=false;
 for(unsigned n=0;n<1024/block;++n){render(false);for(unsigned i=0;i<block;++i){
  // Engine's existing five-millisecond source fade is 240 samples at 48 kHz.
  // Verify it on the paired dry engine; effect state must not extend that gate.
  if(n*block+i<240)originalTail=originalTail||std::abs(dl[i])>1e-7;
  else check(wl[i]==0&&wr[i]==0&&dl[i]==0&&dr[i]==0,"No resurrection after the original source-fade deadline");
 }}check(originalTail,"Retain the original nonzero panic fade");
 check(wet->inspectAudioThread().track.drives==bank,"Panic keeps settings");
 check(wet->enqueueLoop({LoopAction::clear,0,9,wet->generation()}),"Delete");render(false);check(wet->inspectAudioThread().track.drives==defaultDriveBank(),"Delete resets all drive settings");
 std::cout<<"PASS Engine "<<block<<" max PCM error="<<maximum<<"\n";
}
int main(){run(64);run(128);check(!allocations,"No callback new/delete");}
