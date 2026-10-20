#include "core/Engine.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <thread>
extern thread_local bool watch;
extern std::atomic<int> allocations;
extern std::atomic<bool> failWorkerAllocation;
namespace {
using namespace aion;using A=LoopAction;
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
void wait(LoopBank& bank){for(unsigned n=0;bank.finalizationPending()&&n<10000;++n){watch=true;bank.beginBlock();watch=false;std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(!bank.finalizationPending(),"merge worker timeout");}
bool apply(LoopBank& bank,A action,unsigned id,float value=0){watch=true;const bool result=bank.apply(action,id,value);watch=false;return result;}
void seed(LoopBank& bank,unsigned id,unsigned length){check(apply(bank,A::record,id),"merge seed record");watch=true;for(unsigned i=0;i<length;++i)bank.tick({0.2f*std::sin(i*0.11f+id),0.3f*std::cos(i*0.09f+id)});watch=false;check(apply(bank,A::finish,id),"merge seed finish");wait(bank);}
std::vector<std::uint8_t> bytes(LoopBank& bank,unsigned id){watch=true;const bool ok=bank.requestExport(id,bank.view()[id].waveformId);watch=false;check(ok,"merge export request");WavExport out;for(unsigned n=0;n<10000;++n){if(bank.popExport(out))return out.bytes;std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(false,"merge export timeout");return {};}
}
void mergeTests(){
 using namespace aion;using A=LoopAction;
 auto storage=std::make_unique<LoopBank>();auto& b=*storage;b.prepare(4*4096,0);
 check(!apply(b,A::merge,0,0)&&!apply(b,A::merge,0,1)&&!apply(b,A::merge,0,20)&&!apply(b,A::merge,0,1.5f)&&!apply(b,A::merge,0,std::numeric_limits<float>::quiet_NaN()),"merge validates self/empty/range/integer/finite");
 seed(b,1,4097);const auto source=bytes(b,1);const auto sourceId=b.view()[1].waveformId;
 apply(b,A::speed,0,0.5f);b.setStartDelay(0,0.25);
 check(apply(b,A::merge,0,1),"clone source");check(!apply(b,A::merge,2,1),"single bounded merge worker");wait(b);
 auto v=b.view()[0];check(v.state==LoopState::stopped&&v.length==4097&&v.undoCount==0&&v.playbackRate==0.5f&&v.startDelay==0.25&&v.originalBpm==b.view()[1].originalBpm,"clone is stopped, copies tempo, preserves destination controls, no history");
 check(bytes(b,0)==source&&b.view()[1].waveformId==sourceId,"clone exact bytes and untouched source");
 check(apply(b,A::merge,0,1),"mix existing");wait(b);v=b.view()[0];const auto mixed=bytes(b,0);
 check(v.undoCount==1&&v.redoCount==0&&b.nextUndo()==0&&mixed!=source&&v.length==4097,"merge adds one global undo and retains length");
 check(apply(b,A::undoGlobal,0)&&bytes(b,0)==source,"global undo restores exact pre-merge PCM");
 check(apply(b,A::redoGlobal,0)&&bytes(b,0)==mixed,"global redo restores exact merged PCM");
 check(!b.clearMergedSource(0,static_cast<double>(v.mergeSerial)),"Undo/Redo invalidates earlier clear-source confirmation");
 check(apply(b,A::merge,0,1),"fresh merge for confirmation guards");wait(b);v=b.view()[0];
 check(apply(b,A::overdub,1),"new source overdub before old confirmation");
 check(!b.clearMergedSource(0,static_cast<double>(v.mergeSerial))&&b.view()[1].state==LoopState::overdubbing,"old confirmation never clears an in-progress new source take");
 check(apply(b,A::finish,1),"cancel empty source overdub");
 check(!b.clearMergedSource(0,static_cast<double>(v.mergeSerial+1)),"wrong completion cannot clear source");
 check(b.clearMergedSource(0,static_cast<double>(v.mergeSerial))&&b.view()[1].state==LoopState::empty,"confirmed merge clears matching source only");
 check(!b.clearMergedSource(0,static_cast<double>(v.mergeSerial)),"duplicate confirmation harmless");
 seed(b,1,512);check(!b.clearMergedSource(0,static_cast<double>(v.mergeSerial))&&b.view()[1].length==512,"old confirmation cannot delete replacement take");
 const auto before=bytes(b,0);failWorkerAllocation.store(true);check(apply(b,A::merge,0,1),"OOM merge queued");wait(b);
 check(!failWorkerAllocation.load()&&b.view()[0].finalizationFailed&&bytes(b,0)==before&&b.view()[0].mergeSerial==v.mergeSerial&&b.view()[0].undoCount==2,"OOM keeps PCM/history/completion");
 check(apply(b,A::merge,0,1),"recover merge after OOM");apply(b,A::clear,1);wait(b);check(!b.view()[0].finalizationFailed&&b.view()[1].state==LoopState::empty,"source DEL while pinned remains safe");
 seed(b,1,512);check(apply(b,A::merge,0,1),"destination DEL race");apply(b,A::clear,0);wait(b);check(b.view()[0].state==LoopState::empty&&b.view()[0].mergeSerial==0,"destination DEL never resurrects a merge");
 check(apply(b,A::merge,0,1),"prepare drains merge job");b.prepare(4096,0);check(b.view()[0].state==LoopState::empty,"prepare discards pending destination safely");
 seed(b,1,4096);check(!apply(b,A::merge,0,1),"clone rejects insufficient pool before touching destination");
 check(apply(b,A::merge,1,0)==false,"empty source rejected in full pool");
 b.prepare(8192,0);seed(b,0,4096);seed(b,1,4096);check(b.available()==0&&apply(b,A::merge,0,1),"existing-length merge works with full recording pool");wait(b);
 apply(b,A::clearAll,0);seed(b,1,512);failWorkerAllocation.store(true);check(apply(b,A::merge,0,1),"clone OOM queued");wait(b);check(b.view()[0].state==LoopState::empty&&b.view()[0].finalizationFailed&&b.view()[0].mergeSerial==0,"clone OOM leaves empty destination and no success");
 check(apply(b,A::merge,0,1),"clone recovers after OOM");apply(b,A::stopAll,0);wait(b);check(b.view()[0].state==LoopState::stopped&&b.view()[0].mergeSerial==0,"STOP ALL during clone never starts playback or prompts deletion");
 b.prepare(8192,0);b.configureSync({SyncMode::dynamic,133,1,4,4,false,false});
 check(apply(b,A::record,1),"dynamic source record");watch=true;for(unsigned n=0;n<512;++n)b.tick({0.2f*std::sin(n*0.11f),0.3f*std::cos(n*0.09f)});watch=false;
 check(apply(b,A::finish,1)&&apply(b,A::finish,1),"dynamic source immediate finish");wait(b);
 check(apply(b,A::merge,0,1),"dynamic clone");wait(b);check(b.view()[0].originalBpm==133&&b.view()[0].dynamicRecorded&&b.view()[0].state==LoopState::stopped,"clone carries original BPM and dynamic-sync metadata");
 b.prepare(8192,0);b.configureSync({});seed(b,0,512);seed(b,1,512);apply(b,A::sourceRestart,0);
 watch=true;for(unsigned n=0;n<37;++n)b.tick({});watch=false;
 const auto position=b.view()[0].position;check(apply(b,A::merge,0,1),"playing merge");wait(b);check(b.view()[0].state==LoopState::playing&&b.view()[0].position==position,"merge preserves playing source phase");
 apply(b,A::reverse,0,1);apply(b,A::sourceRestart,0);apply(b,A::solo,0);
 // Allocate the observation buffer off audio, then record an entire cycle.
 std::vector<Stereo> reverseBefore(512);watch=true;for(auto& sample:reverseBefore)sample=b.tick({});watch=false;
 const auto storedBefore=bytes(b,0);check(apply(b,A::merge,0,1),"reversed merge");wait(b);check(bytes(b,0)!=storedBefore,"reversed source still updates stored PCM");
 watch=true;for(const auto sample:reverseBefore){const auto rendered=b.tick({});check(sample.left==rendered.left&&sample.right==rendered.right,"playing reversed source retains independent original PCM");}watch=false;
 std::vector<std::uint8_t> blockReference;std::vector<float> renderedReference;
 for(const int block:{64,128}){
  auto engine=std::make_unique<Engine>();engine->prepare(48000,16384);engine->setMonitoring(false);engine->setTarget(1);
  std::uint32_t request=0;EngineView latest{},view{};float l[128]{},r[128]{};float* channels[]={l,r};
  const auto command=[&](A action,unsigned id,float value=0){check(engine->enqueueLoop({action,id,++request,engine->generation(),0,value}),"merge Engine command queued");watch=true;engine->process(channels,2,1);watch=false;};
  const auto settled=[&]{for(unsigned n=0;n<10000;++n){watch=true;engine->process(nullptr,0,0);watch=false;while(engine->popView(view))latest=view;if(!engine->finalizationPending())return;std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(false,"merge Engine wait");};
  const auto exported=[&](unsigned id){settled();check(engine->enqueueExport({id,engine->generation(),latest.tracks[id].waveformId}),"Engine merge export");watch=true;engine->process(nullptr,0,0);watch=false;WavExport output;for(unsigned n=0;n<10000;++n){if(engine->popExport(output))return output.bytes;std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(false,"Engine merge export wait");return output.bytes;};
  for(unsigned id=0;id<2;++id){
   // The command's first frame belongs to the take; seed that frame as well.
   l[0]=0.2f;r[0]=-0.1f;command(A::record,id);
   for(unsigned frame=0;frame<512;frame+=block){for(int i=0;i<block;++i){l[i]=0.2f*std::sin((frame+i)*0.11f+id);r[i]=0.3f*std::cos((frame+i)*0.09f+id);}watch=true;engine->process(channels,2,block);watch=false;}
   command(A::finish,id);settled();
  }
  const auto original=exported(0);command(A::merge,0,1);settled();check(latest.tracks[0].mergeSerial!=0,"Engine publishes committed merge");const auto merged=exported(0);
  command(A::undo,0);check(exported(0)==original,"Engine merge undo exact bytes");command(A::redo,0);check(exported(0)==merged,"Engine merge redo exact bytes");
  if(blockReference.empty())blockReference=merged;else check(blockReference==merged,"merge exact stored stereo bytes at 64/128");
  command(A::mute,1,1);command(A::sourceRestart,0);std::vector<float> rendered(2048*2);
  for(unsigned frame=0;frame<2048;frame+=block){std::fill_n(l,block,0.0f);std::fill_n(r,block,0.0f);watch=true;engine->process(channels,2,block);watch=false;for(int i=0;i<block;++i){rendered[(frame+i)*2]=l[i];rendered[(frame+i)*2+1]=r[i];}}
  if(renderedReference.empty())renderedReference=rendered;else check(renderedReference==rendered,"merge actual rendered stereo PCM invariant at 64/128");
 }
 check(allocations.load()==0,"merge zero callback new/delete");
 std::cout<<"PASS: merge clone/mix/history, bounded pins, DEL/reset/OOM/full-pool and no callback new/delete\n";
}
