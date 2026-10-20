#include "core/Engine.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
extern thread_local bool watch;
extern std::atomic<int> allocations;
extern std::atomic<bool> failWorkerAllocation;
namespace {
void check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
void wait(aion::LoopBank& bank){for(int n=0;bank.finalizationPending()&&n<10000;++n){watch=true;bank.beginBlock();watch=false;std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(!bank.finalizationPending(),"history job completion");}
void take(aion::LoopBank& bank,unsigned id,bool edit,float phase=0){
 using A=aion::LoopAction;watch=true;check(bank.apply(edit?A::overdub:A::record,id),"history take entry");
 for(int i=0;i<512;++i)bank.tick({0.1f+0.3f*std::sin(i*0.11f+phase),-0.07f+0.2f*std::cos(i*0.073f+phase)});
 check(bank.apply(A::finish,id),"history take finish");watch=false;wait(bank);
}
std::vector<aion::Stereo> read(aion::LoopBank& bank,unsigned id=0){
 std::vector<aion::Stereo> values(512);watch=true;check(bank.apply(aion::LoopAction::sourceRestart,id),"history render restart");for(auto& value:values)value=bank.tick({});watch=false;return values;
}
bool equal(const std::vector<aion::Stereo>& a,const std::vector<aion::Stereo>& b){for(unsigned i=0;i<a.size();++i)if(a[i].left!=b[i].left||a[i].right!=b[i].right)return false;return true;}
}
void historyTests(){
 using namespace aion;using A=LoopAction;auto storage=std::make_unique<LoopBank>();auto& bank=*storage;
 bank.prepare(8192,0);take(bank,0,false);auto original=read(bank);
 check(!bank.apply(A::undo,0)&&!bank.apply(A::redoGlobal,0),"fresh recording has no edit history");
 take(bank,0,true,0.5f);auto edited=read(bank);check(!equal(original,edited),"history fixture changed PCM");
 check(bank.view()[0].undoCount==1&&bank.nextUndo()==0,"successful edit checkpoint");
 watch=true;check(bank.apply(A::undoGlobal,0),"global undo");watch=false;
 check(equal(original,read(bank))&&bank.view()[0].redoCount==1&&bank.nextRedo()==0,"undo restores exact original stereo PCM");
 watch=true;check(bank.apply(A::redo,0),"per-loop redo");watch=false;check(equal(edited,read(bank)),"redo restores exact edit PCM");
 bank.apply(A::stop,0);bank.apply(A::speed,0,0.5f);bank.apply(A::pan,0,3);bank.apply(A::volume,0,0.4f);bank.apply(A::feedback,0,0.2f);bank.apply(A::mute,0,1);bank.apply(A::reverse,0,1);bank.apply(A::solo,0);
 watch=true;check(bank.apply(A::undo,0),"stopped undo");watch=false;
 const auto restored=bank.view()[0];check(restored.state==LoopState::stopped&&restored.playbackRate==0.5f&&restored.pan==3&&restored.volume==0.4f&&restored.feedback==0.2f&&restored.muted&&restored.reverse&&restored.solo,"undo retains stopped state and controls");
 bank.apply(A::speed,0,1);bank.apply(A::pan,0,5);bank.apply(A::volume,0,1);bank.apply(A::feedback,0,0.8f);bank.apply(A::mute,0,0);bank.apply(A::reverse,0,0);bank.apply(A::solo,0);bank.apply(A::play,0);
 // Failed/canceled edits must not discard a redo branch or publish a checkpoint.
 watch=true;check(bank.apply(A::overdub,0),"failure history edit");for(int i=0;i<64;++i)bank.tick({0.3f,0.1f});watch=false;
 failWorkerAllocation.store(true);watch=true;check(bank.apply(A::finish,0),"failed history publish");watch=false;wait(bank);
 check(bank.view()[0].undoCount==0&&bank.view()[0].redoCount==1&&bank.nextRedo()==0&&equal(original,read(bank)),"failed edit preserves history and PCM atomically");
 watch=true;check(bank.apply(A::overdub,0),"canceled history edit");bank.tick({});check(bank.apply(A::finish,0),"cancel history publish");check(!bank.apply(A::undo,0),"restore rejects in-flight same-track edit");bank.apply(A::clear,0);watch=false;wait(bank);
 check(bank.nextUndo()==-1&&bank.nextRedo()==-1&&bank.available()==8192,"clear cancels job and all history");
 take(bank,0,false);watch=true;check(bank.apply(A::overdub,0),"stopped commit entry");bank.tick({0.4f,0.2f});bank.apply(A::stop,0);watch=false;wait(bank);
 check(bank.view()[0].state==LoopState::stopped&&bank.view()[0].undoCount==1,"Stop still commits undo checkpoint for completed edit");
 watch=true;check(bank.apply(A::undo,0)&&bank.view()[0].state==LoopState::stopped,"undo never autoplays stopped track");watch=false;
 bank.apply(A::clearAll,0);
 // All tracks at the reference depth, plus retained reverse sources and a job.
 bank.prepare(20*4096,0);
 for(unsigned id=0;id<20;++id){bank.apply(A::reverse,id,1);take(bank,id,false);for(int n=0;n<6;++n)take(bank,id,true,0.2f*n);watch=true;check(bank.apply(A::normalize,id),"history with retained reverse source");watch=false;wait(bank);check(bank.view()[id].undoCount==5,"five checkpoints per loop");}
 check(bank.available()==0,"history leases do not consume the separate capture pool");
 watch=true;check(bank.apply(A::normalize,0),"maximum history clear job");bank.apply(A::clearAll,0);watch=false;wait(bank);
 check(bank.available()==20*4096&&bank.nextUndo()==-1&&bank.nextRedo()==-1,"max history Clear All releases all source/history leases");
 for(const auto& t:bank.view())check(!t.undoCount&&!t.redoCount&&t.state==LoopState::empty,"history clear state");
 take(bank,0,false);take(bank,0,true);watch=true;check(bank.apply(A::record,1),"record another slot during history restore");check(bank.apply(A::undo,0),"restore on unrelated track during capture");bank.apply(A::finish,1);watch=false;
 bank.prepare(4096,0,96000);check(bank.nextUndo()==-1&&bank.nextRedo()==-1&&!bank.view()[0].undoCount,"prepare clears histories after joining worker");
 // Prepare history before rendering, so asynchronous worker duration cannot
 // contaminate the 64/128 comparison of timestamped pointer swaps and fades.
 auto e64=std::make_unique<Engine>(),e128=std::make_unique<Engine>();
 const auto ready=[&](Engine& engine){
  engine.prepare(8000,8192);engine.setMonitoring(false);engine.setAutoPlay(false);
  const auto generation=engine.generation();
  check(engine.enqueueLoop({A::record,0,1,generation,0,0})&&engine.enqueueLoop({A::finish,0,2,generation,256,0}),"history engine record commands");
  std::uint32_t inputFrame=0;
  const auto block=[&](bool input){float left[64]{},right[64]{};if(input)for(int i=0;i<64;++i){left[i]=0.2f+0.3f*std::sin(static_cast<float>(inputFrame+i)*0.11f);right[i]=-0.1f+0.2f*std::cos(static_cast<float>(inputFrame+i)*0.073f);}float* channels[]={left,right};watch=true;engine.process(channels,2,64);EngineView view;while(engine.popView(view)){}watch=false;if(input)inputFrame+=64;};
  for(int i=0;i<8;++i)block(true);
  for(int n=0;engine.finalizationPending()&&n<10000;++n){block(false);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  check(!engine.finalizationPending(),"history engine initial worker");
  const auto editFrame=engine.clock();inputFrame=0;
  check(engine.enqueueLoop({A::play,0,3,generation,editFrame,0})&&engine.enqueueLoop({A::overdub,0,4,generation,editFrame+64,0})&&engine.enqueueLoop({A::finish,0,5,generation,editFrame+256,0}),"history engine edit commands");
  for(int i=0;i<8;++i)block(true);
  for(int n=0;engine.finalizationPending()&&n<10000;++n){block(false);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  check(!engine.finalizationPending(),"history engine edit worker");
  check(engine.enqueueLoop({A::stop,0,6,generation,engine.clock(),0}),"history engine stop");block(false);engine.setTarget(1);
  for(int i=0;i<32;++i)block(false);
  const auto frame=engine.clock();
  for(const auto& c:{LoopCommand{A::play,0,7,generation,frame+32,0},LoopCommand{A::undo,0,8,generation,frame+301,0},LoopCommand{A::redo,0,9,generation,frame+617,0},LoopCommand{A::reverse,0,10,generation,frame+1001,1},LoopCommand{A::undoGlobal,0,11,generation,frame+1201,0},LoopCommand{A::redoGlobal,0,12,generation,frame+1701,0}})check(engine.enqueueLoop(c),"timestamped history command");
 };
 ready(*e64);ready(*e128);
 std::vector<float> l64(2048),r64(2048),l128(2048),r128(2048);EngineView v64{},v128{};
 watch=true;
 for(unsigned offset=0;offset<2048;offset+=64){float* channels[]={l64.data()+offset,r64.data()+offset};e64->process(channels,2,64);while(e64->popView(v64)){};}
 for(unsigned offset=0;offset<2048;offset+=128){float* channels[]={l128.data()+offset,r128.data()+offset};e128->process(channels,2,128);while(e128->popView(v128)){};}
 watch=false;
 check(l64==l128&&r64==r128,"Undo/Redo exact 64/128 playback and transition invariance");
 check(v64.lastRequestId==12&&!v64.rejected&&v64.tracks[0].undoCount==1&&v64.tracks[0].redoCount==0&&v64.nextUndo==0&&v128.lastRequestId==12,"history commands acknowledged at both block sizes");
 check(allocations.load()==0,"Undo/Redo/history transfer must never allocate/free in callback");
 std::cout<<"PASS: exact Undo/Redo PCM, global/local history, stopped controls, atomic failure/cancel, max history/source leases, reset and no callback allocations\n";
}
