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
namespace {
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
void wait(aion::LoopBank& bank){for(unsigned n=0;bank.finalizationPending()&&n<10000;++n){watch=true;bank.beginBlock();watch=false;std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(!bank.finalizationPending(),"stutter worker timeout");}
}
void stutterTests(){
 using namespace aion;using A=LoopAction;
 auto bank=std::make_unique<LoopBank>();bank->prepare(4096,0,8000);
 watch=true;check(bank->apply(A::stutterStart,0)&&!bank->view()[0].stuttering,"empty STUT harmless");bank->apply(A::record,0);check(bank->apply(A::stutterStart,0)&&!bank->view()[0].stuttering,"recording STUT harmless");
 for(unsigned i=0;i<4000;++i)bank->tick({0.2f*std::sin(i*0.071f),0.17f*std::cos(i*0.13f)});bank->apply(A::finish,0);watch=false;wait(*bank);
 std::vector<Stereo> baseline(4000);watch=true;bank->apply(A::sourceRestart,0);for(auto& x:baseline)x=bank->tick({});
 for(unsigned i=0;i<1500;++i)bank->tick({});check(bank->available()==0&&bank->apply(A::stutterStart,0),"STUT works at full pool");
 const auto first=bank->view()[0];check(first.stuttering&&std::abs(first.stutterStart-1500)<1e-8&&std::abs(first.stutterEnd-2500)<1e-8,"original sixteenth fragment");
 for(unsigned i=0;i<1500;++i){const auto x=bank->tick({}),want=baseline[1500+i%1000];check(std::abs(x.left-want.left)<1e-6&&std::abs(x.right-want.right)<1e-6,"fragment PCM wraps");}
 check(bank->view()[0].stutterClock&&std::abs(bank->view()[0].clockProgress-0.75)<1e-14,"held playhead follows original graph clock instead of fragment cursor");
 bank->apply(A::stutterStart,0);check(bank->view()[0].stutterStart==first.stutterStart,"repeated down cannot move region");
 bank->apply(A::stutterStop,0);for(unsigned i=0;i<500;++i){const auto x=bank->tick({}),want=baseline[2000+i];check(std::abs(x.left-want.left)<1e-6&&std::abs(x.right-want.right)<1e-6,"release resumes actual cursor");}
 check(bank->view()[0].stutterClock&&std::abs(bank->view()[0].clockProgress-0.875)<1e-14,"released playhead keeps original graph clock");
 bank->apply(A::sourceRestart,0);check(!bank->view()[0].stutterClock,"replacement source resets visual override");for(unsigned i=0;i<3900;++i)bank->tick({});bank->apply(A::stutterStart,0);
 for(unsigned i=0;i<100;++i)bank->tick({});bank->apply(A::stutterStop,0);{const auto x=bank->tick({});check(std::abs(x.left-baseline[3900].left)<1e-6&&std::abs(x.right-baseline[3900].right)<1e-6,"release after a complete tail fragment resumes its wrapped source cursor");}
 bank->apply(A::stutterStart,0);bank->apply(A::stopAll,0);check(!bank->view()[0].stuttering,"panic releases region");
 bank->apply(A::play,0);bank->apply(A::stutterStart,0);bank->apply(A::retrigger,0);for(unsigned i=0;i<41;++i)bank->tick({});check(!bank->view()[0].stuttering,"new RTRG source releases region");
 bank->apply(A::stutterStart,0);bank->apply(A::clear,0);check(!bank->view()[0].stuttering&&bank->available()==4096,"DEL releases region without PCM copies");watch=false;
 // Production Engine stereo/block invariance and queue-independent cancellation.
 std::vector<float> reference;
 for(const int block:{64,128}){
  auto engine=std::make_unique<Engine>();engine->prepare(8000,8192);engine->setMonitoring(false);engine->setTarget(1);
  engine->enqueueLoop({A::record,0,1,engine->generation(),0,0});std::vector<float> capture(512);for(unsigned i=0;i<512;++i)capture[i]=0.2f*std::sin(i*0.09f);float* input[]={capture.data()};watch=true;engine->process(input,1,512);watch=false;
  engine->enqueueLoop({A::finish,0,2,engine->generation(),512,0});float zero=0,*channels[]={&zero};engine->process(channels,1,1);
  EngineView view{},latest{};for(unsigned n=0;n<10000;++n){watch=true;engine->process(channels,1,0);watch=false;while(engine->popView(view))latest=view;if(latest.tracks[0].state==LoopState::playing)break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  check(latest.tracks[0].state==LoopState::playing,"stutter Engine fixture committed");
  engine->enqueueLoop({A::stutterStart,0,3,engine->generation(),640,0});engine->enqueueLoop({A::stutterStop,0,4,engine->generation(),1200,0});
  std::vector<float> pcm(2048),right(2048);watch=true;for(unsigned offset=0;offset<2048;offset+=block){float* out[]={pcm.data()+offset,right.data()+offset};engine->process(out,2,block);}watch=false;
  check(pcm==right,"STUT stereo equality");if(reference.empty())reference=pcm;else check(reference==pcm,"STUT exact 64/128 equality");
  while(engine->popView(view))latest=view;
  engine->enqueueLoop({A::stutterStart,0,7,engine->generation(),0,0});watch=true;engine->process(channels,1,1);while(engine->popView(view))latest=view;watch=false;check(latest.tracks[0].stuttering,"atomic release fixture is active");
  engine->cancelStutter(0);watch=true;engine->process(channels,1,0);while(engine->popView(view))latest=view;watch=false;check(!latest.tracks[0].stuttering,"atomic release works even with zero-size audio block");
  engine->enqueueLoop({A::stutterStart,0,5,engine->generation(),engine->clock()+1024,0});
  unsigned filled=0;while(engine->enqueueLoop({A::volume,0,6,engine->generation(),engine->clock()+1024,1}))++filled;check(filled>0,"fill command queue");
  engine->cancelStutter();std::vector<float> zeros(2048);float* out[]={zeros.data()};watch=true;engine->process(out,1,2048);while(engine->popView(view))latest=view;watch=false;check(!latest.tracks[0].stuttering,"editor close bypasses full queue and cancels deferred starts");
  engine->prepare(8000,8192);watch=true;engine->process(channels,1,0);while(engine->popView(view))latest=view;watch=false;check(!latest.tracks[0].stuttering,"prepare cancels STUT");
 }
 check(allocations.load()==0,"STUT zero callback new/delete");
 std::cout<<"PASS: STUT fragment/resume PCM, guards, full pool, panic/DEL/RTRG/reset, stereo/64/128 and queue-independent editor-close release\n";
}
