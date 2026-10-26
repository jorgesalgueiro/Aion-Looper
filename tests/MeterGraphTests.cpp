#include "core/LoopMeterGraph.h"
#include "core/LoopMeterObservation.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <limits>
extern thread_local bool watch;
extern std::atomic<int> allocations;
namespace {void check(bool ok,const char* label){if(!ok){std::cerr<<label<<'\n';std::exit(1);}}}
namespace {
auto readingTuple(const aion::MeterReading& value){return std::array<double,7>{value.rms,value.peak,value.linearPeak,value.peakDb,value.rmsPercent,value.peakPercent,value.clipped?1.0:0.0};}
void boundGraphTests(){
    using namespace aion;
    for(unsigned block:{64u,128u}){
        auto engine=std::make_unique<Engine>();auto packet=std::make_unique<MeterWindowEvent>();auto view=std::make_unique<EngineView>();
        engine->prepare(48000,81920,{true,48000,0});engine->setMonitoring(false);engine->setTarget(1);
        engine->configureSync({SyncMode::off,120,1,4,4,false});
        engine->enqueueLoop({LoopAction::record,19,1,engine->generation(),0,0});
        engine->enqueueLoop({LoopAction::finish,19,2,engine->generation(),4096,0});
        std::array<float,128> left{},right{};float* channels[]={left.data(),right.data()};
        watch=true;
        for(unsigned frame=0;frame<8192;frame+=block){left.fill(.8f);right.fill(.6f);engine->process(channels,2,static_cast<int>(block),{},frame<=4096);while(engine->popView(*view)){};}
        LoopMeterGraph graph;graph.setBuffer(true);graph.startGraph();
        LoopMeterBinding binding{engine->generation(),19,1,graph.inspect().analyser,graph.inspect().data};
        const auto startWindow=[&]{LoopCommand command{LoopAction::meterGraphStart,19,0,engine->generation(),engine->clock(),0};
            command.meterGraphTicket=binding.ticket;check(engine->enqueueLoop(command),"Explicit graph start queued");};
        startWindow();
        const auto capture=[&]{
            check(engine->requestLoopMeterWindow(19,engine->generation(),binding.ticket),"Request actual Engine PCM with a graph ticket");
            engine->process(channels,2,static_cast<int>(block));
            check(engine->popLoopMeterWindow(*packet),"Bound Engine reply delivered");while(engine->popView(*view)){};
        };
        const auto timeFor=[&]{const double now=static_cast<double>(view->frame)/view->sampleRate;return CallbackAudibleTime{now,now};};
        capture();check(observeBoundLoopMeterWindow(graph,binding,*packet,*view,view->callbackClock,timeFor())&&graph.value().peak>.01,"Actual bound Engine window reaches the explicit graph with supplied clock");
        const auto accepted=readingTuple(graph.value());
        for(unsigned field=0;field<25;++field){
            auto bad=*packet;auto wrongView=*view;auto receipt=binding;auto clock=view->callbackClock;auto time=timeFor();
            switch(field){
                case 0:bad.graphTicket=0;break;case 1:++bad.graphTicket;break;case 2:++receipt.ticket;break;
                case 3:++receipt.generation;break;case 4:++receipt.data;break;case 5:++receipt.analyser;break;
                case 6:++bad.clearEpoch;break;case 7:++bad.waveformId;break;case 8:++bad.sourceRevision;break;
                case 9:++clock.serial;break;case 10:--bad.endFrame;break;case 11:bad.sampleRate*=2;break;
                case 12:clock.nonRealtime=true;bad.callbackClock=wrongView.callbackClock=clock;break;
                case 13:time.renderedTime+=1;break;case 14:time.audibleTime=std::numeric_limits<double>::quiet_NaN();break;
                case 15:time.audibleTime=time.renderedTime+1;break;case 16:receipt.track=bad.track=20;break;
                case 17:bad.generation=0;break;case 18:wrongView.generation=0;break;
                case 19:clock.beginFrame=clock.endFrame;bad.callbackClock=wrongView.callbackClock=clock;break;
                case 20:clock.hasOutputLatency=false;bad.callbackClock=wrongView.callbackClock=clock;break;
                case 21:time.renderedTime=std::numeric_limits<double>::infinity();break;case 22:receipt.ticket=0;break;case 23:bad.windowGraphTicket=0;break;case 24:bad.graphStartFrame=bad.endFrame+1;break;
            }
            check(!observeBoundLoopMeterWindow(graph,receipt,bad,wrongView,clock,time)&&readingTuple(graph.value())==accepted,"Every stale/malformed graph/source/callback/time rejection preserves the follower");
        }
        const auto old=*packet;
        graph.teardown();graph.clear();graph.setBuffer(true);graph.startGraph();
        binding={engine->generation(),19,2,graph.inspect().analyser,graph.inspect().data};
        check(sameLoopMeterSource(old,*view)&&sameLoopMeterCallback(old,*view),"Old graph reply can still match all native source/callback fields");
        check(!observeBoundLoopMeterWindow(graph,binding,old,*view,view->callbackClock,timeFor()),"Graph recreation cannot re-label an old queued reply even for the exact same source and callback");
        startWindow();capture();check(observeBoundLoopMeterWindow(graph,binding,*packet,*view,view->callbackClock,timeFor()),"New graph request provenance recovers with fresh callback PCM");
        const auto analyser=binding.analyser,data=binding.data,source=packet->sourceRevision;
        engine->enqueueLoop({LoopAction::retrigger,19,3,engine->generation(),engine->clock(),0});
        unsigned calls=0;
        do {capture();check(observeBoundLoopMeterWindow(graph,binding,*packet,*view,view->callbackClock,timeFor()),"Healthy RTRG keeps its graph receipt while each reply matches its current source");}
        while(packet->sourceRevision==source&&++calls<20);
        check(packet->sourceRevision!=source&&graph.inspect().analyser==analyser&&graph.inspect().data==data,"Source replacement never aliases or resets analyser/data identity");
        graph.destroyGraph();const auto retained=readingTuple(graph.value());
        check(!observeBoundLoopMeterWindow(graph,binding,*packet,*view,view->callbackClock,timeFor())&&readingTuple(graph.value())==retained,"Destroyed graphs reject queued windows without mutating history");
        graph.teardown();check(!observeBoundLoopMeterWindow(graph,binding,*packet,*view,view->callbackClock,timeFor()),"Detached retained data is not an active graph");
        watch=false;
    }
    std::cout<<"PASS: bound actual Engine meter packets, 25 atomic rejection fields, same-source old-graph rejection, fresh receipts and retained RTRG analyser at 64/128; zero tracked new/delete\n";
}
}
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
    boundGraphTests();
    check(allocations.load()==0,"Graph lifecycle/read composition has zero tracked new/delete");
    std::cout<<"PASS: explicit meter graph lifetime, stale/short-window rejection, detached versus attached clear, stop timer requests, fresh play and supplied PCM 64/128; zero tracked new/delete\n";
}
