// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>
#include <string_view>
namespace {
thread_local bool watch=false;
thread_local unsigned operations=0;
std::atomic<bool> failAllocation{false},pauseAllocation{false},entered{false},releaseAllocation{false};
void check(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
struct AudioScope{AudioScope(){operations=0;watch=true;}~AudioScope(){watch=false;check(!operations,"Arp connected callback new/delete");}};
}
void* operator new(std::size_t n){
 if(watch)++operations;
 if(failAllocation.exchange(false))throw std::bad_alloc();
 if(pauseAllocation.exchange(false)){entered.store(true,std::memory_order_release);while(!releaseAllocation.load(std::memory_order_acquire))std::this_thread::yield();}
 if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watch&&p)++operations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace aion;
struct Fixture {
 std::unique_ptr<Engine> engine=std::make_unique<Engine>();unsigned block;
 std::array<float,128> left{},right{};
 explicit Fixture(unsigned b):block(b){engine->prepare(8000,32768);engine->setMonitoring(false);}
 void pump(bool zero=true){float* audio[]{left.data(),right.data()};AudioScope guard;engine->process(zero?nullptr:audio,zero?0:2,zero?0:block);}
 LoopView view(unsigned id=0){return engine->inspectAudioThread(id).track;}
 PreparedArpInspection graph(unsigned id=0){AudioScope guard;return engine->inspectPreparedArpAudioThread(id);}
 ArpPreparationRequest request(unsigned id,unsigned track=0){
  ArpPreparationRequest r;r.track=track;r.requestId=id;r.generation=engine->generation();r.clearEpoch=view(track).clearEpoch;
  r.values=arpFactoryTargets();r.seed=42+id;r.activate=true;r.settingsRevision=view(track).arpSettingsRevision;return r;
 }
 ArpPreparationResult result(){ArpPreparationResult r;const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(!engine->popArpPreparation(r)){pump();check(std::chrono::steady_clock::now()<until,"connected preparation timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}return r;
 }
 void expect(unsigned id,ArpPreparationStatus status){const auto r=result();check(r.requestId==id&&r.status==status,"connected preparation terminal identity/status");}
 LoopCommand control(unsigned request,const ArpRenderSettings& settings,std::uint32_t mask=arpAllParameters){
  LoopCommand c{LoopAction::arpDelay,0,request,engine->generation()};c.arp=settings;c.arpMask=mask;c.arpClearEpoch=view().clearEpoch;c.arpSettingsRevision=view().arpSettingsRevision;return c;
 }
};
static void transactions(unsigned block){
 Fixture f(block);check(f.view().arp.values[1]==static_cast<float>(2.0/11.1)&&f.view().arp.values[1]!=arpModDefaults()[1],"factory repetitions mapping, not raw worklet default");
 auto r=f.request(1);r.values[2]=.3f;check(f.engine->enqueueArpPreparation(r),"typed connected prepare");
 check(!f.view().arp.enabled&&!f.graph().active,"admission cannot publish enabled settings without graph");f.expect(1,ArpPreparationStatus::ready);
 auto before=f.view();auto graph=f.graph();check(before.arp==ArpRenderSettings{r.values,r.seed,true}&&before.arpSettingsRevision==1&&before.arpRequest==1&&graph.active&&graph.targets==before.arp.values,"settings commit in the adoption callback");
 r=f.request(2);r.values[2]=.7f;failAllocation=true;check(f.engine->enqueueArpPreparation(r),"typed OOM request");f.expect(2,ArpPreparationStatus::failed);
 check(!failAllocation&&f.view().arp==before.arp&&f.view().arpSettingsRevision==before.arpSettingsRevision&&f.graph().targets==graph.targets&&f.graph().state==graph.state,"OOM preserves complete old settings and graph");
 r=f.request(3);r.activate=false;check(f.engine->enqueueArpPreparation(r),"raw prepare can be admitted for validation");f.expect(3,ArpPreparationStatus::busy);check(f.view().arp==before.arp&&f.graph().state==graph.state,"raw diagnostics cannot replace a live effect");
 r=f.request(4);r.settingsRevision=0;check(f.engine->enqueueArpPreparation(r),"stale settings intent");f.expect(4,ArpPreparationStatus::stale);
 entered=false;releaseAllocation=false;pauseAllocation=true;r=f.request(5);r.values[2]=.8f;check(f.engine->enqueueArpPreparation(r),"pause typed replacement");f.pump();
 const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!entered.load(std::memory_order_acquire)){check(std::chrono::steady_clock::now()<until,"worker allocation barrier");std::this_thread::yield();}
 auto edited=before.arp;edited.values[2]=.2f;auto c=f.control(101,edited,1u<<2);check(f.engine->enqueueLoop(c),"newer direct live target");f.pump(false);f.pump();f.expect(5,ArpPreparationStatus::cancelled);
 releaseAllocation.store(true,std::memory_order_release);check(f.view().arp==edited&&f.graph().targets==edited.values,"new edit invalidates unfinished replacement before adoption");
 auto bad=f.control(102,edited);bad.arp.values[0]=NAN;check(!f.engine->enqueueLoop(bad),"invalid render settings reject before enqueue");
 bad=f.control(103,edited,0);bad.arp.values[0]=.8f;check(f.engine->enqueueLoop(bad),"bad mask enters audio validation");f.pump(false);check(f.view().arp==edited,"unmasked changes reject atomically");
 bad=f.control(104,edited);--bad.arpSettingsRevision;check(f.engine->enqueueLoop(bad),"stale direct command");f.pump(false);check(f.view().arp==edited,"stale direct revision cannot overwrite newer targets");
 auto off=edited;off.enabled=false;c=f.control(105,off,0);check(f.engine->enqueueLoop(c),"direct disable");f.pump(false);check(f.view().arp==off&&!f.graph().active,"disable retires graph off audio, preserving controls");
 auto on=off;on.enabled=true;c=f.control(106,on,0);check(f.engine->enqueueLoop(c),"direct enable reaches audio validation");f.pump(false);check(!f.view().arp.enabled&&!f.graph().active,"enable without worker/adoption is rejected");
 r=f.request(6);check(f.engine->enqueueArpPreparation(r),"fresh typed enable after retirement");f.expect(6,ArpPreparationStatus::ready);check(f.view().arp.enabled&&f.graph().targets==r.values,"fresh enable recovers with matching settings");
 // Cancel an armed take while replacement allocation is paused. The old active
 // graph is deliberately retained: its existence is NOT a ready receipt.
 const auto retained=f.view().arp;entered=false;releaseAllocation=false;pauseAllocation=true;r=f.request(7);r.values[2]=.9f;
 check(f.engine->enqueueArpPreparation(r),"replacement before armed cancellation");f.pump();
 const auto armUntil=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!entered.load(std::memory_order_acquire)){check(std::chrono::steady_clock::now()<armUntil,"arm cancellation allocation barrier");std::this_thread::yield();}
 SyncSettings sync;sync.mode=SyncMode::fixed;sync.bpm=120;sync.bars=1;f.engine->configureSync(sync);
 check(f.engine->enqueueLoop({LoopAction::record,0,110,f.engine->generation()})&&f.engine->enqueueLoop({LoopAction::finish,0,111,f.engine->generation()}),"arm then cancel take");f.pump(false);
 f.expect(7,ArpPreparationStatus::cancelled);releaseAllocation.store(true,std::memory_order_release);
 check(f.view().state==LoopState::empty&&f.view().arp==retained&&f.view().arpRequest==6&&f.graph().active,"arm cancellation preserves current effect but never acknowledges obsolete replacement as ready");
 f.engine->prepare(8000,32768);check(f.view().arp.enabled&&f.graph().active&&f.graph().frame==0,"same-rate stopped prepare retains valid owner and resets clock");
 check(f.engine->enqueueLoop({LoopAction::clear,0,107,f.engine->generation()}),"connected DEL");f.pump(false);check(f.view().arp==ArpRenderSettings{}&&!f.graph().active,"DEL clears both metadata and graph");
 for(unsigned id=0;id<20;++id)check(f.engine->enqueueArpPreparation(f.request(200+id,id)),"twenty typed installs");
 for(unsigned n=0;n<20;++n){auto ready=f.result();check(ready.status==ArpPreparationStatus::ready,"all-track typed preparation success");const auto v=f.view(ready.track);check(v.arp.enabled&&v.arpRequest==ready.requestId&&f.graph(ready.track).targets==v.arp.values,"every ready receipt has matching audio-owned settings");}
 check(f.engine->enqueueLoop({LoopAction::clearAll,0,300,f.engine->generation()}),"connected DEL ALL");f.pump(false);
 for(unsigned id=0;id<20;++id)check(!f.view(id).arp.enabled&&!f.graph(id).active,"DEL ALL leaves no enabled orphan");
 r=f.request(400);check(f.engine->enqueueArpPreparation(r),"enabled owner before rate change");f.expect(400,ArpPreparationStatus::ready);
 f.engine->prepare(96000,32768);check(f.view().arp==ArpRenderSettings{}&&!f.graph().active,"changed-rate core prepare clears metadata and obsolete storage together; product restore must reprepare");
 std::cout<<"PASS atomic Arp settings/adoption/OOM/revision/disable/all-track lifecycle "<<block<<'\n';
}
static void connected(unsigned block,double rate){
 auto dry=std::make_unique<Engine>(),wet=std::make_unique<Engine>();
 for(auto* e:{dry.get(),wet.get()}){e->prepare(rate,65536);e->setMonitoring(false);check(e->enqueue({0,1})&&e->enqueueLoop({LoopAction::record,0,1,e->generation()}),"Arp connected record fixture");}
 std::array<float,128> dl{},dr{},wl{},wr{};float* d[]{dl.data(),dr.data()},* w[]{wl.data(),wr.data()};std::uint64_t frame=0;
 auto render=[&](bool input){for(unsigned i=0;i<block;++i)dl[i]=dr[i]=wl[i]=wr[i]=input?static_cast<float>(.03*std::sin((frame+i)*.073)+.02*std::sin((frame+i)*.031)):0;{AudioScope guard;dry->process(d,2,block,{},true);wet->process(w,2,block,{},true);}frame+=block;};
 auto paired=[&](LoopAction a,unsigned id,double value=0){for(auto* e:{dry.get(),wet.get()})check(e->enqueueLoop({a,0,id,e->generation(),0,value}),"paired Arp source action");};
 for(unsigned n=0;n<8192/block;++n)render(true);paired(LoopAction::finish,1);render(false);paired(LoopAction::play,2);for(unsigned n=0;n<2048/block;++n)render(false);
 ArpRenderSettings settings;settings.enabled=true;settings.seed=99;settings.values[0]=.023f;settings.values[2]=.45f;settings.values[9]=.2f;settings.values[10]=.7f;
 ArpPreparationRequest request;request.requestId=10;request.generation=wet->generation();request.values=settings.values;request.seed=settings.seed;request.activate=true;request.clearEpoch=wet->inspectAudioThread().track.clearEpoch;
 check(wet->enqueueArpPreparation(request),"prepare connected A");ArpPreparationResult result;const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 while(!wet->popArpPreparation(result)){{AudioScope guard;wet->process(nullptr,0,0);}check(std::chrono::steady_clock::now()<until,"connected A ready timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 check(result.status==ArpPreparationStatus::ready&&wet->inspectAudioThread().track.arp==settings,"A install is atomic before first rendered sample");
 ArpDelayGraph arp;SmoothedEqualizer eq;Compressor comp;
 EqSettings qs;qs.enabled=true;qs.values[4]=4;CompressorSettings cs;cs.enabled=true;cs.values[0]=-30;cs.values[5]=1;
 check(arp.prepare(rate,settings.values,settings.seed,frame)&&eq.prepare(rate,qs.values,frame)&&comp.prepare(rate,cs.values,frame),"independent Q/C/A owners");
 LoopCommand q{LoopAction::equalizer,0,11,wet->generation()};q.equalizer=qs;LoopCommand c{LoopAction::compressor,0,12,wet->generation()};c.compressor=cs;
 check(wet->enqueueLoop(q)&&wet->enqueueLoop(c),"Q/C alongside A");
 SignalChain order;double error=0,nonzero=0;constexpr float centre=.7071067811865475f;
 std::uint64_t controlAt=UINT64_MAX,gateClosedAt=UINT64_MAX;std::uint32_t controlMask=0;ArpRenderSettings controlSettings;
 auto checkBlock=[&](std::uint64_t begin,const SignalChain& next,std::uint64_t changeAt,bool input=false){
  render(input);
  for(unsigned i=0;i<block;++i){
   if(begin+i==changeAt){if(next.contains('A')&&!order.contains('A'))check(arp.reset(settings.seed,begin+i),"fresh A on reinsertion");if(next.contains('Q')&&!order.contains('Q'))check(eq.prepare(rate,qs.values,begin+i),"fresh Q on reinsertion");if(next.contains('C')&&!order.contains('C'))check(comp.prepare(rate,cs.values,begin+i),"fresh C on reinsertion");order=next;}
   if(begin+i==controlAt){for(unsigned p=0;p<12;++p)if(controlMask&(1u<<p))check(arp.setTarget(p,controlSettings.values[p]),"independent masked Arp gesture");settings=controlSettings;controlAt=UINT64_MAX;}
   float l=dl[i]/centre,r=dr[i]/centre;
   for(char code:order.text()){
    if(code=='Q')eq.process(&l,&r,&l,&r,1);else if(code=='C')comp.process(&l,&r,2,&l,&r,1);else if(code=='A')check(arp.process(&l,&r,2,&l,&r,1),"independent A render");
    if(begin+i>=gateClosedAt)l=r=0;
   }
   const auto expected=std::clamp((l+r)*.5f*centre,-1.f,1.f);error=std::max({error,std::abs(static_cast<double>(wl[i])-expected),std::abs(static_cast<double>(wr[i])-expected)});nonzero=std::max(nonzero,std::abs(static_cast<double>(wl[i])));
  }
  const auto info=wet->inspectAudioThread().track;PreparedArpInspection actual;{AudioScope guard;actual=wet->inspectPreparedArpAudioThread(0);}
  check(info.arp==settings&&actual.targets==settings.values&&actual.frame==arp.position(),"render settings/automation clock follow live owner and omission");
 };
 auto controls=[&](const ArpRenderSettings& target,std::uint32_t mask){
  const auto info=wet->inspectAudioThread().track;LoopCommand command{LoopAction::arpDelay,0,21,wet->generation(),frame+31};
  command.arp=target;command.arpMask=mask;command.arpClearEpoch=info.clearEpoch;command.arpSettingsRevision=info.arpSettingsRevision;
  check(wet->enqueueLoop(command),"scheduled masked Arp control");controlAt=command.frame;controlMask=mask;controlSettings=target;
 };
 unsigned index=0;
 for(const auto text:{"QCA","AQC","QAC","CQA","CAQ","A","QC","QCA"}){
  const auto next=normalizedSignalChain(text);const auto at=frame+17;LoopCommand change{LoopAction::signalChain,0,20,wet->generation(),at};change.signalChain=next;change.chainClearEpoch=wet->inspectAudioThread().track.clearEpoch;check(wet->enqueueLoop(change),"user-ordered A chain");
  auto target=settings;target.values={.011f+index*.002f,.2f+index*.04f,index%2?.6f:.3f,index%2?1.f:0.f,static_cast<float>(index+2),index%2?1.f:0.f,130.f+index, .8f+index*.02f,1.f+index*.1f,.1f+index*.03f,.5f+index*.1f,.8f};
  controls(target,arpAllParameters);
  for(unsigned n=0;n<1024/block;++n){if(n==1)controls(settings,1u<<2);const auto begin=frame;checkBlock(begin,next,at);}
  ++index;
 }
 // Keep A active across source/history operations; independent owners are not reset.
 for(auto action:{LoopAction::overdub,LoopAction::substitute,LoopAction::multiply}){
  paired(action,30,1);auto target=settings;target.values[2]=settings.values[2]==.3f?.6f:.3f;controls(target,1u<<2);
  const auto state=action==LoopAction::overdub?LoopState::overdubbing:action==LoopAction::substitute?LoopState::substituting:LoopState::multiplying;
  for(unsigned n=0;n<512/block;++n){const auto begin=frame;checkBlock(begin,order,UINT64_MAX,true);check(wet->inspectAudioThread().track.state==state,"live Arp control during actual OD/SUB/MULT");}
  paired(LoopAction::finish,31);checkBlock(frame,order,UINT64_MAX);
 }
 for(auto action:{LoopAction::retrigger,LoopAction::undo,LoopAction::redo}){paired(action,32);for(unsigned n=0;n<1024/block;++n)checkBlock(frame,order,UINT64_MAX);}
 const auto old=wet->inspectAudioThread().track;
 auto replacement=request;replacement.requestId=50;replacement.values[2]=.9f;replacement.seed=123;
 replacement.settingsRevision=old.arpSettingsRevision;replacement.clearEpoch=old.clearEpoch;
 failAllocation=true;check(wet->enqueueArpPreparation(replacement),"audible replacement OOM admitted");
 const auto oomUntil=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 while(!wet->popArpPreparation(result)){{AudioScope guard;wet->process(nullptr,0,0);}check(std::chrono::steady_clock::now()<oomUntil,"audible OOM terminal timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 check(result.status==ArpPreparationStatus::failed&&wet->inspectAudioThread().track.arp==settings,"failed replacement preserves live settings");
 for(unsigned n=0;n<1024/block;++n)checkBlock(frame,order,UINT64_MAX); // Exact old tail, STAY sequence, LFO and controls.
 // The original defers edits in scheduled STOP; the owner approved live controls.
 SyncSettings fixed;fixed.mode=SyncMode::fixed;fixed.bpm=120;fixed.bars=1;
 for(auto* e:{dry.get(),wet.get()})e->configureSync(fixed);
 paired(LoopAction::stop,51);auto target=settings;target.values[2]=.31f;controls(target,1u<<2);std::uint64_t deadline=0;
 for(unsigned n=0;n<512/block;++n){
  checkBlock(frame,order,UINT64_MAX);const auto info=wet->inspectAudioThread().track;
  check(info.state==LoopState::stopping&&info.scheduledFrame>frame,"live Arp edit retains scheduled STOP");
  if(!deadline)deadline=info.scheduledFrame;check(deadline==info.scheduledFrame,"Arp edit cannot move STOP deadline");
 }
 for(auto* e:{dry.get(),wet.get()})e->configureSync(SyncSettings{});
 // Ordinary STOP/PLAY retain delay/STAY/modulation; panic resets runtime in
 // constant work but keeps settings and prepared allocation. Both gate tails.
 paired(LoopAction::stopImmediate,33);gateClosedAt=frame+static_cast<unsigned>(rate*.005);
 for(unsigned n=0;n<1024/block;++n)checkBlock(frame,order,UINT64_MAX);
 for(unsigned i=0;i<block;++i)check(wl[i]==0&&wr[i]==0,"enabled Arp cannot revive ordinary STOP");
 paired(LoopAction::play,34);gateClosedAt=UINT64_MAX;for(unsigned n=0;n<1024/block;++n)checkBlock(frame,order,UINT64_MAX);
 paired(LoopAction::stopAll,35);gateClosedAt=frame+static_cast<unsigned>(rate*.005);
 check(eq.prepare(rate,qs.values,frame)&&comp.prepare(rate,cs.values,frame)&&arp.reset(settings.seed,frame),"independent panic resets, without reallocating A");
 for(unsigned n=0;n<1024/block;++n)checkBlock(frame,order,UINT64_MAX);
 for(unsigned i=0;i<block;++i)check(wl[i]==0&&wr[i]==0,"enabled Arp cannot revive STOP ALL");
 paired(LoopAction::play,36);gateClosedAt=UINT64_MAX;for(unsigned n=0;n<1024/block;++n)checkBlock(frame,order,UINT64_MAX);
 check(error<=1e-6&&nonzero>.001,"connected user-ordered Q/C/A and persistent source/history PCM");
 // Original pan follows the complete chain, and downmixes to mono before its
 // equal-power gains. This is separate from Arp's own stereo auto-pan oracle.
 const auto before=wet->inspectAudioThread().track;
 auto off=settings;off.enabled=false;LoopCommand disable{LoopAction::arpDelay,0,40,wet->generation()};disable.arp=off;disable.arpMask=0;disable.arpClearEpoch=before.clearEpoch;disable.arpSettingsRevision=before.arpSettingsRevision;
 check(wet->enqueueLoop(disable),"connected bypass");render(false);check(wet->inspectAudioThread().track.arp==off,"bypass metadata installed");
 paired(LoopAction::stopImmediate,41);for(unsigned n=0;n<1024/block;++n)render(false);for(unsigned i=0;i<block;++i)check(wl[i]==0&&wr[i]==0,"no retired Arp tail revival");
 std::cout<<"PASS connected user-ordered Arp "<<rate<<" / "<<block<<": peak "<<error<<", signal "<<nonzero<<'\n';
}
static void benchmark(unsigned midiCount=0){
 for(unsigned block:{64u,128u}){
  auto engine=std::make_unique<Engine>();engine->prepare(48000,262144);engine->setMonitoring(false);check(engine->enqueue({0,1}),"benchmark master");
  std::array<float,128> l{},r{};float* audio[]{l.data(),r.data()};
  auto render=[&](bool input){for(unsigned i=0;i<block;++i)l[i]=r[i]=input?static_cast<float>(.03*std::sin(i*.073)):0;engine->process(audio,2,block,{},true);};
  for(unsigned id=0;id<20;++id){check(engine->enqueueLoop({LoopAction::record,id,1,engine->generation()}),"benchmark record");for(unsigned n=0;n<1024/block;++n)render(true);check(engine->enqueueLoop({LoopAction::finish,id,2,engine->generation()}),"benchmark finish");render(false);check(engine->enqueueLoop({LoopAction::play,id,3,engine->generation()}),"benchmark play");render(false);}
  std::array<EqSettings,20> eq{};std::array<CompressorSettings,20> comp{};for(unsigned id=0;id<20;++id){eq[id].enabled=true;comp[id].enabled=true;}
  check(engine->enqueueEffectsBank(eq,comp,100),"benchmark Q/C");render(false);
  for(unsigned id=0;id<20;++id){ArpPreparationRequest request;request.track=id;request.requestId=200+id;request.generation=engine->generation();request.values=arpFactoryTargets();request.seed=42+id;request.activate=true;check(engine->enqueueArpPreparation(request),"benchmark A preparation");}
  unsigned ready=0;const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);ArpPreparationResult receipt;
  while(ready<20){engine->process(nullptr,0,0);while(engine->popArpPreparation(receipt)){check(receipt.status==ArpPreparationStatus::ready,"benchmark A ready");++ready;}check(std::chrono::steady_clock::now()<until,"benchmark preparation timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  std::array<MidiEvent,256> events{};
  if(midiCount){MidiSettings midi;midi.enabled=true;for(unsigned p=0;p<9;++p)midi.bindings[MidiSettings::arpBase+p]=midiBinding(2,20+p);check(engine->configureMidi(midi),"benchmark MIDI mappings");}
  constexpr unsigned warm=512,measured=2048;std::vector<double> times(measured);double checksum=0;
  for(unsigned n=0;n<warm+measured;++n){
   for(unsigned e=0;e<midiCount;++e)events[e]={e*block/midiCount,static_cast<std::uint8_t>(176+e%16),static_cast<std::uint8_t>(20+e%9),static_cast<std::uint8_t>((n+e)%128),3};
   l.fill(0);r.fill(0);const auto start=std::chrono::steady_clock::now();engine->process(audio,2,block,{events.data(),midiCount});
   const auto us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();if(n>=warm)times[n-warm]=us;
   ArpMidiReceipt midi;unsigned received=0;while(engine->popArpMidi(midi))++received;check(received==midiCount,"benchmark accepts every MIDI edit and drains off audio");
   for(unsigned i=0;i<block;++i)checksum+=std::abs(l[i])+std::abs(r[i]);
  }
  for(unsigned id=0;id<20;++id){const auto view=engine->inspectAudioThread(id).track;check(view.state==LoopState::playing&&view.arp.enabled,"benchmark all twenty Q/C/A loops active");}
  check(checksum>1,"benchmark is not silent");std::sort(times.begin(),times.end());double sum=0;unsigned over=0;const double deadline=block/48000.*1e6;
  for(auto time:times){sum+=time;over+=time>deadline;}
  std::cout<<"{\"scope\":\"20 playing loops, Q/C/A default chain, internal targets; wall callback only, excludes device/UI/roundtrip\",\"midiPerCallback\":"<<midiCount<<",\"block\":"<<block<<",\"rate\":48000,\"warmCallbacks\":"<<warm<<",\"measuredCallbacks\":"<<measured<<",\"meanUs\":"<<sum/measured<<",\"p99Us\":"<<times[measured*99/100]<<",\"maximumUs\":"<<times.back()<<",\"deadlineUs\":"<<deadline<<",\"observedOverruns\":"<<over<<",\"checksum\":"<<checksum<<"}\n";
 }
}
int main(int argc,char** argv){
 if(argc==2&&std::string_view(argv[1])=="benchmark"){benchmark();return 0;}
 if(argc==2&&std::string_view(argv[1])=="benchmark-midi"){benchmark(256);return 0;}
 if(argc==2&&std::string_view(argv[1])=="benchmark-midi8"){benchmark(8);return 0;}
 for(unsigned block:{64u,128u}){transactions(block);for(double rate:{8000.,48000.,96000.})connected(block,rate);}
}
