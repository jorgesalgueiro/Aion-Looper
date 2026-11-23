// SPDX-License-Identifier: AGPL-3.0-only
#include "core/DelaySettingsOwner.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>
namespace {
thread_local bool watching=false;thread_local unsigned operations=0;
std::atomic<bool> failNext{false};
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct Guard {Guard(){operations=0;watching=true;}~Guard(){watching=false;check(!operations,"Delay owner callback heap operation");}};
}
void* operator new(std::size_t n){if(watching)++operations;if(failNext.exchange(false))throw std::bad_alloc();if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watching&&p)++operations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace aion;
struct Fixture {
 std::unique_ptr<Engine> engine=std::make_unique<Engine>();DelaySettingsOwner owner{*engine};
 EngineView latest{},initial{};unsigned block;std::array<float,128> left{},right{};
 explicit Fixture(unsigned frames):block(frames){engine->prepare(8000,16384);engine->setMonitoring(false);pump();drain();initial=latest;check(owner.bind(latest),"bind actual default bank");}
 void pump(bool zero=false){float* channels[]{left.data(),right.data()};Guard guard;engine->process(zero?nullptr:channels,zero?0:2,zero?0:static_cast<int>(block));}
 void drain(){while(engine->popView(latest)){};}
 void service(){drain();owner.service(latest);}
 void done(unsigned id,bool zero=false){for(unsigned n=0;n<2000;++n){pump(zero);service();if(!owner.track(id)->pending)return;std::this_thread::sleep_for(std::chrono::milliseconds(1));}std::cerr<<"track="<<id<<" request="<<owner.track(id)->requestId<<" bound="<<owner.bound()<<" restoring="<<owner.restoring()<<" frame="<<latest.frame<<" revision="<<owner.track(id)->revision<<" audioRevision="<<latest.tracks[id].delaySettingsRevision<<'\n';check(false,"Delay owner receipt timeout");}
 void settle(){for(unsigned n=0;n<10;++n){pump();service();}}
 void command(LoopAction action,unsigned id=0){check(engine->enqueueLoop({action,id,9999,engine->generation()}),"ordinary barrier accepted");pump();drain();}
 DelaySettingsOwner::Bank bank(){DelaySettingsOwner::Bank result{};for(unsigned id=0;id<20;++id)result[id]=owner.track(id)->committed;return result;}
};
static void controls(unsigned block){
 Fixture f(block);const auto defaults=f.bank();
 check(!f.owner.bind(f.latest)&&!f.owner.set(20,0,.1,1)&&!f.owner.set(0,99,.1,1)&&!f.owner.set(0,0,NAN,1)
  &&!f.owner.preset(0,10,1)&&!f.owner.repeats(0,DelayRepeatMode::exact,0,1)&&!f.owner.enable(0,true,0),"invalid edits leave owner untouched");
 check(f.owner.set(0,0,.123456789012345,1),"precise candidate admission");
 check(f.owner.track(0)->committed==defaults[0]&&f.owner.intent(0)->values[0]==.123456789012345,"intent is separate from committed audio");
 check(!f.owner.set(0,3,.5,2)&&!f.owner.restore(defaults,2),"pending track excludes another edit and bank");
 f.command(LoopAction::volume,19);f.service();
 check(!f.owner.track(0)->pending&&f.owner.track(0)->committed.values[0]==.123456789012345&&f.latest.lastRequestId==9999,"exact receipt wins over unrelated global acknowledgement");
 f.owner.service(f.initial);check(f.owner.bound()&&f.owner.track(0)->revision==1,"stale snapshot cannot roll confirmed settings back");
 check(f.owner.repeats(0,DelayRepeatMode::exact,3,2),"disabled exact mode metadata");f.done(0);
 check(f.owner.intent(0)->repeatCount==3&&!f.owner.intent(0)->enabled,"exact settings commit without enable");
 check(f.owner.preset(0,8,3),"original preset");f.done(0);
 check(f.owner.track(0)->committed.preset==8&&f.owner.track(0)->committed.repeatMode==DelayRepeatMode::feedback,"legacy preset returns to Feedback");
 const auto old=f.bank();check(f.owner.set(0,3,.31,4,UINT64_MAX),"future edit");f.pump(true);f.owner.cancel(0);f.done(0,true);
 check(f.owner.track(0)->status==DelayEditStatus::cancelled&&f.bank()==old,"cancelled intent rolls back without fresh telemetry");
 check(f.owner.set(0,3,.31,4),"same request ID new transaction");f.done(0);check(f.owner.track(0)->committed.values[3]==.31,"obsolete future head cannot strand fresh edit");
 const auto disabled=f.bank();check(f.owner.enable(0,true,5),"enable allocation candidate");failNext=true;f.pump();f.done(0,true);
 check(!failNext&&f.owner.track(0)->status==DelayEditStatus::failed&&f.bank()==disabled,"failed preparation never persists enabled intent");
 check(f.owner.enable(0,true,6),"enable recovery");f.done(0);f.settle();check(f.owner.track(0)->committed.enabled,"enabled only after native adoption");
 check(f.owner.repeats(0,DelayRepeatMode::exact,2,7),"prepared mode replacement");f.done(0);f.settle();
 check(f.owner.track(0)->committed.repeatMode==DelayRepeatMode::exact,"mode replacement confirmed");
 check(f.owner.enable(0,false,8),"disable");f.done(0);
 check(!f.owner.track(0)->committed.enabled&&f.owner.track(0)->committed.repeatCount==2,"disable retains native repeat choice");
 for(unsigned n=0;n<255;++n)check(f.engine->enqueueLoop({LoopAction::volume,19,10000+n,f.engine->generation(),0,1}),"fill ordinary FIFO");
 const auto before=f.bank();check(!f.owner.set(0,3,.7,9)&&f.bank()==before&&!f.owner.track(0)->pending,"full FIFO changes neither candidate nor committed owner");
 f.settle();check(f.owner.set(0,3,.7,9),"admission after FIFO drains");f.done(0);
 check(f.owner.set(0,3,.42,10),"unread success before late cancellation");f.pump();f.owner.cancel(0);f.owner.service(f.initial);
 check(f.owner.track(0)->status==DelayEditStatus::ready&&f.owner.track(0)->committed.values[3]==.42,"late cancellation does not erase an already committed fact");
 f.drain();check(f.owner.set(0,3,.44,11),"unread individual before clear");f.pump();f.command(LoopAction::clear);f.owner.service(f.latest);
 check(f.owner.bound()&&f.owner.track(0)->status==DelayEditStatus::cancelled&&f.owner.track(0)->committed==DelaySettings{},"clear wins over older unread individual success");
 f.owner.service(f.initial);check(f.owner.track(0)->clearEpoch==1,"old view cannot revive cleared individual settings");
}
static void banks(unsigned block){
 Fixture f(block);auto desired=f.bank();for(unsigned id=0;id<20;++id)desired[id].values[0]=.01123456789+id*.001;
 desired[0].enabled=desired[19].enabled=true;desired[19].repeatMode=DelayRepeatMode::exact;desired[19].repeatCount=3;
 auto invalid=desired;invalid[19].values[3]=NAN;check(!f.owner.restore(invalid,20),"invalid last bank member rejects whole owner intent");
 check(f.owner.restore(desired,20)&&f.owner.restoring()&&*f.owner.intent(19)==desired[19],"bank accepted intent");
 check(!f.owner.enable(19,false,21)&&!f.owner.restore(desired,21),"bank excludes every track and duplicate bank");
 f.done(19);check(!f.owner.restoring()&&f.bank()==desired&&f.owner.restoreReceipt().status==DelayEditStatus::ready,"all twenty originals commit on exact bank receipt");f.settle();
 const auto previous=f.bank();auto replacement=previous;replacement[0].repeatMode=DelayRepeatMode::exact;replacement[0].repeatCount=3;
 check(f.owner.restore(replacement,21),"OOM bank candidate");failNext=true;f.pump();f.done(0,true);
 check(!failNext&&f.bank()==previous&&f.owner.restoreReceipt().status==DelayEditStatus::failed,"bank failure rolls back every committed record");
 check(f.owner.restore(replacement,22,UINT64_MAX),"future bank");f.pump(true);f.owner.cancel(19);f.done(0,true);
 check(f.bank()==previous&&f.owner.restoreReceipt().status==DelayEditStatus::cancelled,"per-track cancellation rolls back whole pending bank");
 check(f.owner.restore(replacement,22),"bank ID reuse");f.done(0);check(f.bank()==replacement,"fresh bank cannot consume old receipt");f.settle();
 // Adoption is already a fact before DEL. The other nineteen records must
 // retain that bank; the cleared record alone returns to defaults.
 auto off=replacement;for(auto& s:off)s.enabled=false;
 check(f.owner.restore(off,23),"unread all-off bank");f.pump();f.pump(true);f.command(LoopAction::clear,19);f.owner.service(f.latest);
 auto expected=off;expected[19]={};check(f.bank()==expected&&f.owner.bound()&&!f.owner.restoring(),"clear after unread success affects only cleared record");
 check(f.owner.track(19)->status==DelayEditStatus::cancelled&&f.owner.track(0)->status==DelayEditStatus::ready,"cleared record cannot revive from bank receipt");
 f.owner.service(f.initial);check(f.owner.track(19)->clearEpoch==1&&f.bank()==expected,"stale view cannot undo clear");
}
static void generations(unsigned block){
 Fixture f(block);check(f.owner.set(0,0,.123456789012345,30),"unread edit before same-rate prepare");f.pump();
 f.engine->prepare(8000,16384);f.pump(true);f.service();
 check(!f.owner.bound()&&!f.owner.track(0)->pending&&f.owner.track(0)->committed.values[0]==.123456789012345,"old ready fact retains precision without claiming current generation");
 const auto saved=f.bank();check(f.owner.bind(f.latest,saved),"explicit known settings bind preserved same-rate audio");
 auto target=saved;target[19].values[3]=.77;check(f.owner.restore(target,31),"unread bank before prepare");f.pump();f.pump(true);
 f.engine->prepare(48000,16384);f.pump(true);f.service();
 check(!f.owner.bound()&&f.bank()==target&&f.owner.restoreReceipt().generation!=f.latest.generation,"old bank success remains an immutable fact across rate reset");
 check(!f.owner.bind(f.latest,target)&&f.owner.bind(f.latest),"reset needs explicit verified defaults, not old bank inference");
 check(f.owner.set(0,3,.6,32,UINT64_MAX),"queued intent before reprepare");f.engine->prepare(96000,16384);f.pump(true);f.service();
 check(!f.owner.bound()&&f.owner.track(0)->status==DelayEditStatus::cancelled&&f.owner.track(0)->committed==DelaySettings{},"queued generation change cannot commit candidate");
 check(f.owner.bind(f.latest),"fresh generation recovery");
 auto mismatched=f.latest;++mismatched.tracks[19].delaySettingsRevision;f.owner.service(mismatched);
 check(!f.owner.bound()&&!f.owner.enable(0,true,33),"unexplained external revision detaches instead of guessing");
}
static void coordinated(unsigned block){
 Fixture f(block);const auto before=f.bank();auto target=before;target[0].enabled=target[19].enabled=true;target[0].values[0]=.012345678901;
 target[19].repeatMode=DelayRepeatMode::exact;target[19].repeatCount=2;
 ArpBankRequest request;request.requestId=80;request.generation=f.engine->generation();request.restoreEffects=true;request.effects.restoreDelays=true;request.effects.delays=target;
 for(unsigned id=0;id<20;++id){const auto info=f.engine->inspectAudioThread(id).track;request.settings[id]=info.arp;request.clearEpochs[id]=info.clearEpoch;request.settingsRevisions[id]=info.arpSettingsRevision;request.effects.delayRevisions[id]=info.delaySettingsRevision;}
 check(f.engine->enqueueArpBank(request),"enclosing coordinated request");ArpBankResult receipt;bool terminal=false;
 for(unsigned n=0;n<2000&&!terminal;++n){f.pump();f.drain();terminal=f.engine->popArpBank(receipt);if(!terminal)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 check(terminal&&receipt.status==ArpPreparationStatus::ready&&receipt.includesDelays,"real joint terminal receipt");
 f.owner.service(f.latest);check(!f.owner.bound(),"unexplained joint revision waits for enclosing proof");
 check(!f.owner.acceptEffectsRestore(f.initial,target,receipt,request.clearEpochs,request.effects.delayRevisions)&&f.bank()==before,"old snapshot cannot accept coordinated originals");
 auto forged=receipt;++forged.delaySettingsRevisions[19];
 check(!f.owner.acceptEffectsRestore(f.latest,target,forged,request.clearEpochs,request.effects.delayRevisions)&&f.bank()==before,"invalid final revision rejects entire bind without mutation");
 auto invalid=target;invalid[19].values[3]=NAN;
 check(!f.owner.acceptEffectsRestore(f.latest,invalid,receipt,request.clearEpochs,request.effects.delayRevisions)&&f.bank()==before,"invalid final candidate rejects entire bind");
 f.command(LoopAction::clear,19);f.owner.service(f.latest);
 check(f.owner.acceptEffectsRestore(f.latest,target,receipt,request.clearEpochs,request.effects.delayRevisions),"fresh receipt and later clear reconcile together");
 auto expected=target;expected[19]={};check(f.owner.bound()&&f.bank()==expected&&f.owner.track(19)->status==DelayEditStatus::cancelled,"clear wins only its track while nineteen joint originals survive");
 check(f.owner.track(0)->requestId==receipt.requestId&&f.owner.track(0)->revision==receipt.delaySettingsRevisions[0],"D owner retains exact enclosing acknowledgement");
 check(f.owner.set(19,0,.23,82),"fresh owned edit after clear");f.done(19);const auto fresh=f.owner.track(19)->committed;
 check(f.owner.acceptEffectsRestore(f.latest,target,receipt,request.clearEpochs,request.effects.delayRevisions)
  &&f.owner.track(19)->committed==fresh&&f.owner.track(19)->requestId==82,"late enclosing fact cannot overwrite a proven newer post-clear edit");
 f.engine->prepare(48000,16384);f.pump();f.service();const auto preserved=f.bank();
 check(!f.owner.acceptEffectsRestore(f.latest,target,receipt,request.clearEpochs,request.effects.delayRevisions)&&f.bank()==preserved,"obsolete generation receipt cannot bind current audio");
}
int main(){for(unsigned block:{64u,128u}){controls(block);banks(block);generations(block);coordinated(block);std::cout<<"PASS Delay settings owner "<<block<<": exact receipts/precision, intent rollback, bank atomicity, clear, OOM, FIFO and generation recovery\n";}}
