#include "plugin/LoopMeterCleanupTimer.h"
#include "plugin/LoopMeterClearDispatcher.h"
#include "plugin/Processor.h"
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<aion::LoopMeterCleanupTimer>);

namespace {
void check(bool ok,const char* label){if(!ok){std::cerr<<label<<std::endl;std::exit(1);}}
double wall(){return juce::Time::getMillisecondCounterHiRes()*.001;}
auto reading(const aion::MeterReading& r){return std::array<double,7>{r.rms,r.peak,r.linearPeak,r.peakDb,r.rmsPercent,r.peakPercent,r.clipped?1.0:0.0};}
struct Fixture {
    std::unique_ptr<aion::Engine> engine=std::make_unique<aion::Engine>(),control=std::make_unique<aion::Engine>();
    std::unique_ptr<aion::LoopMeterLifecycle> owner;
    std::unique_ptr<aion::LoopMeterCleanupTimer> timer;
    std::unique_ptr<aion::EngineView> view=std::make_unique<aion::EngineView>();
    std::unique_ptr<aion::MeterWindowEvent> packet=std::make_unique<aion::MeterWindowEvent>();
    unsigned block;bool& watch;
    Fixture(unsigned size,bool& audioWatch,aion::MeterEventScope scope=aion::MeterEventScope::rawWindows):engine(std::make_unique<aion::Engine>(scope)),block(size),watch(audioWatch){prepare();owner=std::make_unique<aion::LoopMeterLifecycle>(*engine,19);owner->setBuffer(true);timer=std::make_unique<aion::LoopMeterCleanupTimer>(*owner);}
    void cycle(bool offline=false,aion::ProcessorClockObservation entry={}){
        std::array<float,128> l{},r{},cl{},cr{};
        for(unsigned i=0;i<block;++i)l[i]=r[i]=1.8f*static_cast<float>(std::sin((engine->clock()+i)*.037));
        cl=l;cr=r;float* channels[]={l.data(),r.data()},*cc[]={cl.data(),cr.data()};
        watch=true;engine->process(channels,2,static_cast<int>(block),{},offline,entry);control->process(cc,2,static_cast<int>(block),{},offline,entry);watch=false;
        check(l==cl&&r==cr,"Native cleanup timer preserves bit-identical stereo Engine PCM");
        while(engine->popView(*view)){}
    }
    void prepare(){
        for(auto* e:{engine.get(),control.get()}){
            e->prepare(48000,81920,{true,48000,0});e->configureSync({aion::SyncMode::off,120,1,4,4,false});e->setMonitoring(false);e->setTarget(1);
        }
        record();
    }
    void record(unsigned track=19){
        const auto start=engine->clock();
        for(auto* e:{engine.get(),control.get()})check(e->enqueueLoop({aion::LoopAction::record,track,1,e->generation(),start,0})
            &&e->enqueueLoop({aion::LoopAction::finish,track,2,e->generation(),start+4096,0}),"Prepare actual recorded timer PCM");
        for(unsigned n=0;n<8192/block;++n)cycle(true);
    }
    void clearFromAudio(){
        const auto receipt=owner->cleanupRequest();
        for(auto* e:{engine.get(),control.get()})check(e->enqueueLoop({aion::LoopAction::clear,19,3,e->generation(),0,0}),"Queue DEL while native timer is pending");cycle();
        aion::MeterGraphEvent event;unsigned accepted=0;
        while(engine->popMeterGraphEvent(event))if(timer->consumeClear(event))++accepted;
        check(accepted==1&&!timer->pending()&&!owner->inspect().buffer&&!owner->inspect().data
            &&!owner->deliverCleanup(receipt),"Actual DEL receipt cancels JUCE timer and clears attached history");
    }
    void play(){
        for(auto* e:{engine.get(),control.get()}){
            check(e->enqueueLoop({aion::LoopAction::stopImmediate,19,0,e->generation(),0,0})&&e->enqueueLoop({aion::LoopAction::play,19,0,e->generation(),0,0}),"Explicit actual playback fixture");
        }
        check(owner->start(engine->clock()),"Explicit owner start before native timer");
        for(unsigned n=0;n<2048/block;++n){check(owner->request(),"Request actual timer PCM");cycle();check(engine->popLoopMeterWindow(*packet),"Receive actual timer PCM");
            const double t=static_cast<double>(view->frame)/48000;check(owner->observe(*packet,*view,view->callbackClock,{t,t}),"Seed timer history from actual Engine PCM");}
        check(owner->value().peak>0,"Timer history contains nonzero actual PCM");
    }
};
void adapterReadinessTests(bool& audioWatch){
    using namespace aion;
    for(unsigned block:{64u,128u}){
        std::atomic<std::uint32_t> prepared{0}; // Outlives every bound adapter.
        Fixture f(block,audioWatch,MeterEventScope::committedBuffers);
        f.timer=std::make_unique<LoopMeterCleanupTimer>(*f.owner,&prepared);
        MeterGraphEvent installed;check(f.engine->popMeterGraphEvent(installed)&&installed.kind==MeterGraphEventKind::bufferInstalled,
            "Readiness fixture uses actual accepted capture buffer receipt");
        check(!f.timer->consumeBuffer(installed)&&!f.owner->committedBuffer(),"Released adapter retains unapplied buffer receipt");
        prepared.store(f.engine->generation()+1,std::memory_order_release);
        check(!f.timer->consumeBuffer(installed)&&!f.owner->committedBuffer(),"Mismatched prepared generation cannot install buffer metadata");
        prepared.store(f.engine->generation(),std::memory_order_release);
        check(f.timer->consumeBuffer(installed)&&f.owner->committedBuffer()==installed.bufferId,"Matching prepared generation accepts the retained buffer receipt");
        std::array<std::unique_ptr<LoopMeterLifecycle>,20> owners;
        std::array<std::unique_ptr<LoopMeterCleanupTimer>,20> timers;
        LoopMeterClearDispatcher::Routes routes{};
        for(unsigned id=0;id<20;++id){
            if(id==19)routes[id]=f.timer.get();
            else {owners[id]=std::make_unique<LoopMeterLifecycle>(*f.engine,id);
                timers[id]=std::make_unique<LoopMeterCleanupTimer>(*owners[id],&prepared);routes[id]=timers[id].get();}
        }
        LoopMeterClearDispatcher dispatcher(*f.engine,routes);
        f.play();check(dispatcher.drain().started==1,"Readiness fixture retains actual accepted raw window fact");
        check(f.timer->stop(1,3601),"Arm distant deadline before readiness rejection");
        const auto cleanup=f.owner->cleanupRequest();const auto identity=f.owner->inspect();const auto history=reading(f.owner->value());
        for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::clear,19,0,e->generation(),0,0}),"Queue actual clear before guarded delivery");f.cycle();
        const auto frame=f.engine->clock();
        for(auto invalid:{0u,f.engine->generation()+1}){
            prepared.store(invalid,std::memory_order_release);
            const auto result=dispatcher.drain();
            check(!result.current&&!result.examined&&!result.cleared&&!result.applied,"Unready adapter routes prevent the dispatcher from dequeuing actual clear facts");
            check(!f.timer->stop(2)&&f.owner->cleanupRequest()==cleanup&&f.timer->pending(),"Unready stop preserves an already armed deadline");
        }
        MeterGraphEvent cleared;check(f.engine->popMeterGraphEvent(cleared)&&cleared.kind==MeterGraphEventKind::cleared,
            "Guarded dispatcher leaves the original clear receipt queued");
        for(auto invalid:{0u,f.engine->generation()+1}){
            prepared.store(invalid,std::memory_order_release);
            check(!f.timer->consumeClear(cleared)&&f.owner->committedBuffer()==installed.bufferId
                &&f.owner->inspect().analyser==identity.analyser&&f.owner->inspect().data==identity.data
                &&f.owner->cleanupRequest()==cleanup&&f.timer->pending()&&reading(f.owner->value())==history&&f.engine->clock()==frame,
                "Released or mismatched clear delivery leaves graph, buffer, history and pending timer unchanged");
        }
        prepared.store(f.engine->generation(),std::memory_order_release);
        check(f.timer->consumeClear(cleared)&&!f.timer->pending()&&!f.owner->committedBuffer()&&!f.owner->inspect().data,
            "Matching readiness applies the retained actual clear and cancels cleanup");
    }
}
void immediatePlayAdapterTests(bool& audioWatch){
    using namespace aion;
    for(unsigned block:{64u,128u}){
        std::atomic<std::uint32_t> prepared{0};
        Fixture f(block,audioWatch,MeterEventScope::acceptedPlayback);
        f.timer=std::make_unique<LoopMeterCleanupTimer>(*f.owner,&prepared);
        MeterGraphEvent installed;check(f.engine->popMeterGraphEvent(installed)&&installed.kind==MeterGraphEventKind::bufferInstalled,"Actual buffer before native accepted play");
        prepared.store(f.engine->generation(),std::memory_order_release);check(f.timer->consumeBuffer(installed),"Bind actual immediate-play fixture clip");
        const auto play=[&]{for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::stopImmediate,19,0,e->generation(),0,0})
            &&e->enqueueLoop({LoopAction::play,19,0,e->generation(),0,0}),"Queue actual immediate play pair");f.cycle();
            MeterGraphEvent event;check(f.engine->popMeterGraphEvent(event),"Actual accepted playback receipt");
            if(event.kind==MeterGraphEventKind::immediateStop){check(f.timer->consumeImmediateStop(event),"Apply actual stop before native replacement");
                check(f.engine->popMeterGraphEvent(event),"Actual replacement after native stop");}
            check(event.kind==MeterGraphEventKind::immediatePlay,"Actual accepted immediate play receipt");return event;};
        auto played=play();
        for(auto invalid:{0u,f.engine->generation()+1}){
            prepared.store(invalid,std::memory_order_release);
            check(!f.timer->consumeImmediatePlay(played)&&!f.owner->inspect().present&&!f.owner->binding().ticket,
                "Released/mismatched adapter cannot attach an accepted immediate graph");
        }
        prepared.store(f.engine->generation(),std::memory_order_release);
        check(f.timer->consumeImmediatePlay(played)&&f.owner->inspect().present,"Prepared adapter attaches the retained accepted graph receipt");
        for(unsigned n=0;n<2048/block;++n){check(f.owner->request(),"Request native accepted graph PCM");f.cycle();
            check(f.engine->popLoopMeterWindow(*f.packet),"Receive native accepted graph PCM");const double t=static_cast<double>(f.view->frame)/48000;
            check(f.owner->observe(*f.packet,*f.view,f.view->callbackClock,{t,t}),"Observe actual accepted native graph PCM");}
        const auto data=f.owner->inspect().data;const auto history=reading(f.owner->value());check(f.owner->value().peak>0,"Native accepted graph has nonzero history");
        check(f.timer->stop(1,3601)&&f.timer->pending(),"Arm cleanup before accepted native replacement");const auto old=f.owner->cleanupRequest();played=play();
        check(f.timer->consumeImmediatePlay(played)&&!f.timer->pending()&&!f.owner->deliverCleanup(old)&&f.owner->inspect().data!=data
            &&reading(f.owner->value())==history,"Accepted immediate replacement cancels the real JUCE deadline and preserves meter history");
        f.clearFromAudio();
    }
    std::cout<<"PASS: actual immediate play receipts route through prepared JUCE adapters, reject released/mismatched delivery, bind nonzero PCM and cancel obsolete cleanup on replacement at 64/128\n";
}
void clearDispatcherTests(bool& audioWatch){
    using namespace aion;
    for(unsigned block:{64u,128u}){
        Fixture f(block,audioWatch);
        std::array<std::unique_ptr<LoopMeterLifecycle>,20> owners;
        std::array<std::unique_ptr<LoopMeterCleanupTimer>,20> timers;
        std::array<LoopMeterLifecycle*,20> ownerRoutes{};LoopMeterClearDispatcher::Routes routes{};
        for(unsigned id=0;id<20;++id){
            if(id==19){ownerRoutes[id]=f.owner.get();routes[id]=f.timer.get();}
            else {f.record(id);owners[id]=std::make_unique<LoopMeterLifecycle>(*f.engine,id);ownerRoutes[id]=owners[id].get();
                timers[id]=std::make_unique<LoopMeterCleanupTimer>(*owners[id]);routes[id]=timers[id].get();}
            check(ownerRoutes[id]->setBuffer(true)&&ownerRoutes[id]->start(f.engine->clock()),"Start all twenty explicit dispatcher graphs");
        }
        for(unsigned n=0;n<40;++n){
            for(auto* owner:ownerRoutes)check(owner->request(),"Request all dispatcher windows");f.cycle();
            while(f.engine->popLoopMeterWindow(*f.packet)){
                const double t=static_cast<double>(f.view->frame)/48000;
                check(ownerRoutes[f.packet->track]->observe(*f.packet,*f.view,f.view->callbackClock,{t,t}),"Observe actual all-track dispatcher PCM");
            }
        }
        for(auto* owner:ownerRoutes)check(owner->value().peak>0,"Every dispatch route has actual nonzero PCM history");
        auto missing=routes;missing[4]=nullptr;LoopMeterClearDispatcher absent(*f.engine,missing);
        check(!absent.drain().examined,"Incomplete routes cannot consume the shared stream");
        auto duplicate=routes;duplicate[4]=routes[3];LoopMeterClearDispatcher repeated(*f.engine,duplicate);
        check(!repeated.drain().examined,"Duplicate/wrong-track route cannot consume the shared stream");
        LoopMeterLifecycle foreign(*f.control,0);LoopMeterCleanupTimer foreignTimer(foreign);
        auto wrongEngine=routes;wrongEngine[0]=&foreignTimer;LoopMeterClearDispatcher unrelated(*f.engine,wrongEngine);
        check(!unrelated.drain().examined,"Foreign Engine route cannot consume another Engine's stream");
        LoopMeterClearDispatcher dispatcher(*f.engine,routes);
        check(dispatcher.drain(0).current&&!dispatcher.reset(),"Zero budget and same-generation reset cannot consume/repair work");
        auto result=dispatcher.drain(7);check(result.current&&result.examined==7&&result.started==7&&!result.cleared,"Initial starts consume only the requested raw budget");
        result=dispatcher.drain(255);check(result.current&&result.examined==13&&result.started==13,"Invalid route tables left all twenty start facts unread");
        for(auto* timer:routes)check(timer->stop(1),"Arm all twenty native cleanup timers");
        const auto clearAll=[&]{for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::clearAll,0,3,e->generation(),0,0}),"Actual dispatcher Clear All");f.cycle();};
        clearAll();result=dispatcher.drain(7);
        check(result.current&&result.examined==7&&result.cleared==7&&result.applied==7&&!result.rejected,"Slow consumer clears exactly seven matching owners/timers");
        for(unsigned id=0;id<20;++id)check(routes[id]->pending()==(id>=7)&&ownerRoutes[id]->inspect().buffer==(id>=7),"Partial dispatch preserves all undelivered track ownership");
        result=dispatcher.drain(std::numeric_limits<unsigned>::max());
        check(result.current&&result.examined==13&&result.applied==13,"Large drain requests clamp to fixed queue capacity");
        for(unsigned id=0;id<20;++id)check(!routes[id]->pending()&&!ownerRoutes[id]->inspect().data&&ownerRoutes[id]->value().peak==0,"Shared dispatch cancels all timers and attached history exactly once");
        clearAll();f.prepare();
        check(!dispatcher.drain().examined&&!dispatcher.reset(),"Prepare rejects stale routes without consuming new receipts");
        for(auto* owner:ownerRoutes)owner->reset();
        check(!dispatcher.drain().examined&&dispatcher.reset(),"All owners must reset before explicit dispatcher generation reset");
        clearAll();result=dispatcher.drain(5);
        check(result.current&&result.examined==5&&!result.cleared,"Old-generation discard counts against the shared raw budget");
        result=dispatcher.drain(255);
        check(result.current&&result.examined==35&&result.cleared==20&&result.rejected==20&&!result.applied,"Correct generation retains new unbound clears after stale consumer rejection");
        for(unsigned n=0;n<256;++n){
            for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::clear,0,10+n,e->generation(),0,0}),"Fill dispatcher event stream");
            if(n%100==99)f.cycle();
        }
        f.cycle();result=dispatcher.drain(5);
        check(!result.current&&result.examined==5&&!result.applied&&!dispatcher.reset(),"Lost stream drains bounded work without applying clear or repairing health");
        result=dispatcher.drain(255);check(!result.current&&result.examined==250&&!result.applied,"Lost queue remainder drains without silent recovery");
        // First routing may happen after the new generation has already lost
        // an event. It must still drain bounded raw work without healing loss.
        f.prepare();for(auto* owner:ownerRoutes)owner->reset();
        for(unsigned n=0;n<256;++n){
            for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::clear,0,10+n,e->generation(),0,0}),"Fill before first new-generation route capture");
            if(n%100==99)f.cycle();
        }
        f.cycle();LoopMeterOwnerClearDispatcher late(*f.engine,ownerRoutes);
        result=late.drain(32);check(!result.current&&result.examined==32&&!result.cleared&&!result.applied,"Late owner dispatcher captures lost generation only to drain it");
        result=late.drain(255);check(!result.current&&result.examined==223&&!late.reset(),"Late capture cannot repair lost stream health");
    }
    std::cout<<"PASS: sole-consumer dispatcher routes actual 20-track Clear All, bounded stale/lost budgets, missing/duplicate/foreign routes, prepare reset and native timer cancellation at 64/128 with exact paired PCM\n";
}
// Drives the production processor without an editor, while the real message
// loop services its existing Timer. No test calls serviceMeters directly.
double queuedDueCounter=100;
double queuedDueClock() noexcept {return queuedDueCounter;}
void processorDueDeliveryTests(bool& audioWatch){
    for(unsigned block:{64u,128u})for(unsigned track:{0u,19u}){
        queuedDueCounter=100;
        auto p=std::make_unique<AionProcessor>(AionProcessor::ExportDestination{},AionProcessor::OutputLatencyReader{},&queuedDueClock);
        p->setPlayConfigDetails(2,2,48000,static_cast<int>(block));p->prepareToPlay(48000,static_cast<int>(block));p->setNonRealtime(true);
        const auto rpc=[&](const char* text){auto result=p->request({juce::JSON::parse(text)});check(static_cast<bool>(result["ok"]),"Due native RPC");};
        rpc(R"({"operation":"setParameter","id":"syncMode","value":0})");
        juce::AudioBuffer<float> audio(2,static_cast<int>(block));juce::MidiBuffer midi;midi.ensureSize(256);
        const auto cycle=[&]{audio.clear();audioWatch=true;p->processBlock(audio,midi);audioWatch=false;};
        const auto command=[&](const char* action){auto* data=new juce::DynamicObject();juce::var request(data);
            data->setProperty("operation","loop");data->setProperty("action",action);data->setProperty("track",static_cast<int>(track));
            data->setProperty("generation",static_cast<int>(p->inspectCallbackClockAudioThread().generation));
            data->setProperty("expiresAt",static_cast<double>(juce::Time::currentTimeMillis())+4000);
            check(static_cast<bool>(p->request({request})["ok"]),"Due native command");};
        cycle();command("record");for(unsigned n=0;n<512/block;++n)cycle();command("finish");cycle();
        command("stopImmediate");cycle();rpc(R"({"operation":"snapshot"})");
        rpc(R"({"operation":"setParameter","id":"syncMode","value":1})");cycle();
        p->setNonRealtime(false);command("play");cycle();
        aion::QueuedMeterCallbackTime expected;check(p->readQueuedMeterCallbackTimeMessageThread(track,expected),"Native due receipt reads captured entry");
        queuedDueCounter=std::nextafter(expected.deadlineSeconds,-std::numeric_limits<double>::infinity());
        check(!p->requestQueuedMeterGraphDueMessageThread(expected)&&!p->inspectMeterServiceMessageThread().pendingWindows,"Native due gate rejects early configured counter");
        queuedDueCounter=expected.deadlineSeconds;auto bad=expected;bad.queueId+=(1ull<<32);
        check(!p->requestQueuedMeterGraphDueMessageThread(bad),"Native due gate rejects stale full-width receipt");
        check(p->requestQueuedMeterGraphDueMessageThread(expected)&&!p->requestQueuedMeterGraphDueMessageThread(expected),"Native due gate enqueues exact receipt once");
        cycle();rpc(R"({"operation":"snapshot"})");const auto status=p->inspectMeterServiceMessageThread();
        check(status.attachedGraphs==1&&status.windowApplied==1&&!status.pendingWindows&&!status.rejected,"Native due result binds graph through sole prepared route");
        command("stopImmediate");cycle();
        aion::MeterStopCallbackTime stopTime,laterStop;
        check(p->readMeterCleanupCallbackTimeMessageThread(track,stopTime)&&stopTime.clockOrigin==1&&stopTime.delaySeconds==.025
            &&stopTime.callbackSerial==p->inspectCallbackClockAudioThread().serial&&stopTime.cleanupSerial,"Native cleanup readout binds accepted stop and configured clock");
        queuedDueCounter+=.01;cycle();
        check(p->readMeterCleanupCallbackTimeMessageThread(track,laterStop)&&laterStop.deadlineSeconds==stopTime.deadlineSeconds
            &&laterStop.callbackSerial==stopTime.callbackSerial&&laterStop.remainingSeconds<=stopTime.remainingSeconds,"Native later callbacks cannot rearm cleanup command deadline");
        command("play");cycle();
        aion::QueuedMeterCallbackTime replacement;check(p->readQueuedMeterCallbackTimeMessageThread(track,replacement)&&replacement.queueId!=expected.queueId,"Native due fixture replaces queue on same track");
        queuedDueCounter=replacement.deadlineSeconds+1;
        check(!p->requestQueuedMeterGraphDueMessageThread(expected)&&!p->inspectMeterServiceMessageThread().pendingWindows,"Native old timer cannot retarget replacement queue");
        p->releaseResources();check(!p->requestQueuedMeterGraphDueMessageThread(replacement),"Native released processor rejects retained due timer");
        auto held=laterStop;check(!p->readMeterCleanupCallbackTimeMessageThread(track,held)&&held==laterStop,"Native released cleanup readout preserves caller output");
    }
    std::cout<<"PASS: production guarded due delivery uses configured counter, exact receipt and sole result route; early/stale/duplicate/replacement/release rejection at tracks 0/19, 64/128\n";
    std::cout<<"PASS: production cleanup command readout retains accepting callback across later views and rejects release at tracks 0/19, 64/128\n";
}
void processorBufferReceiptTests(bool& audioWatch){
    for(unsigned block:{64u,128u}){
        auto p=std::make_unique<AionProcessor>();p->setPlayConfigDetails(2,2,48000,static_cast<int>(block));p->prepareToPlay(48000,static_cast<int>(block));p->setNonRealtime(true);
        const auto rpc=[&](const char* json){auto reply=p->request({juce::JSON::parse(json)});check(static_cast<bool>(reply["ok"]),"Production buffer receipt RPC");return reply;};
        rpc(R"({"operation":"setParameter","id":"syncMode","value":0})");
        rpc(R"({"operation":"setParameter","id":"masterGain","value":1})");
        juce::AudioBuffer<float> audio(2,static_cast<int>(block));juce::MidiBuffer midi;midi.ensureSize(256);unsigned frame=0;
        const auto cycle=[&]{for(unsigned n=0;n<block;++n){audio.setSample(0,static_cast<int>(n),.3f*static_cast<float>(std::sin((frame+n)*.037)));audio.setSample(1,static_cast<int>(n),-.15f);}
            audioWatch=true;p->processBlock(audio,midi);audioWatch=false;frame+=block;};
        const auto command=[&](const char* action){auto* data=new juce::DynamicObject();juce::var request(data);
            data->setProperty("operation","loop");data->setProperty("action",action);data->setProperty("track",19);
            data->setProperty("generation",static_cast<int>(p->inspectCallbackClockAudioThread().generation));
            data->setProperty("expiresAt",static_cast<double>(juce::Time::currentTimeMillis())+4000);
            check(static_cast<bool>(p->request({request})["ok"]),"Production accepted buffer action");};
        cycle();command("record");for(unsigned n=0;n<512/block;++n)cycle();command("finish");cycle();
        auto snapshot=rpc(R"({"operation":"snapshot"})");auto s=p->inspectMeterServiceMessageThread();
        check(static_cast<int>(snapshot["tracks"][19]["length"])==512&&s.buffers==1&&s.bufferApplied==1&&!s.started&&s.current,
            "Production processor must consume actual committed buffer receipt without inventing graph creation");
        command("clear");cycle();snapshot=rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(snapshot["tracks"][19]["state"].toString()=="empty"&&s.cleared==1&&s.applied==1&&!s.rejected&&!s.started&&!s.pendingCleanups,
            "Production typed clear must apply using accepted buffer identity without any raw graph ticket");
        command("record");for(unsigned n=0;n<512/block;++n)cycle();command("finish");cycle();rpc(R"({"operation":"snapshot"})");
        const auto oldGeneration=p->inspectMeterServiceMessageThread().generation;
        p->releaseResources();p->prepareToPlay(48000,static_cast<int>(block));cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.generation!=oldGeneration&&s.buffers==1&&s.bufferApplied==1&&!s.started&&s.current,
            "Production reprepare seeds retained committed buffer through the same ordered consumer");
        command("stopImmediate");command("play");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.plays==1&&s.playApplied==1&&s.attachedGraphs==1&&!s.started&&s.current,"Production accepted immediate PLAY attaches the actual owner graph through the sole dispatcher");
        command("retrigger");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.plays==1&&s.playApplied==1&&s.attachedGraphs==1,"Production healthy RTRG retains accepted graph identity");
        command("stopImmediate");command("retrigger");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.plays==2&&s.playApplied==2&&s.attachedGraphs==1&&s.stops==1&&s.stopApplied==1&&!s.pendingCleanups,"Production stopped RTRG attaches one replacement graph and cancels accepted stop cleanup");
        command("stop");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.stops==2&&s.stopApplied==2&&s.attachedGraphs==1&&s.pendingCleanups==1,"Production unsynced STOP arms the existing owner cleanup without inferring audio-clock teardown");
        command("play");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.plays==3&&s.playApplied==3&&s.attachedGraphs==1&&!s.pendingCleanups,"Production accepted PLAY cancels pending stop through the sole dispatcher");
        rpc(R"({"operation":"setParameter","id":"syncMode","value":1})");cycle();
        command("stop");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.stops==3&&s.stopApplied==3&&s.attachedGraphs==1&&s.pendingCleanups==1&&!s.rejected,"Production scheduled STOP reaches the prepared cleanup adapter through sole dispatcher");
        command("stop");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.stops==4&&s.stopApplied==4&&s.pendingCleanups==1&&!s.rejected,"Production second STOP overrides pending scheduled cleanup");
        command("stopAll");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.stops==5&&s.stopApplied==5&&s.pendingCleanups==1&&!s.rejected,"Production STOP ALL replaces stopping graph cleanup through prepared route");
        command("stopAll");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.stops==6&&s.stopApplied==6&&s.pendingCleanups==1&&!s.rejected,"Production repeated STOP ALL retains the message-owned stopping graph origin");
        p->setNonRealtime(false);command("play");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.queued==1&&s.queueApplied==1&&s.plays==3&&s.attachedGraphs==1&&s.pendingCleanups==1&&!s.rejected,"Production queued PLAY routes intent and retains old graph/cleanup without a birth");
        aion::QueuedMeterCallbackTime firstTime;
        check(p->readQueuedMeterCallbackTimeMessageThread(19,firstTime)&&firstTime.clockOrigin==1
            &&firstTime.callbackSerial==p->inspectCallbackClockAudioThread().serial,"Production queued deadline uses its actual accepting callback and configured counter");
        cycle();rpc(R"({"operation":"snapshot"})");aion::QueuedMeterCallbackTime laterTime;
        check(p->readQueuedMeterCallbackTimeMessageThread(19,laterTime)&&laterTime.deadlineSeconds==firstTime.deadlineSeconds
            &&laterTime.callbackSerial==firstTime.callbackSerial&&laterTime.remainingSeconds<=firstTime.remainingSeconds
            &&p->inspectCallbackClockAudioThread().serial>firstTime.callbackSerial,"Production later snapshot cannot postpone queued deadline or replace command correlation");
        auto unchangedTime=laterTime;check(!p->readQueuedMeterCallbackTimeMessageThread(20,unchangedTime)&&unchangedTime==laterTime,"Production invalid deadline track preserves output");
        p->setNonRealtime(true);
        check(!p->requestQueuedMeterWindowMessageThread(20)&&p->requestQueuedMeterWindowMessageThread(19)
            &&!p->requestQueuedMeterWindowMessageThread(19),"Production stages one internal window request and rejects invalid/duplicate requests");
        s=p->inspectMeterServiceMessageThread();check(s.pendingWindows==1&&!s.readyWindows&&s.attachedGraphs==1&&s.pendingCleanups==1,"Production enqueue does not create a graph or replace cleanup");
        cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.windows==1&&s.windowApplied==1&&!s.windowIgnored&&!s.pendingWindows&&s.readyWindows==1
            &&s.plays==3&&s.attachedGraphs==1&&s.pendingCleanups==1&&!s.rejected,"Sole production consumer routes typed window acceptance without graph birth");
        command("stopAll");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.queued==2&&s.queueApplied==2&&s.stops==6&&s.pendingCleanups==1&&!s.rejected,"Production queued panic cancels intent without new 65ms stop cleanup");
        check(!p->readQueuedMeterCallbackTimeMessageThread(19,unchangedTime)&&unchangedTime==laterTime,"Production cancellation retires queued clock without stale output");
        command("play");cycle();rpc(R"({"operation":"snapshot"})");command("stopImmediate");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.queued==4&&s.queueApplied==4&&s.pendingCleanups==1&&!s.rejected,"Production explicit queued STOP routes its distinct cleanup policy");
        rpc(R"({"operation":"setParameter","id":"syncBpm","value":999})");
        rpc(R"({"operation":"setParameter","id":"syncBars","value":1})");
        rpc(R"({"operation":"setParameter","id":"syncNum","value":1})");cycle();
        command("play");cycle();snapshot=rpc(R"({"operation":"snapshot"})");
        check(snapshot["tracks"][19]["state"].toString()=="queued","Production late-panic fixture accepts a real queued intent");
        for(unsigned n=0;n<1000&&snapshot["tracks"][19]["state"].toString()=="queued";++n){cycle();snapshot=rpc(R"({"operation":"snapshot"})");}
        check(snapshot["tracks"][19]["state"].toString()=="playing","Production audio crosses grid without proving message callback delivery");
        s=p->inspectMeterServiceMessageThread();check(s.queued==5&&s.queueApplied==5&&s.plays==3&&s.stops==6&&s.pendingCleanups==1&&!s.rejected,
            "Production pending intent retains old cleanup after audible grid");
        command("stopAll");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.queued==6&&s.queueApplied==6&&s.plays==3&&s.stops==6&&s.pendingCleanups==1&&!s.rejected,
            "Production post-grid STOP ALL cancels the pending intent through the sole prepared route without a 65ms stop");
        command("stopAll");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.queued==6&&s.stops==6&&!s.rejected,"Production repeated post-grid panic emits neither duplicate cancellation nor inferred graph stop");
        command("play");cycle();snapshot=rpc(R"({"operation":"snapshot"})");
        for(unsigned n=0;n<1000&&snapshot["tracks"][19]["state"].toString()=="queued";++n){cycle();snapshot=rpc(R"({"operation":"snapshot"})");}
        check(snapshot["tracks"][19]["state"].toString()=="playing","Production STOP NOW fixture crosses audio grid");
        command("stopImmediate");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.queued==8&&s.queueApplied==8&&s.stops==6&&s.pendingCleanups==1&&!s.rejected,"Production late STOP NOW retires queue through prepared cleanup route");
        command("stopAll");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.queued==8&&s.stops==6&&!s.rejected,"Production late STOP NOW leaves no orphan queue cancellation");
        command("play");cycle();snapshot=rpc(R"({"operation":"snapshot"})");
        for(unsigned n=0;n<1000&&snapshot["tracks"][19]["state"].toString()=="queued";++n){cycle();snapshot=rpc(R"({"operation":"snapshot"})");}
        check(snapshot["tracks"][19]["state"].toString()=="playing","Production sync-off STOP fixture crosses audio grid");
        rpc(R"({"operation":"setParameter","id":"syncMode","value":0})");cycle();
        command("stop");cycle();snapshot=rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(snapshot["tracks"][19]["state"].toString()=="stopped"&&s.queued==10&&s.queueApplied==10&&s.stops==6&&s.pendingCleanups==1&&!s.rejected,
            "Production normal STOP after sync off retires queue through prepared cleanup route");
        command("stopAll");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.queued==10&&s.stops==6&&!s.rejected,"Production sync-off STOP leaves no orphan queue cancellation");
        rpc(R"({"operation":"setParameter","id":"syncMode","value":1})");cycle();
        command("play");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        const auto beforeGraph=s;
        check(!p->requestQueuedMeterGraphMessageThread(20)&&p->requestQueuedMeterGraphMessageThread(19)
            &&!p->requestQueuedMeterGraphMessageThread(19),"Production stages one explicit queued graph callback");
        check(p->inspectMeterServiceMessageThread().pendingWindows==1,"Production graph enqueue remains pending");
        cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.windows==beforeGraph.windows+1&&s.windowApplied==beforeGraph.windowApplied+1&&!s.pendingWindows&&!s.readyWindows
            &&s.attachedGraphs==1&&s.pendingCleanups==beforeGraph.pendingCleanups&&!s.rejected,"Production graph acceptance binds through sole prepared route and preserves old cleanup");
        command("stopImmediate");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.stops==beforeGraph.stops+1&&s.stopApplied==beforeGraph.stopApplied+1&&s.pendingCleanups==1&&!s.rejected,"Production STOP NOW uses accepted queued graph origin");
        command("stopAll");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(s.stops==beforeGraph.stops+2&&s.stopApplied==beforeGraph.stopApplied+2&&!s.rejected,"Production repeated panic retains queued graph origin");
        command("clear");cycle();rpc(R"({"operation":"snapshot"})");s=p->inspectMeterServiceMessageThread();
        check(!s.attachedGraphs&&s.applied==1&&!s.rejected,"Production typed clear detaches accepted playback graph");
    }
    std::cout<<"PASS: production committed-buffer subscription consumes actual captured clips, typed clear without graph tickets and retained prepare handoff at 64/128; no inferred graph creation or callback new/delete\n";
    std::cout<<"PASS: production post-grid queued panic cancellation at 64/128, retained old cleanup and no duplicate or inferred graph stop\n";
    std::cout<<"PASS: production post-grid STOP NOW consumes queued intent without orphan cancellation at 64/128\n";
    std::cout<<"PASS: production normal STOP after sync off consumes pending post-grid intent at 64/128\n";
    std::cout<<"PASS: production internal queued-window request and typed result preserve graph/cleanup and subsequent quarantine routing at 64/128\n";
    std::cout<<"PASS: production queued command counter deadline retains original callback across later views and retires on cancellation at 64/128\n";
}
void panicDispatcherTests(bool& audioWatch){
    using namespace aion;
    for(unsigned block:{64u,128u}){
        std::atomic<std::uint32_t> prepared{0};
        Fixture f(block,audioWatch,MeterEventScope::acceptedPlayback);prepared.store(f.engine->generation(),std::memory_order_release);
        f.timer=std::make_unique<LoopMeterCleanupTimer>(*f.owner,&prepared);
        std::array<std::unique_ptr<LoopMeterLifecycle>,20> owners;
        std::array<std::unique_ptr<LoopMeterCleanupTimer>,20> timers;
        std::array<LoopMeterLifecycle*,20> ownerRoutes{};LoopMeterClearDispatcher::Routes routes{};
        for(unsigned id=0;id<20;++id){
            if(id==19){ownerRoutes[id]=f.owner.get();routes[id]=f.timer.get();}
            else {owners[id]=std::make_unique<LoopMeterLifecycle>(*f.engine,id);ownerRoutes[id]=owners[id].get();
                timers[id]=std::make_unique<LoopMeterCleanupTimer>(*owners[id],&prepared);routes[id]=timers[id].get();f.record(id);}
        }
        LoopMeterClearDispatcher dispatcher(*f.engine,routes);
        auto result=dispatcher.drain(7);check(result.examined==7&&result.bufferApplied==7,"Panic bank consumes seven actual committed clips");
        result=dispatcher.drain(32);check(result.examined==13&&result.bufferApplied==13&&!result.rejected,"Panic bank consumes remaining actual committed clips");
        for(unsigned id=0;id<20;++id)for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::stopImmediate,id,0,e->generation(),0,0})
            &&e->enqueueLoop({LoopAction::play,id,0,e->generation(),0,0}),"Queue twenty actual immediate graph origins");f.cycle();
        result=dispatcher.drain(32);check(result.playApplied==20&&!result.rejected,"Panic dispatcher binds all twenty actual immediate origins");
        for(unsigned n=0;n<40;++n){for(auto* owner:ownerRoutes)check(owner->request(),"Request all twenty panic windows");f.cycle();
            while(f.engine->popLoopMeterWindow(*f.packet)){const double t=static_cast<double>(f.view->frame)/48000;
                check(ownerRoutes[f.packet->track]->observe(*f.packet,*f.view,f.view->callbackClock,{t,t}),"Observe all twenty actual panic windows");}}
        std::array<MeterCleanupRequest,20> old{};
        for(auto* owner:ownerRoutes)check(owner->value().peak>0,"Every panic route has nonzero actual history");
        const auto panic=[&]{for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::stopAll,0,0,e->generation(),0,0}),"Queue one actual global panic");f.cycle();};
        panic();result=dispatcher.drain(7);check(result.examined==7&&result.stops==7&&result.stopApplied==7&&!result.rejected,"One panic routes exactly seven facts under partial budget");
        for(unsigned id=0;id<20;++id)check(routes[id]->pending()==(id<7),"Partial panic dispatch preserves undelivered owners");
        result=dispatcher.drain(32);check(result.examined==13&&result.stopApplied==13,"Panic remainder routes thirteen facts");
        for(unsigned id=0;id<20;++id){old[id]=ownerRoutes[id]->cleanupRequest();check(old[id].delay==.015+.05,"All panic routes retain 65ms policy");}
        panic();for(auto invalid:{0u,f.engine->generation()+1}){prepared.store(invalid,std::memory_order_release);
            check(!dispatcher.drain(32).examined,"Released/mismatched panic routes leave entire global batch queued");
            for(unsigned id=0;id<20;++id)check(ownerRoutes[id]->cleanupRequest()==old[id],"Rejected panic routes retain all pending cleanup requests");}
        prepared.store(f.engine->generation(),std::memory_order_release);result=dispatcher.drain(32);
        check(result.examined==20&&result.stopApplied==20&&!result.rejected,"Prepared recovery consumes the retained twenty-track panic batch");
        for(unsigned id=0;id<20;++id)check(ownerRoutes[id]->cleanupRequest().serial!=old[id].serial&&!ownerRoutes[id]->deliverCleanup(old[id]),"Repeated global panic replaces every obsolete cleanup");
        for(unsigned id=0;id<20;++id){old[id]=ownerRoutes[id]->cleanupRequest();for(auto* e:{f.engine.get(),f.control.get()}){
            e->configureSync({SyncMode::fixed,120,1,4,4,false});check(e->enqueueLoop({LoopAction::play,id,0,e->generation(),0,0}),"Queue all twenty actual synced intents");}}
        f.cycle();result=dispatcher.drain(7);check(result.examined==7&&result.queued==7&&result.queueApplied==7&&!result.plays,"Seven queued facts apply without graph births");
        for(unsigned id=0;id<20;++id)check((ownerRoutes[id]->queuedIntentId()!=0)==(id<7),"Partial queue dispatch leaves undelivered owners unchanged");
        result=dispatcher.drain(32);check(result.examined==13&&result.queueApplied==13,"Remaining thirteen queued intents route through prepared bank");
        for(unsigned id=0;id<20;++id){const auto intent=ownerRoutes[id]->queuedIntentId();const auto binding=ownerRoutes[id]->binding();
            check(!ownerRoutes[id]->start(0)&&!ownerRoutes[id]->start(f.engine->clock()+4096),"Every prepared queued route rejects unconditional graph start before grid");
            routes[id]->synchronise();check(ownerRoutes[id]->queuedIntentId()==intent&&ownerRoutes[id]->binding().ticket==binding.ticket
                &&ownerRoutes[id]->cleanupRequest()==old[id]&&routes[id]->pending(),"Rejected starts preserve all twenty queued identities, bindings and actual cleanup deadlines");}
        for(unsigned id=0;id<20;++id)check(routes[id]->requestQueuedWindow(ownerRoutes[id]->queuedIntentId(),ownerRoutes[id]->committedBuffer()),"Request conditional windows on all twenty prepared routes");
        f.cycle();result=dispatcher.drain(7);check(result.examined==7&&result.started==4&&result.windowApplied==3&&!result.rejected,"Typed result routing retains seven-entry raw work budget");
        result=dispatcher.drain(32);check(result.examined==32&&result.windowApplied==16&&!result.rejected,"Typed result routing retains thirty-two-entry raw work budget");
        result=dispatcher.drain(32);check(result.examined==1&&result.windowApplied==1&&!result.rejected,"Typed result routing consumes the final exact result");
        for(unsigned id=0;id<20;++id)check(ownerRoutes[id]->queuedWindowReady()&&ownerRoutes[id]->cleanupRequest()==old[id]&&routes[id]->pending(),"All twenty accepted raw windows preserve old JUCE deadlines without a birth");
        panic();prepared.store(0,std::memory_order_release);check(!dispatcher.drain(32).examined,"Released queued-cancel batch remains unread");
        prepared.store(f.engine->generation(),std::memory_order_release);result=dispatcher.drain(7);
        check(result.examined==7&&result.queueApplied==7&&!result.stops,"Queued panic uses cancel receipts, never 65ms stop facts");
        result=dispatcher.drain(32);check(result.examined==13&&result.queueApplied==13&&!result.rejected,"Queued panic remainder cancels thirteen exact intents");
        for(unsigned id=0;id<20;++id)check(!ownerRoutes[id]->queuedIntentId()&&ownerRoutes[id]->cleanupRequest()==old[id]&&routes[id]->pending()
            &&ownerRoutes[id]->inspect().state==MeterLoopState::stopped,"Every queued panic retains its original old timer without a graph birth");
        for(auto* e:{f.engine.get(),f.control.get()}){
            e->configureSync({SyncMode::fixed,999,1,1,4,false});
            for(unsigned id=0;id<20;++id)check(e->enqueueLoop({LoopAction::play,id,0,e->generation(),0,0}),"Queue twenty late-panic intents");
        }
        f.cycle();result=dispatcher.drain(32);check(result.queueApplied==20&&!result.rejected,"All late-panic intents reach their message owners");
        for(unsigned n=0;n<1000&&f.view->tracks[19].state==LoopState::queued;++n)f.cycle();
        for(unsigned id=0;id<20;++id)check(f.view->tracks[id].state==LoopState::playing&&ownerRoutes[id]->queuedIntentId()
            &&ownerRoutes[id]->inspect().state==MeterLoopState::queued&&ownerRoutes[id]->cleanupRequest()==old[id],"All twenty audio sources cross grid while independent message intents and cleanup remain pending");
        for(unsigned id=0;id<20;++id){const auto intent=ownerRoutes[id]->queuedIntentId();
            check(!ownerRoutes[id]->start(0),"Every prepared queued route rejects unconditional start after grid");routes[id]->synchronise();
            check(ownerRoutes[id]->queuedIntentId()==intent&&ownerRoutes[id]->cleanupRequest()==old[id]&&routes[id]->pending(),"Post-grid rejection retains every old cleanup and queue identity");}
        panic();result=dispatcher.drain(7);check(result.examined==7&&result.queueApplied==7&&!result.stops&&!result.rejected,"Post-grid global cancellation retains seven-entry raw budget");
        for(unsigned id=0;id<20;++id)check((ownerRoutes[id]->queuedIntentId()!=0)==(id>=7),"Late partial cancellation retains undelivered queue identities");
        result=dispatcher.drain(32);check(result.examined==13&&result.queueApplied==13&&!result.rejected,"Post-grid cancellation routes remaining thirteen exact identities");
        for(unsigned id=0;id<20;++id)check(!ownerRoutes[id]->queuedIntentId()&&ownerRoutes[id]->cleanupRequest()==old[id]&&routes[id]->pending(),"Post-grid cancellation leaves every original JUCE cleanup deadline untouched");
        panic();check(!dispatcher.drain(32).examined,"Repeated global late panic has no remaining queued facts or inferred graph births");
        for(auto* e:{f.engine.get(),f.control.get()})for(unsigned id=0;id<20;++id)
            check(e->enqueueLoop({LoopAction::play,id,0,e->generation(),0,0}),"Queue twenty explicit graph callbacks");
        f.cycle();result=dispatcher.drain(32);check(result.queueApplied==20&&!result.rejected,"Route all explicit graph intents");
        for(unsigned id=0;id<20;++id)check(routes[id]->requestQueuedGraph(ownerRoutes[id]->queuedIntentId(),ownerRoutes[id]->committedBuffer()),"Request graph acceptance on every prepared route");
        f.cycle();prepared.store(0,std::memory_order_release);check(!dispatcher.drain(32).examined,"Released graph routes retain pending accepted results");
        prepared.store(f.engine->generation(),std::memory_order_release);
        result=dispatcher.drain(7);check(result.examined==7&&result.started==4&&result.windowApplied==3&&!result.rejected,"Graph result batch retains seven-entry raw budget");
        result=dispatcher.drain(32);check(result.examined==32&&result.windowApplied==16&&!result.rejected,"Graph result batch retains thirty-two-entry raw budget");
        result=dispatcher.drain(32);check(result.examined==1&&result.windowApplied==1&&!result.rejected,"Graph result batch consumes final exact acceptance");
        for(unsigned id=0;id<20;++id)check(ownerRoutes[id]->inspect().present&&ownerRoutes[id]->inspect().state==MeterLoopState::playing
            &&ownerRoutes[id]->binding().ticket==ownerRoutes[id]->queuedWindowTransaction().ticket&&!ownerRoutes[id]->queuedIntentId()
            &&ownerRoutes[id]->cleanupRequest()==old[id]&&routes[id]->pending(),"All twenty graph acceptances bind fresh identities and retain exact old cleanup");
        panic();result=dispatcher.drain(7);check(result.examined==7&&result.stopApplied==7&&!result.queued&&!result.rejected,"Accepted graph panic batch uses seven exact stop origins");
        result=dispatcher.drain(32);check(result.examined==13&&result.stopApplied==13&&!result.queued&&!result.rejected,"Accepted graph panic batch routes remaining thirteen origins");
        for(unsigned id=0;id<20;++id)check(ownerRoutes[id]->cleanupRequest()!=old[id]&&ownerRoutes[id]->cleanupRequest().delay==.065&&routes[id]->pending(),"Graph panic replaces every old deadline with original 65ms policy");
        for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::clearAll,0,0,e->generation(),0,0}),"Clear all armed panic graphs");f.cycle();
        result=dispatcher.drain(32);check(result.applied==20&&!result.rejected,"Clear All routes after twenty panic facts");
        for(unsigned id=0;id<20;++id)check(!routes[id]->pending()&&!ownerRoutes[id]->inspect().data,"Clear All cancels every panic timer and attached history");
    }
    std::cout<<"PASS: actual twenty-track STOP ALL receipts route through prepared adapters with bounded 7/13 drains, nonzero PCM, readiness batch preservation, repeated replacement and Clear All cancellation at 64/128; exact paired stereo PCM\n";
    std::cout<<"PASS: twenty pending post-grid queued cancellations retain bounded 7/13 routing and original cleanup at 64/128\n";
    std::cout<<"PASS: twenty prepared queued routes reject generic starts before/after grid and retain original cleanup at 64/128\n";
    std::cout<<"PASS: twenty prepared queued-window transactions route typed results with bounded 7/32/1 drains and retained cleanup at 64/128\n";
    std::cout<<"PASS: twenty prepared queued-graph transactions bind through bounded 7/32/1 drains, preserve old deadlines and route panic stops in 7/13 batches at 64/128\n";
}
void queuedAdapterTests(bool& audioWatch){
    using namespace aion;
    for(unsigned block:{64u,128u}){
        std::atomic<std::uint32_t> prepared{0};Fixture f(block,audioWatch,MeterEventScope::acceptedPlayback);
        f.timer=std::make_unique<LoopMeterCleanupTimer>(*f.owner,&prepared);prepared.store(f.engine->generation(),std::memory_order_release);
        MeterGraphEvent event;check(f.engine->popMeterGraphEvent(event)&&f.timer->consumeBuffer(event),"Commit actual native queue fixture clip");
        for(auto* e:{f.engine.get(),f.control.get()}){check(e->enqueueLoop({LoopAction::stopImmediate,19,0,e->generation(),0,0}),"Stop capture auto-play before queue");e->configureSync({SyncMode::fixed,120,1,4,4,false});}f.cycle();
        const auto command=[&](LoopAction action,MeterGraphEventKind kind){for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({action,19,0,e->generation(),0,0}),"Queue actual native queue lifecycle action");f.cycle();MeterGraphEvent receipt;
            check(f.engine->popMeterGraphEvent(receipt)&&receipt.kind==kind,"Actual accepted native queued lifecycle receipt");return receipt;};
        event=command(LoopAction::play,MeterGraphEventKind::queuedPlay);
        for(auto invalid:{0u,f.engine->generation()+1}){prepared.store(invalid,std::memory_order_release);
            check(!f.timer->consumeQueuedPlay(event)&&!f.owner->queuedIntentId()&&!f.timer->pending(),"Unprepared/mismatched queue intent cannot mutate owner");}
        prepared.store(f.engine->generation(),std::memory_order_release);check(f.timer->consumeQueuedPlay(event)&&!f.owner->inspect().present&&!f.timer->pending(),"Prepared queue applies without a graph or timer");
        check(!f.owner->start(0)&&f.owner->queuedIntentId()==event.queueId&&!f.owner->inspect().present&&!f.timer->pending(),"Graphless prepared queued owner rejects unconditional graph start");
        check(f.timer->requestQueuedWindow(event.queueId,event.bufferId),"Prepared adapter stages graphless transaction");f.cycle();
        check(f.engine->popMeterGraphEvent(event)&&event.kind==MeterGraphEventKind::started&&!f.timer->consumeQueuedWindowResult(event),"Adapter cannot accept a raw notice as typed completion");
        check(f.engine->popMeterGraphEvent(event)&&event.kind==MeterGraphEventKind::queuedWindowAccepted,"Actual typed adapter result");
        prepared.store(0,std::memory_order_release);check(!f.timer->consumeQueuedWindowResult(event)&&f.owner->queuedWindowTransaction().status==QueuedWindowStatus::pending,"Released adapter retains pending result without mutation");
        prepared.store(f.engine->generation(),std::memory_order_release);check(f.timer->consumeQueuedWindowResult(event)&&f.owner->queuedWindowReady()&&!f.timer->pending(),"Prepared adapter recovers exact typed receipt without a timer or graph");
        const auto queueId=f.owner->queuedIntentId();event=command(LoopAction::stopAll,MeterGraphEventKind::queuedCancel);
        for(auto invalid:{0u,f.engine->generation()+1}){prepared.store(invalid,std::memory_order_release);
            check(!f.timer->consumeQueuedCancel(event)&&f.owner->queuedIntentId()==queueId&&!f.timer->pending(),"Unprepared/mismatched queued cancel retains intent");}
        prepared.store(f.engine->generation(),std::memory_order_release);check(f.timer->consumeQueuedCancel(event)&&!f.owner->queuedIntentId(),"Prepared recovery consumes exact queued cancellation");
        event=command(LoopAction::play,MeterGraphEventKind::queuedPlay);check(f.timer->consumeQueuedPlay(event),"Queue before explicit stop");
        event=command(LoopAction::stopImmediate,MeterGraphEventKind::queuedStop);prepared.store(0,std::memory_order_release);
        check(!f.timer->consumeQueuedStop(event)&&f.owner->queuedIntentId(),"Unprepared explicit queued STOP rejects before mutation");
        prepared.store(f.engine->generation(),std::memory_order_release);check(f.timer->consumeQueuedStop(event)&&!f.owner->queuedIntentId()&&!f.timer->pending()&&!f.owner->inspect().present,"Explicit queued STOP without graph uses teardown without a timer");
    }
    std::cout<<"PASS: accepted queued intents/cancels/stops reject unprepared and mismatched delivery, recover exact retained receipts and never create a graph/timer for first queued play at 64/128; exact paired PCM\n";
}
class ProcessorMeterFixture {
public:
    ProcessorMeterFixture(unsigned size,bool& watch):block(size),audioWatch(watch){
        // Hosts may construct/prepare processors off the message thread. The
        // first message service, not either host call, must create timer routes.
        std::thread constructor([&]{p=std::make_unique<AionProcessor>();control=std::make_unique<AionProcessor>();});constructor.join();
        check(!p->inspectMeterServiceMessageThread().cleanupRoutes&&!control->inspectMeterServiceMessageThread().cleanupRoutes,
            "Host construction cannot create message-owned cleanup adapters");
        check(static_cast<bool>(idle->request({juce::JSON::parse(R"({"operation":"midiConfig","action":"enable","enabled":false})")})["ok"]),"Start shared service before prepare with idle MIDI");
        for(auto* processor:{p.get(),control.get()}){
            processor->setPlayConfigDetails(2,2,48000,static_cast<int>(block));
        }
        preparePair();
        check(!p->inspectMeterServiceMessageThread().cleanupRoutes,"Host prepare cannot create message-owned cleanup adapters");
        for(auto* processor:{p.get(),control.get()}){
            processor->parameters.getParameter("masterGain")->setValueNotifyingHost(.5f);
            processor->parameters.getParameter("inputMonitor")->setValueNotifyingHost(1);
        }
        midi.ensureSize(256);otherMidi.ensureSize(256);p->getStateInformation(saved);
        cycle();queue(12);cycle(); // 240 real clear facts in one callback.
        snapshot();const auto s=p->inspectMeterServiceMessageThread();
        check(s.cleanupRoutes==20&&!s.pendingCleanups,"Production clear dispatcher must own all twenty cleanup adapter routes without inventing stops");
        check(s.current&&s.examined==32&&s.cleared==32&&s.rejected==32&&!s.applied&&!s.started,"Production request drains exactly 32 raw receipts without graph inference");
        generation=s.generation;
    }
    bool complete() const noexcept {return stage==6;}
    void poll(){
        const auto s=p->inspectMeterServiceMessageThread();
        check(s.cleanupRoutes==20&&!s.pendingCleanups,"Production cleanup adapter routes persist through service/release/prepare/loss");
        check(!p->getActiveEditor(),"Meter service must not need an editor");
        if(stage==0){
            if(s.examined<240)return;
            const auto unprepared=idle->inspectMeterServiceMessageThread();
            check(unprepared.timerRunning&&!unprepared.current&&!unprepared.generation&&!unprepared.passes&&!unprepared.cleanupRoutes,
                "Unprepared idle service must not stop and race a host prepare wakeup");
            check(s.examined==240&&s.cleared==240&&s.rejected==240&&s.current&&!s.applied&&!s.started,"Autonomous processor timer drains all twenty routes without MIDI/export/editor");
            queue(2);cycle(); // Leave 40 undelivered old facts across release.
            std::thread host([&]{p->releaseResources();control->releaseResources();});host.join();snapshot();
            const auto released=p->inspectMeterServiceMessageThread();
            check(!released.current&&released.examined==240,"Released processor cannot consume pending meter receipts");
            releasedPasses=released.passes;started=wall();stage=1;
        }else if(stage==1){
            if(wall()-started<.08)return;
            check(!s.current&&s.examined==240&&s.passes==releasedPasses,"Periodic service leaves released meter ownership untouched");
            preparePair();
            cycle();stage=2; // No snapshot/request to wake or reset the owners.
        }else if(stage==2){
            if(s.generation==generation||s.examined<40)return;
            check(s.current&&s.examined==40&&!s.cleared&&!s.applied&&!s.started,"Timer alone resets prepared owners and discards old receipts");
            generation=s.generation;queue(13);cycle(); // 260 facts overflow the 255-slot stream.
            check(!p->inspectMeterServiceMessageThread().current,"Production meter loss is visible before any drain");stage=3;
        }else if(stage==3){
            if(s.examined<295)return;
            check(s.generation==generation&&s.examined==295&&!s.current&&!s.cleared&&!s.applied,"Production lost-generation raw drain stays bounded and cannot heal health");
            snapshot();check(!p->inspectMeterServiceMessageThread().current,"Repeated request cannot heal a lost generation");
            preparePair();cycle();stage=4;
        }else if(stage==4){
            if(s.generation==generation||!s.current)return;
            check(!s.examined&&!s.cleared&&!s.applied,"Only a fresh prepare recovers the lost production service");
            queue(1);cycle();stage=5;
        }else if(stage==5){
            if(s.examined<20)return;
            check(s.examined==20&&s.cleared==20&&s.rejected==20&&s.current&&!s.applied,"Fresh prepared service routes all twenty tracks after loss");
            juce::MemoryBlock after;p->getStateInformation(after);check(saved==after,"Processor meter service leaves serialized parameters/MIDI unchanged");stage=6;
        }
    }
private:
    unsigned block;bool& audioWatch;unsigned stage=0;std::uint32_t generation=0;
    std::uint64_t releasedPasses=0,tick=0;double started=0;
    std::unique_ptr<AionProcessor> p,control,idle=std::make_unique<AionProcessor>();
    juce::AudioBuffer<float> audio{2,128},other{2,128};juce::MidiBuffer midi,otherMidi;juce::MemoryBlock saved;
    void preparePair(){std::thread host([&]{for(auto* processor:{p.get(),control.get()})processor->prepareToPlay(48000,static_cast<int>(block));});host.join();}
    void snapshot(){check(static_cast<bool>(p->request({juce::JSON::parse(R"({"operation":"snapshot"})")})["ok"]),"Production meter snapshot");}
    void queue(unsigned count){
        for(unsigned n=0;n<count;++n)for(auto* processor:{p.get(),control.get()}){
            auto* command=new juce::DynamicObject();juce::var request(command);
            command->setProperty("operation","loop");command->setProperty("action","clearAll");command->setProperty("track",0);
            command->setProperty("generation",static_cast<int>(processor->inspectCallbackClockAudioThread().generation));
            command->setProperty("expiresAt",static_cast<double>(juce::Time::currentTimeMillis())+4000);
            check(static_cast<bool>(processor->request({request})["ok"]),"Queue real production Clear All");
        }
    }
    void cycle(){
        audio.setSize(2,static_cast<int>(block),false,false,true);other.setSize(2,static_cast<int>(block),false,false,true);
        for(unsigned i=0;i<block;++i){const auto sample=.2f*static_cast<float>(std::sin(static_cast<double>(tick+i)*.037));audio.setSample(0,static_cast<int>(i),sample);audio.setSample(1,static_cast<int>(i),-.5f*sample);}
        other.makeCopyOf(audio,true);
        audioWatch=true;p->processBlock(audio,midi);control->processBlock(other,otherMidi);audioWatch=false;tick+=block;
        bool audible=false;
        for(int c=0;c<2;++c)for(int i=0;i<static_cast<int>(block);++i){check(audio.getSample(c,i)==other.getSample(c,i),"Processor meter service preserves exact paired stereo PCM");audible|=audio.getSample(c,i)!=0;}
        check(audible,"Processor meter fixture monitors actual nonzero PCM");
    }
};
struct ShutdownState {
    std::atomic<bool> armed{false},done{false},memberOwned{false},parameterOwned{false},memberOnMessage{false},parameterOnMessage{false};
    std::atomic<unsigned> members{0},parameters{0};
    std::mutex mutex;std::condition_variable changed;bool releaseUnowned=false;
};
struct ShutdownResource {
    std::shared_ptr<ShutdownState> state;
    ~ShutdownResource(){
        if(!state->armed.load(std::memory_order_acquire))return;
        auto* manager=juce::MessageManager::getInstanceWithoutCreating();
        const bool owned=manager&&manager->currentThreadHasLockedMessageManager();
        state->memberOwned.store(owned);state->memberOnMessage.store(manager&&manager->isThisTheMessageThread());
        state->members.fetch_add(1,std::memory_order_release);
        // The old-code negative run pauses here until the message test elects
        // to join it. Parameters cannot be freed against a running UI callback.
        if(!owned){std::unique_lock lock(state->mutex);state->changed.wait(lock,[&]{return state->releaseUnowned;});}
    }
};
class ShutdownParameter final : public juce::AudioParameterBool {
public:
    explicit ShutdownParameter(std::shared_ptr<ShutdownState> observer)
        :AudioParameterBool(juce::ParameterID{"shutdownProbe",1},"Shutdown probe",false),state(std::move(observer)){}
    ~ShutdownParameter() override {
        if(!state->armed.load(std::memory_order_acquire))return;
        auto* manager=juce::MessageManager::getInstanceWithoutCreating();
        state->parameterOwned.store(manager&&manager->currentThreadHasLockedMessageManager());
        state->parameterOnMessage.store(manager&&manager->isThisTheMessageThread());
        state->parameters.fetch_add(1,std::memory_order_release);
    }
private:
    std::shared_ptr<ShutdownState> state;
};
// Observe actual std::function member cleanup and AudioProcessor-owned parameter
// deletion, after AionProcessor's own members and APVTS have been destroyed.
class ProcessorShutdownFixture {
public:
    ProcessorShutdownFixture(unsigned size,bool& watch):block(size),audioWatch(watch){begin();}
    ~ProcessorShutdownFixture(){check(!worker.joinable(),"Shutdown fixture must finish host destruction before teardown");}
    bool complete() const noexcept {return which==5;}
    void poll(){
        if(complete())return;
        if(worker.joinable()){
            if(state->members.load(std::memory_order_acquire)&&!state->memberOwned.load()){
                {std::lock_guard lock(state->mutex);state->releaseUnowned=true;}state->changed.notify_one();
                worker.join();
                check(false,"Background processor destruction must retain message ownership through member and base parameter cleanup");
            }
            if(!state->done.load(std::memory_order_acquire))return;
            worker.join();verify(false);advance();return;
        }
        if(which!=4&&processor->inspectMeterServiceMessageThread().passes<=initialPasses)return;
        if(which==1)processor->releaseResources();
        state->armed.store(true,std::memory_order_release);
        if(which==3){processor.reset();verify(true);advance();return;}
        const bool nested=which==2;
        worker=std::thread([held=std::move(processor),observer=state,nested]() mutable {
            if(nested){const juce::MessageManagerLock outer;held.reset();}
            else held.reset();
            observer->done.store(true,std::memory_order_release);
        });
    }
private:
    unsigned block,which=0;bool& audioWatch;std::uint64_t initialPasses=0;
    std::shared_ptr<ShutdownState> state;
    std::unique_ptr<AionProcessor> processor;
    std::thread worker;
    void verify(bool onMessage){
        check(state->members.load()==1&&state->parameters.load()==1&&state->memberOwned.load()&&state->parameterOwned.load(),
            "Message ownership must cover both native members and AudioProcessor base parameters");
        check(state->memberOnMessage.load()==onMessage&&state->parameterOnMessage.load()==onMessage,
            "Shutdown ownership must preserve the host-selected destruction thread");
    }
    void advance(){if(++which<5)begin();}
    void begin(){
        state=std::make_shared<ShutdownState>();auto resource=std::make_shared<ShutdownResource>();resource->state=state;
        processor=std::make_unique<AionProcessor>(AionProcessor::ExportDestination{},[resource]{return aion::DeviceOutputLatency{};});
        processor->addParameter(new ShutdownParameter(state));
        if(which==4)return; // Never prepared: APVTS still owns a live timer.
        processor->setPlayConfigDetails(2,2,48000,static_cast<int>(block));processor->prepareToPlay(48000,static_cast<int>(block));
        processor->parameters.getParameter("masterGain")->setValueNotifyingHost(.5f);
        processor->parameters.getParameter("inputMonitor")->setValueNotifyingHost(1);
        juce::AudioBuffer<float> audio(2,static_cast<int>(block));audio.clear();
        for(int n=0;n<static_cast<int>(block);++n){audio.setSample(0,n,.25f);audio.setSample(1,n,-.125f);}
        juce::MidiBuffer midi;midi.ensureSize(256);
        audioWatch=true;processor->processBlock(audio,midi);audioWatch=false;
        check(audio.getMagnitude(0,static_cast<int>(block))>0,"Shutdown fixture renders actual nonzero PCM before stopping audio calls");
        processor->request({juce::JSON::parse(R"({"operation":"snapshot"})")});
        const auto status=processor->inspectMeterServiceMessageThread();check(status.current&&status.timerRunning&&status.cleanupRoutes==20,"Shutdown begins with live processor service and all cleanup adapters");
        initialPasses=status.passes;
    }
};
double cleanupCounter=0;
double readCleanupCounter() noexcept {return cleanupCounter;}
class CorrelatedCleanupRunner final : private juce::Timer {
public:
    explicit CorrelatedCleanupRunner(bool& watch):audioWatch(watch){begin();startTimer(5);}
    bool finished() const noexcept {return done;}
private:
    bool& audioWatch;bool done=false;unsigned index=0,phase=0;
    std::unique_ptr<Fixture> f;double due=0,started=0,receipt=0;
    std::uint64_t frame=0,data=0;aion::MeterCleanupRequest request{};
    void command(aion::LoopAction action,std::uint64_t at=0){for(auto* e:{f->engine.get(),f->control.get()})
        check(e->enqueueLoop({action,19,0,e->generation(),at,0}),"Queue correlated real-timer fixture action");}
    aion::MeterGraphEvent next(aion::MeterGraphEventKind kind){aion::MeterGraphEvent e;
        check(f->engine->popMeterGraphEvent(e)&&e.kind==kind,"Actual correlated timer event kind");return e;}
    bool consume(const aion::MeterGraphEvent& e){using K=aion::MeterGraphEventKind;
        return e.kind==K::immediateStop?f->timer->consumeImmediateStop(e):e.kind==K::queuedStop?f->timer->consumeQueuedStop(e):
            e.kind==K::scheduledStop?f->timer->consumeScheduledStop(e):f->timer->consumeStopAll(e);}
    void play(){command(aion::LoopAction::play);f->cycle();check(f->timer->consumeImmediatePlay(next(aion::MeterGraphEventKind::immediatePlay)),"Correlated timer attaches accepted graph");}
    void begin(){using namespace aion;
        const unsigned block=index<8?(index<4?64:128):((index-8)/2?128:64),kind=index<8?index%4:0;
        cleanupCounter=10+index*10;f=std::make_unique<Fixture>(block,audioWatch,MeterEventScope::acceptedPlayback);
        f->timer=std::make_unique<LoopMeterCleanupTimer>(*f->owner,nullptr,&readCleanupCounter);
        check(f->timer->consumeBuffer(next(MeterGraphEventKind::bufferInstalled)),"Correlated timer accepts actual clip");
        command(LoopAction::stopImmediate);f->cycle();play();
        if(kind==3){command(LoopAction::stopImmediate);f->cycle();check(consume(next(MeterGraphEventKind::immediateStop)),"Prepare old graph for correlated queued stop");
            for(auto* e:{f->engine.get(),f->control.get()})e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
            command(LoopAction::play);f->cycle();check(f->timer->consumeQueuedPlay(next(MeterGraphEventKind::queuedPlay)),"Correlated timer retains old cleanup while queued");}
        else if(kind==1)for(auto* e:{f->engine.get(),f->control.get()})e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
        const double entry=cleanupCounter;command(kind==1?LoopAction::stop:kind==2?LoopAction::stopAll:LoopAction::stopImmediate,f->engine->clock()+block/2);
        f->cycle(false,{entry,true});auto event=next(kind==1?MeterGraphEventKind::scheduledStop:kind==2?MeterGraphEventKind::stopAll:kind==3?MeterGraphEventKind::queuedStop:MeterGraphEventKind::immediateStop);
        cleanupCounter=entry+event.stopLeadSeconds+1;receipt=cleanupCounter;
        const auto old=f->owner->cleanupRequest();auto bad=event;bad.callbackClock.sampleRate=0;
        check(!consume(bad)&&f->owner->cleanupRequest()==old,"Malformed live correlation cannot mutate cleanup");
        if(index>=8){if(index%2)event.callbackClock.nonRealtime=true;else event.callbackClock.processorEntry={};}
        check(consume(event)&&f->timer->pending()&&f->owner->inspect().present,"Correlated cleanup is armed asynchronously");
        double left=-1;check(f->timer->remaining(left)&&(index<8?left==0:std::abs(left-.025)<1e-12),"Real timer uses command deadline or explicit missing/offline fallback");
        frame=f->engine->clock();data=f->owner->inspect().data;started=wall();phase=index<8?0:3;
    }
    void detached(){check(!f->timer->pending()&&!f->owner->inspect().present&&f->owner->inspect().data==data&&f->engine->clock()==frame,
        "Actual correlated JUCE callback retains data and frozen audio clock");}
    void advance(){f.reset();if(++index==12){done=true;stopTimer();std::cout<<"PASS: real configured-clock JUCE cleanup, all four overdue stop policies, future sample offset, early wake, missing/offline fallback and malformed rejection at 64/128; exact paired PCM\n";}
        else begin();}
    void timerCallback() override {
        if(phase==0){if(f->owner->inspect().present)return;detached();
            if(index%4){advance();return;}
            play();const double entry=200+index*10;cleanupCounter=entry;
            command(aion::LoopAction::stopImmediate,f->engine->clock()+f->block/2);f->cycle(false,{entry,true});const auto event=next(aion::MeterGraphEventKind::immediateStop);
            check(consume(event),"Arm future sub-block command cleanup");due=entry+static_cast<double>(f->block/2)/48000+.025;
            double left=0;check(f->timer->remaining(left)&&std::abs(left-(due-entry))<1e-12,"Receipt before logical command still schedules timer");
            frame=f->engine->clock();data=f->owner->inspect().data;request=f->owner->cleanupRequest();started=wall();phase=1;return;}
        if(phase==1){if(wall()-started<.05)return;
            check(f->owner->inspect().present&&f->timer->pending(),"Early real wake cannot deliver before configured deadline");
            cleanupCounter=due-.001;f->timer->synchronise();double left=0;
            check(f->timer->remaining(left)&&std::abs(left-.001)<1e-12&&f->owner->cleanupRequest()==request,"Synchronise cannot postpone correlated deadline");
            cleanupCounter=due;phase=2;return;}
        if(phase==3){if(wall()-started<.04)return;
            check(f->owner->inspect().present&&f->timer->pending(),"Fallback remains receipt-origin when counter is frozen");cleanupCounter=receipt+.025;phase=2;return;}
        if(f->owner->inspect().present)return;detached();advance();
    }
};
// Exercise the default production timer, with no editor and no explicit graph
// request. The audio clock is frozen while the configured counter advances.
class AutomaticQueuedRunner final : private juce::Timer {
public:
    explicit AutomaticQueuedRunner(bool& watch):audioWatch(watch){begin();startTimer(5);}
    bool finished() const noexcept {return done;}
private:
    bool& audioWatch;bool done=false;unsigned index=0,phase=0,block=64,mode=0;
    double started=0,due=0;
    std::unique_ptr<AionProcessor> p;
    juce::AudioBuffer<float> audio;juce::MidiBuffer midi;
    void rpc(const char* text){check(static_cast<bool>(p->request({juce::JSON::parse(text)})["ok"]),"Automatic queued RPC");}
    void cycle(){audio.clear();audioWatch=true;p->processBlock(audio,midi);audioWatch=false;}
    void command(const char* action,unsigned track){auto* data=new juce::DynamicObject();juce::var request(data);
        data->setProperty("operation","loop");data->setProperty("action",action);data->setProperty("track",static_cast<int>(track));
        data->setProperty("generation",static_cast<int>(p->inspectCallbackClockAudioThread().generation));
        data->setProperty("expiresAt",static_cast<double>(juce::Time::currentTimeMillis())+4000);
        check(static_cast<bool>(p->request({request})["ok"]),"Automatic queued command");}
    void both(const char* action){command(action,0);command(action,19);cycle();}
    void begin(){
        mode=index%5;block=index<5?64:128;phase=0;queuedDueCounter=100;
        p=std::make_unique<AionProcessor>(AionProcessor::ExportDestination{},AionProcessor::OutputLatencyReader{},&queuedDueClock);
        p->setPlayConfigDetails(2,2,48000,static_cast<int>(block));p->prepareToPlay(48000,static_cast<int>(block));p->setNonRealtime(true);
        audio.setSize(2,static_cast<int>(block));midi.ensureSize(256);
        rpc(R"({"operation":"setParameter","id":"syncMode","value":0})");cycle();
        for(unsigned track:{0u,19u}){
            command("record",track);cycle();for(unsigned n=0;n<512/block;++n)cycle();
            command("finish",track);cycle();cycle();
        }
        both("stopImmediate");
        rpc(R"({"operation":"snapshot"})");
        if(mode==4){both("play");rpc(R"({"operation":"snapshot"})");
            check(p->inspectMeterServiceMessageThread().attachedGraphs==2,"Automatic fixture attaches old graphs");}
        rpc(R"({"operation":"setParameter","id":"syncMode","value":1})");cycle();
        p->setNonRealtime(mode==3);
        if(mode==4)both("stopImmediate");
        both("play");rpc(R"({"operation":"snapshot"})");
        aion::QueuedMeterCallbackTime receipt;
        if(mode==3){check(!p->readQueuedMeterCallbackTimeMessageThread(0,receipt),"Offline queue cannot borrow live counter");due=101;}
        else {check(p->readQueuedMeterCallbackTimeMessageThread(0,receipt),"Automatic queue retains actual command deadline");due=receipt.deadlineSeconds;
            check(p->readQueuedMeterCallbackTimeMessageThread(19,receipt),"Second track has independent retained queue");due=std::max(due,receipt.deadlineSeconds);
            check(due>100.026,"Automatic fixture has positive original grid delay");}
        started=wall();
    }
    void advance(){p.reset();if(++index==10){done=true;stopTimer();
            std::cout<<"PASS: autonomous production queued callbacks at 64/128 on tracks 0/19, immutable early/late counter deadlines, one-shot result, cancellation, release, offline exclusion and old-cleanup survival without editor or manual requests\n";
        }else begin();}
    void timerCallback() override {
        if(phase==0){if(wall()-started<.06)return;
            auto status=p->inspectMeterServiceMessageThread();check(!status.pendingWindows&&!status.windowApplied,"Frozen counter cannot fire early queued callback");
            if(mode==1)both("stopAll");
            if(mode==2)p->releaseResources();
            if(mode==4){queuedDueCounter=100.026;phase=4;started=wall();return;}
            queuedDueCounter=due+1;phase=mode==1||mode==2||mode==3?3:1;started=wall();return;}
        if(phase==4){if(wall()-started<.07)return;
            const auto status=p->inspectMeterServiceMessageThread();check(!status.attachedGraphs&&!status.pendingCleanups&&!status.pendingWindows,"Old cleanup detaches before still-pending queued callback");
            queuedDueCounter=due+1;phase=1;started=wall();return;}
        if(phase==1){const auto status=p->inspectMeterServiceMessageThread();if(status.pendingWindows!=2)return;
            check(!status.attachedGraphs,"Automatic enqueue is not a graph birth");cycle();phase=2;started=wall();return;}
        if(wall()-started<.07)return;
        const auto status=p->inspectMeterServiceMessageThread();
        if(phase==2){check(status.attachedGraphs==2&&status.windowApplied==2&&!status.pendingWindows&&!status.rejected,"Automatic typed results bind both exact graph identities once");
            cycle();check(p->inspectMeterServiceMessageThread().windowApplied==2,"Later service cannot repeat consumed callback");}
        else check(!status.pendingWindows&&!status.windowApplied,"Cancelled, released and offline callbacks never enqueue");
        advance();
    }
};
class Runner final : private juce::Timer {
public:
    explicit Runner(bool& watch):audioWatch(watch){automatic=std::make_unique<AutomaticQueuedRunner>(watch);startTimer(5);}
    ~Runner() override {stopTimer();check(!cleanupWorker.joinable(),"Cleanup worker must finish before owner destruction");}
private:
    bool& audioWatch;std::atomic<std::uint32_t> cleanupPrepared{0}; // Outlives bound fixture adapters.
    std::unique_ptr<CorrelatedCleanupRunner> correlated;
    std::unique_ptr<AutomaticQueuedRunner> automatic;
    std::unique_ptr<Fixture> f;std::unique_ptr<ProcessorMeterFixture> processorMeters;
    std::unique_ptr<ProcessorShutdownFixture> shutdown;unsigned stage=0;bool lateStopSyncOff=false;
    double started=0,expectedDelay=0;std::uint64_t frame=0,data=0;std::array<double,7> held{};
    aion::MeterCleanupRequest obsolete{};
    std::thread cleanupWorker;
    std::atomic<bool> cleanupDestroyed{false};
    void capture(){started=wall();frame=f->engine->clock();data=f->owner->inspect().data;held=reading(f->owner->value());}
    void stopped(){check(!f->owner->inspect().present&&!f->timer->pending()&&f->owner->inspect().data==data
        &&reading(f->owner->value())==held&&f->engine->clock()==frame,"Real timer detaches analyser but retains data/history with frozen audio clock");}
    void acceptedPlay(bool seed=true){
        for(auto* e:{f->engine.get(),f->control.get()})check(e->enqueueLoop({aion::LoopAction::stopImmediate,19,0,e->generation(),0,0})
            &&e->enqueueLoop({aion::LoopAction::play,19,0,e->generation(),0,0}),"Queue actual accepted stop/play timer pair");f->cycle();
        aion::MeterGraphEvent event;unsigned plays=0;
        while(f->engine->popMeterGraphEvent(event)){
            if(event.kind==aion::MeterGraphEventKind::immediateStop)check(f->timer->consumeImmediateStop(event),"Route actual accepted stop before timed replacement");
            else {check(event.kind==aion::MeterGraphEventKind::immediatePlay&&f->timer->consumeImmediatePlay(event),"Route actual accepted timer playback");++plays;}
        }
        check(plays==1,"Exactly one actual timer playback birth");
        if(seed)for(unsigned n=0;n<2048/f->block;++n){check(f->owner->request(),"Request accepted stop timer PCM");f->cycle();
            check(f->engine->popLoopMeterWindow(*f->packet),"Receive accepted stop timer PCM");const double t=static_cast<double>(f->view->frame)/48000;
            check(f->owner->observe(*f->packet,*f->view,f->view->callbackClock,{t,t}),"Seed accepted stop timer history");}
    }
    aion::MeterGraphEvent acceptedStop(aion::LoopAction action=aion::LoopAction::stopImmediate){
        for(auto* e:{f->engine.get(),f->control.get()})check(e->enqueueLoop({action,19,0,e->generation(),0,0}),"Queue actual stop for native Timer");f->cycle();
        const auto kind=action==aion::LoopAction::stopAll?aion::MeterGraphEventKind::stopAll:aion::MeterGraphEventKind::immediateStop;
        aion::MeterGraphEvent event;check(f->engine->popMeterGraphEvent(event)&&event.kind==kind,"Actual accepted native stop receipt");return event;
    }
    void beginAcceptedStops(){
        const auto block=f->block;f=std::make_unique<Fixture>(block,audioWatch,aion::MeterEventScope::acceptedPlayback);
        cleanupPrepared.store(f->engine->generation(),std::memory_order_release);
        f->timer=std::make_unique<aion::LoopMeterCleanupTimer>(*f->owner,&cleanupPrepared);
        aion::MeterGraphEvent installed;check(f->engine->popMeterGraphEvent(installed)&&f->timer->consumeBuffer(installed),"Install actual native stop fixture buffer");
        acceptedPlay();check(f->owner->value().peak>0,"Actual accepted stop fixture has nonzero meter history");
        const auto event=acceptedStop();const auto presentData=f->owner->inspect().data;const auto presentHistory=reading(f->owner->value());
        for(auto invalid:{0u,f->engine->generation()+1}){
            cleanupPrepared.store(invalid,std::memory_order_release);
            check(!f->timer->consumeImmediateStop(event)&&f->owner->inspect().present&&f->owner->inspect().data==presentData
                &&!f->timer->pending()&&!f->owner->cleanupRequest().serial&&reading(f->owner->value())==presentHistory,
                "Released or mismatched accepted stop cannot mutate an attached graph or consume the retained receipt");
        }
        cleanupPrepared.store(f->engine->generation(),std::memory_order_release);
        check(f->timer->consumeImmediateStop(event),"Accepted immediate stop arms actual JUCE cleanup");capture();stage=12;
    }
    void begin(unsigned block){
        lateStopSyncOff=false;
        shutdown=std::make_unique<ProcessorShutdownFixture>(block,audioWatch);
        processorMeters=std::make_unique<ProcessorMeterFixture>(block,audioWatch);
        f=std::make_unique<Fixture>(block,audioWatch);f->play();capture();
        check(f->timer->stop(1)&&f->owner->cleanupRequest().delay==.025,"Native immediate stop captures 25ms");stage=0;
        const auto receipt=f->owner->cleanupRequest();
        check(!f->timer->stop(std::numeric_limits<double>::quiet_NaN())&&f->owner->cleanupRequest()==receipt
            &&f->timer->pending(),"Rejected native stop preserves the pending deadline and owner");
        // Block the message thread deliberately: cleanup must wait for delivery.
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        check(f->owner->inspect().present,"Blocked message thread cannot deliver a timer callback");
    }
    void timerCallback() override {
        if(automatic){if(!automatic->finished())return;automatic.reset();correlated=std::make_unique<CorrelatedCleanupRunner>(audioWatch);return;}
        if(correlated){if(!correlated->finished())return;correlated.reset();begin(64);return;}
        shutdown->poll();
        processorMeters->poll();
        switch(stage){
        case 0:
            if(f->owner->inspect().present)return;
            check(wall()-started>=.025,"Real immediate callback never early");stopped();
            f->play();check(f->timer->stop(2),"Arm cancelled stop");f->play();f->timer->synchronise();capture();stage=1;break;
        case 1:
            if(wall()-started<.08)return;
            check(f->owner->inspect().present&&!f->timer->pending()&&f->owner->inspect().data==data,"Stopping-to-play cancels old timer without detaching replacement");
            capture();check(f->timer->stop(3)&&f->timer->stop(3,3.08),"Repeated stop replaces 25ms by scheduled cleanup");expectedDelay=f->owner->cleanupRequest().delay;stage=2;break;
        case 2:
            f->timer->synchronise(); // Frequent validation must not postpone delivery.
            if(f->owner->inspect().present)return;
            check(wall()-started>=expectedDelay,"Scheduled callback waits delta plus 50ms");stopped();
            f->play();capture();check(f->timer->stop(4,4.01),"Arm original queued-play timer");expectedDelay=f->owner->cleanupRequest().delay;obsolete=f->owner->cleanupRequest();
            check(f->owner->setState(aion::MeterLoopState::queued),"Intervening queued state");f->play();
            check(f->owner->cleanupRequest()==obsolete,"Queued-to-play retains old timer receipt");
            f->timer->synchronise();data=f->owner->inspect().data;held=reading(f->owner->value());frame=f->engine->clock();stage=3;break;
        case 3:
            f->timer->synchronise();
            if(f->owner->inspect().present)return;
            check(wall()-started>=expectedDelay,"Queued replacement uses retained original deadline");stopped();
            f->play();check(f->timer->stop(5),"Arm prepare-stale timer");obsolete=f->owner->cleanupRequest();
            f->prepare();check(f->owner->reset()&&f->owner->setBuffer(true),"Prepare barrier resets owner");f->play();capture();stage=4;break;
        case 4:
            // No upper callback-latency assumption: a busy message queue may
            // deliver the runner before another due Timer in the same batch.
            if(wall()-started<.08||f->timer->pending())return;
            check(f->owner->inspect().present&&!f->timer->pending()&&f->owner->inspect().data==data
                &&!f->owner->deliverCleanup(obsolete),"Stale native callback rejects after prepare without synchronise");
            capture();check(f->timer->stop(6),"Reprepared timer recovers");stage=5;break;
        case 5:
            if(f->owner->inspect().present)return;
            check(wall()-started>=.025,"Reprepared timer respects 25ms");stopped();
            f->play();check(f->timer->stop(7),"Arm destruction fixture");obsolete=f->owner->cleanupRequest();f->timer.reset();capture();stage=6;break;
        case 6:
            if(wall()-started<.08)return;
            check(f->owner->inspect().present&&f->owner->inspect().data==data&&f->owner->deliverCleanup(obsolete),"Message-thread destruction cancels delivery without editing owner");
            f->timer=std::make_unique<aion::LoopMeterCleanupTimer>(*f->owner);f->play();
            check(f->timer->stop(8),"Arm timer before actual native DEL");f->clearFromAudio();
            f->record();f->owner->setBuffer(true);f->play();capture();stage=7;break;
        case 7:
            if(wall()-started<.08)return;
            if(!processorMeters->complete())return;
            if(!shutdown->complete())return;
            check(f->owner->inspect().present&&f->owner->inspect().data==data&&!f->timer->pending()
                &&reading(f->owner->value())==held,"Cleared timer cannot detach the same-generation re-recorded graph");
            check(f->timer->stop(9,3609),"Arm distant cleanup before host-owned adapter destruction");
            obsolete=f->owner->cleanupRequest();capture();cleanupDestroyed.store(false,std::memory_order_release);
            cleanupWorker=std::thread([this,timer=std::move(f->timer)]() mutable {
                const juce::MessageManagerLock ownership;
                check(ownership.lockWasGained(),"Host cleanup destruction must retain message ownership");
                timer.reset();cleanupDestroyed.store(true,std::memory_order_release);
            });stage=8;break;
        case 8:
            // Never join a worker awaiting the message manager from this callback.
            if(!cleanupDestroyed.load(std::memory_order_acquire))return;
            cleanupWorker.join();
            check(f->owner->inspect().present&&f->owner->inspect().data==data&&f->owner->cleanupRequest()==obsolete
                &&reading(f->owner->value())==held&&f->engine->clock()==frame,
                "Host-owned armed timer destruction cancels callbacks without editing the surviving owner or PCM history");
            check(f->owner->deliverCleanup(obsolete),"Surviving owner still recognizes its explicit cleanup receipt");
            cleanupPrepared.store(f->engine->generation(),std::memory_order_release);
            f->timer=std::make_unique<aion::LoopMeterCleanupTimer>(*f->owner,&cleanupPrepared);f->play();capture();
            check(f->timer->stop(10),"Arm cleanup before host release invalidation");obsolete=f->owner->cleanupRequest();
            cleanupPrepared.store(0,std::memory_order_release);stage=9;break;
        case 9:
            if(wall()-started<.08||f->timer->pending())return;
            check(f->owner->inspect().present&&f->owner->inspect().data==data&&f->owner->cleanupRequest()==obsolete
                &&reading(f->owner->value())==held&&f->engine->clock()==frame,
                "Released generation must prevent armed cleanup callback from changing owner");
            check(!f->timer->stop(11)&&f->owner->cleanupRequest()==obsolete&&!f->timer->pending(),
                "Released adapter rejects a new stop without changing owner or scheduling cleanup");
            cleanupPrepared.store(f->engine->generation(),std::memory_order_release);f->timer->synchronise();
            check(!f->timer->pending()&&f->owner->inspect().present,"Readiness recovery cannot resurrect a cancelled wall deadline");
            check(f->timer->stop(12),"Fresh prepared stop can recover after released callback cancellation");capture();stage=10;break;
        case 10:
            if(f->owner->inspect().present)return;
            check(wall()-started>=.025,"Readiness recovery keeps original 25ms wall delay");stopped();
            f->play();capture();check(f->timer->stop(13),"Arm cleanup before completed-generation mismatch");obsolete=f->owner->cleanupRequest();
            cleanupPrepared.store(f->engine->generation()+1,std::memory_order_release);stage=11;break;
        case 11:
            if(wall()-started<.08||f->timer->pending())return;
            check(f->owner->inspect().present&&f->owner->inspect().data==data&&f->owner->cleanupRequest()==obsolete
                &&reading(f->owner->value())==held&&f->engine->clock()==frame,
                "A different completed generation cannot deliver an old adapter cleanup receipt");
            check(!f->timer->stop(14)&&f->owner->cleanupRequest()==obsolete&&!f->timer->pending()
                &&f->owner->inspect().data==data&&reading(f->owner->value())==held,
                "A mismatched completed generation rejects new stops without owner or deadline mutation");
            cleanupPrepared.store(f->engine->generation(),std::memory_order_release);f->timer->synchronise();
            check(!f->timer->pending()&&f->owner->deliverCleanup(obsolete),"Generation cancellation retains explicit owner metadata without rescheduling");
            beginAcceptedStops();break;
        case 12: {
            if(f->owner->inspect().present)return;
            check(wall()-started>=.025,"Accepted stop cannot detach before the original 25ms wall interval");stopped();
            const auto event=acceptedStop();
            for(auto invalid:{0u,f->engine->generation()+1}){
                cleanupPrepared.store(invalid,std::memory_order_release);
                check(!f->timer->consumeImmediateStop(event)&&!f->timer->pending()&&f->owner->inspect().data==data&&reading(f->owner->value())==held,
                    "Unprepared or mismatched accepted stop cannot consume metadata or rearm cleanup");
            }
            cleanupPrepared.store(f->engine->generation(),std::memory_order_release);
            check(f->timer->consumeImmediateStop(event)&&!f->timer->pending()&&!f->timer->consumeImmediateStop(event),"Detached accepted stop is a valid no-op and duplicate rejection");
            for(auto* e:{f->engine.get(),f->control.get()})check(e->enqueueLoop({aion::LoopAction::clear,19,0,e->generation(),0,0}),"Queue actual clear after native stop teardown");f->cycle();
            aion::MeterGraphEvent cleared;check(f->engine->popMeterGraphEvent(cleared)&&f->timer->consumeClear(cleared)
                &&f->owner->inspect().data==data&&reading(f->owner->value())==held,"Actual clear after timed teardown retains original detached history");
            f->record();check(f->engine->popMeterGraphEvent(cleared)&&f->timer->consumeBuffer(cleared),"Reinstall actual native stop buffer");
            acceptedPlay();check(f->timer->consumeImmediateStop(acceptedStop()),"Arm accepted stop before repeated tap");obsolete=f->owner->cleanupRequest();
            check(f->timer->consumeImmediateStop(acceptedStop())&&f->owner->cleanupRequest().serial!=obsolete.serial&&!f->owner->deliverCleanup(obsolete),"Repeated accepted stop replaces the real JUCE deadline");
            capture();stage=14;break;
        }
        case 14: {
            if(f->owner->inspect().present)return;
            check(wall()-started>=.025,"Repeated accepted stop retains a fresh 25ms minimum");stopped();
            acceptedPlay();const auto oldData=f->owner->inspect().data;held=reading(f->owner->value());
            check(f->timer->consumeImmediateStop(acceptedStop()),"Arm accepted stop before real replacement cancellation");obsolete=f->owner->cleanupRequest();acceptedPlay(false);
            check(f->owner->inspect().data!=oldData&&!f->timer->pending()&&!f->owner->deliverCleanup(obsolete)&&reading(f->owner->value())==held,"Accepted PLAY cancels real stop cleanup without resetting held meter history");
            capture();stage=15;break;
        }
        case 15:
            if(wall()-started<.08)return;
            check(f->owner->inspect().present&&!f->timer->pending()&&f->owner->inspect().data==data&&reading(f->owner->value())==held&&f->engine->clock()==frame,"Cancelled accepted stop cannot later detach replacement graph with frozen audio clock");
            for(auto* e:{f->engine.get(),f->control.get()}){e->configureSync({aion::SyncMode::fixed,120,1,4,4,false});
                check(e->enqueueLoop({aion::LoopAction::stop,19,0,e->generation(),0,0}),"Queue actual scheduled native stop");}f->cycle();
            {aion::MeterGraphEvent event;check(f->engine->popMeterGraphEvent(event)&&event.kind==aion::MeterGraphEventKind::scheduledStop,"Actual native scheduled stop receipt");
                expectedDelay=event.stopLeadSeconds+.05;
                for(auto invalid:{0u,f->engine->generation()+1}){cleanupPrepared.store(invalid,std::memory_order_release);
                    check(!f->timer->consumeScheduledStop(event)&&!f->timer->pending()&&!f->owner->cleanupRequest().serial,"Scheduled adapter readiness rejects before mutation");}
                cleanupPrepared.store(f->engine->generation(),std::memory_order_release);
                check(f->timer->consumeScheduledStop(event)&&f->owner->cleanupRequest().delay==expectedDelay,"Actual scheduled receipt arms original delta plus 50ms");
                obsolete=f->owner->cleanupRequest();check(!f->timer->consumeScheduledStop(event)&&f->owner->cleanupRequest()==obsolete,"Duplicate scheduled receipt cannot postpone JUCE cleanup");}
            capture();stage=16;break;
        case 16:
            if(f->owner->inspect().present){check(f->timer->pending(),"Scheduled cleanup remains pending until actual timer delivery");return;}
            check(wall()-started>=expectedDelay,"Scheduled actual timer cannot teardown before grid delta plus 50ms");stopped();
            for(auto* e:{f->engine.get(),f->control.get()})e->configureSync({aion::SyncMode::off,120,1,4,4,false});acceptedPlay();
            {const auto event=acceptedStop(aion::LoopAction::stopAll);
                for(auto invalid:{0u,f->engine->generation()+1}){cleanupPrepared.store(invalid,std::memory_order_release);
                    check(!f->timer->consumeStopAll(event)&&!f->timer->pending()&&!f->owner->cleanupRequest().serial,"Released/mismatched panic cannot mutate attached graph");}
                cleanupPrepared.store(f->engine->generation(),std::memory_order_release);capture();
                check(f->timer->consumeStopAll(event)&&f->owner->cleanupRequest().delay==.015+.05,"Actual panic arms 65ms JUCE cleanup");
                obsolete=f->owner->cleanupRequest();check(!f->timer->consumeStopAll(event)&&f->owner->cleanupRequest()==obsolete,"Duplicate panic cannot postpone native timer");}
            stage=17;break;
        case 17:
            if(f->owner->inspect().present)return;
            check(wall()-started>=.015+.05,"Actual panic cannot detach before 65ms wall time");stopped();
            check(f->timer->consumeStopAll(acceptedStop(aion::LoopAction::stopAll))&&!f->timer->pending(),"Detached panic is a native ordered no-op");
            acceptedPlay();check(f->timer->consumeStopAll(acceptedStop(aion::LoopAction::stopAll)),"First actual repeated panic");obsolete=f->owner->cleanupRequest();
            {const auto event=acceptedStop(aion::LoopAction::stopAll);capture();check(f->timer->consumeStopAll(event)
                &&f->owner->cleanupRequest().serial!=obsolete.serial&&!f->owner->deliverCleanup(obsolete),"Repeated panic replaces native cleanup while audio already stopped");}
            stage=18;break;
        case 18:
            if(f->owner->inspect().present)return;
            check(wall()-started>=.015+.05,"Repeated panic retains fresh 65ms wall minimum");stopped();
            acceptedPlay();check(f->timer->consumeStopAll(acceptedStop(aion::LoopAction::stopAll)),"Arm panic before replacement");obsolete=f->owner->cleanupRequest();acceptedPlay(false);
            check(!f->timer->pending()&&!f->owner->deliverCleanup(obsolete),"Actual PLAY cancels native panic timer");capture();stage=19;break;
        case 19:
            if(wall()-started<.09)return;
            check(f->owner->inspect().present&&!f->timer->pending()&&f->owner->inspect().data==data&&reading(f->owner->value())==held&&f->engine->clock()==frame,"Cancelled panic cannot detach replacement graph with frozen audio clock");
            capture();check(f->timer->consumeImmediateStop(acceptedStop()),"Arm actual old stop before native queued PLAY");obsolete=f->owner->cleanupRequest();
            for(auto* e:{f->engine.get(),f->control.get()}){e->configureSync({aion::SyncMode::fixed,999,1,1,4,false});
                check(e->enqueueLoop({aion::LoopAction::play,19,0,e->generation(),0,0}),"Queue actual synced play with old timer");}f->cycle();
            {aion::MeterGraphEvent event;check(f->engine->popMeterGraphEvent(event)&&f->timer->consumeQueuedPlay(event)&&f->owner->cleanupRequest()==obsolete&&f->timer->pending(),"Accepted queue retains actual pending JUCE stop request");}
            check(!f->owner->start(0),"Generic start rejects before grid with a real old JUCE timer");f->timer->synchronise();
            check(f->owner->cleanupRequest()==obsolete&&f->timer->pending(),"Rejected generic start cannot cancel or replace the old timer");
            check(f->timer->requestQueuedWindow(f->owner->queuedIntentId(),f->owner->committedBuffer()),"Real old timer retains an explicit window request");f->cycle();
            {aion::MeterGraphEvent event;check(f->engine->popMeterGraphEvent(event)&&event.kind==aion::MeterGraphEventKind::started,"Real timer raw reset notice");
                check(f->engine->popMeterGraphEvent(event)&&f->timer->consumeQueuedWindowResult(event)&&f->owner->queuedWindowReady()
                    &&f->owner->cleanupRequest()==obsolete&&f->timer->pending(),"Typed acceptance retains the real original cleanup deadline");}
            for(auto* e:{f->engine.get(),f->control.get()})check(e->enqueueLoop({aion::LoopAction::stopAll,19,0,e->generation(),0,0}),"Cancel actual queued PLAY with panic");f->cycle();
            {aion::MeterGraphEvent event;check(f->engine->popMeterGraphEvent(event)&&f->timer->consumeQueuedCancel(event)&&f->owner->cleanupRequest()==obsolete&&f->timer->pending(),"Accepted queued panic preserves real old stop timer");}
            frame=f->engine->clock();stage=20;break;
        case 20:
            if(f->owner->inspect().present)return;
            check(wall()-started>=.025,"Retained old stop cannot teardown before original 25ms minimum");stopped();
            check(!f->owner->queuedIntentId(),"Queued panic removed the accepted intent before old timer delivery");
            for(auto* e:{f->engine.get(),f->control.get()})e->configureSync({aion::SyncMode::off,120,1,4,4,false});
            acceptedPlay();capture();check(f->timer->consumeImmediateStop(acceptedStop()),"Arm a fresh real cleanup before post-grid pending intent");obsolete=f->owner->cleanupRequest();
            for(auto* e:{f->engine.get(),f->control.get()}){
                e->configureSync({aion::SyncMode::fixed,999,1,1,4,false});
                check(e->enqueueLoop({aion::LoopAction::play,19,0,e->generation(),0,0}),"Queue real post-grid panic timer fixture");
            }
            f->cycle();
            {aion::MeterGraphEvent event;check(f->engine->popMeterGraphEvent(event)&&f->timer->consumeQueuedPlay(event),"Deliver actual pending post-grid intent");}
            for(unsigned n=0;n<1000&&f->view->tracks[19].state==aion::LoopState::queued;++n)f->cycle();
            check(f->view->tracks[19].state==aion::LoopState::playing&&f->owner->inspect().state==aion::MeterLoopState::queued
                &&f->owner->cleanupRequest()==obsolete&&f->timer->pending(),"Real old JUCE timer remains independent of pending intent after audio grid");
            check(!f->owner->start(0),"Generic start rejects after grid with a real old JUCE timer");f->timer->synchronise();
            check(f->owner->cleanupRequest()==obsolete&&f->timer->pending(),"Post-grid rejection leaves the original real deadline intact");
            for(auto* e:{f->engine.get(),f->control.get()})check(e->enqueueLoop({aion::LoopAction::stopAll,19,0,e->generation(),0,0}),"Cancel late queued intent without graph callback");
            f->cycle();
            {aion::MeterGraphEvent event;check(f->engine->popMeterGraphEvent(event)&&f->timer->consumeQueuedCancel(event)
                &&f->owner->cleanupRequest()==obsolete&&f->timer->pending(),"Post-grid cancellation retains the real original cleanup timer");}
            frame=f->engine->clock();stage=21;break;
        case 21:
            if(f->owner->inspect().present)return;
            check(wall()-started>=.025,"Post-grid cancellation cannot shorten the original real 25ms cleanup");stopped();
            check(!f->owner->queuedIntentId(),"Late queued panic removes pending intent before frozen-clock teardown");
            for(auto* e:{f->engine.get(),f->control.get()})e->configureSync({aion::SyncMode::off,120,1,4,4,false});
            acceptedPlay();capture();check(f->timer->consumeImmediateStop(acceptedStop()),"Arm old real timer before late STOP NOW");obsolete=f->owner->cleanupRequest();
            for(auto* e:{f->engine.get(),f->control.get()}){
                e->configureSync({aion::SyncMode::fixed,999,1,1,4,false});
                check(e->enqueueLoop({aion::LoopAction::play,19,0,e->generation(),0,0}),"Queue late STOP NOW real timer fixture");
            }
            f->cycle();
            {aion::MeterGraphEvent event;check(f->engine->popMeterGraphEvent(event)&&f->timer->consumeQueuedPlay(event),"Deliver pending STOP NOW intent to real adapter");}
            for(unsigned n=0;n<1000&&f->view->tracks[19].state==aion::LoopState::queued;++n)f->cycle();
            check(f->view->tracks[19].state==aion::LoopState::playing&&f->owner->inspect().state==aion::MeterLoopState::queued,"Real STOP NOW fixture crosses grid without message birth");
            for(auto* e:{f->engine.get(),f->control.get()}){
                if(lateStopSyncOff)e->configureSync({aion::SyncMode::off,999,1,1,4,false});
                const auto action=lateStopSyncOff?aion::LoopAction::stop:aion::LoopAction::stopImmediate;
                check(e->enqueueLoop({action,19,0,e->generation(),0,0}),"Stop pending intent after grid");
            }
            f->cycle();
            started=wall();
            {aion::MeterGraphEvent event;check(f->engine->popMeterGraphEvent(event)&&f->timer->consumeQueuedStop(event)
                &&f->owner->cleanupRequest().serial!=obsolete.serial&&!f->owner->deliverCleanup(obsolete)&&f->timer->pending(),"Late STOP NOW replaces actual JUCE deadline and rejects old timer");}
            frame=f->engine->clock();stage=22;break;
        case 22:
            if(f->owner->inspect().present)return;
            check(wall()-started>=.025,"Late STOP NOW retains real 25ms cleanup minimum");stopped();
            check(!f->owner->queuedIntentId(),"Late STOP NOW clears intent before frozen-clock teardown");
            if(!lateStopSyncOff){lateStopSyncOff=true;stage=21;break;}
            for(auto* e:{f->engine.get(),f->control.get()})e->configureSync({aion::SyncMode::off,120,1,4,4,false});
            acceptedPlay();capture();check(f->timer->consumeImmediateStop(acceptedStop()),"Arm cleanup before a delayed window transaction");obsolete=f->owner->cleanupRequest();
            for(auto* e:{f->engine.get(),f->control.get()}){
                e->configureSync({aion::SyncMode::fixed,999,1,1,4,false});
                check(e->enqueueLoop({aion::LoopAction::play,19,0,e->generation(),0,0}),"Queue before delayed result");
            }
            f->cycle();
            {aion::MeterGraphEvent event;check(f->engine->popMeterGraphEvent(event)&&f->timer->consumeQueuedPlay(event),"Apply intent before delayed result");}
            check(f->timer->requestQueuedWindow(f->owner->queuedIntentId(),f->owner->committedBuffer()),"Stage window without processing audio");
            frame=f->engine->clock();stage=23;break;
        case 23:
            if(f->owner->inspect().present)return;
            check(wall()-started>=.025,"Pending window cannot postpone the original old cleanup");stopped();
            check(f->owner->queuedWindowTransaction().status==aion::QueuedWindowStatus::obsolete&&!f->owner->queuedWindowReady(),"Old JUCE cleanup retires pending transaction without queue cancellation");
            f->cycle();
            {aion::MeterGraphEvent event;check(f->engine->popMeterGraphEvent(event)&&event.kind==aion::MeterGraphEventKind::started,"Delayed audio accepts conditional reset after old cleanup");
                check(f->engine->popMeterGraphEvent(event)&&event.kind==aion::MeterGraphEventKind::queuedWindowAccepted&&!f->timer->consumeQueuedWindowResult(event),"Retired typed acceptance cannot resurrect a graph or timer");}
            check(!f->owner->inspect().present&&!f->timer->pending()&&f->owner->inspect().data==data&&reading(f->owner->value())==held,"Delayed acceptance retains detached data/history");
            if(f->block==64){begin(128);break;}
            stopTimer();juce::MessageManager::getInstance()->stopDispatchLoop();break;
        }
    }
};
}

