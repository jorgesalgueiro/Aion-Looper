#include "core/Engine.h"
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
extern thread_local bool watch;
extern std::atomic<int> allocations;
namespace {
void check(bool ok,const char* what){if(!ok){std::cerr<<what<<'\n';std::exit(1);}}
void fill(unsigned id,aion::MeterWindowEvent& event) noexcept {
    event.clearEpoch=id+100;event.waveformId=event.endFrame+1000;event.sourceRevision=event.endFrame+id;
    const bool host=event.endFrame%128==0;
    event.callbackClock={event.endFrame+1,event.endFrame,event.endFrame,host?event.endFrame+123:0,event.sampleRate,event.generation,host,id%2!=0,id%3?0:id+32,id%3==0};
    event.callbackClock.processorEntry={id%2?static_cast<double>(event.endFrame)*.001:0,id%2!=0};
    if(id%4==1&&event.endFrame>=64){event.callbackClock.beginFrame-=64;event.callbackClock.deviceEntry={static_cast<double>(event.endFrame)*.001,event.endFrame+1,64,true};}
    for(unsigned i=0;i<event.samples.size();++i)
        event.samples[i]=static_cast<float>((event.endFrame%10000)*2048+id*32+i);
}
void coherent(const aion::MeterWindowEvent& event){
    check(event.track<20&&event.clearEpoch==event.track+100&&event.waveformId==event.endFrame+1000&&event.sourceRevision==event.endFrame+event.track,"Meter packet identity coherent");
    const bool host=event.endFrame%128==0;
    aion::CallbackClockStamp expected{event.endFrame+1,event.endFrame,event.endFrame,host?event.endFrame+123:0,event.sampleRate,event.generation,host,event.track%2!=0,event.track%3?0:event.track+32,event.track%3==0,{event.track%2?static_cast<double>(event.endFrame)*.001:0,event.track%2!=0}};
    if(event.track%4==1&&event.endFrame>=64){expected.beginFrame-=64;expected.deviceEntry={static_cast<double>(event.endFrame)*.001,event.endFrame+1,64,true};}
    check(aion::validCallbackClock(event.callbackClock)&&event.callbackClock==expected,"Concurrent every-field callback clock tuple coherent, including optional/offline flags");
    for(unsigned i=0;i<event.samples.size();++i)
        check(event.samples[i]==static_cast<float>((event.endFrame%10000)*2048+event.track*32+i),"Meter packet PCM not torn/reused");
}
}
void meterTransferTests(){
    using namespace aion;
    auto transfer=std::make_unique<MeterTransfer>();
    auto event=std::make_unique<MeterWindowEvent>();
    watch=true;
    check(!transfer->request(20)&&!transfer->request(0xffffffffu),"Reject invalid meter request before bit shift");
    check(!transfer->pop(*event,0),"No unprepared meter observation");
    for(unsigned i=0;i<1000;++i)check(transfer->request(19),"Coalesced request accepted");
    unsigned fills=0;
    transfer->publish(1,128,48000,[&](unsigned id,MeterWindowEvent& e){++fills;fill(id,e);});
    check(fills==1&&transfer->pop(*event,1)&&event->track==19&&event->endFrame==128,"Repeated requests coalesce to one current window");
    coherent(*event);check(!transfer->pop(*event,1),"No unsolicited or duplicate window");

    // Full queue preserves its seven immutable packets and retains all pending
    // tracks, without forcing audio to wait or overwrite the consumer's slot.
    for(unsigned id=0;id<20;++id)transfer->request(id);
    for(unsigned block=0;block<500;++block){
        fills=0;transfer->publish(1,256+block*64,48000,[&](unsigned id,MeterWindowEvent& e){++fills;fill(id,e);});
        check(fills<=2,"At most two PCM copies on each saturated callback");
    }
    unsigned count=0,seen=0;
    while(transfer->pop(*event,1)){coherent(*event);seen|=1u<<event->track;++count;check(event->endFrame<=448,"Queue-full must preserve old queued PCM");}
    check(count==7,"Meter queue has exactly seven usable packets");
    for(unsigned block=0;block<20;++block){
        transfer->publish(1,40000+block*128,48000,fill);
        while(transfer->pop(*event,1)){coherent(*event);seen|=1u<<event->track;++count;check(event->endFrame>=40000,"Recovery recaptures current window, not failed scratch");}
    }
    check(count==20&&seen==0xfffffu,"All twenty requested tracks recover fairly after saturation");

    // Producer reset leaves queue ownership alone. The consumer drains only a
    // fixed maximum and cannot return stale PCM after sample-clock reuse.
    for(unsigned id=0;id<20;++id)transfer->request(id);
    for(unsigned i=0;i<4;++i)transfer->publish(1,50000+i*128,48000,fill);
    transfer->reset();event->endFrame=123;event->samples.fill(0.125f);
    check(!transfer->pop(*event,2)&&event->endFrame==123&&event->samples[0]==0.125f,"Stale generation drain leaves output unchanged");
    transfer->publish(2,0,8000,fill);check(!transfer->pop(*event,2),"Prepare cancels old pending requests");
    transfer->request(0);transfer->publish(2,0,8000,fill);
    check(transfer->pop(*event,2)&&event->generation==2&&event->sampleRate==8000&&event->endFrame==0,"Fresh request uses reset clock/rate/epoch");
    coherent(*event);
    watch=false;

    // Two real owners exchange complete PCM packets. Requests deliberately cross
    // publish boundaries; the handshake guarantees all tracks, no timing sleeps.
    auto threaded=std::make_unique<MeterTransfer>();
    std::atomic<bool> done{false};
    std::thread consumer([&]{
        auto received=std::make_unique<MeterWindowEvent>();watch=true;
        for(unsigned n=0;n<4000;++n){
            const auto id=n%20;threaded->request(id);
            while(!threaded->pop(*received,7))std::this_thread::yield();
            check(received->track==id&&received->sampleRate==96000,"Concurrent meter request owner/rate");coherent(*received);
        }
        watch=false;done.store(true,std::memory_order_release);
    });
    watch=true;
    for(std::uint64_t frame=0;!done.load(std::memory_order_acquire);frame+=64){threaded->publish(7,frame,96000,fill);std::this_thread::yield();}
    watch=false;consumer.join();

    // Actual Engine windows are identical to audio-owned taps, metadata and
    // local sample stamps. Opt-in transfer never changes the rendered audio.
    for(double rate:{8000.0,48000.0})for(unsigned block:{64u,128u}){
        auto a=std::make_unique<Engine>(),b=std::make_unique<Engine>();
        auto observation=std::make_unique<MeterWindowEvent>();
        std::array<float,2048> raw{};
        std::array<float,128> al{},ar{},bl{},br{};
        for(auto* engine:{a.get(),b.get()}){
            engine->prepare(rate,81920);engine->setMonitoring(false);engine->setTarget(1);
            engine->configureSync({SyncMode::off,120,1,4,4,false});
            check(engine->enqueueLoop({LoopAction::record,19,1,engine->generation(),0,0}),"Meter transfer real record");
            check(engine->enqueueLoop({LoopAction::finish,19,2,engine->generation(),4096,0}),"Meter transfer real finish");
        }
        bool nonzero=false;
        for(unsigned offset=0;offset<8192;offset+=block){
            for(unsigned i=0;i<block;++i){al[i]=bl[i]=0.35f*std::sin((offset+i)*0.031f);ar[i]=br[i]=0.2f*std::cos((offset+i)*0.053f);}
            float* ac[]={al.data(),ar.data()},*bc[]={bl.data(),br.data()};
            // Finish deterministically on the worker, then verify the nonzero
            // playback/transfer path with ordinary nonblocking live callbacks.
            const bool offline=offset<=4096;
            watch=true;a->requestLoopMeterWindow(19);a->process(ac,2,static_cast<int>(block),{},offline);b->process(bc,2,static_cast<int>(block),{},offline);
            a->copyLoopMeterWindow(19,raw);
            check(a->popLoopMeterWindow(*observation),"Engine requested packet delivered");
            const auto view=a->inspectAudioThread(19);
            check(observation->samples==raw&&observation->endFrame==offset+block&&observation->endFrame==view.frame&&observation->sampleRate==rate&&observation->generation==a->generation()&&observation->track==19&&observation->clearEpoch==view.track.clearEpoch&&observation->waveformId==view.track.waveformId&&observation->sourceRevision==view.track.sourceRevision,"Real PCM/clip identity/frame stamp is one coherent callback observation");
            check(al==bl&&ar==br,"Request/pop meter transfer is passive for exact stereo PCM");watch=false;
            nonzero|=std::any_of(raw.begin(),raw.end(),[](float v){return std::abs(v)>0.01f;});
        }
        check(nonzero&&observation->waveformId!=0,"Actual committed nonzero loop transferred");
        const auto oldClear=observation->clearEpoch,oldWave=observation->waveformId;
        a->requestLoopMeterWindow(19);watch=true;a->process(nullptr,0,0);watch=false;
        check(a->enqueueLoop({LoopAction::clear,19,3,a->generation(),a->clock(),0}),"Clear for stale clip identity");
        float* ac[]={al.data(),ar.data()};a->process(ac,2,static_cast<int>(block),{},true);
        const auto cleared=a->inspectAudioThread(19);
        check(a->popLoopMeterWindow(*observation)&&observation->clearEpoch==oldClear&&observation->waveformId==oldWave&&observation->endFrame==8192&&observation->clearEpoch!=cleared.track.clearEpoch,"Delayed same-generation packet exposes stale clear/clip identity; zero callback does not advance time");
        a->requestLoopMeterWindow(19);watch=true;a->process(nullptr,0,0);check(a->popLoopMeterWindow(*observation),"Clear gets fresh snapshot on request");watch=false;
        a->copyLoopMeterWindow(19,raw);
        check(observation->waveformId==0&&observation->clearEpoch==cleared.track.clearEpoch&&observation->samples==raw,"Clear packet has fresh identity and exact current PCM including existing fade tail");
        // The tap observes real output, so it must also retain the tail until the
        // rolling 2048-frame window has moved past it, then report exact silence.
        for(unsigned n=0;n<4096/block;++n)a->process(nullptr,0,static_cast<int>(block),{},true);
        a->requestLoopMeterWindow(19);watch=true;a->process(nullptr,0,0);check(a->popLoopMeterWindow(*observation),"Post-tail clear request delivered");watch=false;
        check(std::all_of(observation->samples.begin(),observation->samples.end(),[](float v){return v==0;}),"Clear tail eventually leaves raw window exactly silent");
        a->requestLoopMeterWindow(19);a->process(nullptr,0,0);a->prepare(rate==8000?48000:8000,81920);
        observation->endFrame=777;
        watch=true;check(!a->popLoopMeterWindow(*observation)&&observation->endFrame==777,"Actual Engine discards prepared-generation stale packets");
        a->requestLoopMeterWindow(0);a->process(nullptr,0,0);check(a->popLoopMeterWindow(*observation)&&observation->track==0&&observation->endFrame==0&&observation->generation==a->generation(),"Engine fresh empty/reset request recovers without audio frames");watch=false;
    }
    check(allocations.load()==0,"Meter transfer/publish/pop has zero tracked new/delete on both owners");
    std::cout<<"PASS: bounded on-request meter transfer, 20-track fairness, overflow/recovery, stale generation/clip metadata, 4000 concurrent PCM packets and exact passive Engine stereo at 8/48kHz 64/128; no tracked new/delete\n";
}
