#include "core/Engine.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>
extern thread_local bool watch;
extern std::atomic<int> allocations;
extern std::atomic<bool> failWorkerAllocation;
namespace {
void check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
void wait(aion::LoopBank& bank){
 for(int n=0;bank.finalizationPending()&&n<10000;++n){watch=true;bank.beginBlock();bank.tick({});watch=false;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 check(!bank.finalizationPending(),"worker completion timeout");
}
std::vector<aion::Stereo> wave(unsigned length){std::vector<aion::Stereo> data(length);for(unsigned i=0;i<length;++i)data[i]={0.1f+0.3f*std::sin(0.11f*i),-0.1f+0.2f*std::cos(0.073f*i)};return data;}
void capture(aion::LoopBank& bank,unsigned id,const std::vector<aion::Stereo>& data){
 using A=aion::LoopAction;check(bank.apply(A::record,id),"record entry");
 watch=true;for(auto sample:data)bank.tick(sample);check(bank.apply(A::finish,id),"finish entry");watch=false;wait(bank);
}
std::vector<aion::Stereo> render(aion::LoopBank& bank,unsigned id){
 check(bank.apply(aion::LoopAction::retrigger,id),"retrigger committed clip");
 std::vector<aion::Stereo> data(bank.view()[id].length);
 watch=true;for(auto& sample:data)sample=bank.tick({});watch=false;return data;
}
bool equal(const std::vector<aion::Stereo>& a,const std::vector<aion::Stereo>& b){if(a.size()!=b.size())return false;for(unsigned i=0;i<a.size();++i)if(a[i].left!=b[i].left||a[i].right!=b[i].right)return false;return true;}
}
void loopTests(){
 using namespace aion;using A=LoopAction;
 LoopBank bank;bank.prepare(8192,0);auto input=wave(512);
 check(bank.apply(A::record,0)&&!bank.apply(A::record,1),"one capture owner");
 watch=true;for(auto sample:input)bank.tick(sample);check(bank.apply(A::finish,0),"publish take");watch=false;
 check(bank.view()[0].state==LoopState::finalizing,"finish publishes saving state");
 check(!bank.apply(A::record,1),"finalization retains capture ownership");wait(bank);
 check(bank.view()[0].state==LoopState::playing&&bank.view()[0].length==512,"processed take commits");
 auto original=render(bank,0);check(original[73].left!=0,"processed waveform is audible");
 check(bank.apply(A::overdub,0),"overdub entry");
 watch=true;for(unsigned i=0;i<input.size();++i){auto out=bank.tick(input[i]);check(out.left==original[i].left&&out.right==original[i].right,"old audio stays immutable while capturing edit");}check(bank.apply(A::finish,0),"publish edit");watch=false;wait(bank);
 auto edited=render(bank,0);check(!equal(edited,original)&&edited.size()==original.size(),"edit replaces PCM only after finalization");
 check(bank.apply(A::overdub,0),"fault edit entry");for(auto sample:input)bank.tick(sample);
 failWorkerAllocation.store(true);watch=true;check(bank.apply(A::finish,0),"fault job publish");watch=false;wait(bank);
 check(bank.view()[0].finalizationFailed,"worker allocation failure reported");
 check(equal(render(bank,0),edited),"failed edit preserves prior PCM exactly");
 check(bank.apply(A::substitute,0),"substitute entry");for(auto sample:input)bank.tick(sample);check(bank.apply(A::finish,0),"substitute finish");
 bank.apply(A::stopAll,0);wait(bank);check(bank.view()[0].state==LoopState::stopped,"stop while saving prevents late autoplay");
 bank.apply(A::play,0);check(bank.apply(A::overdub,0),"cancel edit entry");for(auto sample:input)bank.tick(sample);bank.apply(A::finish,0);bank.apply(A::clear,0);wait(bank);
 check(bank.view()[0].state==LoopState::empty&&bank.available()==8192,"clear while saving discards result and releases pinned blocks");
 bank.setAutoPlay(false);capture(bank,0,input);check(bank.view()[0].state==LoopState::stopped,"auto-play off survives worker completion");
 bank.apply(A::mute,0,1);bank.apply(A::clear,0);check(bank.view()[0].muted,"clear preserves reference explicit mute");
 bank.apply(A::mute,0,0);bank.setAutoPlay(true);
 check(bank.apply(A::record,0)&&bank.apply(A::finish,0)&&bank.view()[0].state==LoopState::empty,"zero-frame finish releases recorder");
 bank.prepare(4096,0);auto full=wave(4096);capture(bank,0,full);
 check(bank.available()==0&&!bank.apply(A::record,1),"resident PCM counts toward capture budget");
 auto before=render(bank,0);check(bank.apply(A::overdub,0),"full resident clip can still edit in separate take pool");
 for(unsigned i=0;i<4096;++i)bank.tick(full[i]);bank.tick({});wait(bank);
 check(bank.exhausted()&&bank.view()[0].length==4096,"full take auto-finalizes without truncating base");
 check(!equal(before,render(bank,0)),"full-capacity edit commits");
 for(int iteration=0;iteration<32;++iteration){
  bank.apply(A::overdub,0);for(int i=0;i<64;++i)bank.tick(full[i]);bank.apply(A::finish,0);
  bank.apply(A::clearAll,0);wait(bank);check(bank.available()==4096&&bank.view()[0].state==LoopState::empty,"repeated cancel cannot resurrect or leak pool");capture(bank,0,full);
 }
 bank.apply(A::overdub,0);bank.tick({});bank.apply(A::finish,0);bank.prepare(4096,0,96000);
 check(bank.view()[0].state==LoopState::empty&&!bank.finalizationPending(),"prepare joins worker before storage reset");
 // NORM is an immutable worker transaction with no capture storage or callback scan.
 bank.prepare(4096,0);capture(bank,0,input);auto preNorm=render(bank,0);
 const auto capacityBeforeNorm=bank.available();
 bank.apply(A::speed,0,0.5f);bank.apply(A::pan,0,3);bank.apply(A::mute,0,1);
 const auto phaseBeforeNorm=bank.view()[0].position;
 watch=true;check(bank.apply(A::normalize,0),"normalize publish");watch=false;
 check(bank.view()[0].position==phaseBeforeNorm&&bank.available()==capacityBeforeNorm,"normalize keeps phase and resident budget");
 check(!bank.apply(A::normalize,0)&&!bank.apply(A::record,1),"one worker transaction; normalize cannot race capture");
 bank.apply(A::stop,0);wait(bank);
 check(bank.view()[0].state==LoopState::stopped&&bank.view()[0].playbackRate==0.5f&&bank.view()[0].pan==3&&bank.view()[0].muted,"normalize retains knobs/mute and respects Stop");
 bank.apply(A::speed,0,1);bank.apply(A::pan,0,5);bank.apply(A::mute,0,0);auto normalized=render(bank,0);
 check(!equal(preNorm,normalized),"normalize changes sub-unity PCM");
 watch=true;check(bank.apply(A::normalize,0),"repeat normalize publish");watch=false;wait(bank);
 check(equal(normalized,render(bank,0)),"normalize is idempotent");
 // A live normalize commit must keep the current source phase, not retrigger.
 bank.apply(A::retrigger,0);for(int i=0;i<37;++i)bank.tick({});
 check(bank.apply(A::normalize,0),"live normalize phase publish");
 for(int i=0;i<17;++i)bank.tick({});const auto livePhase=bank.view()[0].position;
 for(int n=0;bank.finalizationPending()&&n<10000;++n){watch=true;bank.beginBlock();watch=false;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 check(!bank.finalizationPending()&&bank.view()[0].state==LoopState::playing&&bank.view()[0].position==livePhase,"live normalization commit preserves playback phase");
 bank.apply(A::stop,0);check(bank.apply(A::normalize,0),"stopped normalize");wait(bank);
 check(bank.view()[0].state==LoopState::stopped,"stopped normalize cannot autoplay");
 // Worker failure must retain the unnormalized prior clip exactly.
 bank.apply(A::clear,0);capture(bank,0,input);auto old=render(bank,0);
 failWorkerAllocation.store(true);watch=true;check(bank.apply(A::normalize,0),"normalize OOM publish");watch=false;wait(bank);
 check(bank.view()[0].finalizationFailed&&equal(old,render(bank,0)),"normalize allocation failure rolls back PCM");
 for(int n=0;n<32;++n){check(bank.apply(A::normalize,0),"normalize cancellation publish");bank.apply(A::clearAll,0);wait(bank);check(bank.view()[0].state==LoopState::empty&&bank.available()==4096,"clear discards normalized result and releases base lease");capture(bank,0,input);}
 check(bank.apply(A::normalize,0),"normalize reset publish");bank.prepare(4096,0);
 check(!bank.finalizationPending()&&!bank.apply(A::normalize,0),"reset joins normalize worker; empty normalize rejected");
 check(bank.apply(A::record,0)&&!bank.apply(A::normalize,0),"capture cannot normalize unfinished PCM");bank.apply(A::clear,0);
 // Preserve the existing pan/rate/fade regressions on finalized, DC-corrected PCM.
 bank.prepare(8192,0);capture(bank,0,input);auto source=render(bank,0);
 for(float speed : {0.25f,0.5f,0.37f,2.0f}){
  bank.apply(A::speed,0,speed);bank.apply(A::retrigger,0);
  watch=true;
  double phase=0;
  for(unsigned i=0;i<2048;++i){const auto out=bank.tick({});const auto index=static_cast<unsigned>(phase);const auto next=(index+1)%source.size();const float fraction=static_cast<float>(phase-index);const float expected=source[index].left+(source[next].left-source[index].left)*fraction;check(std::abs(out.left-expected)<1e-6f,"fractional playback interpolation/wrap regression");phase=std::fmod(phase+speed,static_cast<double>(source.size()));}
  watch=false;
 }
 check(!bank.apply(A::overdub,0),"varispeed edit restriction");bank.apply(A::speed,0,1);bank.apply(A::pan,0,0);bank.apply(A::retrigger,0);
 for(unsigned i=0;i<source.size();++i){auto out=bank.tick({});check(out.right==0&&std::abs(out.left-source[i].left/0.7071067811865475f)<1e-6f,"reference hard-left pan regression");}
 bank.apply(A::pan,0,10);bank.apply(A::retrigger,0);check(bank.tick({}).left==0,"hard-right pan regression");
 bank.prepare(8192,4);capture(bank,0,input);for(int i=0;i<32;++i)bank.tick({});auto last=bank.tick({});bank.apply(A::stop,0);
 for(int i=0;i<4;++i){const auto out=bank.tick({});const float multiplier=static_cast<float>(3-i)/4;check(std::abs(out.left-last.left*multiplier)<1e-6f,"stop release envelope regression");}
 // Worker completion time is asynchronous. Compare deterministic playback after both
 // stopped clips have committed, then apply commands at the same relative samples.
 Engine engine64,engine128;
 const auto ready=[&](Engine& engine,bool normalize=false){
  engine.prepare(8000,8192);engine.setMonitoring(false);engine.setAutoPlay(false);
  const auto generation=engine.generation();check(engine.enqueueLoop({A::record,0,1,generation,0,0}),"engine record queue");check(engine.enqueueLoop({A::finish,0,2,generation,256,0}),"engine finish queue");
  for(unsigned offset=0;offset<512;offset+=64){float left[64],right[64];for(unsigned i=0;i<64;++i){left[i]=input[offset+i].left;right[i]=input[offset+i].right;}float* channels[]={left,right};watch=true;engine.process(channels,2,64);watch=false;}
  for(int n=0;engine.finalizationPending()&&n<10000;++n){float left[64]{},right[64]{};float* channels[]={left,right};watch=true;engine.process(channels,2,64);watch=false;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  check(!engine.finalizationPending(),"engine worker completion");
  if(normalize){
   check(engine.enqueueLoop({A::normalize,0,3,generation,engine.clock(),0}),"engine normalize queue");
   float left[64]{},right[64]{};float* channels[]={left,right};watch=true;engine.process(channels,2,64);watch=false;
   for(int n=0;engine.finalizationPending()&&n<10000;++n){watch=true;engine.process(channels,2,64);watch=false;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
   check(!engine.finalizationPending(),"engine normalize completion");
  }
  engine.setTarget(1);
  for(int n=0;n<32;++n){float zero[64]{};float* channels[]={zero};watch=true;engine.process(channels,1,64);watch=false;}
  const auto frame=engine.clock();
  check(engine.enqueueLoop({A::play,0,3,generation,frame+32,0}),"sample deadline play");
  check(engine.enqueueLoop({A::speed,0,4,generation,frame+301,0.37f}),"sample deadline fractional rate");
  check(engine.enqueueLoop({A::pan,0,5,generation,frame+617,10}),"sample deadline pan");
  check(engine.enqueueLoop({A::speed,0,6,generation,frame+1001,2}),"sample deadline double rate");
 };
 ready(engine64);ready(engine128);
 std::vector<float> left64(2048),right64(2048),left128(2048),right128(2048);
 watch=true;
 for(unsigned offset=0;offset<2048;offset+=64){float* channels[]={left64.data()+offset,right64.data()+offset};engine64.process(channels,2,64);}
 for(unsigned offset=0;offset<2048;offset+=128){float* channels[]={left128.data()+offset,right128.data()+offset};engine128.process(channels,2,128);}
 watch=false;
 check(left64==left128&&right64==right128,"committed PCM/sample commands and rate/pan ramps invariant at 64/128");
 check(left64[31]==0&&left64[32]!=0,"play command exact sample boundary");
 Engine normalized64,normalized128;ready(normalized64,true);ready(normalized128,true);
 std::fill(left64.begin(),left64.end(),0);std::fill(right64.begin(),right64.end(),0);std::fill(left128.begin(),left128.end(),0);std::fill(right128.begin(),right128.end(),0);
 watch=true;
 for(unsigned offset=0;offset<2048;offset+=64){float* channels[]={left64.data()+offset,right64.data()+offset};normalized64.process(channels,2,64);}
 for(unsigned offset=0;offset<2048;offset+=128){float* channels[]={left128.data()+offset,right128.data()+offset};normalized128.process(channels,2,128);}
 watch=false;
 check(left64==left128&&right64==right128,"normalized stopped clip playback and sample commands invariant at 64/128");
 const auto generation=engine64.generation();engine64.prepare(16000,8192);check(!engine64.enqueueLoop({A::record,0,9,generation,0,0}),"reset rejects stale generation");
 // Twenty simultaneous playback slots, with serial capture and background finalization.
 bank.prepare(20*4096,0);for(unsigned id=0;id<20;++id)capture(bank,id,input);
 watch=true;for(int i=0;i<8192;++i){const auto out=bank.tick({});check(std::isfinite(out.left)&&std::isfinite(out.right),"20-track finite playback");}bank.apply(A::stopAll,0);watch=false;
 check(allocations.load()==0,"audio thread must not allocate or free during ownership transfer/capture/play/edit");
 std::cout<<"PASS: immutable capture/edit finalization, worker OOM rollback, clear/stop/reset cancellation, pool exhaustion, 20 tracks, audio-thread allocation guard\n";
}
