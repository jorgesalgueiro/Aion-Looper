#include "core/LoopMeterGraph.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
extern thread_local bool watch;
extern std::atomic<int> allocations;
namespace {void check(bool ok,const char* label){if(!ok){std::cerr<<label<<'\n';std::exit(1);}}}
void meterGraphTests(){
    using namespace aion;
    LoopMeterGraph graph;std::array<float,2048> samples{};samples.fill(1.25f);
    watch=true;
    check(!graph.startGraph(),"No graph without a committed buffer");
    graph.setBuffer(true);check(graph.startGraph(),"Fresh graph binds analyser/data");
    const auto first=graph.inspect().data;
    check(graph.observe(samples,first,1,1,1,true)&&graph.value().clipped,"Graph meter observes real clipping");
    const auto held=graph.value();
    check(!graph.observe(samples,first+1,5,5,1,true)&&graph.value().peak==held.peak&&graph.value().clipped,"Stale graph packet atomically preserves holds");
    check(!graph.observe(std::span<const float>(samples).first(128),first,5,5,1,true)&&graph.value().peak==held.peak,"Short active window rejected without history edit");
    check(!graph.stop(NAN)&&graph.inspect().state==MeterLoopState::playing,"Invalid event clock rejected without lifetime edit");
    graph.stop(1);check(graph.inspect().teardownDelay==0.025,"Original immediate stop timer request");
    check(graph.observe(samples,first,1,1,1,true)&&graph.value().peak==held.peak,"Stopping graph still samples before explicit teardown");
    graph.teardown();check(graph.inspect().data==first&&!graph.inspect().analyser,"Teardown retains data ownership but disconnects analyser");
    graph.observe(samples,999,1,1,1,true);check(graph.value().peak==held.peak,"Stalled clock teardown retains pending observation/holds");
    graph.clear();graph.observe(samples,999,1.1,1.1,1,true);check(graph.value().peak==held.peak&&graph.value().clipped,"Clear after teardown preserves original detached-meter hold");
    graph.setBuffer(true);graph.startGraph();const auto second=graph.inspect().data;check(second!=first,"Fresh play never reuses prior data identity");
    samples.fill(0.1f);graph.observe(samples,second,1.2,1.2,1,true);check(graph.value().peak==static_cast<double>(0.1f)&&!graph.value().clipped,"Fresh analyser resets old clipping/peak");
    graph.clear();check(graph.value().peak==0&&!graph.value().clipped&&!graph.inspect().data,"Clear with attached analyser resets meter and data");
    graph.setBuffer(true);graph.startGraph();graph.destroyGraph();
    graph.observe(samples,0,2,2,1,true);check(graph.value().peak==0,"Destroyed graph cannot sample retained PCM");
    graph.teardown();graph.startGraph();graph.stop(2,2.5);check(graph.inspect().teardownDelay==0.55,"Scheduled deadline plus original 50ms cleanup request");
    check(graph.startGraph()&&graph.inspect().teardownDelay<0,"Play while stopping cancels old teardown and creates fresh graph");
    graph.stop(3,4);graph.setState(MeterLoopState::queued);
    check(graph.startGraph()&&graph.inspect().teardownDelay==1.05,"Queued state change preserves original pending stop timer on play");
    watch=false;
    // Identical analyser observations assembled from live-style 64/128 PCM
    // partitions. Caller explicitly resets the window on fresh graph creation.
    MeterReading baseline{};
    for(unsigned block:{64u,128u}){
        LoopMeterGraph meter;MeterWindow window;meter.setBuffer(true);meter.startGraph();std::array<float,2048> raw{};
        watch=true;for(unsigned offset=0;offset<4096;offset+=block){for(unsigned i=0;i<block;++i)window.pushStereo(0.4f,-0.1f);}
        window.copy(raw);check(meter.observe(raw,meter.inspect().data,1,1,1,true),"Assembled native window accepted");
        const auto reading=meter.value();watch=false;
        if(block==64)baseline=reading;else check(reading.rms==baseline.rms&&reading.peak==baseline.peak&&reading.rmsPercent==baseline.rmsPercent,"Exact supplied-window graph meter 64/128 invariance");
    }
    check(allocations.load()==0,"Graph lifecycle/read composition has zero tracked new/delete");
    std::cout<<"PASS: explicit meter graph lifetime, stale/short-window rejection, detached versus attached clear, stop timer requests, fresh play and supplied PCM 64/128; zero tracked new/delete\n";
}
