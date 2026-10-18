#include "core/Engine.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>
#include <atomic>
extern std::atomic<bool> watch;
extern std::atomic<int> allocations;
namespace {
void check(bool ok, const char* message) { if (!ok) { std::cerr << message << '\n'; std::exit(1); } }
void near(float a, float b, const char* message) { check(std::abs(a-b) < 0.00001f, message); }
}
constexpr float centre = 0.7071067811865475f;
void loopTests() {
    using namespace aion;
    LoopBank bank; bank.prepare(8192, 0);
    check(!bank.apply(LoopAction::play, 0), "empty track cannot play");
    check(!bank.apply(LoopAction::record, 20), "track index bounds");
    check(bank.apply(LoopAction::record, 0), "record starts");
    watch = true;
    for (int i = 0; i < 4; ++i) { auto out=bank.tick({0.25f, 0.5f}); near(out.left,0,"record does not double-monitor"); }
    check(bank.apply(LoopAction::finish,0),"record finishes into playback");
    for(int i=0;i<12;++i) {auto out=bank.tick({});near(out.left,0.375f*centre,"left wraps exactly");near(out.right,0.375f*centre,"right retained");}
    check(bank.apply(LoopAction::overdub,0),"overdub begins");
    for(int i=0;i<4;++i) bank.tick({0.1f,0.1f});
    check(bank.apply(LoopAction::finish,0),"overdub ends");
    auto out=bank.tick({}); near(out.left,0.4f*centre,"overdub applies 80 percent feedback");near(out.right,0.4f*centre,"stereo overdub");
    check(bank.view()[0].length==4,"overdub preserves length");
    check(bank.apply(LoopAction::substitute,0),"substitute begins");
    for(int i=0;i<4;++i) bank.tick({0.6f,-0.4f});
    bank.apply(LoopAction::finish,0);out=bank.tick({});near(out.left,0.1f*centre,"substitute replaces old PCM");near(out.right,0.1f*centre,"substitute stereo");
    bank.apply(LoopAction::stop,0);out=bank.tick({});near(out.left,0,"stop output");
    bank.apply(LoopAction::play,0);out=bank.tick({});near(out.left,0.1f*centre,"restart replays retained audio");
    bank.apply(LoopAction::clear,0);check(bank.available()==8192,"clear recycles fixed pool blocks");
    bank.apply(LoopAction::record,0);bank.tick({std::numeric_limits<float>::infinity(),0.1f});
    bank.apply(LoopAction::finish,0);out=bank.tick({});near(out.left,0.05f*centre,"nonfinite capture sanitized");near(out.right,0.05f*centre,"short rerecord ignores old tail");
    watch = false;
    check(allocations.load()==0,"record/play/overdub/substitute/clear must not allocate or free");

    bank.prepare(4096,0);bank.apply(LoopAction::record,0);
    for(int i=0;i<4096;++i) bank.tick({0.25f,0.5f});
    out=bank.tick({1,1});check(bank.exhausted(),"pool exhaustion reported");
    near(out.left,0.375f*centre,"pool exhaustion preserves captured PCM");
    check(bank.view()[0].length==4096,"pool exhaustion exact length");
    check(!bank.apply(LoopAction::record,1),"full pool rejects another recording");
    bank.apply(LoopAction::clear,0);check(bank.apply(LoopAction::record,1),"capacity reusable after clear");

    bank.prepare(20*4096,0);
    watch=true;
    for(unsigned i=0;i<20;++i) check(bank.apply(LoopAction::record,i),"all 20 track slots record");
    bank.tick({0.01f,0.02f});
    for(unsigned i=0;i<20;++i) bank.apply(LoopAction::finish,i);
    out=bank.tick({});near(out.left,0.3f*centre,"20-track sum");near(out.right,0.3f*centre,"20-track stereo sum");
    bank.apply(LoopAction::mute,0,1);out=bank.tick({});near(out.left,0.285f*centre,"mute excludes track");
    for(int frame=0;frame<8192;++frame)bank.tick({});
    bank.apply(LoopAction::stopAll,0);near(bank.tick({}).left,0,"stop all");
    watch=false;check(allocations.load()==0,"20-track callback allocation/free");

    bank.prepare(4096,4);bank.apply(LoopAction::record,0);
    for(int i=0;i<8;++i)bank.tick({0.5f,0.5f});
    bank.apply(LoopAction::finish,0);
    for(int i=0;i<4;++i)near(bank.tick({}).left,centre*0.125f*static_cast<float>(i+1),"play attack fade");
    bank.apply(LoopAction::stop,0);
    for(int i=0;i<4;++i)near(bank.tick({}).left,centre*0.125f*static_cast<float>(3-i),"stop release fade");

    bank.prepare(8192,0);bank.setAutoPlay(false);
    bank.apply(LoopAction::record,0);bank.tick({0.2f,0.3f});bank.apply(LoopAction::finish,0);
    check(bank.view()[0].state==LoopState::stopped,"auto-play off finishes stopped");
    near(bank.tick({}).left,0,"auto-play off is silent");
    bank.apply(LoopAction::retrigger,0);near(bank.tick({}).left,0.25f*centre,"retrigger starts stopped PCM");
    bank.apply(LoopAction::stop,0);bank.apply(LoopAction::clear,0);bank.setAutoPlay(true);
    bank.apply(LoopAction::record,0);bank.tick({0.1f,0.1f});bank.tick({0.4f,0.4f});bank.apply(LoopAction::finish,0);
    bank.tick({});bank.apply(LoopAction::retrigger,0);near(bank.tick({}).left,0.1f*centre,"retrigger resets exact phase");
    bank.apply(LoopAction::record,1);bank.tick({0.2f,0.2f});bank.apply(LoopAction::finish,1);
    watch=true;
    bank.apply(LoopAction::solo,1);near(bank.tick({}).left,0.2f*centre,"solo gates other tracks");
    check(bank.view()[1].solo&&!bank.view()[0].solo,"solo snapshot");
    bank.apply(LoopAction::clear,2);check(bank.view()[1].solo,"clearing another track preserves solo");
    bank.apply(LoopAction::mute,0,1);bank.apply(LoopAction::solo,0);near(bank.tick({}).left,0,"solo respects explicit mute");
    bank.apply(LoopAction::solo,0);near(bank.tick({}).left,0.2f*centre,"unsolo restores other tracks without losing mute");
    bank.apply(LoopAction::clearAll,0);near(bank.tick({}).left,0,"clear all silence");
    check(bank.available()==8192&&!bank.view()[0].solo,"clear all recycles pool and solo");
    watch=false;check(allocations.load()==0,"solo/retrigger/clear-all callback allocation/free");

    bank.prepare(4096,0);bank.setAutoPlay(false);
    bank.apply(LoopAction::record,0);bank.apply(LoopAction::record,1);bank.tick({0.2f,0.2f});
    check(bank.view()[1].state==LoopState::empty,"simultaneous recording pool shortage leaves empty slot usable");
    for(int i=1;i<=4096;++i)bank.tick({0.2f,0.2f});
    check(bank.view()[0].state==LoopState::stopped,"full pool honors auto-play off");
    check(bank.view()[0].length==4096,"full pool retains take with auto-play off");

    bank.prepare(4096,0);bank.setAutoPlay(true);
    bank.apply(LoopAction::record,0);
    for(int i=0;i<4;++i)bank.tick({0.1f*static_cast<float>(i),0.1f*static_cast<float>(i)});
    bank.apply(LoopAction::finish,0);
    watch=true;
    check(bank.apply(LoopAction::speed,0,0.5f),"half speed accepted");
    for(int i=0;i<8;++i){const float phase=0.5f*i;const float expected=phase<=3?phase*0.1f:(4-phase)*0.3f;near(bank.tick({}).left,expected*centre,"half speed fractional interpolation and wrap");}
    bank.apply(LoopAction::speed,0,2);
    near(bank.tick({}).left,0,"double speed first sample");near(bank.tick({}).left,0.2f*centre,"double speed skips source frame");
    check(!bank.apply(LoopAction::overdub,0)&&!bank.apply(LoopAction::substitute,0),"reference varispeed capture restriction");
    const auto phaseBefore=bank.view()[0].position;bank.apply(LoopAction::speed,0,1);check(bank.view()[0].position==phaseBefore,"rate change preserves phase");
    check(bank.apply(LoopAction::overdub,0),"unity speed edit allowed");check(!bank.apply(LoopAction::speed,0,0.5f),"active edit rejects speed change");bank.apply(LoopAction::finish,0);
    check(!bank.apply(LoopAction::speed,0,0.24f)&&!bank.apply(LoopAction::speed,0,2.01f)&&!bank.apply(LoopAction::pan,0,-1)&&!bank.apply(LoopAction::pan,0,11),"speed and pan bounds");
    bank.apply(LoopAction::stop,0);bank.apply(LoopAction::clear,0);bank.apply(LoopAction::record,0);bank.tick({0.2f,0.6f});bank.apply(LoopAction::finish,0);
    bank.apply(LoopAction::pan,0,0);out=bank.tick({});near(out.left,0.4f,"reference pan left downmix");near(out.right,0,"hard left silences right");
    bank.apply(LoopAction::pan,0,10);out=bank.tick({});near(out.left,0,"hard right silences left");near(out.right,0.4f,"reference pan right downmix");
    bank.apply(LoopAction::pan,0,5);out=bank.tick({});near(out.left,0.4f*centre,"reference center gain");near(out.right,0.4f*centre,"reference center right gain");
    bank.apply(LoopAction::volume,0,0.2f);bank.apply(LoopAction::feedback,0,0.4f);bank.apply(LoopAction::speed,0,2);bank.apply(LoopAction::clear,0);
    const auto cleared=bank.view()[0];check(cleared.volume==1&&cleared.feedback==0.8f&&cleared.pan==5&&cleared.playbackRate==1,"reference clear resets controls");
    watch=false;check(allocations.load()==0,"pan and speed callback allocation/free");

    bank.prepare(4096,4);bank.apply(LoopAction::record,0);
    for(int i=0;i<64;++i)bank.tick({0.4f,0.4f});bank.apply(LoopAction::finish,0);
    for(int i=0;i<4;++i)bank.tick({});
    bank.apply(LoopAction::pan,0,0);float previousRight=0.4f*centre;
    for(int i=0;i<16;++i){out=bank.tick({});check(out.right<=previousRight+0.000001f,"pan interpolation is monotonic");previousRight=out.right;}
    near(out.left,0.4f,"pan ramp reaches target");near(out.right,0,"pan ramp reaches silence");
    const auto startPhase=bank.view()[0].position;bank.apply(LoopAction::speed,0,2);
    for(int i=0;i<16;++i)bank.tick({});
    check(bank.view()[0].position==(startPhase+24)%64,"rate ramp integrates smoothly without phase reset");

    Engine a,b;a.prepare(8000,4096);b.prepare(8000,4096);a.setMonitoring(false);b.setMonitoring(false);a.setTarget(1);b.setTarget(1);
    const auto enqueue=[](Engine& e) {
        const LoopAction actions[]={LoopAction::record,LoopAction::finish,LoopAction::overdub,LoopAction::finish,LoopAction::stop,LoopAction::play,LoopAction::clear};
        const std::uint64_t deadlines[]={8,24,32,40,48,64,80};
        for(unsigned i=0;i<7;++i)check(e.enqueueLoop({actions[i],0,i+1,e.generation(),deadlines[i],0}),"enqueue sample-deadline action");
    };
    enqueue(a);enqueue(b);
    std::vector<float> al(512,0.25f),ar(512,0.5f),bl=al,br=ar;
    watch=true;
    for(int i=0;i<512;i+=64){float* data[]={al.data()+i,ar.data()+i};a.process(data,2,64);}
    for(int i=0;i<512;i+=128){float* data[]={bl.data()+i,br.data()+i};b.process(data,2,128);}
    watch=false;
    check(allocations.load()==0,"engine looper callback allocation/free");
    check(al==bl&&ar==br,"loop command timing invariant across 64/128 blocks");
    check(al[23]==0&&al[24]>0,"finish command exact sample boundary");
    check(al.back()==0,"clear release reaches silence");
    EngineView snapshot{};while(a.popView(snapshot)){};
    check(snapshot.lastRequestId==7&&!snapshot.rejected,"audio command acknowledgement");
    const auto generation=a.generation();a.prepare(8000,4096);
    check(!a.enqueueLoop({LoopAction::record,0,8,generation,0,0}),"stale generation rejected");

    for(unsigned i=0;i<255;++i)check(a.enqueueLoop({LoopAction::stop,0,i+10,a.generation(),0,0}),"bounded command queue capacity");
    check(!a.enqueueLoop({LoopAction::stop,0,999,a.generation(),0,0}),"command queue overflow rejects");
    float silent[64]{};float* silence[]={silent};
    a.process(silence,1,64);check(a.popView(snapshot)&&snapshot.lastRequestId==137,"128 command/block execution budget");
    a.process(silence,1,64);check(a.popView(snapshot)&&snapshot.lastRequestId==264,"overflow backlog retained for next block");
    b.prepare(8000,4096);b.setTarget(1);
    check(b.enqueueLoop({LoopAction::record,0,10,b.generation(),0,0}),"record before reprepare");
    check(b.enqueueLoop({LoopAction::finish,0,11,b.generation(),8,0}),"finish before reprepare");
    float samples[16];for(auto& v:samples)v=0.2f;float* mono[]={samples};b.process(mono,1,16);
    b.prepare(8000,4096);b.setTarget(1);
    check(b.enqueueLoop({LoopAction::play,0,12,b.generation(),0,0}),"same-rate reprepare retains loop");
    for(auto& v:samples)v=0;b.process(mono,1,16);check(samples[0]>0,"retained PCM replays");
    while(b.popView(snapshot)){};
    check(snapshot.tracks[0].length==8&&!snapshot.audioReset,"same-rate lifecycle preserves audio");
    b.prepare(16000,4096);b.process(mono,1,16);while(b.popView(snapshot)){};
    check(snapshot.tracks[0].state==LoopState::empty&&snapshot.audioReset,"rate change reset explicitly reported");
    a.prepare(8000,4096);b.prepare(8000,4096);a.setTarget(1);b.setTarget(1);
    for(auto* engine : {&a,&b}) {
        const auto gen=engine->generation();
        check(engine->enqueueLoop({LoopAction::record,0,1,gen,0,0}),"rate fixture record");
        check(engine->enqueueLoop({LoopAction::finish,0,2,gen,256,0}),"rate fixture finish");
        check(engine->enqueueLoop({LoopAction::speed,0,3,gen,257,0.37f}),"rate fixture fractional speed");
        check(engine->enqueueLoop({LoopAction::pan,0,4,gen,379,10}),"rate fixture pan");
        check(engine->enqueueLoop({LoopAction::speed,0,5,gen,701,2}),"rate fixture double speed");
    }
    std::vector<float> rateA(2048),rateB(2048),rightA(2048),rightB(2048);
    for(int i=0;i<2048;++i){rateA[i]=rateB[i]=0.2f*std::sin(0.1f*i);rightA[i]=rightB[i]=0.1f*std::cos(0.1f*i);}
    watch=true;
    for(int i=0;i<2048;i+=64){float* data[]={rateA.data()+i,rightA.data()+i};a.process(data,2,64);}
    for(int i=0;i<2048;i+=128){float* data[]={rateB.data()+i,rightB.data()+i};b.process(data,2,128);}
    watch=false;
    check(rateA==rateB&&rightA==rightB,"fractional speed/pan/ramps invariant across 64/128 blocks");
    check(allocations.load()==0,"fractional rate callback allocation/free");
    std::cout<<"PASS: native stereo record/play/stop/overdub/substitute, 20 tracks, bounded pool reuse/exhaustion, fades, deadline invariance, generation checks, no callback new/delete\n";
}
