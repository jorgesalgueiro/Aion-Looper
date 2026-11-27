// SPDX-License-Identifier: AGPL-3.0-only
// Regressions from independent review of the reverb migration after 37206b4.
#include "core/ReverbSettingsOwner.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string_view>
#include <thread>
namespace {
thread_local bool watched = false;
thread_local unsigned operations = 0;
void check(bool ok, const char* why) { if (!ok) { std::cerr << why << '\n'; std::exit(1); } }
struct AudioScope {
 AudioScope() { operations = 0; watched = true; }
 ~AudioScope() { watched = false; check(!operations, "audit regression: callback new/delete"); }
};
}
void* operator new(std::size_t n) { if(watched)++operations; if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { if(watched&&p)++operations;std::free(p); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p,std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p,std::size_t) noexcept { ::operator delete(p); }
using namespace aion;
struct Fixture {
 std::unique_ptr<Engine> engine=std::make_unique<Engine>();
 std::unique_ptr<EngineView> latest=std::make_unique<EngineView>();
 ReverbSettingsOwner owner{*engine};
 std::array<float,128> left{},right{}; unsigned block;
 explicit Fixture(unsigned frames):block(frames) { engine->prepare(8000,16384);engine->setMonitoring(false);pump();check(owner.bind(*latest),"bind default settings owner"); }
 void pump(std::span<const MidiEvent> midi={}) {
  float* channels[]{left.data(),right.data()};
  {AudioScope scope;engine->process(channels,2,static_cast<int>(block),midi);}
  while(engine->popView(*latest)){}
 }
 void settle() { for(unsigned i=0;i<10;++i)pump(); }
 void finishOwner() {
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  do {pump();owner.service(*latest);check(std::chrono::steady_clock::now()<deadline,"owner completion timeout");
      if(owner.track(0)->pending)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }while(owner.track(0)->pending);
 }
};
static void bankThenEdit(unsigned block) {
 Fixture f(block);ReverbSettingsOwner::Bank target{};
 target[0].enabled=true;target[0].values[2]=.1;
 check(f.owner.restore(target,100),"bank request accepted");f.finishOwner();
 check(f.owner.restoreReceipt().status==ReverbEditStatus::ready&&f.owner.bound(),"bank restore completed");f.settle();
 check(f.owner.set(0,2,.2,101),"post-bank prepared edit accepted");f.finishOwner();
 check(f.engine->inspectAudioThread(0).track.reverb.values[2]==.2,"post-bank DSP edit actually committed");
 check(f.owner.track(0)->status==ReverbEditStatus::ready&&f.owner.bound()
       &&f.owner.track(0)->committed.values[2]==.2,"bank followed by prepared edit must acknowledge ready without detaching owner");
 // Interleave another bank (all-off) and prepared enable. Installation stamps
 // must remain fresh regardless of whether the bank allocates a graph.
 auto off=target;off[0].values[2]=.2;off[0].enabled=false;
 f.settle();check(f.owner.restore(off,102),"all-off bank accepted");f.finishOwner();f.settle();
 check(f.owner.enable(0,true,103),"enable after all-off bank accepted");f.finishOwner();
 check(f.owner.bound()&&f.owner.track(0)->status==ReverbEditStatus::ready,"enable after all-off bank has distinct installation identity");
}
static void midiStopAcknowledgement(unsigned block,bool queued) {
 Fixture f(block);const auto generation=f.engine->generation();
 check(f.engine->enqueueLoop({LoopAction::volume,0,17,generation,0,1}),"ordinary acknowledgement marker");f.pump();
 check(f.latest->lastRequestId==17&&!f.latest->rejected,"ordinary marker acknowledged");
 MidiSettings midi;midi.enabled=true;midi.revision=1;check(f.engine->configureMidi(midi),"enable MIDI Stop");f.pump();
 if(queued)check(f.engine->enqueueLoop({LoopAction::volume,0,18,generation,UINT64_MAX,1}),"future ordinary head");
 ReverbBankRequest request;request.requestId=9000;request.generation=generation;request.frame=UINT64_MAX;
 for(unsigned id=0;id<20;++id){const auto info=f.engine->inspectAudioThread(id).track;
  request.clearEpochs[id]=info.clearEpoch;request.settingsRevisions[id]=info.reverbSettingsRevision;}
 std::uint64_t transaction=0;check(f.engine->enqueueReverbBank(request,transaction),"future bank descriptor accepted");
 const MidiEvent stop{0,252,0,0,1};f.pump({&stop,1});
 check(f.latest->lastRequestId==(queued?18u:17u)&&f.latest->rejected==queued,
       "MIDI Stop must not use a tracked reverb-bank ID as an ordinary UI acknowledgement");
 ReverbBankResult receipt;
 for(unsigned i=0;!f.engine->popReverbBank(receipt);++i){check(i<8,"bank Stop cancellation receipt");f.pump();}
 check(receipt.requestId==9000&&receipt.transaction==transaction&&receipt.status==ReverbEditStatus::cancelled,
       "MIDI Stop returns separate correlated bank cancellation");
}
int main(int argc,char** argv) {
 check(argc==3,"usage: reverb audit CASE BLOCK");const std::string_view which=argv[1];const auto block=static_cast<unsigned>(std::strtoul(argv[2],nullptr,10));
 check(block==64||block==128,"audit 64/128 block contract");
 if(which=="bank_edit")bankThenEdit(block);
 else if(which=="midi_stop_pending")midiStopAcknowledgement(block,false);
 else if(which=="midi_stop_queued")midiStopAcknowledgement(block,true);
 else check(false,"unknown audit case");
 std::cout<<"PASS reverb audit "<<which<<" at "<<block<<" samples; zero callback new/delete\n";
}
