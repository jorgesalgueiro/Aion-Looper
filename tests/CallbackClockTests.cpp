#include "core/Engine.h"
#include "core/QueuedMeterCallbackTime.h"
#include "core/MeterStopCallbackTime.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
extern thread_local bool watch;
extern std::atomic<int> allocations;
namespace {void check(bool ok,const char* label){if(!ok){std::cerr<<label<<'\n';std::exit(1);}}}
void callbackClockTests(){
    using namespace aion;
    for(double rate:{8000.,44100.,48000.,96000.})for(unsigned block:{0u,1u,64u,128u,1024u})
    for(std::uint64_t base:{std::uint64_t{0},(std::uint64_t{1}<<63)+17})for(unsigned kind=0;kind<4;++kind){
        MeterGraphEvent e;e.kind=kind==0?MeterGraphEventKind::immediateStop:kind==1?MeterGraphEventKind::scheduledStop:kind==2?MeterGraphEventKind::stopAll:MeterGraphEventKind::queuedStop;
        e.serial=3;e.generation=7;e.track=19;e.ticket=kind==3?0:2;e.bufferId=(1ull<<61)+11;e.queueId=kind==3?(1ull<<60)+9:0;
        e.stopLeadSeconds=kind==1?.134567:kind==2?.015:0;e.frame=base+(block?block-1:0);
        e.callbackClock={9,base,base+block,0,rate,7};e.callbackClock.processorEntry={100.25,true};
        const double command=100.25+static_cast<double>(block?block-1:0)/rate,delay=kind==0||kind==3?.025:e.stopLeadSeconds+.05;
        for(double lag:{0.,.04,.25}){const double wall=100.25+static_cast<double>(block)/rate+lag;MeterStopCallbackTime out;
            watch=true;const bool ok=meterStopCallbackTime(e,73,{73,wall},7,out);watch=false;
            check(ok&&out.commandSeconds==command&&out.delaySeconds==delay&&out.deadlineSeconds==command+delay
                &&out.remainingSeconds==std::max(0.,command+delay-wall)&&out.eventSerial==3&&out.callbackSerial==9&&out.bufferId==e.bufferId
                &&out.queueId==e.queueId&&out.cleanupSerial==0,"Stop command deadline retains unrounded policy across receipt lag and full-width frames");}
        MeterStopCallbackTime held;check(meterStopCallbackTime(e,73,{73,101},7,held),"Seed stop clock rejection output");
        const auto reject=[&](MeterGraphEvent bad,MonotonicClockPoint now=MonotonicClockPoint{73,101}){auto out=held;check(!meterStopCallbackTime(bad,73,now,7,out)&&out==held,"Malformed stop clock rejects without output mutation");};
        auto bad=e;bad.kind=MeterGraphEventKind::queuedCancel;reject(bad);bad=e;bad.serial=0;reject(bad);bad=e;bad.bufferId=0;reject(bad);
        bad=e;bad.retiredTicket=1;reject(bad);bad=e;bad.generation=8;reject(bad);bad=e;bad.track=20;reject(bad);
        bad=e;bad.queueDelaySeconds=.1;reject(bad);bad=e;bad.queueId=kind==3?0:1;reject(bad);
        if(kind!=3){bad=e;bad.ticket=0;reject(bad);}bad=e;bad.stopLeadSeconds=kind==1?0:.1;reject(bad);
        bad=e;bad.callbackClock.processorEntry={};reject(bad);bad=e;bad.callbackClock.nonRealtime=true;reject(bad);
        bad=e;bad.callbackClock.generation=8;reject(bad);bad=e;bad.frame=base+block+1;reject(bad);
        bad=e;bad.callbackClock.sampleRate=0;reject(bad);bad=e;bad.callbackClock.processorEntry.seconds=1e308;reject(bad,{73,1e308});
        reject(e,{74,101});reject(e,{73,99});reject(e,{73,std::numeric_limits<double>::quiet_NaN()});
        bad=e;bad.callbackClock.hasHostTimeNs=true;bad.callbackClock.hostTimeNs=std::numeric_limits<std::uint64_t>::max();bad.callbackClock.hasOutputLatency=true;bad.callbackClock.outputLatencySamples=12345;
        MeterStopCallbackTime independent;check(meterStopCallbackTime(bad,73,{73,101},7,independent)&&independent==held,"Stop command counter excludes host time and output latency");
    }
    {MeterGraphEvent e;e.kind=MeterGraphEventKind::immediateStop;e.serial=1;e.generation=7;e.ticket=1;e.bufferId=1;e.callbackClock={1,0,0,0,48000,7};e.callbackClock.processorEntry={0,true};
        MeterStopCallbackTime t;check(meterStopCallbackTime(e,73,{73,0},7,t)&&t.commandSeconds==0&&t.deadlineSeconds==.025,"Known zero stop counter is valid");
        const auto held=t;for(double bad:{-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
            auto corrupt=e;corrupt.stopLeadSeconds=bad;check(!meterStopCallbackTime(corrupt,73,{73,1},7,t)&&t==held,"Nonfinite stop lead preserves output");}}
    std::cout<<"PASS: stop command counter policy, 25ms/unrounded lead+50ms/65ms, immutable late deadline, full-width frames, zero/variable blocks and invalid/offline/foreign-clock rejection\n";
    for(double rate:{8000.0,44100.0,48000.0,96000.0})for(unsigned block:{0u,1u,64u,128u,1024u})
    for(std::uint64_t base:{std::uint64_t{0},(std::uint64_t{1}<<63)+17})for(double delay:{0.,.0000001,.1345}){
        MeterGraphEvent event;event.kind=MeterGraphEventKind::queuedPlay;event.serial=3;event.generation=7;event.track=19;
        event.queueId=(std::uint64_t{1}<<60)+9;event.bufferId=(std::uint64_t{1}<<61)+11;event.queueDelaySeconds=delay;
        event.frame=base+(block?block-1:0);event.callbackClock={9,base,base+block,0,rate,7};event.callbackClock.processorEntry={100.25,true};
        const double command=100.25+static_cast<double>(block?block-1:0)/rate,deadline=command+delay;
        for(double lag:{0.,.04,.25}){
            const double wall=100.25+static_cast<double>(block)/rate+lag;QueuedMeterCallbackTime time;
            watch=true;const bool ok=queuedMeterCallbackTime(event,73,{73,wall},7,time);watch=false;
            check(ok&&time.generation==7&&time.track==19&&time.eventSerial==3&&time.callbackSerial==9&&time.clockOrigin==73
                &&time.queueId==event.queueId&&time.bufferId==event.bufferId&&time.commandSeconds==command&&time.deadlineSeconds==deadline
                &&time.remainingSeconds==std::max(0.,deadline-wall),"Queued callback maps exact local offset and immutable unrounded deadline across lag and full-width frames");
        }
    }
    MeterGraphEvent queued;queued.kind=MeterGraphEventKind::queuedPlay;queued.serial=1;queued.generation=7;queued.track=19;
    queued.queueId=8;queued.bufferId=9;queued.frame=105;queued.queueDelaySeconds=.1;queued.callbackClock={2,100,228,0,48000,7};queued.callbackClock.processorEntry={100,true};
    QueuedMeterCallbackTime held;check(queuedMeterCallbackTime(queued,73,{73,101},7,held),"Seed queued timing before invalid inputs");
    const auto reject=[&](MeterGraphEvent event,MonotonicClockPoint wall=MonotonicClockPoint{73,101},std::uint32_t generation=7,std::uint64_t origin=73){
        auto result=held;check(!queuedMeterCallbackTime(event,origin,wall,generation,result)&&result==held,"Invalid queued timing leaves caller output unchanged");};
    auto bad=queued;bad.kind=MeterGraphEventKind::started;reject(bad);bad=queued;bad.serial=0;reject(bad);
    bad=queued;bad.queueId=0;reject(bad);bad=queued;bad.bufferId=0;reject(bad);bad=queued;bad.track=20;reject(bad);
    bad=queued;bad.retiredTicket=1;reject(bad);bad=queued;bad.stopLeadSeconds=.015;reject(bad);
    bad=queued;bad.generation=8;reject(bad);reject(queued,{73,101},8);reject(queued,{0,101});reject(queued,{74,101});reject(queued,{73,101},7,0);
    bad=queued;bad.callbackClock.generation=8;reject(bad);bad=queued;bad.callbackClock.serial=0;reject(bad);
    bad=queued;bad.callbackClock.nonRealtime=true;reject(bad);bad=queued;bad.callbackClock.processorEntry={};reject(bad);
    bad=queued;bad.frame=99;reject(bad);bad=queued;bad.frame=228;reject(bad);
    bad=queued;bad.callbackClock.endFrame=99;reject(bad);bad=queued;bad.callbackClock.endFrame=100+static_cast<std::uint64_t>(std::numeric_limits<int>::max())+1;reject(bad);
    for(double value:{-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
        bad=queued;bad.queueDelaySeconds=value;reject(bad);bad=queued;bad.callbackClock.sampleRate=value;reject(bad);
        bad=queued;bad.callbackClock.processorEntry={value,true};reject(bad);reject(queued,{73,value});
    }
    reject(queued,{73,99});bad=queued;bad.callbackClock.sampleRate=0;reject(bad);
    bad=queued;bad.callbackClock.processorEntry.seconds=1e308;reject(bad,{73,1e308});
    bad=queued;bad.callbackClock.sampleRate=1e-308;reject(bad);
    bad=queued;bad.callbackClock.hasHostTimeNs=true;bad.callbackClock.hostTimeNs=std::numeric_limits<std::uint64_t>::max();
    bad.callbackClock.hasOutputLatency=true;bad.callbackClock.outputLatencySamples=std::numeric_limits<std::uint32_t>::max();
    QueuedMeterCallbackTime independent;check(queuedMeterCallbackTime(bad,73,{73,101},7,independent)&&independent==held,"Queued command time never uses host nanoseconds or audible output latency");
    auto startup=queued;startup.frame=0;startup.queueDelaySeconds=0;startup.callbackClock={1,0,0,0,48000,7};startup.callbackClock.processorEntry={0,true};
    check(queuedMeterCallbackTime(startup,73,{73,0},7,independent)&&independent.commandSeconds==0&&independent.deadlineSeconds==0
        &&independent.remainingSeconds==0,"Known-zero command counter and due time differ from missing correlation");
    Transport host;host.hostTimeNs=123;check(validatedTransport(host).hostTimeNs==0,"Missing host clears stale callback time");
    host.hostValid=true;check(validatedTransport(host).hostTimeNs==0,"Missing optional timestamp clears stale value");
    host.hasHostTimeNs=true;host.hostTimeNs=0;check(validatedTransport(host).hasHostTimeNs,"Engaged zero timestamp is distinct from missing timestamp");
    for(double rate:{8000.0,48000.0})for(unsigned block:{64u,128u}){
        auto a=std::make_unique<Engine>(),b=std::make_unique<Engine>();a->prepare(rate,8192);b->prepare(rate,8192);a->setTarget(1);b->setTarget(1);
        a->setMonitoring(true);b->setMonitoring(true);
        std::array<float,128> al{},ar{},bl{},br{};float* ac[]={al.data(),ar.data()};float* bc[]={bl.data(),br.data()};
        auto packet=std::make_unique<MeterWindowEvent>();EngineView view;
        watch=true;
        for(unsigned n=0;n<32;++n){
            al.fill(.2f);ar.fill(-.1f);bl=al;br=ar;const auto begin=a->clock();const unsigned frames=n%7?block:0;
            host={};host.hostValid=n%4!=3;host.callbackFrame=begin;host.hasHostTimeNs=n%4<2;host.hostTimeNs=n%4==1?0:(std::uint64_t{1}<<63)+n;
            host.hasSamples=true;host.samplePosition=n%2?-48000:999999;host.hasSeconds=true;host.seconds=n%2?-1:999;
            a->transport=validatedTransport(host);check(a->requestLoopMeterWindow(n%20),"Request paired meter window");
            const ProcessorClockObservation entry{n%4?static_cast<double>(n)*.125:std::numeric_limits<double>::quiet_NaN(),n%3!=0};
            a->process(ac,2,frames,{},n%5==0,entry);b->process(bc,2,frames,{},n%5==0);
            check(al==bl&&ar==br,"Raw host timestamp observation leaves actual monitored PCM bit-identical");
            check(a->popView(view)&&a->popLoopMeterWindow(*packet),"Coherent callback view and PCM packet published");
            const auto& clock=view.callbackClock;
            check(clock.processorEntry==validatedProcessorClock(entry)&&!b->inspectCallbackClockAudioThread().processorEntry.available,"Processor entry normalized and transferred without affecting default callers or PCM");
            check(validCallbackClock(clock)&&clock==a->inspectCallbackClockAudioThread()&&clock.serial==n+1&&clock.beginFrame==begin&&clock.endFrame==begin+frames&&clock.sampleRate==rate,"Local callback interval/rate/serial exact at 64/128 and zero frames");
            check(clock.hasHostTimeNs==(host.hostValid&&host.hasHostTimeNs)&&clock.hostTimeNs==(clock.hasHostTimeNs?host.hostTimeNs:0)&&clock.nonRealtime==(n%5==0),"Raw optional 64-bit timestamp and offline status remain exact");
            check(sameLoopMeterCallback(*packet,view)&&sameLoopMeterSource(*packet,view),"Transferred raw window and source can match exact callback snapshot");
            check(!a->popView(view)&&!a->popLoopMeterWindow(*packet),"No duplicate callback or PCM snapshot");
        }
        // Full queues preserve old tuples. Zero-frame callbacks all share one
        // frame/rate/generation/source: only serial prevents false pairing.
        for(unsigned n=0;n<32;++n){a->transport={};a->requestLoopMeterWindow(19);a->process(ac,2,0,{},false,{static_cast<double>(n),true});}
        MeterWindowEvent retained;EngineView retainedView;unsigned pairs=0;
        while(a->popView(view)){check(a->popLoopMeterWindow(*packet)&&sameLoopMeterCallback(*packet,view),"Saturated queues retain matching original timestamp/PCM tuples");retained=*packet;retainedView=view;++pairs;}
        check(pairs==7,"Seven bounded callback/PCM packets survive saturation");
        a->requestLoopMeterWindow(19);a->process(ac,2,0);check(a->popView(view)&&a->popLoopMeterWindow(*packet),"Fresh clock/window recover after saturation");
        check(view.frame==retainedView.frame&&packet->sourceRevision==retained.sourceRevision&&sameLoopMeterCallback(*packet,view)&&!sameLoopMeterCallback(retained,view),"Serial rejects old same-frame same-source zero-callback PCM");
        auto bad=view;bad.callbackClock.serial=0;check(!sameLoopMeterCallback(*packet,bad),"Zero serial is not a callback observation");
        bad=view;bad.callbackClock.beginFrame=bad.frame+1;check(!sameLoopMeterCallback(*packet,bad),"Reversed callback range rejected");
        bad=view;bad.sampleRate=rate+1;check(!sameLoopMeterCallback(*packet,bad),"Rate-incoherent clock/PCM pair rejected");
        bad=view;bad.callbackClock.hostTimeNs=1;check(!sameLoopMeterCallback(*packet,bad),"Unavailable timestamp cannot carry stale nanoseconds");
        bad=view;bad.callbackClock.processorEntry={1,false};check(!sameLoopMeterCallback(*packet,bad),"Unavailable processor entry cannot carry stale seconds");
        bad=view;bad.callbackClock.processorEntry={1,true};check(!sameLoopMeterCallback(*packet,bad),"Different processor entry rejects exact callback pairing");
        for(double invalid:{-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
            a->process(ac,2,0,{},false,{invalid,true});check(a->inspectCallbackClockAudioThread().processorEntry==ProcessorClockObservation{},"Invalid processor entry clears stale availability and seconds");
            check(a->popView(view)&&view.callbackClock.processorEntry==ProcessorClockObservation{},"Consumer drains each invalid-entry view before reset test");
        }
        const auto generation=a->generation();watch=false;
        a->prepare(rate,8192);check(a->generation()!=generation&&a->inspectCallbackClockAudioThread().serial==0,"Prepare clears prior raw callback observation");
        watch=true;a->requestLoopMeterWindow(19);a->process(ac,2,0);check(a->popView(view)&&a->popLoopMeterWindow(*packet)&&sameLoopMeterCallback(*packet,view)&&!sameLoopMeterCallback(retained,view),"Reset clock reuse cannot match prior generation");
        // Audio-owner input stamped for a different callback must not be paired.
        a->transport.hostValid=a->transport.hasHostTimeNs=true;a->transport.hostTimeNs=std::numeric_limits<std::uint64_t>::max();a->transport.callbackFrame=a->clock()+1;
        a->process(ac,2,block);check(!a->inspectCallbackClockAudioThread().hasHostTimeNs,"Mismatched callback-start host observation suppressed");
        watch=false;
    }
    check(allocations.load()==0,"Raw callback timestamp acquisition/transfer/validation has zero tracked new/delete");
    std::cout<<"PASS: queued command counter correlation, immutable deadline and late-receipt remainder, zero/variable blocks, 8/44.1/48/96k rates, full-width frames, malformed/offline/foreign-clock rejection and host/latency independence\n";
    std::cout<<"PASS: raw optional callback clock, exact uint64/ns and 64/128/zero-frame ranges, bounded transfer, same-frame serial guard, reset and bit-identical PCM; zero tracked new/delete\n";
}