void meterCleanupTimerTests(bool& audioWatch,std::atomic<int>& allocations){
    adapterReadinessTests(audioWatch);
    immediatePlayAdapterTests(audioWatch);
    clearDispatcherTests(audioWatch);
    processorBufferReceiptTests(audioWatch);
    processorDueDeliveryTests(audioWatch);
    panicDispatcherTests(audioWatch);
    queuedAdapterTests(audioWatch);
    std::mutex mutex;std::condition_variable changed;bool finished=false;
    std::thread watchdog([&]{std::unique_lock lock(mutex);if(!changed.wait_for(lock,std::chrono::seconds(30),[&]{return finished;})){
        std::cerr<<"Native cleanup timer message-loop watchdog expired"<<std::endl;std::_Exit(1);}});
    {Runner runner(audioWatch);juce::MessageManager::getInstance()->runDispatchLoop();}
    {std::lock_guard lock(mutex);finished=true;}changed.notify_one();watchdog.join();
    check(allocations.load()==0,"Native timer actual Engine callbacks have zero tracked new/delete");
    std::cout<<"PASS: real JUCE message-loop cleanup at 64/128, 25/50ms, late delivery, cancellation/replacement, queued-play retention, prepare/destruction and exact PCM; zero tracked callback new/delete\n";
    std::cout<<"PASS: actual DEL receipt cancels pending JUCE cleanup and preserves same-generation re-recorded graph at 64/128\n";
    std::cout<<"PASS: production processor-owned twenty-track meter routing, 32-work request budget, autonomous editor-free timer, host-thread release/prepare, idle wakeup retention, frozen release, loss latch and fresh recovery at 64/128; exact nonzero stereo PCM and unchanged state\n";
    std::cout<<"PASS: prepared/released/unprepared host shutdown, nested message ownership and direct message-thread destruction at 64/128; actual native member and AudioProcessor base parameter destructors retain message ownership, after nonzero PCM and real service callbacks\n";
    std::cout<<"PASS: production twenty-track cleanup adapters are created by message service after host construction/prepare, retained across release/loss/reprepare, routed through the sole clear dispatcher and destroyed under host-held message ownership; armed adapter destruction preserves its surviving owner at 64/128\n";
    std::cout<<"PASS: prepared-generation cleanup guard cancels released/mismatched callbacks without owner or PCM-history mutation, rejects released stops, never resurrects cancelled deadlines and recovers on fresh 25ms stops at 64/128\n";
    std::cout<<"PASS: actual accepted stop receipts drive 25ms JUCE teardown with frozen audio clocks, repeated stop replacement, prepared rejection, detached clear history and real accepted PLAY cancellation at 64/128; exact paired PCM and zero tracked callback new/delete\n";
    std::cout<<"PASS: actual scheduled STOP receipts route through production prepared adapters; real JUCE timers retain delta plus 50ms with frozen audio clocks, duplicate/readiness rejection and second STOP override at 64/128\n";
    std::cout<<"PASS: actual STOP ALL receipts drive 65ms JUCE teardown with frozen audio, repeated replacement, detached no-op, readiness/duplicate rejection and accepted PLAY cancellation at 64/128; zero tracked callback new/delete\n";
    std::cout<<"PASS: actual queued PLAY and queued panic retain the real old JUCE stop timer until frozen-clock teardown at 64/128; no analyser birth and zero tracked callback new/delete\n";
    std::cout<<"PASS: actual post-grid queued panic preserves the original real 25ms JUCE cleanup and frozen-clock data/history at 64/128\n";
    std::cout<<"PASS: actual post-grid STOP NOW replaces real JUCE cleanup with 25ms, obsolete timer rejection and frozen-clock history at 64/128\n";
    std::cout<<"PASS: actual normal STOP after sync off replaces real JUCE cleanup with 25ms and frozen-clock history at 64/128\n";
    std::cout<<"PASS: generic queued start rejection retains real JUCE cleanup before/after grid and frozen-clock history at 64/128\n";
    std::cout<<"PASS: queued window acceptance retains actual old JUCE deadline; cleanup-before-result prevents resurrection with frozen-clock history at 64/128\n";
}
