#include "core/Engine.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>
extern thread_local bool watch;
extern std::atomic<int> allocations;
namespace {void check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}}
void metronomeTests(){
 using namespace aion;
 Engine a,b;a.prepare(48000,8192);b.prepare(48000,8192);a.setMonitoring(false);b.setMonitoring(false);
 a.setMetronome(true,0.5f);b.setMetronome(true,0.5f);
 std::vector<float> left(96000),right(96000),other(96000),otherRight(96000);
 watch=true;
 for(unsigned i=0;i<96000;i+=64){float* channels[]={left.data()+i,right.data()+i};a.process(channels,2,64);}
 for(unsigned i=0;i<96000;i+=128){float* channels[]={other.data()+i,otherRight.data()+i};b.process(channels,2,128);}
 watch=false;
 check(left==other&&left==right&&left==otherRight,"metronome stereo/64/128 exact invariance");
 check(left[0]==0&&left[1]>0&&left[24000]==0&&left[24001]>0,"clicks at exact sample-grid boundaries");
 check(left[1]>left[24001],"bar accent has higher frequency");
 check(a.clock()==96000,"metronome follows engine clock while master muted");
 EngineView meterView{};bool meteredClick=false;
 while(a.popView(meterView)){check(meterView.masterPeak==0,"monitor click excluded from MASTER meter");meteredClick|=meterView.outputPeak>0.01f;}
 check(meteredClick,"device output meter includes the click");

 a.setMetronome(false,0.5f);float silent[128]{};float* muted[]={silent};watch=true;a.process(muted,1,128);watch=false;
 for(float sample:silent)check(sample==0,"disable removes click tail");
 // Monitor clicks must never enter the recorded loop, including with MASTER muted.
 Engine recorder;recorder.prepare(48000,8192);recorder.setMonitoring(false);recorder.setMetronome(true,0.5f);
 const auto generation=recorder.generation();check(recorder.enqueueLoop({LoopAction::record,0,1,generation,0,0}),"record silence with click");
 float l[64]{},r[64]{};float* channels[]={l,r};bool heard=false;
 for(unsigned i=0;i<4096;i+=64){std::fill_n(l,64,0);std::fill_n(r,64,0);watch=true;recorder.process(channels,2,64);watch=false;for(float value:l)heard|=std::abs(value)>0.01f;}
 check(heard,"metronome bypasses muted MASTER");
 recorder.setMetronome(false,0.5f);check(recorder.enqueueLoop({LoopAction::finish,0,2,generation,0,0}),"finish silent capture");
 EngineView latest{},view{};
 for(unsigned i=0;i<10000;++i){std::fill_n(l,64,0);std::fill_n(r,64,0);watch=true;recorder.process(channels,2,64);watch=false;while(recorder.popView(view))latest=view;if(latest.tracks[0].state==LoopState::playing)break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 check(latest.tracks[0].length==4096&&latest.tracks[0].state==LoopState::playing,"silent take commits with exact length");
 recorder.setTarget(1);
 for(unsigned i=0;i<8192;i+=64){std::fill_n(l,64,0);std::fill_n(r,64,0);watch=true;recorder.process(channels,2,64);watch=false;for(float value:l)check(value==0,"monitor click excluded from recorded PCM");}
 Metronome metro;metro.prepare(std::numeric_limits<double>::quiet_NaN());metro.configure(std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),0);metro.setVolume(std::numeric_limits<float>::infinity());metro.setEnabled(true);
 watch=true;for(unsigned i=0;i<10000;++i)check(metro.tick(i)==0,"nonfinite controls remain silent/finite");watch=false;
 check(allocations.load()==0,"metronome audio path must not allocate/free");
 std::cout<<"PASS: native metronome sample grid, stereo/64/128, muted-master bypass, capture exclusion, stop and finite controls; no callback new/delete\n";
}
