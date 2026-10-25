#include "core/LoopMeterGraph.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <limits>
extern thread_local bool watch;
extern std::atomic<int> allocations;
namespace {void check(bool ok,const char* label){if(!ok){std::cerr<<label<<'\n';std::exit(1);}}}
void playbackClockTests(){
    using namespace aion;
    PlaybackClockInput clock{true,true,10,9,1000,1025,0.1,0.2};
    watch=true;
    check(playbackTime(clock)==9.025,"Timestamp interpolation uses milliseconds and supersedes latency fallback");
    clock.wallTime=3000;check(playbackTime(clock)==10,"Timestamp cannot select future audio");
    clock.wallTime=-10000;check(playbackTime(clock)==0,"Timestamp cannot select negative audio");
    clock.contextTime=0;check(playbackTime(clock)==10-0.1-0.2,"Startup stamp uses both supplied fallback latencies once");
    clock.hasOutputTimestamp=false;clock.baseLatency=-1;clock.outputLatency=0.25;
    check(playbackTime(clock)==9.75,"Invalid base latency does not hide valid output latency");
    clock.baseLatency=std::numeric_limits<double>::infinity();check(playbackTime(clock)==9.75,"Nonfinite latency contributes zero");
    clock.currentTime=std::numeric_limits<double>::quiet_NaN();check(playbackTime(clock)==0,"Invalid current clock clamps to origin");
    clock.hasContext=false;check(playbackTime(clock)==0,"Missing context is silent clock origin");
    watch=false;
    for(unsigned block:{64u,128u}){
        LoopMeterGraph graph;graph.setBuffer(true);graph.startGraph();MeterWindow window;std::array<float,2048> samples{};
        const auto id=graph.inspect().data;
        watch=true;
        for(unsigned offset=0;offset<4096;offset+=block)for(unsigned i=0;i<block;++i)window.pushStereo(1.25f,1.25f);
        window.copy(samples);clock={true,false,1,0,0,0,0.125,0.125};
        check(graph.observe(samples,id,clock,1,true)&&graph.value().peak==0&&!graph.value().clipped,"Audible latency defers a future clipping observation at 64/128");
        samples.fill(0);clock.currentTime=1.25;
        check(graph.observe(samples,id,clock,1,true)&&graph.value().peak==1.25&&graph.value().clipped,"Audible clock selects earlier clipping observation, not newest silent window");
        const auto held=graph.value();clock.currentTime=2;
        check(!graph.observe(samples,id+1,clock,1,true)&&graph.value().peak==held.peak,"Clock overload preserves stale-window atomic rejection");
        clock.hasContext=false;
        check(graph.observe({},0,clock,1,true)&&graph.value().peak==0&&!graph.value().clipped,"Missing context resets clock/follower despite stale caller context token");
        watch=false;
    }
    check(allocations.load()==0,"Playback clock selection/composition has zero tracked new/delete");
    std::cout<<"PASS: reference timestamp/fallback audible clock, delayed clip selection and graph guards at 64/128; zero tracked new/delete\n";
}
