// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"
#include "core/ArpSettings.h"
#include <atomic>
#include <chrono>
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string_view>
#include <thread>
namespace {
thread_local bool watch=false;thread_local unsigned heapOperations=0;
std::atomic<unsigned> failAt{0};
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct Guard {Guard(){heapOperations=0;watch=true;}~Guard(){watch=false;check(!heapOperations,"coordinated B bank callback heap operation");}};
template<class F>void wait(F ready,const char* why){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(15);while(!ready()){check(std::chrono::steady_clock::now()<end,why);std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
}
void* operator new(std::size_t n){if(watch)++heapOperations;
 if(failAt.load()&&failAt.fetch_sub(1)==1)throw std::bad_alloc();
 if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watch&&p)++heapOperations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace aion;
struct Fixture {
 std::unique_ptr<Engine> engine=std::make_unique<Engine>();unsigned block;std::array<float,128> l{},r{};
 explicit Fixture(unsigned frames,double rate=8000):block(frames){engine->prepare(rate,16384);engine->setMonitoring(false);}
 void pump(){float* audio[]{l.data(),r.data()};Guard guard;engine->process(audio,2,static_cast<int>(block),{});}
 ArpBankRequest request(unsigned id,unsigned variant,bool arpOn,bool reverbsOn,unsigned only=0){
  ArpBankRequest request;request.requestId=id;request.generation=engine->generation();request.restoreEffects=true;request.effects.restoreReverbs=true;
  for(unsigned track=0;track<20;++track){const auto info=engine->inspectAudioThread(track).track;
   request.clearEpochs[track]=info.clearEpoch;request.settingsRevisions[track]=info.arpSettingsRevision;
   request.effects.reverbRevisions[track]=info.reverbSettingsRevision;
   ArpSettings a;a.enabled=arpOn;a.values[0]=.01+.001*variant;check(compileArpSettings(a,120,id+track,request.settings[track]),"joint Arp descriptor");
   auto& reverb=request.effects.reverbs[track];reverb.enabled=reverbsOn||track==only;
   reverb.values[0]=1.0;reverb.values[1]=.25;reverb.values[2]=.8+.2*variant;reverb.values[3]=1.0+.1*variant;
   reverb.values[4]=8000;reverb.values[5]=.02;
  }return request;
 }
 ArpBankResult result(){ArpBankResult receipt;wait([&]{pump();return engine->popArpBank(receipt);},"joint B result timeout");return receipt;}
 void same(const ArpBankRequest& request){for(unsigned id=0;id<20;++id){const auto info=engine->inspectAudioThread(id).track;
  check(info.arp==request.settings[id]&&info.reverb==request.effects.reverbs[id],"complete old or new coordinated Arp/B bank");}}
 ArpBankRequest threeWay(unsigned id,unsigned variant){auto request=this->request(id,variant,true,true);request.effects.restoreDelays=true;
  for(unsigned track=0;track<20;++track){const auto info=engine->inspectAudioThread(track).track;request.effects.delayRevisions[track]=info.delaySettingsRevision;
   auto& delay=request.effects.delays[track];delay.enabled=true;delay.values[0]=.02+.001*variant;delay.values[3]=.3;}
  return request;}
 void sameAll(const ArpBankRequest& request){for(unsigned id=0;id<20;++id){const auto info=engine->inspectAudioThread(id).track;
  check(info.arp==request.settings[id]&&info.reverb==request.effects.reverbs[id]&&info.delay==request.effects.delays[id],"complete Arp/Delay/B joint bank");}}
 void reverbsSame(const ArpBankRequest& request){for(unsigned id=0;id<20;++id)check(engine->inspectAudioThread(id).track.reverb==request.effects.reverbs[id],"unchanged B owner");}
 void settle(){for(unsigned n=0;n<120;++n){pump();std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
 ReverbBankRequest independent(unsigned id){ReverbBankRequest b;b.requestId=id;b.generation=engine->generation();
  for(unsigned track=0;track<20;++track){const auto info=engine->inspectAudioThread(track).track;b.clearEpochs[track]=info.clearEpoch;b.settingsRevisions[track]=info.reverbSettingsRevision;b.settings[track]=info.reverb;}
  return b;
 }
};
static void admission(unsigned block){
 Fixture f(block);auto old=f.request(1,1,true,true);check(f.engine->enqueueArpBank(old),"initial coordinated Arp/B bank");check(f.result().status==ArpPreparationStatus::ready,"initial Arp/B receipt");f.same(old);f.settle();
 auto invalid=f.request(2,1,true,true);invalid.effects.reverbs[19].values[2]=0;check(!f.engine->enqueueArpBank(invalid),"invalid B descriptor rejects before any worker job");
 auto stale=f.request(3,1,true,true);++stale.effects.reverbRevisions[19];check(f.engine->enqueueArpBank(stale),"stale B descriptor queued");check(f.result().status==ArpPreparationStatus::stale,"stale last B revision blocks entire group");f.reverbsSame(old);
 std::uint64_t transaction=0;check(f.engine->enqueueReverbBank(f.independent(4),transaction),"independent B bank admission");check(!f.engine->enqueueArpBank(f.request(5,1,true,true)),"outstanding B bank excludes joint admission");
 ReverbBankResult independentReceipt;wait([&]{f.pump();return f.engine->popReverbBank(independentReceipt);},"independent B receipt timeout");
 check(independentReceipt.status==ReverbEditStatus::ready,"independent B receipt ready");f.settle();
 auto pending=f.request(6,1,true,true);check(f.engine->enqueueArpBank(pending),"joint admission after B receipt");f.pump();
 LoopCommand edit{LoopAction::reverb,0,7,f.engine->generation()};std::uint64_t editToken=0;
 check(!f.engine->enqueueReverbEdit(edit,editToken),"coordinator excludes individual B edit during admission");
 check(!f.engine->enqueueReverbBank(f.independent(8),transaction),"coordinator excludes independent B bank during admission");
 wait([&]{f.pump();return f.engine->inspectAudioThread(0).track.reverbRequest==pending.requestId;},"joint B commit before unread-receipt check");
 check(!f.engine->enqueueReverbBank(f.independent(9),transaction),"unread joint receipt excludes independent B bank");
 ArpBankResult joint;check(f.engine->popArpBank(joint)&&joint.status==ArpPreparationStatus::ready&&joint.includesReverbs,"joint B receipt after unread exclusion");
 ArpBankResult consumed;check(!f.engine->popArpBank(consumed),"same receipt cannot be consumed twice");f.settle();
 check(f.engine->enqueueReverbBank(f.independent(10),transaction),"independent B recovers only after receipt consumption");
 wait([&]{f.pump();return f.engine->popReverbBank(independentReceipt);},"fresh B receipt timeout");check(independentReceipt.status==ReverbEditStatus::ready,"fresh B receipt ready");
 // An over-budget B bank must be rejected by the audio-side preflight before any
 // worker allocation: failAt stays armed and the exact request reports busy.
 Fixture big(block,384000);std::array<ReverbSettings,20> before{};for(unsigned id=0;id<20;++id)before[id]=big.engine->inspectAudioThread(id).track.reverb;
 failAt=1;auto oversized=big.request(11,4,true,true);check(big.engine->enqueueArpBank(oversized),"valid but over-budget Arp/B descriptor");
 const auto busy=big.result();check(busy.status==ArpPreparationStatus::busy,"B aggregate budget rejects before publishing work");
 check(failAt.exchange(0)==1,"no worker allocation occurred for the rejected B bank");
 for(unsigned id=0;id<20;++id)check(big.engine->inspectAudioThread(id).track.reverb==before[id],"B unchanged after budget rejection");
 std::cout<<"PASS coordinated Arp/B admission "<<block<<": invalid/stale descriptors, B edit/bank exclusion, unread receipt and audio-side budget rejection\n";
}
static void run(unsigned block,unsigned scenario){
 Fixture f(block);auto old=f.request(1,1,true,true);check(f.engine->enqueueArpBank(old),"initial Arp/B bank");const auto first=f.result();
 check(first.status==ArpPreparationStatus::ready&&first.includesReverbs,"initial joint B receipt");f.same(old);
 for(unsigned id=0;id<20;++id)check(first.reverbSettingsRevisions[id]==old.effects.reverbRevisions[id]+1,"receipt owns every B revision");f.settle();
 if(scenario==0){
  auto next=f.request(2,2,true,true);check(f.engine->enqueueArpBank(next),"replacement Arp/B bank");const auto receipt=f.result();
  check(receipt.status==ArpPreparationStatus::ready&&receipt.requestId==2&&receipt.includesReverbs,"replacement B receipt");f.same(next);
  for(unsigned id=0;id<20;++id)check(receipt.reverbSettingsRevisions[id]==next.effects.reverbRevisions[id]+1,"B receipt revision proof");
  f.settle();auto off=f.request(2,3,false,false);check(f.engine->enqueueArpBank(off),"all-off Arp/B recovery");check(f.result().status==ArpPreparationStatus::ready,"all-off B recovery");f.same(off);
 }
 if(scenario==1){
  auto next=f.request(3,2,false,true,0);failAt=1;check(f.engine->enqueueArpBank(next),"B-only OOM descriptor");
  const auto receipt=f.result();check(receipt.status==ArpPreparationStatus::failed,"B worker allocation failure fails the joint bank");
  check(failAt.exchange(0)==0,"B allocation failure was consumed");f.reverbsSame(old);f.settle();
  auto recovered=f.request(4,2,true,true);check(f.engine->enqueueArpBank(recovered),"B recovery after OOM");check(f.result().status==ArpPreparationStatus::ready,"B recovery receipt");f.same(recovered);
 }
 if(scenario==3){
  auto next=f.threeWay(7,2);check(f.engine->enqueueArpBank(next),"three-storage Arp/Delay/B descriptor");const auto receipt=f.result();
  check(receipt.status==ArpPreparationStatus::ready&&receipt.includesDelays&&receipt.includesReverbs,"three-storage joint receipt");f.sameAll(next);
  for(unsigned id=0;id<20;++id)check(receipt.reverbSettingsRevisions[id]==next.effects.reverbRevisions[id]+1&&receipt.delaySettingsRevisions[id]==next.effects.delayRevisions[id]+1,"three-storage revision proof");
  f.settle();
 }
 if(scenario==2){
  auto next=f.request(5,2,true,true);check(f.engine->enqueueArpBank(next),"joint B bank for cancellation");
  f.engine->cancelReverbEdit();const auto receipt=f.result();check(receipt.status==ArpPreparationStatus::cancelled,"B cancellation invalidates the joint bank");
  f.reverbsSame(old);f.settle();auto recovered=f.request(6,2,true,true);check(f.engine->enqueueArpBank(recovered),"B recovery after cancellation");
  check(f.result().status==ArpPreparationStatus::ready,"B cancellation recovery receipt");f.same(recovered);
 }
 std::cout<<"PASS coordinated Arp/B scenario "<<block<<"/"<<scenario<<": atomic commit, OOM rollback/cancellation and same-ID recovery\n";
}
int main(int argc,char** argv){
 std::cout<<std::unitbuf;unsigned scenario=0;const bool admissionOnly=argc==2&&std::string_view(argv[1])=="--admission";
 check(argc<=2,"optional scenario or --admission only");
 if(argc==2&&!admissionOnly){const auto text=std::string_view(argv[1]);const auto parsed=std::from_chars(text.data(),text.data()+text.size(),scenario);check(parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&scenario<=3,"scenario must be 0..3");}
 for(unsigned block:{64u,128u}){
  if(argc==1||admissionOnly)admission(block);
  if(argc==1)for(unsigned s=0;s<=3;++s)run(block,s);
  else if(!admissionOnly)run(block,scenario);
 }
}
