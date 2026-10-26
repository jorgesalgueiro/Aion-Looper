#include "core/CallbackAudibleTime.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>
extern thread_local bool watch;
extern std::atomic<int> allocations;
namespace {
void check(bool ok,const char* label){if(!ok){std::cerr<<label<<'\n';std::exit(1);}}
}
void callbackAudibleTimeTests(){
    using namespace aion;
    std::uint64_t observations=0;
    watch=true;
    // Independent hardware timeline: at hardware frame H the FIFO has emitted
    // max(0,H-L) local frames. Query at callback delivery and between deliveries,
    // with a consumer delayed beyond the last available rendered block too.
    for(double rate:{8000.0,44100.0,48000.0,96000.0})for(unsigned block:{64u,128u})
    for(unsigned latency:{0u,1u,32u,256u,4096u})for(unsigned index=0;index<80;++index){
        const std::uint64_t begin=static_cast<std::uint64_t>(index)*block;
        CallbackClockStamp stamp{index+1,begin,begin+block,~std::uint64_t{0},rate,3,true,false,latency,true};
        const CallbackClockCorrelation correlation{stamp,{19,100+static_cast<double>(begin)/rate}};
        for(unsigned lag:{0u,1u,31u,64u,128u,4096u}){
            const auto hardwareFrame=begin+lag;
            const double emitted=hardwareFrame>latency?static_cast<double>(hardwareFrame-latency):0;
            const double expected=std::min(emitted,static_cast<double>(stamp.endFrame))/rate;
            CallbackAudibleTime result;
            check(callbackAudibleTime(stamp,correlation,{19,100+static_cast<double>(hardwareFrame)/rate},3,result),"Coherent live callback correlation accepted");
            check(std::abs(result.audibleTime-expected)<1e-12,"Audible estimate matches independent delayed hardware FIFO timeline");
            check(result.renderedTime==static_cast<double>(stamp.endFrame)/rate,"Rendered end remains the observation time");
            ++observations;
        }
    }
    CallbackClockStamp stamp{1,1024,1152,0,48000,7,false,false,256,true};
    const CallbackClockCorrelation correlation{stamp,{1,100}};
    CallbackAudibleTime result{42,43};
    const auto unchanged=result;
    const auto rejected=[&](CallbackClockStamp candidate,CallbackClockCorrelation anchor,MonotonicClockPoint wall,unsigned generation){
        check(!callbackAudibleTime(candidate,anchor,wall,generation,result)&&result==unchanged,"Invalid/mismatched clock atomically preserves prior result");
    };
    for(double bad:{-1.0,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){
        auto anchor=correlation;anchor.entry.seconds=bad;rejected(stamp,anchor,{1,100},7);
        rejected(stamp,correlation,{1,bad},7);
    }
    rejected(stamp,correlation,{0,100},7);rejected(stamp,correlation,{2,100},7);
    rejected(stamp,correlation,{1,99},7);rejected(stamp,correlation,{1,100},8);
    auto bad=stamp;bad.nonRealtime=true;rejected(bad,{bad,correlation.entry},{1,100},7);
    bad=stamp;bad.hasOutputLatency=false;bad.outputLatencySamples=0;rejected(bad,{bad,correlation.entry},{1,100},7);
    bad=stamp;bad.endFrame=bad.beginFrame;rejected(bad,{bad,correlation.entry},{1,100},7);
    bad=stamp;bad.endFrame=(std::uint64_t{1}<<53)+1;rejected(bad,{bad,correlation.entry},{1,100},7);
    bad=stamp;bad.sampleRate=std::numeric_limits<double>::denorm_min();rejected(bad,{bad,correlation.entry},{1,100},7);
    bad=stamp;bad.serial++;rejected(bad,correlation,{1,100},7);
    bad=stamp;bad.hostTimeNs=1;bad.hasHostTimeNs=true;rejected(bad,correlation,{1,100},7);
    bad=stamp;bad.outputLatencySamples++;rejected(bad,correlation,{1,100},7);
    bad=stamp;bad.beginFrame++;rejected(bad,correlation,{1,100},7);
    bad=stamp;bad.sampleRate=44100;rejected(bad,correlation,{1,100},7);
    // At entry the wrong endFrame-L formula is ahead by precisely one block.
    check(callbackAudibleTime(stamp,correlation,{1,100},7,result),"Known entry accepted");
    check(std::abs((static_cast<double>(stamp.endFrame)-256)/48000-result.audibleTime-128.0/48000)<1e-15,"Fixture detects one-block lead");
    // Raw host clock availability/origin cannot affect a correctly paired model.
    auto raw=stamp;raw.hostTimeNs=std::numeric_limits<std::uint64_t>::max();raw.hasHostTimeNs=true;
    CallbackAudibleTime rawResult;
    check(callbackAudibleTime(raw,{raw,correlation.entry},{1,100},7,rawResult)&&rawResult==result,"Raw host nanoseconds are never treated as native monotonic time");
    std::uint64_t splitObservations=0;
    for(double rate:{8000.0,44100.0,48000.0,96000.0})for(unsigned block:{64u,128u})
    for(unsigned latency:{0u,1u,256u,4096u})for(unsigned total:{1u,block-1,block,block+1,3*block+17})
    for(unsigned index=0;index<12;++index){
        const std::uint64_t deliveredBegin=static_cast<std::uint64_t>(index)*total;
        for(unsigned offset=0;offset<total;offset+=block){
            const auto length=std::min(block,total-offset);
            const auto begin=deliveredBegin+offset;
            CallbackClockStamp part{index+1,begin,begin+length,0,rate,3,false,false,latency,true};
            const CallbackClockCorrelation anchor{part,{19,100+static_cast<double>(deliveredBegin)/rate},total,offset};
            for(unsigned lag:{0u,1u,block,4*block}){
                CallbackAudibleTime split;
                const auto physical=deliveredBegin+lag;
                const double emitted=physical>latency?static_cast<double>(physical-latency):0;
                const double expected=std::min(emitted,static_cast<double>(part.endFrame))/rate;
                check(callbackAudibleTime(part,anchor,{19,100+static_cast<double>(physical)/rate},3,split),"Explicit split delivery range accepted");
                check(std::abs(split.audibleTime-expected)<1e-12,"Split must subtract its offset before applying whole-device delivery time");
                ++splitObservations;
            }
        }
    }
    result=unchanged;
    for(const auto& range:{std::pair{0u,1u},std::pair{64u,0u},std::pair{128u,1u},std::pair{256u,129u},std::pair{4096u,1025u},std::pair{~0u,~0u}}){
        auto anchor=correlation;anchor.deliveredFrames=range.first;anchor.frameOffset=range.second;
        rejected(stamp,anchor,{1,100},7);
    }
    watch=false;
    check(allocations.load()==0,"Callback audible-time model has zero tracked new/delete");
    std::cout<<"PASS: "<<observations<<" callback-start/FIFO audible-time observations at 64/128; startup, delayed consumer, clock rejection; zero tracked new/delete\n";
    std::cout<<"PASS: "<<splitObservations<<" split-buffer FIFO observations, partial tails, malformed ranges; zero tracked new/delete\n";
}
