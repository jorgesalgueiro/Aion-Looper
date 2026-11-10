// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string>
static thread_local bool watch=false;
static unsigned operations=0;
void* operator new(std::size_t n){if(watch)++operations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watch)++operations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
static void check(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
static void connected(unsigned block,double rate){
 using namespace aion;
 auto dry=std::make_unique<Engine>(),wet=std::make_unique<Engine>();
 for(auto* e:{dry.get(),wet.get()}){e->prepare(rate,32768);e->setMonitoring(false);check(e->enqueue({0,1})&&e->enqueueLoop({LoopAction::record,0,1,e->generation()}),"seed capture");}
 std::array<float,128> dl{},dr{},wl{},wr{};float* d[]{dl.data(),dr.data()},* w[]{wl.data(),wr.data()};std::uint64_t frame=0;
 auto render=[&](bool capture){
  for(unsigned i=0;i<block;++i)dl[i]=dr[i]=wl[i]=wr[i]=capture?static_cast<float>(.08*std::sin((frame+i)*.131)+.03*std::sin((frame+i)*.027)):0;
  watch=true;dry->process(d,2,block,{},true);wet->process(w,2,block,{},true);watch=false;frame+=block;
 };
 for(unsigned i=0;i<8192/block;++i)render(true);
 for(auto* e:{dry.get(),wet.get()})check(e->enqueueLoop({LoopAction::finish,0,2,e->generation()}),"finish fixture");render(false);
 for(auto* e:{dry.get(),wet.get()})check(e->enqueueLoop({LoopAction::play,0,3,e->generation()}),"play fixture");
 for(unsigned i=0;i<2048/block;++i)render(false);
 EqSettings qs;qs.enabled=true;qs.values[4]=12;
 CompressorSettings cs;cs.enabled=true;cs.values[0]=-30;cs.values[1]=8;cs.values[5]=1;
 LoopCommand q{LoopAction::equalizer,0,4,wet->generation()};q.equalizer=qs;
 LoopCommand c{LoopAction::compressor,0,5,wet->generation()};c.compressor=cs;
 check(wet->enqueueLoop(q)&&wet->enqueueLoop(c),"connected effects");
 SmoothedEqualizer eq;Compressor comp,oppositeComp;SmoothedEqualizer oppositeEq;
 check(eq.prepare(rate,qs.values,frame)&&comp.prepare(rate,cs.values,frame)&&oppositeEq.prepare(rate,qs.values,frame)&&oppositeComp.prepare(rate,cs.values,frame),"independent DSP owners");
 SignalChain previous;double error=0,difference=0;constexpr float centre=.7071067811865475f;
 const auto original=wet->inspectAudioThread().track;
 for(const auto text:{"QC","CQ","Q","A","C","CQ","QCATFODBVKZG"}){
  const auto next=normalizedSignalChain(text);const auto at=frame+17;
  LoopCommand command{LoopAction::signalChain,0,6,wet->generation(),at};command.signalChain=next;
  command.chainClearEpoch=original.clearEpoch;check(wet->enqueueLoop(command),"timed chain reorder");
  for(unsigned n=0;n<1024/block;++n){
   const auto begin=frame;render(false);
   for(unsigned i=0;i<block;++i){
    if(begin+i==at){
     if(next.contains('Q')&&!previous.contains('Q'))check(eq.prepare(rate,qs.values,at),"reinsert Q");
     if(next.contains('C')&&!previous.contains('C'))check(comp.prepare(rate,cs.values,at),"reinsert C");
     previous=next;
    }
    float l=dl[i]/centre,r=dr[i]/centre,ol=l,orr=r;
    for(char code:previous.text()){
     if(code=='Q')eq.process(&l,&r,&l,&r,1);
     if(code=='C')comp.process(&l,&r,2,&l,&r,1);
    }
    oppositeComp.process(&ol,&orr,2,&ol,&orr,1);oppositeEq.process(&ol,&orr,&ol,&orr,1);
    const float expected=std::clamp((l+r)*.5f*centre,-1.f,1.f);
    error=std::max({error,std::abs(static_cast<double>(wl[i])-expected),std::abs(static_cast<double>(wr[i])-expected)});
    if(std::string_view(text)=="QC")difference=std::max(difference,std::abs(static_cast<double>(expected)-(ol+orr)*.5f*centre));
    if(std::string_view(text)=="A"&&begin+i>=at)check(wl[i]==dl[i]&&wr[i]==dr[i],"omitted ported effects bypass exactly; A is not falsely enabled");
   }
   const auto v=wet->inspectAudioThread().track;
   check(v.waveformId==original.waveformId&&v.length==original.length&&v.undoCount==original.undoCount&&v.sourceRevision==original.sourceRevision,"routing cannot edit stored PCM/history/source ownership");
   check(v.signalChain==next,"audio-owned chain receipt");
  }
 }
 check(error<=1e-6&&difference>.001,"ordered nonzero connected PCM matches independent persistent stage owners and differs from opposite order");
 // Stale clear epoch, invalid structure and a full FIFO cannot overwrite routing.
 LoopCommand bad{LoopAction::signalChain,0,7,wet->generation()};bad.signalChain=normalizedSignalChain("CQ");bad.chainClearEpoch=original.clearEpoch+1;
 check(wet->enqueueLoop(bad),"stale epoch admitted for audio validation");render(false);check(wet->inspectAudioThread().track.signalChain==SignalChain{},"stale clear epoch rejection");
 bad.signalChain.size=14;check(!wet->enqueueLoop(bad),"invalid chain rejected before enqueue");
 for(unsigned i=0;i<255;++i)check(wet->enqueueLoop({LoopAction::volume,0,8,wet->generation(),0,1}),"fill ordinary queue");
 bad.signalChain=normalizedSignalChain("CQ");bad.chainClearEpoch=original.clearEpoch;check(!wet->enqueueLoop(bad),"full queue rejects chain atomically");
 render(false);render(false);render(false);check(wet->inspectAudioThread().track.signalChain==SignalChain{},"rejected reorder cannot leak");
 bad.requestId=9;check(wet->enqueueLoop(bad),"recovery after overflow");render(false);
 check(wet->enqueueLoop({LoopAction::stopAll,0,10,wet->generation()}),"panic");render(false);check(wet->inspectAudioThread().track.signalChain==bad.signalChain,"panic preserves chain");
 wet->prepare(rate,32768);check(wet->inspectAudioThread().track.signalChain==bad.signalChain,"same-rate prepare retains chain");
 check(wet->enqueueLoop({LoopAction::clear,0,11,wet->generation()}),"DEL");render(false);check(wet->inspectAudioThread().track.signalChain==SignalChain{},"DEL resets chain");
 SyncSettings sync;sync.mode=SyncMode::fixed;sync.bpm=999;sync.bars=1;wet->configureSync(sync);
 bad.generation=wet->generation();bad.chainClearEpoch=wet->inspectAudioThread().track.clearEpoch;
 check(wet->enqueueLoop(bad)&&wet->enqueueLoop({LoopAction::record,0,12,wet->generation()}),"configured arm");render(false);
 check(wet->inspectAudioThread().track.state==LoopState::armed,"armed fixture");check(wet->enqueueLoop({LoopAction::finish,0,13,wet->generation()}),"cancel arm");render(false);
 check(wet->inspectAudioThread().track.signalChain==bad.signalChain,"arm cancellation preserves user order");
 std::cout<<"PASS connected chain "<<rate<<" / "<<block<<": peak "<<error<<", opposite-order difference "<<difference<<'\n';
}
static void bankAdmission(){
 using namespace aion;auto e=std::make_unique<Engine>();e->prepare(48000,32768);
 std::array<EqSettings,20> eq{};std::array<CompressorSettings,20> comp{};std::array<SignalChain,20> chain{};
 for(unsigned i=0;i<20;++i){eq[i].enabled=true;comp[i].enabled=true;chain[i]=normalizedSignalChain(i%2?"CQ":"Q");}
 for(unsigned i=0;i<196;++i)check(e->enqueueLoop({LoopAction::volume,0,1,e->generation(),0,1}),"bank capacity fixture");
 check(!e->enqueueEffectsBank(eq,comp,chain,500),"sixty-command bank rejects with only fifty-nine free slots");
 std::array<float,128> l{},r{};float* audio[]{l.data(),r.data()};
 auto render=[&]{watch=true;e->process(audio,2,64);watch=false;};render();render();
 for(unsigned i=0;i<20;++i)check(!e->inspectAudioThread(i).track.equalizer.enabled&&!e->inspectAudioThread(i).track.compressor.enabled&&e->inspectAudioThread(i).track.signalChain==SignalChain{},"failed bank publishes no prefix");
 auto invalid=chain;invalid[19].size=14;check(!e->enqueueEffectsBank(eq,comp,invalid,500),"invalid last track rejects entire bank");render();
 check(e->enqueueEffectsBank(eq,comp,chain,500),"complete bank admitted after drain");render();
 for(unsigned i=0;i<20;++i){const auto t=e->inspectAudioThread(i).track;check(t.equalizer==eq[i]&&t.compressor==comp[i]&&t.signalChain==chain[i],"all settings and routing reach audio together");}
 const auto old=e->inspectAudioThread(19).track;check(e->enqueueLoop({LoopAction::clearAll,0,600,e->generation()}),"clear all");
 LoopCommand stale{LoopAction::signalChain,19,601,e->generation()};stale.signalChain=normalizedSignalChain("CQ");stale.chainClearEpoch=old.clearEpoch;check(e->enqueueLoop(stale),"queued obsolete chain");render();
 for(unsigned i=0;i<20;++i)check(e->inspectAudioThread(i).track.signalChain==SignalChain{},"clear all resets all chains and rejects pre-clear epochs");
 e->prepare(48000,32768);check(!e->enqueueLoop(stale),"old generation rejected before queue");
}
int main(int argc,char** argv){
 if(argc==2&&std::string_view(argv[1])=="normalize"){std::string s;while(std::getline(std::cin,s))std::cout<<aion::normalizedSignalChain(s).text()<<'\n';return 0;}
 for(double rate:{32000.,48000.,96000.})for(unsigned block:{64u,128u})connected(block,rate);
 bankAdmission();
 check(operations==0,"zero callback allocation/deallocation");
}
