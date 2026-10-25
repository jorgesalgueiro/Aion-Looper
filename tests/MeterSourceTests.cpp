#include "core/Engine.h"
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
extern thread_local bool watch;
extern std::atomic<int> allocations;
namespace {void check(bool ok,const char* label){if(!ok){std::cerr<<label<<'\n';std::exit(1);}}}
void meterSourceTests(){
 using namespace aion;
 unsigned cases=0;
 for(double rate:{8000.0,48000.0})for(unsigned id:{0u,19u}){
  auto bank=std::make_unique<LoopBank>();bank->prepare(81920,0,rate);
  const auto empty=bank->trackView(id).sourceRevision;
  check(bank->apply(LoopAction::record,id),"Source fixture capture starts");
  watch=true;for(unsigned n=0;n<4096;++n)bank->tick({0.3f*std::sin(n*0.031f),0.2f*std::cos(n*0.043f)});watch=false;
  check(bank->trackView(id).sourceRevision==empty,"Capture without playback has no source transition");
  bank->apply(LoopAction::finish,id);bank->completeOfflineEdits();
  auto revision=bank->trackView(id).sourceRevision;
  check(revision==empty+1&&bank->view()[id].sourceRevision==revision,"Committed autoplay creates one native source revision");
  watch=true;for(unsigned n=0;n<2048;++n)bank->tick({});
  bank->apply(LoopAction::volume,id,0.8f);bank->apply(LoopAction::pan,id,4);bank->apply(LoopAction::mute,id,1);bank->apply(LoopAction::mute,id,0);
  bank->apply(LoopAction::reverse,id,1);bank->apply(LoopAction::stutterStart,id);bank->apply(LoopAction::stutterStop,id);
  check(bank->trackView(id).sourceRevision==revision,"Controls, reverse latch and STUT retain their current source");
  check(bank->apply(LoopAction::retrigger,id),"Healthy retrigger accepted");
  const auto deadline=bank->trackView(id).phaseApplyFrame;
  check(deadline>6144&&bank->trackView(id).sourceRevision==revision,"Future RTRG does not label old PCM as its replacement");
  for(std::uint64_t frame=6144;frame<deadline;++frame)bank->tick({});
  check(bank->trackView(id).sourceRevision==revision,"Source stamp unchanged through last pre-deadline sample");
  std::array<float,2048> before{},after{};bank->copyMeterWindow(id,before);bank->tick({});bank->copyMeterWindow(id,after);
  check(bank->trackView(id).sourceRevision==++revision,"RTRG replacement stamped at actual source application sample");
  check(std::equal(after.begin(),after.end()-1,before.begin()+1),"Healthy replacement preserves rolling analyser history");
  watch=false;
  const auto normSource=revision;
  check(bank->apply(LoopAction::normalize,id),"NORM starts");bank->completeOfflineEdits();
  check(bank->trackView(id).sourceRevision==normSource,"NORM retains source identity even if stored clip changes");
  // Cancellation epochs are not source revisions. A queued restart cancelled by
  // STOP must never add another transition when its old deadline arrives.
  watch=true;check(bank->apply(LoopAction::retrigger,id),"Second scheduled RTRG");
  const auto cancelledDeadline=bank->trackView(id).phaseApplyFrame;
  bank->apply(LoopAction::stopImmediate,id);check(bank->trackView(id).sourceRevision==++revision,"Actual source stop invalidates old telemetry");
  bank->apply(LoopAction::stopImmediate,id);check(bank->trackView(id).sourceRevision==revision,"Repeated stopped STOP is not a new source");
  for(unsigned n=0;n<static_cast<unsigned>(rate*0.01)+1;++n)bank->tick({});
  check(cancelledDeadline&&bank->trackView(id).sourceRevision==revision,"Cancelled future source cannot resurrect a meter stamp");
  bank->apply(LoopAction::play,id);check(bank->trackView(id).sourceRevision==++revision,"Same-clip replay has a new source token");watch=false;
  for(auto mode:{SyncMode::fixed,SyncMode::dynamic}){
   bank->configureSync({mode,240,1,1,4,false});
   watch=true;bank->apply(LoopAction::stop,id);check(bank->trackView(id).state==LoopState::stopping&&bank->trackView(id).sourceRevision==revision,"Queued stop retains active source until its grid");
   unsigned n=0;while(bank->trackView(id).state==LoopState::stopping&&n++<rate)bank->tick({});
   check(bank->trackView(id).state==LoopState::stopped&&bank->trackView(id).sourceRevision==++revision,"Grid stop advances exactly once");
   bank->apply(LoopAction::play,id);check(bank->trackView(id).state==LoopState::queued&&bank->trackView(id).sourceRevision==revision,"Queued play does not advance a source early");
   n=0;while(bank->trackView(id).state==LoopState::queued&&n++<rate)bank->tick({});
   check(bank->trackView(id).state==LoopState::playing&&bank->trackView(id).sourceRevision==++revision,"Grid source starts exactly once");watch=false;
  }
  watch=true;bank->apply(LoopAction::clear,id);check(bank->trackView(id).sourceRevision==++revision,"Clear invalidates prior source without recycling serial");
  bank->resetTimeline();check(bank->trackView(id).sourceRevision==++revision,"Timeline reset invalidates source stamp");watch=false;++cases;
 }
 for(unsigned block:{64u,128u}){
  auto engine=std::make_unique<Engine>();engine->prepare(48000,81920);engine->configureSync({SyncMode::off,120,1,4,4,false});engine->setMonitoring(false);engine->setTarget(1);
  engine->enqueueLoop({LoopAction::record,19,1,engine->generation(),0,0});engine->enqueueLoop({LoopAction::finish,19,2,engine->generation(),4096,0});
  std::array<float,128> l{},r{};float* channels[]={l.data(),r.data()};
  for(unsigned offset=0;offset<8192;offset+=block){for(unsigned i=0;i<block;++i){l[i]=0.3f*std::sin((offset+i)*0.031f);r[i]=0.2f*std::cos((offset+i)*0.043f);}watch=true;engine->process(channels,2,static_cast<int>(block),{},offset<=4096);watch=false;}
  auto packet=std::make_unique<MeterWindowEvent>();auto view=std::make_unique<EngineView>();
  while(engine->popView(*view)){}
  watch=true;engine->requestLoopMeterWindow(19);engine->process(nullptr,0,0);check(engine->popLoopMeterWindow(*packet)&&engine->popView(*view)&&sameLoopMeterSource(*packet,*view),"Actual message packet matches coherent source view");
  const auto old=*packet;const auto source=packet->sourceRevision;
  engine->enqueueLoop({LoopAction::retrigger,19,3,engine->generation(),engine->clock(),0});engine->process(channels,2,static_cast<int>(block));while(engine->popView(*view)){}
  check(sameLoopMeterSource(old,*view),"Pre-deadline snapshot still belongs to old native source");
  unsigned callbacks=0;while(engine->inspectAudioThread(19).track.sourceRevision==source&&callbacks++<16){engine->process(channels,2,static_cast<int>(block));while(engine->popView(*view)){};}
  check(engine->inspectAudioThread(19).track.sourceRevision!=source,"Scheduled replacement must apply within its bounded deadline");
  check(!sameLoopMeterSource(old,*view)&&old.clearEpoch==view->tracks[19].clearEpoch&&old.waveformId==view->tracks[19].waveformId,"Same-clip same-generation delayed packet is rejected after source replacement");
  engine->requestLoopMeterWindow(19);engine->process(nullptr,0,0);check(engine->popLoopMeterWindow(*packet)&&engine->popView(*view)&&sameLoopMeterSource(*packet,*view)&&packet->sourceRevision!=source,"New source packet recovers without graph/analyser reset");
  auto bad=*packet;bad.generation=0;check(!sameLoopMeterSource(bad,*view),"Reject zero generation");bad=*packet;bad.track=20;check(!sameLoopMeterSource(bad,*view),"Reject invalid track");bad=*packet;++bad.clearEpoch;check(!sameLoopMeterSource(bad,*view),"Reject stale clear");bad=*packet;++bad.waveformId;check(!sameLoopMeterSource(bad,*view),"Reject stale clip");
  watch=false;++cases;
 }
 check(allocations.load()==0,"Source stamps and view validation have zero callback new/delete");
 std::cout<<"PASS: meter native source revisions, exact RTRG deadline/history, controls/NORM preservation, cancelled sources, grid stop/play, clear/reset and actual same-clip stale packet rejection at 64/128; "<<cases<<" fixtures, zero tracked new/delete\n";
}
