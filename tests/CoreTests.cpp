#include "core/Engine.h"
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <thread>
#include <vector>
thread_local bool watch=false;
std::atomic<bool> failWorkerAllocation{false};
std::atomic<int> allocations{0};
void* operator new(std::size_t n) { if(watch) ++allocations; if(!watch && failWorkerAllocation.exchange(false))throw std::bad_alloc(); if(auto* p=std::malloc(n == 0 ? 1 : n)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { if(watch) ++allocations; std::free(p); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }
static void check(bool ok, const char* what) { if(!ok) { std::cerr<<what<<'\n'; std::exit(1); } }
void loopTests();
void reverseTests();
void historyTests();
void multiplyTests();
void syncTests();
void metronomeTests();
void meterTests();
void meterWindowTests();
void meterTransferTests();
void meterSourceTests();
void meterGraphTests();
void playbackClockTests();
void countInTests();
void startPhaseTests();
void stutterTests();
void exportTests();
void mergeTests();
void midiTests();
void waveformTests();
void liveInputTests();
static void hostTransportTests() {
    aion::Transport good;good.hostValid=true;good.playing=good.recording=good.looping=true;good.callbackFrame=64;
    good.hasBpm=good.hasPpq=good.hasSamples=good.hasSeconds=good.hasTimeSignature=good.hasBarStart=good.hasLoopPoints=true;
    good.bpm=137.3;good.ppq=-1.25;good.samplePosition=-12000;good.seconds=-0.25;good.numerator=7;good.denominator=8;good.barStartPpq=-3.5;good.loopStartPpq=-8;good.loopEndPpq=6;
    auto valid=aion::validatedTransport(good);check(valid.hasBpm&&valid.bpm==137.3&&valid.hasPpq&&valid.ppq==-1.25&&valid.hasSamples&&valid.samplePosition==-12000&&valid.hasTimeSignature&&valid.numerator==7&&valid.denominator==8&&valid.hasLoopPoints&&valid.loopStartPpq==-8,"Valid host fractional meter/tempo/negative preroll");
    for(double bad:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}){
        auto t=good;t.bpm=t.ppq=t.seconds=t.barStartPpq=t.loopStartPpq=bad;const auto clean=aion::validatedTransport(t);
        check(!clean.hasBpm&&!clean.hasPpq&&!clean.hasSeconds&&!clean.hasBarStart&&!clean.hasLoopPoints&&clean.bpm==120&&clean.ppq==0&&clean.seconds==0&&clean.barStartPpq==0&&clean.loopStartPpq==0&&clean.loopEndPpq==0,"Nonfinite host fields never escape validation");
    }
    for(double bad:{0.0,-1.0}){auto t=good;t.bpm=bad;check(!aion::validatedTransport(t).hasBpm,"Host tempo must be positive");}
    for(int bad:{0,-1}){auto t=good;t.denominator=bad;check(!aion::validatedTransport(t).hasTimeSignature,"Host denominator must be positive");t=good;t.numerator=bad;check(!aion::validatedTransport(t).hasTimeSignature,"Host numerator must be positive");}
    auto t=good;t.loopEndPpq=t.loopStartPpq;check(!aion::validatedTransport(t).hasLoopPoints,"Empty host cycle unavailable");t.loopEndPpq-=1;check(!aion::validatedTransport(t).hasLoopPoints,"Reversed host cycle unavailable");
    t=good;t.hostValid=false;valid=aion::validatedTransport(t);check(!valid.hostValid&&!valid.playing&&!valid.recording&&!valid.looping&&!valid.hasBpm&&!valid.hasPpq&&!valid.hasSamples&&!valid.hasSeconds&&!valid.hasTimeSignature&&!valid.hasBarStart&&!valid.hasLoopPoints&&valid.callbackFrame==64,"Host loss resets every stale flag, retaining callback stamp");
    for(int block:{64,128}){
        auto engine=std::make_unique<aion::Engine>();engine->prepare(48000,8192);std::array<float,128> left{},right{};float* channels[]={left.data(),right.data()};
        for(unsigned n=0;n<32;++n){t=good;t.callbackFrame=engine->clock();t.ppq=double(n);engine->transport=aion::validatedTransport(t);watch=true;engine->process(channels,2,block);watch=false;}
        aion::EngineView view;unsigned popped=0;
        while(engine->popView(view)){++popped;check(view.hostTransport.callbackFrame+block==view.frame&&view.hostTransport.ppq==double(view.hostTransport.callbackFrame/block),"Saturated snapshot queue never mixes host position and audio block");}
        check(popped==7,"Host telemetry retains bounded original snapshot capacity");
        engine->transport={};engine->transport.callbackFrame=engine->clock();watch=true;engine->process(channels,2,block);watch=false;
        check(engine->popView(view)&&!view.hostTransport.hostValid&&view.hostTransport.callbackFrame+block==view.frame,"Host loss publishes after slow-editor recovery");
        engine->transport=good;engine->prepare(48000,8192);check(!engine->transport.hostValid&&!engine->transport.hasBpm,"Prepare resets prior host observation");
    }
    check(allocations==0,"Host snapshot callback allocation/free");
    std::cout<<"PASS: optional host validation, preroll, meter/cycle, loss/reset and coherent bounded snapshots at 64/128\n";
}
int main() {
    hostTransportTests();
    aion::SpscQueue<unsigned, 8> q; unsigned value=0;
    for(unsigned i=0;i<7;++i) check(q.push(i),"queue capacity");
    check(!q.push(99),"overflow must reject");
    for(unsigned i=0;i<7;++i) {check(q.pop(value)&&value==i,"FIFO");} check(!q.pop(value),"underflow");
    aion::SpscQueue<unsigned,1024> threaded;
    std::thread producer([&]{for(unsigned i=0;i<100000;++i)while(!threaded.push(i))std::this_thread::yield();});
    for(unsigned i=0;i<100000;++i){while(!threaded.pop(value))std::this_thread::yield();check(value==i,"concurrent FIFO");} producer.join();
    aion::Engine a,b; a.prepare(48000, 8192); b.prepare(48000, 8192);
    check(a.enqueue({32,1.0f})&&b.enqueue({32,1.0f}),"timestamped gain");
    check(!a.enqueue({0,std::numeric_limits<float>::quiet_NaN()}),"reject nonfinite control");
    std::vector<float> x(2048,0.5f), y=x;
    float* ax[]={x.data()}; float* by[]={y.data()};
    watch=true;
    for(int offset=0;offset<2048;offset+=64){float* block[]={ax[0]+offset};a.process(block,1,64);}
    for(int offset=0;offset<2048;offset+=128){float* block[]={by[0]+offset};b.process(block,1,128);}
    watch=false;
    check(allocations==0,"callback allocation/free"); check(x==y,"64/128 block invariance");
    check(x[31]==0.0f&&x[32]>0.0f,"sample accurate command boundary");
    check(std::abs(x.back()-0.5f)<1e-5f,"smoothing reaches target"); check(a.clock()==2048,"sample clock");
    x[0]=std::numeric_limits<float>::infinity(); a.process(ax,1,1);check(x[0]==0.0f,"nonfinite audio");
    loopTests();
    reverseTests();
    historyTests();
    multiplyTests();
    syncTests();
    metronomeTests();
    meterTests();
    meterWindowTests();
    meterTransferTests();
    meterSourceTests();
    meterGraphTests();
    playbackClockTests();
    countInTests();
    startPhaseTests();
    stutterTests();
    exportTests();
    mergeTests();
    midiTests();
    waveformTests();
    liveInputTests();
    std::cout<<"PASS: queue bounds/concurrency, sample timing, smoothing, 64/128 invariance, no callback new/delete, finite output\n";
}
