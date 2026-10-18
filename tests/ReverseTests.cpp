#include "core/LoopBank.h"
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
void wait(aion::LoopBank& bank){
 for(int n=0;bank.finalizationPending()&&n<10000;++n){watch=true;bank.beginBlock();watch=false;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 check(!bank.finalizationPending(),"reverse worker completion");
}
void capture(aion::LoopBank& bank,unsigned id){
 using A=aion::LoopAction;watch=true;check(bank.apply(A::record,id),"reverse capture entry");
 for(int i=0;i<512;++i)bank.tick({0.1f+0.3f*std::sin(i*0.11f),-0.07f+0.2f*std::cos(i*0.073f)});
 check(bank.apply(A::finish,id),"reverse capture finish");watch=false;wait(bank);
}
std::vector<aion::Stereo> read(aion::LoopBank& bank){
 std::vector<aion::Stereo> samples(512);watch=true;for(auto& sample:samples)sample=bank.tick({});watch=false;return samples;
}
bool equal(const std::vector<aion::Stereo>& a,const std::vector<aion::Stereo>& b){for(unsigned i=0;i<a.size();++i)if(a[i].left!=b[i].left||a[i].right!=b[i].right)return false;return true;}
}
void reverseTests(){
 using namespace aion;using A=LoopAction;
 auto bankStorage=std::make_unique<LoopBank>();auto& bank=*bankStorage;
 bank.prepare(4096,0);capture(bank,0);bank.apply(A::retrigger,0);const auto forward=read(bank);
 watch=true;check(bank.apply(A::reverse,0,1)&&bank.view()[0].reverse,"reverse requested flag");watch=false;
 check(equal(forward,read(bank)),"reverse toggle must leave playing forward source unchanged");
 watch=true;bank.apply(A::retrigger,0);watch=false;const auto reversed=read(bank);
 for(unsigned i=0;i<512;++i)check(reversed[i].left==forward[511-i].left&&reversed[i].right==forward[511-i].right,"reverse source exact sample ordering");
 watch=true;check(bank.apply(A::normalize,0),"normalize reversed source");watch=false;wait(bank);
 check(bank.available()==0&&equal(reversed,read(bank)),"NORM keeps independent playing reversed source at full resident budget");
 watch=true;bank.apply(A::retrigger,0);watch=false;const auto normalizedReverse=read(bank);
 check(!equal(normalizedReverse,reversed),"reverse retrigger picks up normalized stored PCM");
 watch=true;bank.apply(A::reverse,0,0);watch=false;
 check(equal(normalizedReverse,read(bank)),"reverse toggle off must wait for next source");
 watch=true;bank.apply(A::stop,0);check(bank.apply(A::play,0),"play after reverse toggle off");watch=false;
 const auto normalizedForward=read(bank);for(unsigned i=0;i<512;++i)check(normalizedForward[i].left==normalizedReverse[511-i].left,"stop/play latches pending direction");
 // A reversed source can outlive a normalized stored clip and a failed edit.
 bank.apply(A::clear,0);bank.apply(A::reverse,0,1);capture(bank,0);
 check(equal(reversed,read(bank)),"fresh capture auto-play latches requested reverse");
 watch=true;check(bank.apply(A::normalize,0),"retained reverse source before failed edit");watch=false;wait(bank);
 watch=true;check(bank.apply(A::overdub,0),"edit with held reverse source");for(int i=0;i<64;++i)bank.tick({0.3f,-0.1f});watch=false;
 failWorkerAllocation.store(true);watch=true;check(bank.apply(A::finish,0),"failed reverse edit publish");watch=false;wait(bank);
 const auto failedEdit=read(bank);check(bank.view()[0].finalizationFailed,"reverse edit failure reported");
 for(unsigned i=0;i<512;++i)check(failedEdit[i].left==reversed[(i+64)%512].left&&failedEdit[i].right==reversed[(i+64)%512].right,"failed edit keeps held reverse PCM and phase");
 watch=true;check(bank.apply(A::normalize,0),"stop with held reverse job");bank.apply(A::stopAll,0);watch=false;wait(bank);
 check(bank.view()[0].state==LoopState::stopped,"stop during reverse finalization cannot autoplay");
 watch=true;check(bank.apply(A::play,0),"replay current reversed stored clip");watch=false;
 check(!equal(reversed,read(bank)),"new play uses normalized stored PCM after stopped hold release");
 // Editing/cancellation must release both the stored clip and any held reverse source.
 bank.apply(A::clear,0);
 for(int n=0;n<32;++n){
  bank.apply(A::reverse,0,1);capture(bank,0);
  watch=true;check(bank.apply(A::normalize,0),"reverse hold normalization");watch=false;wait(bank);
  watch=true;check(bank.apply(A::normalize,0),"reverse clear job publish");bank.apply(A::clearAll,0);watch=false;wait(bank);
  check(bank.available()==4096&&bank.view()[0].state==LoopState::empty&&!bank.view()[0].reverse,"clear cancels job and held reverse source");
 }
 bank.apply(A::reverse,0,1);capture(bank,0);bank.apply(A::retrigger,0);const auto old=read(bank);
 failWorkerAllocation.store(true);watch=true;check(bank.apply(A::normalize,0),"reverse OOM job publish");watch=false;wait(bank);
 check(bank.view()[0].finalizationFailed&&equal(old,read(bank)),"reverse source survives worker allocation failure");
 watch=true;check(bank.apply(A::normalize,0),"reverse prepare reset publish");watch=false;bank.prepare(4096,0,96000);
 check(bank.available()==4096&&!bank.view()[0].reverse&&!bank.finalizationPending(),"reset joins and releases reversed sources");
 // One held reversed source per slot is bounded; Clear All retires at most 40 clips.
 bank.prepare(20*4096,0);
 for(unsigned id=0;id<20;++id){bank.apply(A::reverse,id,1);capture(bank,id);watch=true;check(bank.apply(A::normalize,id),"20 reverse holds");watch=false;wait(bank);}
 watch=true;for(int i=0;i<1024;++i){auto out=bank.tick({});check(std::isfinite(out.left)&&std::isfinite(out.right),"20 reversed tracks finite");}
 bank.apply(A::clearAll,0);watch=false;
 check(bank.available()==20*4096,"20 reversed tracks release resident budget");
 check(!bank.apply(A::reverse,0,0.5f),"reverse rejects nonboolean value");
 check(allocations.load()==0,"reverse must not allocate or free in callback");
 std::cout<<"PASS: reverse next-source latch, immutable reversed NORM source, stop/play, OOM, cancellation/reset, 20 tracks and callback allocation guard\n";
}
