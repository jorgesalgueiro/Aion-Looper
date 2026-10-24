#include "core/Engine.h"
#include "core/MeterFollower.h"
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
extern thread_local bool watch;
extern std::atomic<int> allocations;
namespace {void check(bool ok,const char* label){if(!ok){std::cerr<<label<<'\n';std::exit(1);}}}
void meterWindowTests(){
 using namespace aion;
 std::array<float,MeterWindow::size> window{};
 MeterWindow raw;watch=true;
 for(unsigned i=0;i<17;++i)raw.pushStereo(static_cast<float>(i)/32,-static_cast<float>(i)/64);
 raw.copy(window);for(unsigned i=0;i<2031;++i)check(window[i]==0,"Meter window zero prefix");
 for(unsigned i=0;i<17;++i)check(window[2031+i]==static_cast<float>(i)/128,"Meter window signed stereo downmix order");
 raw.reset();raw.pushStereo(0.75f,-0.75f);raw.pushStereo(std::numeric_limits<float>::infinity(),0.25f);raw.copy(window);
 for(float v:window)check(v==0,"Meter antiphase and invalid mixed sample protection");
 for(unsigned i=0;i<6000;++i)raw.pushMono(static_cast<float>(i)/8192);
 raw.copy(window);for(unsigned i=0;i<2048;++i)check(window[i]==static_cast<float>(3952+i)/8192,"Meter repeated rolling wraps");
 watch=false;
 unsigned cases=0;
 for(unsigned id:{0u,19u})for(float pan:{0.0f,5.0f,10.0f})for(float volume:{0.25f,1.0f,2.0f}){
  std::array<float,4096> baseline{};
  for(unsigned block:{64u,128u}){
   auto bank=std::make_unique<LoopBank>();bank->prepare(81920,0,48000);
   check(bank->apply(LoopAction::record,id),"Meter fixture records");
   watch=true;for(unsigned i=0;i<4096;++i)bank->tick({0.35f*std::sin(i*0.031f),0.2f*std::cos(i*0.053f)});watch=false;
   check(bank->apply(LoopAction::finish,id),"Meter fixture finishes");bank->completeOfflineEdits();
   check(bank->trackView(id).state==LoopState::playing,"Real worker PCM committed before meter test");
   bank->apply(LoopAction::pan,id,pan);bank->apply(LoopAction::volume,id,volume);
   std::array<float,4096> expected{};
   watch=true;
   for(unsigned offset=0;offset<4096;offset+=block){
    bank->beginBlock();
    for(unsigned i=0;i<block;++i){const auto out=bank->tick({});expected[offset+i]=out.left*0.5f+out.right*0.5f;}
    check(bank->copyMeterWindow(id,window),"Read post-fader loop window");
    for(unsigned i=0;i<2048;++i){const int at=static_cast<int>(offset+block+i)-2048;check(window[i]==(at<0?0:expected[static_cast<unsigned>(at)]),"Actual loop tap matches rendered pan/volume PCM at every callback boundary");}
   }
   MeterFollower follower;const auto reading=follower.update(window,1,1,1,1,1,true);
   double sum=0,peak=0;for(float v:window){sum+=static_cast<double>(v)*v;peak=std::max(peak,std::abs(static_cast<double>(v)));}
   check(std::abs(reading.rms-std::sqrt(sum/2048))<1e-12&&reading.peak==peak&&peak>0.01,"Collected actual loop PCM feeds unchanged RMS/peak calculation");
   std::array<float,2048> untouched{};untouched.fill(0.123f);check(!bank->copyMeterWindow(20,untouched)&&untouched[0]==0.123f,"Invalid loop tap preserves destination");
   bank->apply(LoopAction::clear,id);bank->copyMeterWindow(id,window);for(float v:window)check(v==0,"Clear invalidates old raw window");
   watch=false;
   if(block==64)baseline=expected;else check(expected==baseline,"Exact real loop PCM/window 64/128 invariance");
   ++cases;
  }
 }
 // Production Engine tap is independent of MASTER, input monitor and click output.
 for(unsigned block:{64u,128u}){
  auto a=std::make_unique<Engine>(),b=std::make_unique<Engine>();
  for(auto* e:{a.get(),b.get()}){e->prepare(48000,81920);e->configureSync({SyncMode::off,120,1,4,4,false});check(e->enqueueLoop({LoopAction::record,0,1,e->generation(),0,0}),"Engine meter record command");}
  a->setMonitoring(false);a->setTarget(0);b->setMonitoring(true);b->setTarget(2);b->setMetronome(true,0.5f);
  std::array<float,128> al{},ar{},bl{},br{};std::array<float,2048> other{};
  for(unsigned offset=0;offset<8192;offset+=block){
   if(offset==4096)for(auto* e:{a.get(),b.get()})check(e->enqueueLoop({LoopAction::finish,0,2,e->generation(),0,0}),"Engine finish command");
   for(unsigned i=0;i<block;++i){al[i]=bl[i]=0.3f*std::sin((offset+i)*0.031f);ar[i]=br[i]=0.2f*std::cos((offset+i)*0.071f);}
   float* ac[]={al.data(),ar.data()},*bc[]={bl.data(),br.data()};watch=true;a->process(ac,2,static_cast<int>(block),{},true);b->process(bc,2,static_cast<int>(block),{},true);watch=false;
  }
  watch=true;a->copyLoopMeterWindow(0,window);b->copyLoopMeterWindow(0,other);watch=false;
  check(window==other&&std::any_of(window.begin(),window.end(),[](float v){return std::abs(v)>0.01f;}),"Real Engine tap excludes MASTER/input monitor/metronome");
  const auto saved=window;watch=true;a->process(nullptr,0,0);a->copyLoopMeterWindow(0,window);watch=false;check(window==saved,"Zero-sample callback preserves raw meter window");
  a->prepare(16000,81920);watch=true;a->copyLoopMeterWindow(0,window);watch=false;for(float v:window)check(v==0,"Prepare resets all raw meter PCM");
 }
 check(allocations.load()==0,"Meter collection/copy has zero callback new/delete");
 std::cout<<"PASS: meter PCM prefix/wrap/downmix/clear/reset, "<<cases<<" actual loop pan/gain/window cases, Engine MASTER/monitor/click isolation, zero callbacks and exact 64/128; no tracked new/delete\n";
}
