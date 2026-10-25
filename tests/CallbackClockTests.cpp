#include "core/Engine.h"
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
    Transport host;host.hostTimeNs=123;check(validatedTransport(host).hostTimeNs==0,"Missing host clears stale callback time");
    host.hostValid=true;check(validatedTransport(host).hostTimeNs==0,"Missing optional timestamp clears stale value");
    host.hasHostTimeNs=true;host.hostTimeNs=0;check(validatedTransport(host).hasHostTimeNs,"Engaged zero timestamp is distinct from missing timestamp");
    for(double rate:{8000.0,48000.0})for(unsigned block:{64u,128u}){
        auto a=std::make_unique<Engine>(),b=std::make_unique<Engine>();a->prepare(rate,8192);b->prepare(rate,8192);a->setTarget(1);b->setTarget(1);
        std::array<float,128> al{},ar{},bl{},br{};float* ac[]={al.data(),ar.data()};float* bc[]={bl.data(),br.data()};
        auto packet=std::make_unique<MeterWindowEvent>();EngineView view;
        watch=true;
        for(unsigned n=0;n<32;++n){
            al.fill(.2f);ar.fill(-.1f);bl=al;br=ar;const auto begin=a->clock();const unsigned frames=n%7?block:0;
            host={};host.hostValid=n%4!=3;host.callbackFrame=begin;host.hasHostTimeNs=n%4<2;host.hostTimeNs=n%4==1?0:(std::uint64_t{1}<<63)+n;
            host.hasSamples=true;host.samplePosition=n%2?-48000:999999;host.hasSeconds=true;host.seconds=n%2?-1:999;
            a->transport=validatedTransport(host);check(a->requestLoopMeterWindow(n%20),"Request paired meter window");
            a->process(ac,2,frames,{},n%5==0);b->process(bc,2,frames,{},n%5==0);
            check(al==bl&&ar==br,"Raw host timestamp observation leaves actual monitored PCM bit-identical");
            check(a->popView(view)&&a->popLoopMeterWindow(*packet),"Coherent callback view and PCM packet published");
            const auto& clock=view.callbackClock;
            check(validCallbackClock(clock)&&clock==a->inspectCallbackClockAudioThread()&&clock.serial==n+1&&clock.beginFrame==begin&&clock.endFrame==begin+frames&&clock.sampleRate==rate,"Local callback interval/rate/serial exact at 64/128 and zero frames");
            check(clock.hasHostTimeNs==(host.hostValid&&host.hasHostTimeNs)&&clock.hostTimeNs==(clock.hasHostTimeNs?host.hostTimeNs:0)&&clock.nonRealtime==(n%5==0),"Raw optional 64-bit timestamp and offline status remain exact");
            check(sameLoopMeterCallback(*packet,view)&&sameLoopMeterSource(*packet,view),"Transferred raw window and source can match exact callback snapshot");
            check(!a->popView(view)&&!a->popLoopMeterWindow(*packet),"No duplicate callback or PCM snapshot");
        }
        // Full queues preserve old tuples. Zero-frame callbacks all share one
        // frame/rate/generation/source: only serial prevents false pairing.
        for(unsigned n=0;n<32;++n){a->transport={};a->requestLoopMeterWindow(19);a->process(ac,2,0);}
        MeterWindowEvent retained;EngineView retainedView;unsigned pairs=0;
        while(a->popView(view)){check(a->popLoopMeterWindow(*packet)&&sameLoopMeterCallback(*packet,view),"Saturated queues retain matching original timestamp/PCM tuples");retained=*packet;retainedView=view;++pairs;}
        check(pairs==7,"Seven bounded callback/PCM packets survive saturation");
        a->requestLoopMeterWindow(19);a->process(ac,2,0);check(a->popView(view)&&a->popLoopMeterWindow(*packet),"Fresh clock/window recover after saturation");
        check(view.frame==retainedView.frame&&packet->sourceRevision==retained.sourceRevision&&sameLoopMeterCallback(*packet,view)&&!sameLoopMeterCallback(retained,view),"Serial rejects old same-frame same-source zero-callback PCM");
        auto bad=view;bad.callbackClock.serial=0;check(!sameLoopMeterCallback(*packet,bad),"Zero serial is not a callback observation");
        bad=view;bad.callbackClock.beginFrame=bad.frame+1;check(!sameLoopMeterCallback(*packet,bad),"Reversed callback range rejected");
        bad=view;bad.sampleRate=rate+1;check(!sameLoopMeterCallback(*packet,bad),"Rate-incoherent clock/PCM pair rejected");
        bad=view;bad.callbackClock.hostTimeNs=1;check(!sameLoopMeterCallback(*packet,bad),"Unavailable timestamp cannot carry stale nanoseconds");
        const auto generation=a->generation();watch=false;
        a->prepare(rate,8192);check(a->generation()!=generation&&a->inspectCallbackClockAudioThread().serial==0,"Prepare clears prior raw callback observation");
        watch=true;a->requestLoopMeterWindow(19);a->process(ac,2,0);check(a->popView(view)&&a->popLoopMeterWindow(*packet)&&sameLoopMeterCallback(*packet,view)&&!sameLoopMeterCallback(retained,view),"Reset clock reuse cannot match prior generation");
        // Audio-owner input stamped for a different callback must not be paired.
        a->transport.hostValid=a->transport.hasHostTimeNs=true;a->transport.hostTimeNs=std::numeric_limits<std::uint64_t>::max();a->transport.callbackFrame=a->clock()+1;
        a->process(ac,2,block);check(!a->inspectCallbackClockAudioThread().hasHostTimeNs,"Mismatched callback-start host observation suppressed");
        watch=false;
    }
    check(allocations.load()==0,"Raw callback timestamp acquisition/transfer/validation has zero tracked new/delete");
    std::cout<<"PASS: raw optional callback clock, exact uint64/ns and 64/128/zero-frame ranges, bounded transfer, same-frame serial guard, reset and bit-identical PCM; zero tracked new/delete\n";
}
