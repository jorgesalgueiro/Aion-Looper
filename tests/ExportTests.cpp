#include "core/Engine.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>
extern thread_local bool watch;
extern std::atomic<int> allocations;
extern std::atomic<bool> failWorkerAllocation;
namespace {
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
void wait(aion::LoopBank& bank){for(unsigned n=0;bank.finalizationPending()&&n<10000;++n){watch=true;bank.beginBlock();watch=false;std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(!bank.finalizationPending(),"export fixture finalization timeout");}
aion::WavExport take(aion::LoopBank& bank){aion::WavExport out;for(unsigned n=0;n<10000;++n){if(bank.popExport(out))return out;std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(false,"export worker timeout");return out;}
void seed(aion::LoopBank& bank,unsigned length){watch=true;check(bank.apply(aion::LoopAction::record,0),"seed export");for(unsigned i=0;i<length;++i)bank.tick({0.2f*std::sin(i*0.11f),0.3f*std::cos(i*0.09f)});bank.apply(aion::LoopAction::finish,0);watch=false;wait(bank);}
}
void exportTests(){
 using namespace aion;using A=LoopAction;
 auto bank=std::make_unique<LoopBank>();bank->prepare(4096,0,48000);
 watch=true;check(bank->requestExport(0,0),"empty export produces a result");watch=false;check(take(*bank).bytes.empty(),"empty export cannot fabricate PCM");
 seed(*bank,4096);auto before=bank->view()[0];
 watch=true;check(bank->available()==0&&bank->requestExport(0,before.waveformId),"full pool export");check(!bank->requestExport(0,before.waveformId),"one outstanding export bound");bank->apply(A::clear,0);watch=false;
 auto original=take(*bank);check(original.bytes.size()==44+4096*4,"pinned export survives DEL without scanning on callback");
 seed(*bank,4096);before=bank->view()[0];
 watch=true;check(bank->requestExport(0,before.waveformId+1),"stale identity gives error");watch=false;check(take(*bank).bytes.empty(),"stale click cannot export replacement PCM");
 watch=true;bank->requestExport(0,before.waveformId);watch=false;bank->prepare(8192,0,96000);auto reset=take(*bank);check(reset.bytes==original.bytes,"prepare joins worker and preserves pinned old-rate bytes until consumed");
 seed(*bank,512);before=bank->view()[0];failWorkerAllocation.store(true);
 watch=true;bank->requestExport(0,before.waveformId);watch=false;check(take(*bank).bytes.empty(),"export allocation failure reported");check(!failWorkerAllocation.load(),"allocation failure exercised worker");
 check(bank->view()[0].waveformId==before.waveformId&&bank->view()[0].length==512,"OOM keeps current PCM/history");
 watch=true;bank->requestExport(0,before.waveformId);watch=false;check(take(*bank).bytes.size()==44+512*4,"export recovers after OOM");
 // Independent request FIFO: a future performance command must not block export.
 std::vector<std::uint8_t> reference;
 for(const int block:{64,128}){
  auto engine=std::make_unique<Engine>();engine->prepare(48000,8192);engine->setMonitoring(false);
  engine->enqueueLoop({A::record,0,1,engine->generation(),0,0});
  float l[128]{},r[128]{};float* input[]={l,r};
  watch=true;for(int frame=0;frame<512;frame+=block){for(int i=0;i<block;++i){l[i]=0.2f*std::sin((frame+i)*0.11f);r[i]=0.3f*std::cos((frame+i)*0.09f);}engine->process(input,2,block);}watch=false;
  engine->enqueueLoop({A::finish,0,2,engine->generation(),0,0});engine->process(input,2,1);
  EngineView view{},latest{};for(unsigned n=0;n<10000;++n){watch=true;engine->process(nullptr,0,0);watch=false;while(engine->popView(view))latest=view;if(latest.tracks[0].state==LoopState::playing)break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  check(latest.tracks[0].state==LoopState::playing,"Engine export committed");
  engine->enqueueLoop({A::volume,0,3,engine->generation(),engine->clock()+100000,0});
  check(engine->enqueueExport({0,engine->generation(),latest.tracks[0].waveformId}),"Engine export accepted");check(!engine->enqueueExport({0,engine->generation(),latest.tracks[0].waveformId}),"bounded Engine export");
  watch=true;engine->process(nullptr,0,0);watch=false;WavExport exported;bool ready=false;for(unsigned n=0;n<10000&&!ready;++n){ready=engine->popExport(exported);if(!ready)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  check(ready&&exported.bytes.size()==44+512*4,"export independent of future-command FIFO and zero-block processing");
  if(reference.empty())reference=exported.bytes;else check(reference==exported.bytes,"WAV exact 64/128 connected capture bytes");
  check(engine->enqueueExport({0,engine->generation(),latest.tracks[0].waveformId}),"request before prepare");engine->prepare(48000,8192);watch=true;engine->process(nullptr,0,0);watch=false;
  ready=false;for(unsigned n=0;n<10000&&!ready;++n){ready=engine->popExport(exported);if(!ready)std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(ready&&exported.bytes.empty(),"generation reset rejects queued old export");
 }
 check(allocations.load()==0,"export zero callback new/delete");
 std::cout<<"PASS: WAV pin/DEL/reset/OOM/identity, bounded independent requests, zero-block/64/128 and no callback new/delete\n";
}
