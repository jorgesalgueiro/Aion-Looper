#include "core/LoopBank.h"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <thread>
#include <string_view>
int main(int argc,char** argv){
 const bool callbackStart=argc==2&&std::string_view(argv[1])=="--callback-start";
 const bool accepted=callbackStart||(argc==2&&std::string_view(argv[1])=="--accepted-playback");
 std::array<aion::MeterGraphEvent,20> queues{};std::uint32_t ticket=0;
 auto storage=std::make_unique<aion::LoopBank>(accepted?aion::MeterEventScope::acceptedPlayback:aion::MeterEventScope::rawWindows,callbackStart?aion::QueuedStartMode::messageCallback:aion::QueuedStartMode::sampleGrid);auto& bank=*storage;std::string line;bank.prepare(81920,0);bank.resetMeterGraphEvents(7);
 while(std::getline(std::cin,line)){
  std::istringstream input(line);std::string op;unsigned id;double value;if(!(input>>op>>id>>value))return 2;
  float right=static_cast<float>(value);(void)(input>>right);bool ok=true;aion::Stereo out{};using A=aion::LoopAction;
  if(op=="prepare"){bank.prepare(id,0,value);bank.resetMeterGraphEvents(7);queues={};ticket=0;}
  else if(op=="record")ok=bank.apply(A::record,id);
  else if(op=="finish")ok=bank.apply(A::finish,id);
  else if(op=="merge")ok=bank.apply(A::merge,id,static_cast<float>(value));
  else if(op=="undo")ok=bank.apply(A::undo,id);
  else if(op=="redo")ok=bank.apply(A::redo,id);
  else if(op=="clear")ok=bank.apply(A::clear,id);
  else if(op=="inspect"){const auto v=bank.view()[id];std::cout<<std::setprecision(17)<<1<<' '<<v.length<<' '<<static_cast<unsigned>(v.state)<<' '<<v.originalBpm<<' '<<v.dynamicRecorded<<' '<<v.undoCount<<' '<<v.redoCount<<' '<<v.position<<' '<<v.startDelay<<' '<<v.playbackRate<<' '<<v.mergeSerial<<'\n';continue;}
  else if(op=="tick")out=bank.tick({static_cast<float>(value),right});
  else if(op=="wait"){for(unsigned n=0;bank.finalizationPending()&&n<10000;++n){bank.beginBlock();std::this_thread::sleep_for(std::chrono::milliseconds(1));}ok=!bank.finalizationPending();}
  else if(op=="peaks"){
   std::array<float,aion::LoopBank::waveformWidth> peaks{};ok=bank.copyWaveform(id,bank.view()[id].waveformId,peaks);
   std::cout<<std::setprecision(17)<<ok;for(auto peak:peaks)std::cout<<' '<<peak;std::cout<<'\n';continue;
  }
  else if(op=="export"){
   if(!bank.requestExport(id,bank.view()[id].waveformId))return 3;
   aion::WavExport data;bool ready=false;
   for(unsigned n=0;n<10000&&!ready;++n){ready=bank.popExport(data);if(!ready)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
   if(!ready)return 4;const char* hex="0123456789abcdef";for(const auto b:data.bytes)std::cout<<hex[b>>4]<<hex[b&15];std::cout<<'\n';continue;
  }
  else if(op=="delay")ok=bank.setStartDelay(id,value);
  else if(op=="nudge")ok=bank.apply(A::nudgeStart,id,static_cast<float>(value));
  else if(op=="quantize")ok=bank.apply(A::quantizeStart,id);
  else if(op=="apply")ok=bank.apply(A::applyStart,id);
  else if(op=="stut")ok=bank.apply(value?aion::LoopAction::stutterStart:aion::LoopAction::stutterStop,id);
  else if(op=="region"){const auto v=bank.view()[id];std::cout<<std::setprecision(17)<<1<<' '<<v.stuttering<<' '<<v.stutterStart<<' '<<v.stutterEnd<<' '<<v.stutterClock<<' '<<v.clockProgress<<'\n';continue;}
  else if(op=="timeline")bank.resetTimeline();
  else if(op=="rtrg")ok=bank.apply(A::retrigger,id);
  else if(op=="stop")ok=bank.apply(A::stopImmediate,id);
  else if(op=="play"){
   ok=bank.apply(A::play,id);aion::MeterGraphEvent event;
   while(bank.popMeterGraphEvent(event,7))if(event.kind==aion::MeterGraphEventKind::queuedPlay)queues[event.track]=event;
  }
  else if(op=="birth")ok=id<queues.size()&&bank.startQueuedMeterGraph(id,++ticket,queues[id].queueId,queues[id].bufferId);
  else if(op=="stopgrid")ok=bank.apply(A::stop,id);
  else if(op=="retrigger")ok=bank.apply(A::sourceRestart,id);
  else if(op=="speed")ok=bank.apply(A::speed,id,static_cast<float>(value));
  else if(op=="solo")ok=bank.apply(A::solo,id);
  else if(op=="reverse")ok=bank.apply(A::reverse,id,static_cast<float>(value));
  else if(op=="sync"){unsigned num=static_cast<unsigned>(right),bars=1;input>>bars;bank.configureSync({static_cast<aion::SyncMode>(id),value,bars,num,4,false});}
  else return 2;
  const auto v=bank.view()[0];const auto clock=bank.syncView();
  std::cout<<std::setprecision(17)<<ok<<' '<<clock.frame<<' '<<clock.origin<<' '<<v.startDelay<<' '<<v.phaseApplyFrame<<' '<<v.position<<' '<<out.left<<' '<<out.right<<'\n';
 }
}
