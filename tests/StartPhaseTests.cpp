#include "core/Engine.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <thread>
#include <vector>
extern thread_local bool watch;
extern std::atomic<int> allocations;
namespace {void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
void wait(aion::LoopBank& bank){for(unsigned n=0;bank.finalizationPending()&&n<10000;++n){watch=true;bank.beginBlock();watch=false;std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(!bank.finalizationPending(),"phase worker timeout");}}
void startPhaseTests(){
 using namespace aion;using A=LoopAction;
 auto store=std::make_unique<LoopBank>();auto& bank=*store;bank.prepare(4096,0,8000);
 watch=true;check(bank.apply(A::record,0),"phase capture");for(unsigned i=0;i<257;++i)bank.tick({0.1f+0.3f*std::sin(i*0.11f),-0.1f+0.2f*std::cos(i*0.073f)});check(bank.apply(A::finish,0),"phase finish");watch=false;wait(bank);
 std::vector<Stereo> baseline(257);watch=true;bank.apply(A::retrigger,0);for(auto& x:baseline)x=bank.tick({});
 check(bank.available()==0&&bank.setStartDelay(0,0.333)&&bank.view()[0].startDelay==0.333,"phase is double precision and works at full pool budget");
 check(bank.apply(A::quantizeStart,0)&&bank.view()[0].startDelay==0.333&&bank.view()[0].phaseApplyFrame==0,"Q on clip below half a sixteenth is a no-op");
 const auto previous=bank.view()[0].position;bank.tick({});check(bank.view()[0].position==(previous+1)%257,"dragging metadata does not restart");
 const auto frame=bank.syncView().frame;check(bank.apply(A::applyStart,0)&&bank.view()[0].phaseApplyFrame==frame+400,"50ms audio deadline");
 check(bank.setStartDelay(0,0.75)&&bank.apply(A::reverse,0,1),"later metadata changes accepted");
 for(unsigned i=0;i<400;++i)bank.tick({});check(bank.view()[0].phaseApplyFrame==frame+400,"deadline sample has not run early");
 const auto output=bank.tick({});const double phase=0.333*257;const unsigned index=static_cast<unsigned>(phase),next=(index+1)%257;const float fraction=static_cast<float>(phase-index);
 const auto want=baseline[index].left+(baseline[next].left-baseline[index].left)*fraction;
 check(std::abs(output.left-want)<1e-6&&bank.view()[0].phaseApplyFrame==0,"deadline uses original double phase and reverse latch");
 const auto firstDue=bank.syncView().frame+400;bank.apply(A::applyStart,0);
 for(unsigned i=0;i<10;++i)bank.tick({});bank.setStartDelay(0,0.125);bank.apply(A::applyStart,0);
 for(unsigned i=0;i<390;++i)bank.tick({});bank.tick({});
 check(bank.view()[0].position==193&&bank.view()[0].phaseApplyFrame==firstDue+10,"rapid phase releases retain the first deadline and later pending source");
 for(unsigned i=0;i<9;++i)bank.tick({});bank.tick({});check(bank.view()[0].position==33&&bank.view()[0].phaseApplyFrame==0,"rapid phase releases apply their second deadline");
 for(unsigned i=0;i<127;++i)check(bank.apply(A::applyStart,0),"bounded phase FIFO accepts reserved slots");
 check(!bank.apply(A::applyStart,0),"full phase FIFO rejects without dropping accepted sources");
 bank.configureSync({SyncMode::off,999,1,4,4,false});bank.setStartDelay(0,0.75);
 check(!bank.apply(A::quantizeStart,0)&&bank.view()[0].startDelay==0.75,"Q FIFO rejection restores metadata at full memory budget");
 check(!bank.apply(A::nudgeStart,0,1)&&bank.view()[0].startDelay==0.75,"nudge FIFO rejection preserves phase and PCM at full pool");
 check(!bank.apply(A::nudgeStart,0,0)&&!bank.apply(A::nudgeStart,0,0.5f)&&!bank.apply(A::nudgeStart,20,1)&&!bank.apply(A::nudgeStart,0,std::numeric_limits<float>::quiet_NaN()),"invalid nudge directions and tracks rejected");
 bank.apply(A::stopAll,0);for(unsigned i=0;i<1000;++i)bank.tick({});check(bank.view()[0].state==LoopState::stopped&&bank.view()[0].phaseApplyFrame==0&&bank.view()[0].startDelay==0.75,"panic cancels restart but preserves phase metadata");
 check(bank.apply(A::applyStart,0)&&bank.view()[0].phaseApplyFrame==0,"release on stopped loop does not start playback");
 check(bank.apply(A::quantizeStart,0)&&bank.view()[0].startDelay==1&&bank.view()[0].state==LoopState::stopped&&bank.view()[0].phaseApplyFrame==0,"stopped Q rounds half up, preserves endpoint one and never starts playback");
 check(!bank.apply(A::quantizeStart,20),"Q invalid track rejected");
 check(!bank.setStartDelay(20,0.5)&&!bank.setStartDelay(0,-0.1)&&!bank.setStartDelay(0,1.1)&&!bank.setStartDelay(0,std::numeric_limits<double>::quiet_NaN()),"invalid phase rejected");
 bank.setStartDelay(0,0.01);check(bank.apply(A::nudgeStart,0,-1)&&std::abs(bank.view()[0].startDelay-(0.01-(60.0/999.0/16.0)/(257.0/8000.0)+1))<1e-14&&bank.view()[0].state==LoopState::stopped&&bank.view()[0].phaseApplyFrame==0,"stopped left nudge wraps without playing");
 bank.apply(A::clear,0);check(bank.view()[0].startDelay==0&&bank.view()[0].phaseApplyFrame==0,"DEL clears phase and pending restart");watch=false;
 bank.prepare(8192,0,8000);watch=true;bank.apply(A::record,0);for(unsigned i=0;i<4000;++i)bank.tick({0.1f,0.2f});bank.apply(A::finish,0);watch=false;wait(bank);
 watch=true;bank.setStartDelay(0,std::nextafter(0.125,0.0));check(bank.apply(A::quantizeStart,0)&&bank.view()[0].startDelay==0,"Q below half matches JavaScript without adding rounding noise");
 bank.setStartDelay(0,0.125);check(bank.apply(A::quantizeStart,0)&&bank.view()[0].startDelay==0.25,"Q exact half rounds upward on four divisions");
 bank.setStartDelay(0,0.375);check(bank.apply(A::quantizeStart,0)&&bank.view()[0].startDelay==0.5,"Q repeated half rounding");
 bank.setStartDelay(0,0);check(bank.apply(A::nudgeStart,0,-1)&&bank.view()[0].startDelay==0.9375,"left nudge wraps one 1/64 note");
 check(bank.apply(A::nudgeStart,0,1)&&bank.view()[0].startDelay==1,"right nudge retains exact endpoint one");
 check(bank.apply(A::nudgeStart,0,1)&&bank.view()[0].startDelay==0.0625,"right nudge wraps past endpoint");
 bank.apply(A::clear,0);for(unsigned i=0;i<500;++i)bank.tick({});check(bank.apply(A::quantizeStart,0)&&bank.view()[0].state==LoopState::empty&&bank.view()[0].phaseApplyFrame==0,"Q clear cancels all future sources, empty Q harmless");watch=false;
 watch=true;check(bank.apply(A::nudgeStart,0,1)&&bank.view()[0].startDelay==0&&bank.view()[0].phaseApplyFrame==0,"empty nudge harmless");watch=false;
 bank.prepare(4096,0,8000);watch=true;bank.apply(A::record,0);for(unsigned i=0;i<17;++i)bank.tick({0.2f,0.1f});bank.apply(A::finish,0);watch=false;wait(bank);
 watch=true;bank.setStartDelay(0,0.217);for(unsigned i=0;i<100;++i){check(bank.apply(A::nudgeStart,0,i%2?-1:1),"rapid tiny nudges accepted at full pool");const double phase=bank.view()[0].startDelay;check(phase>=0&&phase<=1,"multi-cycle tiny nudges stay in valid phase range");}
 bank.apply(A::stopAll,0);for(unsigned i=0;i<500;++i)bank.tick({});check(bank.view()[0].state==LoopState::stopped&&bank.view()[0].phaseApplyFrame==0,"panic cancels every rapid nudge");watch=false;
 // Production Engine: same timestamped controls, actual PCM and exact block invariance.
 for(const unsigned variant:{0u,1u,2u}){
 std::vector<float> reference;
 for(const int block:{64,128}){
  auto engine=std::make_unique<Engine>();engine->prepare(8000,8192);engine->setMonitoring(false);engine->setTarget(1);
  engine->enqueueLoop({A::record,0,1,engine->generation(),0,0});std::vector<float> capture(512);for(unsigned i=0;i<512;++i)capture[i]=0.2f*std::sin(i*0.09f);float* input[]={capture.data()};watch=true;engine->process(input,1,512);watch=false;
  engine->enqueueLoop({A::finish,0,2,engine->generation(),512,0});float zero=0,*channels[]={&zero};watch=true;engine->process(channels,1,1);watch=false;
  EngineView view{},latest{};for(unsigned n=0;n<10000;++n){watch=true;engine->process(channels,1,0);watch=false;while(engine->popView(view))latest=view;if(latest.tracks[0].state==LoopState::playing)break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  check(engine->clock()==513&&latest.tracks[0].state==LoopState::playing,"engine phase fixture commits without advancing time");
  engine->enqueueLoop({A::startDelay,0,3,engine->generation(),640,0.333});engine->enqueueLoop({A::applyStart,0,4,engine->generation(),704,0});
  if(variant>0)engine->enqueueLoop({A::quantizeStart,0,5,engine->generation(),720,0});
  if(variant>1)engine->enqueueLoop({A::nudgeStart,0,6,engine->generation(),736,1});
  std::vector<float> pcm(2048),right(2048);watch=true;for(unsigned offset=0;offset<2048;offset+=block){float* out[]={pcm.data()+offset,right.data()+offset};engine->process(out,2,block);}watch=false;
  while(engine->popView(view))latest=view;check(latest.tracks[0].startDelay==(variant>1?0.48828125:variant>0?0:0.333),"SPSC preserves double phase; optional audio-owned Q follows in order");check(pcm==right,"phase stereo equality");if(reference.empty())reference=pcm;else check(reference==pcm,"phase exact 64/128 equality");
 }
 }
 check(allocations.load()==0,"phase processing has zero callback new/delete");
 std::cout<<"PASS: Start metadata/full precision, 50ms source/reverse latch, panic/DEL/guards, full pool, Engine stereo/64/128 and callback allocation contract\n";
}
