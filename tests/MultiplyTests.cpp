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
using A=aion::LoopAction;using S=aion::LoopState;
void check(bool ok,const char* text){if(!ok){std::cerr<<text<<'\n';std::exit(1);}}
void wait(aion::LoopBank& b){for(int i=0;b.finalizationPending()&&i<10000;++i){watch=true;b.beginBlock();watch=false;std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(!b.finalizationPending(),"multiply worker timeout");}
void seed(aion::LoopBank& b,unsigned id,unsigned length){watch=true;check(b.apply(A::record,id),"multiply seed record");for(unsigned i=0;i<length;++i)b.tick({0.1f+0.3f*std::sin(i*0.11f),-0.1f+0.2f*std::cos(i*0.073f)});check(b.apply(A::finish,id),"multiply seed finish");watch=false;wait(b);}
void append(aion::LoopBank& b,unsigned length){watch=true;check(b.apply(A::multiply,0,1),"multiply capture start");for(unsigned i=0;i<length;++i)b.tick({0.2f+0.3f*std::sin(i*0.091f),-0.2f+0.1f*std::cos(i*0.053f)});check(b.apply(A::multiply,0,1),"multiply capture finish");watch=false;wait(b);}
std::vector<aion::Stereo> read(aion::LoopBank& b,unsigned length){std::vector<aion::Stereo> out(length);watch=true;check(b.apply(A::retrigger,0),"multiply render");for(auto& frame:out)frame=b.tick({});watch=false;return out;}
bool equal(const std::vector<aion::Stereo>& a,const std::vector<aion::Stereo>& b){if(a.size()!=b.size())return false;for(unsigned i=0;i<a.size();++i)if(a[i].left!=b[i].left||a[i].right!=b[i].right)return false;return true;}
}
void multiplyTests(){
 auto storage=std::make_unique<aion::LoopBank>();auto& b=*storage;b.prepare(16384,0);seed(b,0,512);const auto original=read(b,512);
 for(unsigned length:{0u,1u,127u}){append(b,length);check(b.view()[0].length==512&&!b.view()[0].undoCount&&equal(original,read(b,512)),"short multiply discards take and history");}
 append(b,128);check(b.view()[0].length==640&&b.view()[0].undoCount==1&&b.nextUndo()==-1,"multiply adds only local history like JS");const auto extended=read(b,640);
 watch=true;check(b.apply(A::undo,0),"multiply undo");watch=false;check(b.view()[0].length==512&&equal(original,read(b,512)),"undo restores shorter exact clip");
 watch=true;check(b.apply(A::redo,0),"multiply redo");watch=false;check(b.view()[0].length==640&&equal(extended,read(b,640)),"redo restores longer exact clip");
 b.apply(A::stop,0);b.apply(A::speed,0,0.5f);watch=true;check(b.apply(A::multiply,0,1),"stopped varispeed uses instant doubling");watch=false;wait(b);check(b.view()[0].state==S::stopped&&b.view()[0].length==1280&&b.view()[0].playbackRate==0.5f,"stopped double retains state and speed");
 b.apply(A::play,0);for(int i=0;i<128;++i)b.tick({});check(!b.apply(A::multiply,0,1)&&!b.apply(A::multiply,0,0),"playing varispeed rejects capture and no-input doubling");b.apply(A::stop,0);b.apply(A::speed,0,1);b.apply(A::play,0);for(int i=0;i<128;++i)b.tick({});
 watch=true;check(b.apply(A::multiply,0,0),"playing with no input doubles");watch=false;wait(b);check(b.view()[0].state==S::playing&&b.view()[0].length==2560,"no-input double restarts playing");
 // Worker failure keeps both the current PCM and redo branch.
 b.apply(A::undo,0);const auto count=b.view()[0].redoCount;const auto length=b.view()[0].length;const auto before=read(b,length);
 failWorkerAllocation.store(true);watch=true;check(b.apply(A::multiply,0,0),"OOM doubling request");watch=false;wait(b);check(b.view()[0].finalizationFailed&&b.view()[0].length==length&&b.view()[0].redoCount==count&&equal(before,read(b,length)),"failed double is atomic");
 watch=true;check(b.apply(A::multiply,0,1),"multiply controls entry");check(!b.apply(A::speed,0,0.5f)&&!b.apply(A::normalize,0)&&!b.apply(A::undo,0)&&!b.apply(A::record,1),"capture owns editing lane");for(int i=0;i<128;++i)b.tick({0.2f,0.1f});b.apply(A::stopAll,0);watch=false;wait(b);check(b.view()[0].state==S::stopped&&b.view()[0].length==length+128,"Stop commits append without autoplay");
 b.apply(A::play,0);watch=true;check(b.apply(A::multiply,0,1),"clear multiply entry");for(int i=0;i<512;++i)b.tick({0.3f,0.1f});check(b.apply(A::finish,0),"clear multiply job");b.apply(A::clearAll,0);watch=false;wait(b);check(b.view()[0].state==S::empty&&b.available()==16384&&!b.view()[0].undoCount,"Clear All cancels append/history and returns take blocks");
 // Full memory, exact splice boundary, and growth reservation during history swap.
 b.prepare(8192,0);seed(b,0,4097);check(!b.apply(A::multiply,0,0),"doubling must fit current clip budget");
 watch=true;check(b.apply(A::multiply,0,1),"bounded multiply");for(int i=0;i<9000;++i)b.tick({0.2f,0.1f});watch=false;wait(b);check(b.view()[0].length==8192&&b.exhausted()&&b.available()==0,"multiply auto-finishes at exact memory boundary");
 b.apply(A::undo,0);check(b.view()[0].length==4097,"budget undo shorter");check(b.apply(A::record,1)==false,"no whole block remains for another recording");
 b.apply(A::clearAll,0);seed(b,0,4096);watch=true;check(b.apply(A::multiply,0,0),"budget double exact");watch=false;wait(b);b.apply(A::undo,0);seed(b,1,512);check(!b.apply(A::redo,0)&&b.view()[0].length==4096&&b.view()[0].redoCount==1,"larger redo cannot exceed shared resident budget");b.apply(A::clear,1);check(b.apply(A::redo,0)&&b.view()[0].length==8192,"larger redo succeeds after space released");
 // A growing in-flight job must reserve its eventual resident space against a different track's redo.
 b.prepare(12288,0);seed(b,0,4096);b.apply(A::multiply,0,0);wait(b);b.apply(A::undo,0);seed(b,1,4096);
 check(b.available()==4096,"growth reservation fixture has one free block");b.apply(A::stop,0);check(b.apply(A::multiply,1,0),"other-track growth job");check(b.available()==0,"pending doubling reserves displayed capacity");check(!b.apply(A::redo,0),"history restore respects in-flight multiply growth");b.apply(A::clear,1);wait(b);check(b.apply(A::redo,0),"canceled growth reservation releases space");
 // Clear/reset while worker owns an independently pinned base, including reverse source.
 b.apply(A::clearAll,0);seed(b,0,512);b.apply(A::reverse,0,1);b.apply(A::retrigger,0);b.apply(A::normalize,0);wait(b);b.apply(A::multiply,0,0);b.apply(A::clear,0);wait(b);check(b.view()[0].state==S::empty,"cleared reversed double cannot resurrect");
 seed(b,0,512);b.apply(A::multiply,0,0);b.prepare(4096,0,96000);check(!b.finalizationPending()&&b.view()[0].state==S::empty,"prepare joins multiply worker before reset");
 // Timestamped capture starts/finishes inside blocks; compare committed audio
 // after worker completion, not wall-clock-dependent asynchronous commit blocks.
 const auto render=[](unsigned blockSize){
  auto engine=std::make_unique<aion::Engine>();engine->prepare(8000,16384);engine->setMonitoring(false);
  const auto generation=engine->generation();
  const auto run=[&](unsigned count,bool input,unsigned size){
   std::vector<float> left(count),right(count);for(unsigned i=0;input&&i<count;++i){left[i]=0.1f+0.3f*std::sin(i*0.11f);right[i]=-0.1f+0.2f*std::cos(i*0.073f);}
   watch=true;for(unsigned i=0;i<count;i+=size){float* channels[]={left.data()+i,right.data()+i};engine->process(channels,2,static_cast<int>(size));aion::EngineView view;while(engine->popView(view)){};}watch=false;
   return std::make_pair(left,right);
  };
  const auto settle=[&]{for(int n=0;engine->finalizationPending()&&n<10000;++n){run(64,false,64);std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(!engine->finalizationPending(),"multiply engine worker timeout");};
  check(engine->enqueueLoop({A::record,0,1,generation,0,0})&&engine->enqueueLoop({A::finish,0,2,generation,257,0}),"multiply engine seed commands");run(512,true,blockSize);settle();
  auto start=engine->clock();check(engine->enqueueLoop({A::multiply,0,3,generation,start+19,1})&&engine->enqueueLoop({A::finish,0,4,generation,start+532,0}),"timestamped multiply capture commands");run(768,true,blockSize);settle();
  check(engine->enqueueLoop({A::stop,0,5,generation,engine->clock(),0}),"multiply engine stop");run(2048,false,blockSize);engine->setTarget(1);run(2048,false,blockSize);
  check(engine->enqueueLoop({A::retrigger,0,6,generation,engine->clock()+17,0}),"multiply engine audition");return run(2048,false,blockSize);
 };
 const auto block64=render(64),block128=render(128);check(block64==block128,"multiply captured PCM and timed playback exact at 64/128 blocks");
 check(allocations.load()==0,"multiply/history growth performs no callback allocation/free");
 std::cout<<"PASS: multiply short/append/double, unequal-length history, varispeed, atomic OOM/cancel/stop/reset, shared memory limits and callback allocation contract\n";
}
