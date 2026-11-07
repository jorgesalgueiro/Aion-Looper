#include "core/LoopMeterLifecycle.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
extern thread_local bool watch;
extern std::atomic<int> allocations;
extern std::atomic<bool> failWorkerAllocation;
extern std::atomic<bool> holdWorkerAllocation;
extern std::atomic<unsigned> heldWorkerAllocations;
namespace {
void check(bool ok,const char* label){if(!ok){std::cerr<<label<<'\n';std::exit(1);}}
auto reading(const aion::MeterReading& r){return std::array<double,7>{r.rms,r.peak,r.linearPeak,r.peakDb,r.rmsPercent,r.peakPercent,r.clipped?1.:0.};}
struct Fixture {
    std::unique_ptr<aion::Engine> engine=std::make_unique<aion::Engine>(aion::MeterEventScope::committedBuffers);
    std::unique_ptr<aion::Engine> control=std::make_unique<aion::Engine>();
    std::unique_ptr<aion::EngineView> view=std::make_unique<aion::EngineView>();
    std::unique_ptr<aion::MeterWindowEvent> packet=std::make_unique<aion::MeterWindowEvent>();
    unsigned block,request=0;
    explicit Fixture(unsigned size,aion::MeterEventScope scope=aion::MeterEventScope::committedBuffers):engine(std::make_unique<aion::Engine>(scope)),block(size){prepare();}
    void prepare(){for(auto* e:{engine.get(),control.get()}){e->prepare(48000,81920,{true,48000,0});e->configureSync({aion::SyncMode::off,120,1,4,4,false});e->setMonitoring(false);e->setTarget(1);}}
    void command(aion::LoopAction action,unsigned id=19){++request;for(auto* e:{engine.get(),control.get()})check(e->enqueueLoop({action,id,request,e->generation(),0,0}),"Queue paired buffer fixture action");}
    void cycle(bool offline=false,aion::ProcessorClockObservation entry={}){
        std::array<float,128> l{},r{},cl{},cr{};
        for(unsigned i=0;i<block;++i){l[i]=.3f*static_cast<float>(std::sin((engine->clock()+i)*.037));r[i]=-.5f*l[i];}cl=l;cr=r;
        float* channels[]={l.data(),r.data()},*other[]={cl.data(),cr.data()};
        watch=true;engine->process(channels,2,static_cast<int>(block),{},offline,entry);control->process(other,2,static_cast<int>(block),{},offline,entry);watch=false;
        check(l==cl&&r==cr,"Buffer receipts preserve exact paired stereo PCM at 64/128");
        while(engine->popView(*view)){}
    }
    void record(unsigned id=19){command(aion::LoopAction::record,id);for(unsigned n=0;n<512/block;++n)cycle();command(aion::LoopAction::finish,id);cycle(true);}
    aion::MeterGraphEvent next(aion::MeterGraphEventKind kind,unsigned id=19){
        aion::MeterGraphEvent event;const bool found=engine->popMeterGraphEvent(event);
        if(!found||event.kind!=kind||event.track!=id){
            std::cerr<<"Actual audio owner emits the expected ordered buffer/window fact: expected "<<static_cast<unsigned>(kind)
                <<"/"<<id<<", found "<<found<<" "<<static_cast<unsigned>(event.kind)<<"/"<<event.track<<", frame "<<engine->clock()<<", request "<<request<<'\n';std::exit(1);
        }return event;
    }
};
}
void meterPlaybackTests(){
    using namespace aion;
    for(unsigned block:{64u,128u}){
        Fixture f(block,MeterEventScope::acceptedPlayback);LoopMeterLifecycle owner(*f.engine,19);
        f.record();auto installed=f.next(MeterGraphEventKind::bufferInstalled);
        check(owner.consumeBuffer(installed)&&!owner.inspect().present,"Auto-play capture remains outside immediate play origin mapping");
        f.command(LoopAction::stopImmediate);f.command(LoopAction::play);const auto start=f.engine->clock();f.cycle();
        auto played=f.next(MeterGraphEventKind::immediatePlay);
        check(played.frame==start&&played.bufferId==installed.bufferId&&played.ticket==1
            &&f.engine->loopMeterGraphCurrent(played.generation,19,played.ticket),"Accepted immediate play creates its raw window at the audio-owner sample boundary");
        check(owner.consumeImmediatePlay(played)&&owner.inspect().present&&owner.binding().ticket==played.ticket,
            "Accepted immediate play binds message graph without a second audio reset command");
        check(!owner.consumeImmediatePlay(played),"Duplicate accepted play cannot reset analyser history");
        for(unsigned n=0;n<2048/block;++n){check(owner.request(),"Request accepted immediate graph PCM");f.cycle();
            check(f.engine->popLoopMeterWindow(*f.packet),"Receive accepted immediate graph PCM");
            if(n==0)check(std::all_of(f.packet->samples.begin(),f.packet->samples.end()-2*block,[](float sample){return sample==0;})
                &&std::any_of(f.packet->samples.end()-2*block,f.packet->samples.end(),[](float sample){return sample!=0;}),
                "Accepted immediate play window zero-prefix excludes PCM from the previous graph");
            const double t=static_cast<double>(f.view->frame)/48000;
            check(owner.observe(*f.packet,*f.view,f.view->callbackClock,{t,t}),"Accepted graph observes its actual bound PCM");}
        const auto identity=owner.inspect();const auto held=reading(owner.value());check(owner.value().peak>0,"Accepted playback seeds nonzero real meter history");
        f.command(LoopAction::retrigger);for(unsigned n=0;n<512/block;++n)f.cycle();
        MeterGraphEvent event;
        check(!f.engine->popMeterGraphEvent(event)&&owner.inspect().analyser==identity.analyser
            &&f.engine->loopMeterGraphCurrent(played.generation,19,played.ticket),"Healthy RTRG retains graph and raw window identity");
        f.command(LoopAction::play);f.cycle();check(f.view->rejected&&!f.engine->popMeterGraphEvent(event),"Rejected already-playing PLAY publishes no accepted birth");
        check(owner.stop(1)&&owner.cleanupRequest().serial,"Arm old graph cleanup before replacement receipt");const auto obsolete=owner.cleanupRequest();
        f.command(LoopAction::stopImmediate);f.command(LoopAction::play);f.cycle();
        check(owner.consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop)),"Consume ordered actual stop before immediate replacement");
        const auto pendingStop=owner.cleanupRequest();auto replacement=f.next(MeterGraphEventKind::immediatePlay);
        auto wrong=replacement;++wrong.bufferId;
        check(!owner.consumeImmediatePlay(wrong)&&owner.cleanupRequest()==pendingStop&&owner.inspect().analyser==identity.analyser,
            "Wrong-buffer immediate play leaves analyser and pending cleanup unchanged");
        check(owner.consumeImmediatePlay(replacement)&&owner.inspect().analyser!=identity.analyser&&owner.inspect().data!=identity.data
            &&reading(owner.value())==held&&!owner.cleanupRequest().serial&&!owner.deliverCleanup(obsolete),
            "Accepted immediate replacement creates fresh analyser/data, retains history and cancels old cleanup");
        check(!owner.consumeBuffer(installed),"Older buffer receipt cannot reorder an accepted graph birth");
        // Two births can be queued before the message service. Their ordering is
        // still meaningful even though only the final raw window is current.
        for(unsigned n=0;n<2;++n){f.command(LoopAction::stopImmediate);f.command(LoopAction::play);}f.cycle();
        check(owner.consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop)),"Consume first batched stop");
        auto first=f.next(MeterGraphEventKind::immediatePlay);
        check(owner.consumeImmediatePlay(first)&&!f.engine->loopMeterGraphCurrent(first.generation,19,first.ticket),
            "Ordered delayed birth receipt may apply metadata without accepting obsolete PCM");
        const auto oldData=owner.inspect().data;
        check(owner.consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop)),"Consume second batched stop");
        auto second=f.next(MeterGraphEventKind::immediatePlay);
        check(first.ticket+1==second.ticket&&owner.consumeImmediatePlay(second)&&owner.inspect().data!=oldData&&owner.binding().ticket==second.ticket,
            "Later queued birth owns the final raw window and fresh analyser identity");
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,120,1,4,4,false});
        f.command(LoopAction::stopImmediate);f.command(LoopAction::retrigger);f.cycle();
        check(owner.consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop)),"Consume stop before stopped synced RTRG");
        auto retriggered=f.next(MeterGraphEventKind::immediatePlay);
        check(f.view->tracks[19].state==LoopState::playing&&owner.consumeImmediatePlay(retriggered),"Stopped RTRG bypasses grid and explicitly creates an immediate graph");
        f.command(LoopAction::stopImmediate);f.command(LoopAction::play);f.cycle();
        check(owner.consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop)),"Consume explicit stop before queued PLAY");
        check(f.view->tracks[19].state==LoopState::queued&&owner.consumeQueuedPlay(f.next(MeterGraphEventKind::queuedPlay))&&!f.engine->popMeterGraphEvent(event),"Queued PLAY carries intent without using the immediate graph creation path");
        unsigned cycles=0;while(f.view->tracks[19].state==LoopState::queued&&++cycles<2000)f.cycle();
        check(f.view->tracks[19].state==LoopState::playing&&!f.engine->popMeterGraphEvent(event),"Audible grid transition is not an inferred graph birth");
        f.command(LoopAction::clear);f.cycle();event=f.next(MeterGraphEventKind::cleared);
        check(owner.consumeClear(event)&&!owner.inspect().data&&!owner.committedBuffer(),"Accepted immediate graph clears through existing typed route");
        check(!owner.consumeImmediatePlay(retriggered),"Clear invalidates older accepted graph births");
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::off,120,1,4,4,false});
        f.record();installed=f.next(MeterGraphEventKind::bufferInstalled);check(owner.consumeBuffer(installed),"Re-record accepted graph fixture buffer");
        f.prepare();check(owner.reset(),"Prepare resets accepted playback owner");installed=f.next(MeterGraphEventKind::bufferInstalled);check(owner.consumeBuffer(installed),"Prepare retained buffer ownership");
        check(!owner.consumeImmediatePlay(second),"Prior-generation accepted birth cannot attach an analyser");
        // Exhaustion must invalidate telemetry instead of recycling a ticket.
        LoopCommand exhausted{LoopAction::meterGraphStart,19,0,f.engine->generation(),0,0};exhausted.meterGraphTicket=0xffffffffu;
        check(f.engine->enqueueLoop(exhausted),"Queue last full-width raw window ticket");f.cycle();(void)f.next(MeterGraphEventKind::started);
        f.command(LoopAction::stopImmediate);f.command(LoopAction::retrigger);f.cycle();
        check(!f.engine->meterGraphEventsCurrent()&&!f.engine->popMeterGraphEvent(event)&&f.view->tracks[19].state==LoopState::playing,
            "Exhausted graph tickets fail closed without rejecting or changing audible playback");
    }
    check(allocations.load()==0,"Accepted immediate play callbacks have zero tracked new/delete");
    std::cout<<"PASS: accepted immediate PLAY/stopped RTRG graph receipts, healthy RTRG retention, exact bound PCM, replacement/old cleanup cancellation, ordered delayed delivery, queued non-inference, clear/prepare and ticket exhaustion at 64/128; zero tracked callback new/delete\n";
}
void meterStopTests(){
    using namespace aion;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u}){
        Fixture f(block,MeterEventScope::acceptedPlayback);LoopMeterLifecycle owner(*f.engine,id);
        f.record(id);check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Accept actual stop fixture clip");
        const auto play=[&]{f.command(LoopAction::stopImmediate,id);f.command(LoopAction::play,id);f.cycle();
            return f.next(MeterGraphEventKind::immediatePlay,id);};
        auto played=play();check(owner.consumeImmediatePlay(played),"Actual immediate graph before stop");
        for(unsigned n=0;n<2048/block;++n){check(owner.request(),"Request stop fixture PCM");f.cycle();check(f.engine->popLoopMeterWindow(*f.packet),"Receive stop fixture PCM");
            const double t=static_cast<double>(f.view->frame)/48000;check(owner.observe(*f.packet,*f.view,f.view->callbackClock,{t,t}),"Seed stop history from actual Engine PCM");}
        const auto identity=owner.inspect();const auto history=reading(owner.value());check(owner.value().peak>0,"Actual stop history is nonzero");
        const auto frame=f.engine->clock();f.command(LoopAction::stopImmediate,id);f.cycle();auto stopped=f.next(MeterGraphEventKind::immediateStop,id);
        check(stopped.frame==frame&&stopped.ticket==played.ticket&&stopped.bufferId==played.bufferId&&!stopped.retiredTicket
            &&f.engine->loopMeterGraphCurrent(stopped.generation,id,played.ticket),"Accepted stop preserves the actual window ticket and names the committed graph clip");
        auto wrong=stopped;++wrong.ticket;check(!owner.consumeImmediateStop(wrong)&&!owner.cleanupRequest().serial,"Wrong graph stop cannot arm cleanup");
        wrong=stopped;++wrong.bufferId;check(!owner.consumeImmediateStop(wrong)&&owner.inspect().analyser==identity.analyser,"Wrong clip stop cannot change graph lifetime");
        check(owner.consumeImmediateStop(stopped)&&owner.inspect().state==MeterLoopState::stopping&&owner.inspect().analyser==identity.analyser
            &&owner.inspect().data==identity.data&&reading(owner.value())==history&&owner.cleanupRequest().delay==.025,
            "Accepted immediate stop arms 25ms without detaching analyser or changing history");
        const auto first=owner.cleanupRequest();
        check(!owner.consumeImmediateStop(stopped)&&owner.cleanupRequest()==first,"Duplicate stop cannot postpone its cleanup");
        for(unsigned n=0;n<8192/block;++n)f.cycle();
        check(owner.inspect().present&&owner.cleanupRequest()==first,"Advancing audio sample time cannot deliver a message-owned cleanup");
        f.command(LoopAction::stopImmediate,id);f.cycle();auto repeated=f.next(MeterGraphEventKind::immediateStop,id);
        check(owner.consumeImmediateStop(repeated)&&owner.cleanupRequest().serial==first.serial+1&&!owner.deliverCleanup(first),
            "Repeated explicit stop replaces pending cleanup after audio has already stopped");
        const auto last=owner.cleanupRequest();check(owner.deliverCleanup(last)&&!owner.inspect().present&&owner.inspect().data==identity.data
            &&reading(owner.value())==history,"Explicit cleanup detaches while retaining data and meter history");
        f.command(LoopAction::stop,id);f.cycle();auto detached=f.next(MeterGraphEventKind::immediateStop,id);
        check(owner.consumeImmediateStop(detached)&&!owner.cleanupRequest().serial&&!owner.inspect().present&&owner.inspect().data==identity.data
            &&reading(owner.value())==history,"A stop after actual teardown is a valid no-op and never rearms cleanup");
        f.command(LoopAction::clear,id);f.cycle();check(owner.consumeClear(f.next(MeterGraphEventKind::cleared,id))&&!owner.inspect().buffer
            &&owner.inspect().data==identity.data&&reading(owner.value())==history,"Actual clear after stopped teardown retains the original detached-history quirk");
        f.command(LoopAction::stopImmediate,id);f.cycle();MeterGraphEvent event;
        check(!f.engine->popMeterGraphEvent(event)&&!owner.consumeImmediateStop(stopped),"Clear cancels the stop origin and rejects old stop receipts");
        f.record(id);check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Install new take after detached clear");
        played=play();check(owner.consumeImmediatePlay(played),"New graph after cleared stop origin");
        f.command(LoopAction::stopImmediate,id);f.command(LoopAction::play,id);f.cycle();stopped=f.next(MeterGraphEventKind::immediateStop,id);
        check(owner.consumeImmediateStop(stopped),"Ordered stop before same-callback replacement");const auto pending=owner.cleanupRequest();
        played=f.next(MeterGraphEventKind::immediatePlay,id);
        check(owner.consumeImmediatePlay(played)&&!owner.cleanupRequest().serial&&!owner.deliverCleanup(pending)&&!owner.consumeImmediateStop(stopped),
            "Same-callback accepted PLAY cancels prior stop and rejects retained stop metadata");
        const auto beforeEdit=owner.inspect().data;
        f.command(LoopAction::normalize,id);f.command(LoopAction::stopImmediate,id);f.cycle();
        stopped=f.next(MeterGraphEventKind::immediateStop,id);
        check(owner.consumeImmediateStop(stopped)&&owner.inspect().data==beforeEdit,"Native finalizing state cannot suppress the attached graph's explicit stop");
        const auto editStop=owner.cleanupRequest();f.cycle(true);
        check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id))&&owner.cleanupRequest()==editStop&&owner.inspect().data==beforeEdit,
            "Accepted worker buffer after STOP retains pending cleanup and graph identity");
        f.command(LoopAction::play,id);f.cycle();played=f.next(MeterGraphEventKind::immediatePlay,id);
        check(owner.consumeImmediatePlay(played)&&!owner.cleanupRequest().serial,"Accepted restart after stopped worker completion cancels cleanup");
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,120,1,4,4,false});
        f.command(LoopAction::stop,id);f.cycle();auto scheduled=f.next(MeterGraphEventKind::scheduledStop,id);
        check(f.view->tracks[id].state==LoopState::stopping&&!owner.consumeImmediateStop(scheduled)
            &&owner.consumeScheduledStop(scheduled)&&owner.cleanupRequest().delay==scheduled.stopLeadSeconds+.05,
            "Scheduled STOP uses its distinct delta plus 50ms policy, never immediate cleanup");
        f.command(LoopAction::stopImmediate,id);f.cycle();stopped=f.next(MeterGraphEventKind::immediateStop,id);
        check(owner.consumeImmediateStop(stopped)&&owner.cleanupRequest().delay==.025,"Explicit immediate stop can override a scheduled audio stop");
        f.command(LoopAction::retrigger,id);f.cycle();played=f.next(MeterGraphEventKind::immediatePlay,id);check(owner.consumeImmediatePlay(played),"Restart after immediate stop override");
        f.command(LoopAction::stopAll,id);f.cycle();event=f.next(MeterGraphEventKind::stopAll,id);
        check(owner.consumeStopAll(event)&&owner.cleanupRequest().delay==.015+.05,"STOP ALL uses its distinct 65ms policy");
        LoopCommand raw{LoopAction::meterGraphStart,id,0,f.engine->generation(),0,0};raw.meterGraphTicket=played.ticket+1;
        check(f.engine->enqueueLoop(raw),"Queue explicit raw window replacement");f.cycle();(void)f.next(MeterGraphEventKind::started,id);
        f.command(LoopAction::stopImmediate,id);f.cycle();check(!f.engine->popMeterGraphEvent(event),"Raw component window reset cannot fabricate an accepted playback stop origin");
        for(unsigned n=0;n<256;++n){f.command(LoopAction::clear,id);f.cycle();}
        check(!f.engine->meterGraphEventsCurrent()&&!owner.consumeImmediateStop(stopped),"Lost generation refuses retained stop receipts");
        unsigned budget=255;check(!f.engine->popMeterGraphEvent(event,f.engine->generation(),budget)&&!budget,"Stop stream loss drain stays bounded");
        f.prepare();check(owner.reset()&&!owner.consumeImmediateStop(stopped)&&!owner.inspect().present&&!owner.cleanupRequest().serial,"Prepare cancels old stop and graph lifetime");
    }
    check(allocations.load()==0,"Accepted stop callback paths have zero tracked new/delete");
    // Fractional grids must retain the unrounded seconds delta, not derive it
    // from the integer audible deadline or treat that deadline as wall time.
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(double bpm:{120.,137.3,999.}){
        Fixture f(block,MeterEventScope::acceptedPlayback);LoopMeterLifecycle owner(*f.engine,id);
        f.record(id);check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Scheduled fixture commits actual clip");
        f.command(LoopAction::stopImmediate,id);f.command(LoopAction::play,id);f.cycle();
        auto played=f.next(MeterGraphEventKind::immediatePlay,id);check(owner.consumeImmediatePlay(played),"Scheduled fixture uses actual immediate origin");
        for(unsigned n=0;n<2048/block;++n){check(owner.request(),"Scheduled history request");f.cycle();check(f.engine->popLoopMeterWindow(*f.packet),"Scheduled history PCM");
            const double t=static_cast<double>(f.view->frame)/48000;check(owner.observe(*f.packet,*f.view,f.view->callbackClock,{t,t}),"Scheduled history observation");}
        const auto identity=owner.inspect();const auto held=reading(owner.value());check(owner.value().peak>0,"Scheduled history must be nonzero");
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,bpm,1,1,4,false});
        SyncClock grid;grid.reset(48000);grid.configure({SyncMode::fixed,bpm,1,1,4,false},f.engine->clock());
        const auto frame=f.engine->clock();const double lead=grid.nextGridSeconds(frame)-static_cast<double>(frame)/48000;
        f.command(LoopAction::stop,id);f.cycle();auto event=f.next(MeterGraphEventKind::scheduledStop,id);
        check(event.frame==frame&&event.stopLeadSeconds==lead&&event.ticket==played.ticket&&event.bufferId==played.bufferId
            &&f.engine->loopMeterGraphCurrent(event.generation,id,event.ticket),"Scheduled fact preserves exact grid delta, clip and raw token at acceptance");
        for(double invalid:{0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
            auto wrong=event;wrong.stopLeadSeconds=invalid;check(!owner.consumeScheduledStop(wrong)&&!owner.cleanupRequest().serial,"Invalid scheduled delta cannot mutate graph or cleanup");}
        auto wrong=event;++wrong.ticket;check(!owner.consumeScheduledStop(wrong),"Scheduled wrong ticket rejected");
        wrong=event;++wrong.bufferId;check(!owner.consumeScheduledStop(wrong),"Scheduled wrong clip rejected");
        wrong=event;++wrong.generation;check(!owner.consumeScheduledStop(wrong),"Scheduled wrong generation rejected");
        check(!owner.consumeImmediateStop(event)&&owner.consumeScheduledStop(event)&&owner.cleanupRequest().delay==lead+.05
            &&owner.inspect().analyser==identity.analyser&&reading(owner.value())==held,"Scheduled stop retains analyser/history with exact cleanup interval");
        const auto pending=owner.cleanupRequest();check(!owner.consumeScheduledStop(event)&&owner.cleanupRequest()==pending,"Scheduled duplicate never postpones cleanup");
        unsigned cycles=0;while(f.view->tracks[id].state==LoopState::stopping&&++cycles<1000)f.cycle();
        MeterGraphEvent extra;check(f.view->tracks[id].state==LoopState::stopped&&!f.engine->popMeterGraphEvent(extra)
            &&owner.inspect().present&&owner.cleanupRequest()==pending,"Audible scheduled completion neither republishes stop nor detaches message graph");
        check(owner.deliverCleanup(pending)&&!owner.inspect().present&&owner.inspect().data==identity.data&&reading(owner.value())==held,"Scheduled cleanup retains detached history");
        check(!owner.consumeScheduledStop(event),"Retained scheduled duplicate rejected after teardown");
        f.command(LoopAction::retrigger,id);f.cycle();played=f.next(MeterGraphEventKind::immediatePlay,id);check(owner.consumeImmediatePlay(played),"Stopped retrigger establishes replacement origin");
        f.command(LoopAction::stop,id);f.cycle();event=f.next(MeterGraphEventKind::scheduledStop,id);check(owner.consumeScheduledStop(event),"Arm second scheduled stop");
        const auto second=owner.cleanupRequest();f.command(LoopAction::stop,id);f.cycle();
        check(owner.consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop,id))&&owner.cleanupRequest().delay==.025&&!owner.deliverCleanup(second),"Second STOP overrides scheduled cleanup with immediate policy");
        f.command(LoopAction::retrigger,id);f.cycle();check(owner.consumeImmediatePlay(f.next(MeterGraphEventKind::immediatePlay,id))&&!owner.cleanupRequest().serial,"Replacement cancels overridden cleanup");
        f.command(LoopAction::stop,id);f.cycle();event=f.next(MeterGraphEventKind::scheduledStop,id);check(owner.consumeScheduledStop(event),"Arm before typed clear");
        f.command(LoopAction::clear,id);f.cycle();check(owner.consumeClear(f.next(MeterGraphEventKind::cleared,id))&&!owner.inspect().data&&!owner.cleanupRequest().serial&&!owner.consumeScheduledStop(event),"Attached clear cancels scheduled cleanup and resets history");
    }
    check(allocations.load()==0,"Scheduled STOP callback paths have zero tracked new/delete");
    std::cout<<"PASS: accepted immediate and scheduled STOP receipts, exact unrounded grid delta plus 50ms, overrides, retained window/clip/history, actual audible completion, malformed/stale rejection and replacement/clear at tracks 0/19, 64/128, 120/137.3/999 BPM; exact paired stereo PCM and zero tracked callback new/delete\n";
}
void meterPanicTests(){
    using namespace aion;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u}){
        Fixture f(block,MeterEventScope::acceptedPlayback);LoopMeterLifecycle owner(*f.engine,id);
        f.record(id);check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Panic fixture commits actual clip");
        // Capture auto-play has no mapped analyser origin.
        f.command(LoopAction::stopAll,id);f.cycle();MeterGraphEvent event;
        check(!f.engine->popMeterGraphEvent(event),"Panic cannot fabricate capture auto-play graph ownership");
        f.command(LoopAction::play,id);f.cycle();auto played=f.next(MeterGraphEventKind::immediatePlay,id);
        check(owner.consumeImmediatePlay(played),"Panic fixture uses actual immediate-play graph");
        for(unsigned n=0;n<2048/block;++n){check(owner.request(),"Request panic history PCM");f.cycle();check(f.engine->popLoopMeterWindow(*f.packet),"Receive panic history PCM");
            const double t=static_cast<double>(f.view->frame)/48000;check(owner.observe(*f.packet,*f.view,f.view->callbackClock,{t,t}),"Seed panic history from actual PCM");}
        const auto identity=owner.inspect();const auto held=reading(owner.value());check(owner.value().peak>0,"Panic history is nonzero");
        const auto frame=f.engine->clock();f.command(LoopAction::stopAll,id);f.cycle();event=f.next(MeterGraphEventKind::stopAll,id);
        check(event.frame==frame&&event.ticket==played.ticket&&event.bufferId==played.bufferId&&event.stopLeadSeconds==.015
            &&f.engine->loopMeterGraphCurrent(event.generation,id,event.ticket),"Panic fact names actual frame, clip and unchanged raw window");
        for(double invalid:{0.,.025,-1.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){
            auto wrong=event;wrong.stopLeadSeconds=invalid;check(!owner.consumeStopAll(wrong)&&!owner.cleanupRequest().serial,"Panic wrong delta rejects before mutation");}
        auto wrong=event;++wrong.ticket;check(!owner.consumeStopAll(wrong),"Panic wrong window rejects");
        wrong=event;++wrong.bufferId;check(!owner.consumeStopAll(wrong),"Panic wrong clip rejects");
        wrong=event;++wrong.generation;check(!owner.consumeStopAll(wrong),"Panic wrong generation rejects");
        check(!owner.consumeImmediateStop(event)&&!owner.consumeScheduledStop(event)&&owner.consumeStopAll(event)
            &&owner.cleanupRequest().delay==.015+.05&&owner.inspect().analyser==identity.analyser&&reading(owner.value())==held,"Panic retains attached identity/history with 65ms cleanup");
        const auto first=owner.cleanupRequest();check(!owner.consumeStopAll(event)&&owner.cleanupRequest()==first,"Duplicate panic cannot postpone cleanup");
        f.command(LoopAction::stopAll,id);f.cycle();event=f.next(MeterGraphEventKind::stopAll,id);
        check(owner.consumeStopAll(event)&&owner.cleanupRequest().serial!=first.serial&&!owner.deliverCleanup(first),"Repeated panic replaces cleanup even after native audio is stopped");
        const auto second=owner.cleanupRequest();check(owner.deliverCleanup(second)&&!owner.inspect().present&&owner.inspect().data==identity.data&&reading(owner.value())==held,"Panic teardown retains data/history");
        f.command(LoopAction::stopAll,id);f.cycle();check(owner.consumeStopAll(f.next(MeterGraphEventKind::stopAll,id))&&!owner.cleanupRequest().serial,"Panic after detached teardown is an ordered no-op");
        f.command(LoopAction::play,id);f.cycle();check(owner.consumeImmediatePlay(f.next(MeterGraphEventKind::immediatePlay,id)),"Replay establishes new panic origin");
        f.command(LoopAction::stopAll,id);f.command(LoopAction::play,id);f.cycle();check(owner.consumeStopAll(f.next(MeterGraphEventKind::stopAll,id)),"Apply ordered panic before replacement");
        const auto obsolete=owner.cleanupRequest();check(owner.consumeImmediatePlay(f.next(MeterGraphEventKind::immediatePlay,id))&&!owner.deliverCleanup(obsolete)&&!owner.cleanupRequest().serial,"Same-callback PLAY cancels panic cleanup");
        // After any accepted queued PLAY, audio state cannot prove whether the
        // original 100ms lead callback has created its new graph. Fail closed.
        f.command(LoopAction::stopImmediate,id);f.cycle();check(owner.consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop,id)),"Stop before queued-intent quarantine");
        const auto retained=owner.cleanupRequest();for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,999,1,1,4,false});
        f.command(LoopAction::play,id);f.cycle();check(f.view->tracks[id].state==LoopState::queued&&owner.consumeQueuedPlay(f.next(MeterGraphEventKind::queuedPlay,id))&&!f.engine->popMeterGraphEvent(event),"Queued intent never invents graph ownership");
        f.command(LoopAction::stopAll,id);f.cycle();check(owner.consumeQueuedCancel(f.next(MeterGraphEventKind::queuedCancel,id))&&!f.engine->popMeterGraphEvent(event)&&owner.cleanupRequest()==retained,"Queued panic cannot replace retained old cleanup with an inferred active-graph delay");
        f.command(LoopAction::play,id);f.cycle();check(owner.consumeQueuedPlay(f.next(MeterGraphEventKind::queuedPlay,id)),"Consume second actual queued intent");unsigned cycles=0;while(f.view->tracks[id].state==LoopState::queued&&++cycles<1000)f.cycle();
        check(f.view->tracks[id].state==LoopState::playing,"Reach actual audible queued transition");
        const auto pendingQueue=owner.queuedIntentId();const auto beforeLate=owner.inspect();const auto beforeLateReading=reading(owner.value());
        f.command(LoopAction::stopAll,id);f.cycle();
        check(f.engine->popMeterGraphEvent(event)&&event.kind==MeterGraphEventKind::queuedCancel&&event.queueId==pendingQueue,
            "Late STOP ALL must publish cancellation for the still-pending queued intent after audible grid");
        auto wrongLate=event;++wrongLate.queueId;check(!owner.consumeQueuedCancel(wrongLate)&&owner.queuedIntentId()==pendingQueue,"Late panic cannot cancel a different queued intent");
        check(owner.consumeQueuedCancel(event)&&!owner.queuedIntentId()&&owner.queuedCallbackDelay()==-1&&owner.cleanupRequest()==retained
            &&owner.inspect().state==MeterLoopState::stopped&&owner.inspect().analyser==beforeLate.analyser&&owner.inspect().data==beforeLate.data
            &&reading(owner.value())==beforeLateReading&&!owner.consumeQueuedCancel(event)&&!f.engine->popMeterGraphEvent(wrongLate),
            "Late queued panic preserves old cleanup and analyser/history without reviving a 65ms graph stop");
        f.command(LoopAction::stopAll,id);f.cycle();check(!f.engine->popMeterGraphEvent(wrongLate),"Repeated late queued panic cannot re-emit the cancelled intent or recover quarantined graph ownership");
        f.command(LoopAction::play,id);f.cycle();
        for(unsigned n=0;n<1000&&f.view->tracks[id].state==LoopState::queued;++n)f.cycle();
        check(f.view->tracks[id].state==LoopState::playing,"Advance a second queue before its message receipts are consumed");
        f.command(LoopAction::stopAll,id);f.cycle();
        check(owner.consumeQueuedPlay(f.next(MeterGraphEventKind::queuedPlay,id))&&owner.consumeQueuedCancel(f.next(MeterGraphEventKind::queuedCancel,id))
            &&!owner.queuedIntentId()&&owner.cleanupRequest()==retained&&!f.engine->popMeterGraphEvent(wrongLate),
            "Delayed message service consumes ordered queued intent and late panic without a graph birth");
        f.command(LoopAction::retrigger,id);f.cycle();check(owner.consumeImmediatePlay(f.next(MeterGraphEventKind::immediatePlay,id)),"Actual stopped retrigger restores known panic origin");
        f.command(LoopAction::stopAll,id);f.cycle();event=f.next(MeterGraphEventKind::stopAll,id);check(owner.consumeStopAll(event),"Panic works after explicit origin recovery");
        f.command(LoopAction::clear,id);f.cycle();check(owner.consumeClear(f.next(MeterGraphEventKind::cleared,id))&&!owner.inspect().data&&!owner.cleanupRequest().serial&&!owner.consumeStopAll(event),"Attached clear invalidates panic and resets history");
        f.command(LoopAction::stopAll,id);f.cycle();check(!f.engine->popMeterGraphEvent(event),"Clear removes panic origin");
        f.prepare();check(owner.reset()&&!owner.consumeStopAll(event),"Prepare rejects retained panic");
        f.record(id);check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Commit take after panic prepare");
        f.command(LoopAction::stopImmediate,id);f.command(LoopAction::play,id);f.cycle();check(owner.consumeImmediatePlay(f.next(MeterGraphEventKind::immediatePlay,id)),"Establish panic origin before stream loss");
        f.command(LoopAction::stopAll,id);f.cycle();event=f.next(MeterGraphEventKind::stopAll,id);
        for(unsigned n=0;n<256;++n){f.command(LoopAction::stopAll,id);f.cycle();}
        check(!f.engine->meterGraphEventsCurrent()&&!owner.consumeStopAll(event),"Panic stream loss rejects retained receipts");
        unsigned budget=255;check(!f.engine->popMeterGraphEvent(event,f.engine->generation(),budget)&&!budget,"Lost panic batch drains within raw budget");
        f.prepare();check(owner.reset()&&owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Prepare restores healthy retained buffer without a graph origin");
        f.command(LoopAction::stopAll,id);f.cycle();check(!f.engine->popMeterGraphEvent(event),"Retained clip after prepare cannot recover prior panic graph origin");
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(auto action:{LoopAction::overdub,LoopAction::substitute,LoopAction::multiply,LoopAction::normalize}){
        Fixture f(block,MeterEventScope::acceptedPlayback);LoopMeterLifecycle owner(*f.engine,id);
        f.record(id);check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Commit panic edit fixture");
        f.command(LoopAction::stopImmediate,id);f.command(LoopAction::play,id);f.cycle();check(owner.consumeImmediatePlay(f.next(MeterGraphEventKind::immediatePlay,id)),"Establish edit graph before panic");
        const auto data=owner.inspect().data;
        for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({action,id,0,e->generation(),0,action==LoopAction::multiply?1.:0.}),"Queue actual capture/edit before panic");
        if(action!=LoopAction::normalize)for(unsigned n=0;n<256/block;++n)f.cycle(); // MULT requires at least 128 captured frames; NORM shares the panic batch.
        f.command(LoopAction::stopAll,id);f.cycle();
        check(owner.consumeStopAll(f.next(MeterGraphEventKind::stopAll,id))&&owner.cleanupRequest().delay==.015+.05,"Panic retains known graph through actual capture/edit/finalizing state");
        const auto pending=owner.cleanupRequest();
        f.cycle(true); // Host-declared offline completion accepts the worker result on the next callback.
        check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id))&&owner.cleanupRequest()==pending&&owner.inspect().data==data,
            "Worker-accepted buffer after panic preserves pending cleanup and analyser data");
    }
    check(allocations.load()==0,"Panic callbacks have zero tracked new/delete");
    std::cout<<"PASS: accepted STOP ALL receipts, 65ms cleanup, repeated/detached panic, capture/edit ordering, replacement/clear/prepare/loss, malformed identity rejection and queued-origin quarantine at tracks 0/19, 64/128; exact paired stereo PCM and zero tracked callback new/delete\n";
    std::cout<<"PASS: post-grid pending queued panic cancellation, stale/duplicate/repeated rejection, delayed ordered message delivery and retained old cleanup/history at tracks 0/19 and 64/128\n";
}
namespace {
void stopCallbackClockTests(){
    using namespace aion;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(unsigned mode=0;mode<5;++mode){
        Fixture f(block,MeterEventScope::acceptedPlayback);LoopMeterLifecycle owner(*f.engine,id);
        f.record(id);check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Stop clock installs actual clip");
        f.command(LoopAction::stopImmediate,id);f.cycle();
        if(mode!=4){f.command(LoopAction::play,id);f.cycle();check(owner.consumeImmediatePlay(f.next(MeterGraphEventKind::immediatePlay,id)),"Stop clock attaches actual graph");}
        if(mode>=3){if(mode==3){f.command(LoopAction::stopImmediate,id);f.cycle();check(owner.consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop,id)),"Stop clock retains old graph for queued stop");}
            for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
            f.command(LoopAction::play,id);f.cycle(false,{99,true});check(owner.consumeQueuedPlay(f.next(MeterGraphEventKind::queuedPlay,id)),"Stop clock queues real intent");}
        else if(mode==1)for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
        const auto begin=f.engine->clock(),at=begin+block/2;const auto action=mode==1?LoopAction::stop:mode==2?LoopAction::stopAll:LoopAction::stopImmediate;
        for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({action,id,++f.request,e->generation(),at,0}),"Queue actual sub-block stop");
        f.cycle(false,{100,true});const auto kind=mode==1?MeterGraphEventKind::scheduledStop:mode==2?MeterGraphEventKind::stopAll:mode>=3?MeterGraphEventKind::queuedStop:MeterGraphEventKind::immediateStop;
        const auto event=f.next(kind,id);check(event.frame==at&&event.callbackClock==f.view->callbackClock,"Stop fact owns exact accepting callback");
        check(mode==1?owner.consumeScheduledStop(event):mode==2?owner.consumeStopAll(event):mode>=3?owner.consumeQueuedStop(event):owner.consumeImmediateStop(event),"Stop fact commits original cleanup policy");
        MeterStopCallbackTime first;
        if(mode==4){check(!owner.cleanupRequest().serial&&!owner.readCleanupCallbackTime(73,{73,101},first),"Graphless queued stop cannot invent cleanup correlation");continue;}
        check(owner.readCleanupCallbackTime(73,{73,100+static_cast<double>(block)/48000},first)
            &&first.commandSeconds==100+static_cast<double>(block/2)/48000&&first.cleanupSerial==owner.cleanupRequest().serial
            &&first.delaySeconds==owner.cleanupRequest().delay,"Actual stop readout binds exact cleanup request");
        const auto cleanup=owner.cleanupRequest();f.cycle();MeterStopCallbackTime later;
        check(owner.readCleanupCallbackTime(73,{73,101},later)&&later.deadlineSeconds==first.deadlineSeconds&&later.callbackSerial==first.callbackSerial
            &&later.remainingSeconds==0&&owner.cleanupRequest()==cleanup,"Late/missing processor view cannot move cleanup command deadline");
        for(unsigned n=0;n<1000&&f.view->tracks[id].state!=LoopState::stopped;++n)f.cycle();
        check(f.view->tracks[id].state==LoopState::stopped,"Scheduled audio stop reaches grid before new queued play");
        // Queuing a new source retains the old independent cleanup and its clock.
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
        f.command(LoopAction::play,id);f.cycle(false,{101,true});check(owner.consumeQueuedPlay(f.next(MeterGraphEventKind::queuedPlay,id)),"Queue new source before old cleanup");
        check(owner.readCleanupCallbackTime(73,{73,102},later)&&later.deadlineSeconds==first.deadlineSeconds,"New queued intent retains old cleanup origin");
        check(owner.deliverCleanup(cleanup),"Deliver original cleanup explicitly");auto unchanged=later;
        check(!owner.readCleanupCallbackTime(73,{73,102},unchanged)&&unchanged==later,"Delivered cleanup cannot expose stale timestamp");
        check(owner.requestQueuedGraph(owner.queuedIntentId(),owner.committedBuffer()),"Deliver retained graph callback after old cleanup");
        const auto pending=owner.queuedWindowTransaction();LoopCommand mirror{LoopAction::meterQueuedGraphRequest,id,0,f.control->generation(),0,0};
        mirror.meterGraphTicket=pending.ticket;mirror.meterQueueId=pending.queueId;mirror.meterBufferId=pending.bufferId;
        check(f.control->enqueueLoop(mirror),"Mirror replacement graph for exact PCM");f.cycle();(void)f.next(MeterGraphEventKind::started,id);
        check(owner.consumeQueuedWindowResult(f.next(MeterGraphEventKind::queuedGraphAccepted,id)),"Attach replacement graph for cleanup correlation");
        f.command(LoopAction::stopImmediate,id);f.cycle(false,{103,true});check(owner.consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop,id)),"Replace cleanup with new actual stop");
        check(owner.readCleanupCallbackTime(73,{73,103},later)&&later.cleanupSerial>first.cleanupSerial&&later.eventSerial>first.eventSerial
            &&later.commandSeconds==103,"New stop replaces cleanup clock by exact request identity");
        check(owner.stop(0),"Direct uncorrelated stop replaces request");unchanged=later;
        check(!owner.readCleanupCallbackTime(73,{73,104},unchanged)&&unchanged==later,"Uncorrelated manual cleanup cannot reuse older stop clock");
        f.prepare();check(owner.reset()&&!owner.readCleanupCallbackTime(73,{73,102},unchanged)&&unchanged==later,"Prepare clears cleanup correlation");
    }
    check(allocations.load()==0,"Stop clock capture has zero callback new/delete");
    std::cout<<"PASS: actual stop callback capture, cleanup-request binding, sub-block deadlines, old-cleanup retention, graphless rejection and prepare at tracks 0/19, 64/128; exact paired PCM\n";
}
void queuedDueDeliveryTests(){
    using namespace aion;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(unsigned cleanupOrder:{0u,1u,2u}){
        Fixture f(block,MeterEventScope::acceptedPlayback);LoopMeterLifecycle owner(*f.engine,id);
        f.record(id);check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Due callback installs clip");
        f.command(LoopAction::stopImmediate,id);f.cycle();
        if(cleanupOrder){f.command(LoopAction::play,id);f.cycle();check(owner.consumeImmediatePlay(f.next(MeterGraphEventKind::immediatePlay,id)),"Due callback attaches old graph");
            f.command(LoopAction::stopImmediate,id);f.cycle();check(owner.consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop,id)),"Due callback retains old cleanup");}
        const auto cleanup=owner.cleanupRequest();
        const auto queue=[&]{for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,120,1,1,4,false});
            f.command(LoopAction::play,id);f.cycle(false,{100,true});const auto q=f.next(MeterGraphEventKind::queuedPlay,id);
            check(owner.consumeQueuedPlay(q),"Due callback accepts intent");QueuedMeterCallbackTime time;
            check(owner.readQueuedCallbackTime(73,{73,100},time),"Capture retained due callback receipt");return time;};
        const auto expected=queue();const auto prior=owner.queuedWindowTransaction();const auto graph=owner.inspect();
        const auto rejects=[&](QueuedMeterCallbackTime bad,MonotonicClockPoint now){
            check(!owner.requestQueuedGraphDue(bad,73,now)&&owner.queuedWindowTransaction().ticket==prior.ticket
                &&owner.queuedWindowTransaction().status==prior.status&&owner.cleanupRequest()==cleanup
                &&owner.inspect().analyser==graph.analyser,"Stale or early due callback cannot mutate request, graph or cleanup");};
        rejects(expected,{73,std::nextafter(expected.deadlineSeconds,-std::numeric_limits<double>::infinity())});
        const MonotonicClockPoint due{73,expected.deadlineSeconds};
        auto bad=expected;++bad.generation;rejects(bad,due);bad=expected;bad.track=(id+1)%20;rejects(bad,due);
        bad=expected;bad.queueId+=(1ull<<32);rejects(bad,due);bad=expected;bad.bufferId+=(1ull<<32);rejects(bad,due);
        bad=expected;++bad.eventSerial;rejects(bad,due);bad=expected;++bad.callbackSerial;rejects(bad,due);
        bad=expected;++bad.clockOrigin;rejects(bad,due);bad=expected;bad.commandSeconds+=.1;rejects(bad,due);
        bad=expected;bad.deadlineSeconds=100;rejects(bad,due);rejects(expected,{74,due.seconds});
        rejects(expected,{73,std::numeric_limits<double>::quiet_NaN()});
        if(cleanupOrder==1)check(owner.deliverCleanup(cleanup),"Old cleanup before due delivery retains real intent");
        auto observation=expected;observation.remainingSeconds=999; // An old readout is not delivery authority.
        check(owner.requestQueuedGraphDue(observation,73,due)&&!owner.requestQueuedGraphDue(expected,73,due),"Due callback enqueues once at exact original deadline");
        const auto pending=owner.queuedWindowTransaction();
        LoopCommand mirror{LoopAction::meterQueuedGraphRequest,id,0,f.control->generation(),0,0};
        mirror.meterGraphTicket=pending.ticket;mirror.meterQueueId=expected.queueId;mirror.meterBufferId=expected.bufferId;
        check(f.control->enqueueLoop(mirror),"Mirror guarded callback for exact PCM");
        if(cleanupOrder==2)check(owner.deliverCleanup(cleanup),"Cleanup after enqueue retires guarded callback result");
        f.cycle();(void)f.next(MeterGraphEventKind::started,id);const auto result=f.next(MeterGraphEventKind::queuedGraphAccepted,id);
        check(owner.consumeQueuedWindowResult(result)==(cleanupOrder!=2),"Due callback result retains existing cleanup retirement policy");
        check(!owner.requestQueuedGraphDue(expected,73,{73,due.seconds+1}),"Duplicate late due receipt cannot create another graph");
        f.command(LoopAction::stopImmediate,id);f.cycle();check(owner.consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop,id)),"Stop guarded callback graph origin");
        const auto replacement=queue();const auto before=owner.queuedWindowTransaction();
        check(replacement.queueId!=expected.queueId&&!owner.requestQueuedGraphDue(expected,73,{73,replacement.deadlineSeconds+1})
            &&owner.queuedWindowTransaction().ticket==before.ticket,"Old timer cannot retarget a new queue on same clip and track");
        // Saturation rejects without reserving a ticket; the same due receipt retries.
        for(unsigned n=0;n<255;++n)f.command(LoopAction::mute,id);
        check(!owner.requestQueuedGraphDue(replacement,73,{73,replacement.deadlineSeconds+1})
            &&owner.queuedWindowTransaction().ticket==before.ticket,"Full FIFO preserves due callback for retry");
        f.cycle();f.cycle();check(owner.requestQueuedGraphDue(replacement,73,{73,replacement.deadlineSeconds+1}),"Late due callback retries after bounded FIFO drain");
        f.prepare();check(owner.reset()&&!owner.requestQueuedGraphDue(replacement,73,{73,replacement.deadlineSeconds+1}),"Prepared generation rejects retained timer receipt");
    }
    check(allocations.load()==0,"Due callback transactions retain zero tracked callback new/delete");
    std::cout<<"PASS: guarded due graph delivery, exact deadline, stale full-width identities, no track retarget, cleanup ordering, duplicate/FIFO/prepare rejection and exact paired PCM at tracks 0/19, 64/128\n";
}
void queuedCallbackClockTests(){
    using namespace aion;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool attached:{false,true}){
        Fixture f(block,MeterEventScope::acceptedPlayback);LoopMeterLifecycle owner(*f.engine,id);
        f.record(id);check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Queued clock installs actual clip");
        f.command(LoopAction::stopImmediate,id);f.cycle();
        if(attached){f.command(LoopAction::play,id);f.cycle();check(owner.consumeImmediatePlay(f.next(MeterGraphEventKind::immediatePlay,id)),"Queued clock retains explicit old graph");
            f.command(LoopAction::stopImmediate,id);f.cycle();check(owner.consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop,id)),"Queued clock arms independent cleanup");}
        const auto cleanup=owner.cleanupRequest();const auto binding=owner.binding();const auto old=owner.inspect();
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,120,1,1,4,false});
        const auto begin=f.engine->clock(),at=begin+block/2;++f.request;
        for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::play,id,f.request,e->generation(),at,0}),"Queue command at exact mid-callback sample");
        f.cycle(false,{17.25,true});const auto event=f.next(MeterGraphEventKind::queuedPlay,id);
        check(event.frame==at&&event.callbackClock==f.view->callbackClock&&event.callbackClock.beginFrame==begin
            &&event.callbackClock.endFrame==begin+block&&event.callbackClock.processorEntry==ProcessorClockObservation{17.25,true},"Queued intent carries its own exact callback correlation");
        check(owner.consumeQueuedPlay(event),"Queued clock owner accepts actual intent");
        QueuedMeterCallbackTime first;const double wall=17.25+static_cast<double>(block)/48000;
        check(owner.readQueuedCallbackTime(73,{73,wall},first)&&first.commandSeconds==17.25+static_cast<double>(block/2)/48000
            &&first.deadlineSeconds==first.commandSeconds+event.queueDelaySeconds,"Actual queued deadline starts at accepting sample offset");
        f.cycle();check(!f.view->callbackClock.processorEntry.available&&f.view->callbackClock.serial>event.callbackClock.serial,"Later view deliberately has no correlation");
        QueuedMeterCallbackTime later;check(owner.readQueuedCallbackTime(73,{73,wall+.5},later)&&later.deadlineSeconds==first.deadlineSeconds
            &&later.callbackSerial==event.callbackClock.serial&&later.remainingSeconds==0,"Late receipt preserves command deadline without replacing it with later view");
        check(owner.inspect().analyser==old.analyser&&owner.binding().ticket==binding.ticket&&owner.cleanupRequest()==cleanup,"Deadline reads do not create graphs, bind PCM or rearm cleanup");
        if(attached){check(owner.deliverCleanup(cleanup)&&owner.readQueuedCallbackTime(73,{73,wall+.5},later),"Old cleanup leaves live callback deadline readable from stopped");
            f.command(LoopAction::clear,id);f.cycle();check(owner.consumeClear(f.next(MeterGraphEventKind::cleared,id)),"Clear retires queued clock intent");}
        else{f.command(LoopAction::stopAll,id);f.cycle();check(owner.consumeQueuedCancel(f.next(MeterGraphEventKind::queuedCancel,id)),"Panic retires queued clock intent");}
        auto unchanged=later;check(!owner.readQueuedCallbackTime(73,{73,wall+.5},unchanged)&&unchanged==later,"Retired queued deadline cannot leak stale output");
        check(!owner.requestQueuedGraphDue(first,73,{73,first.deadlineSeconds+1}),"Cancelled or cleared due receipt cannot enqueue");
        if(!attached){
            MidiSettings settings;settings.enabled=true;settings.revision=1;settings.bindings.fill(0);settings.bindings[id]=midiBinding(1,60);
            for(auto* e:{f.engine.get(),f.control.get()})check(e->configureMidi(settings),"Configure public MIDI input for zero callback");
            const MidiEvent note{0,144,60,100,3};
            watch=true;f.engine->process(nullptr,0,0,{&note,1},false,{17.30,true});f.control->process(nullptr,0,0,{&note,1},false,{17.30,true});watch=false;
            const auto zero=f.next(MeterGraphEventKind::queuedPlay,id);
            QueuedMeterCallbackTime zeroTime;
            check(zero.callbackClock.serial>event.callbackClock.serial&&zero.callbackClock.beginFrame==zero.callbackClock.endFrame
                &&owner.consumeQueuedPlay(zero)&&owner.readQueuedCallbackTime(73,{73,17.35},zeroTime)
                &&zeroTime.commandSeconds==17.30,"Actual MIDI queue in zero callback owns its current same-frame counter correlation");
        }
        f.prepare();check(owner.reset()&&!owner.readQueuedCallbackTime(73,{73,wall+.5},unchanged)&&unchanged==later,"Prepare clears retained command correlation");
    }
    for(unsigned block:{64u,128u}){
        Fixture f(block,MeterEventScope::acceptedPlayback);LoopMeterLifecycle owner(*f.engine,19);
        f.record();check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled)),"Loss clock installs actual clip");f.command(LoopAction::stopImmediate);f.cycle();
        const auto queue=[&]{for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,120,1,1,4,false});
            f.command(LoopAction::play);f.cycle(false,{100,true});const auto q=f.next(MeterGraphEventKind::queuedPlay);check(owner.consumeQueuedPlay(q),"Loss clock accepts intent");return q;};
        auto q=queue();QueuedMeterCallbackTime first;check(owner.readQueuedCallbackTime(73,{73,101},first),"Seed clock before event loss");
        LoopCommand bad{LoopAction::meterQueuedGraphRequest,19,0,f.engine->generation(),0,0};bad.meterGraphTicket=1;bad.meterQueueId=q.queueId+(1ull<<32);bad.meterBufferId=q.bufferId;
        for(unsigned n=0;n<255;++n)check(f.engine->enqueueLoop(bad),"Fill actual event stream with typed negatives");f.cycle();f.cycle();
        check(f.engine->enqueueLoop(bad),"Overflow full clock event stream");f.cycle();auto held=first;
        check(!owner.readQueuedCallbackTime(73,{73,101},held)&&held==first,"Event loss invalidates queued clock without stale output");
        check(!owner.requestQueuedGraphDue(first,73,{73,first.deadlineSeconds+1}),"Lost event stream rejects due delivery");
        MeterGraphEvent discarded;unsigned budget=255;
        check(!f.engine->popMeterGraphEvent(discarded,f.engine->generation(),budget)&&budget==0,"Clock loss recovery first drains all retained raw entries with bounded work");
        f.prepare();check(owner.reset()&&owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled)),"Clock loss prepare recovers retained clip");q=queue();
        check(owner.readQueuedCallbackTime(73,{73,101},held)&&held.generation!=first.generation&&held.queueId==q.queueId,"Fresh generation recovers queued clock with new provenance");
    }
    check(allocations.load()==0,"Queued command clock capture has zero callback new/delete");
    std::cout<<"PASS: actual queued command clock capture, exact sub-block offset, later/missing-view independence, immutable late deadline, old cleanup, cancel/clear/prepare/loss recovery and paired PCM at tracks 0/19, 64/128\n";
}
void queuedGraphTransactionTests(){
    using namespace aion;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool late:{false,true})
    for(unsigned order=0;order<5;++order)for(auto stop:{LoopAction::stopImmediate,LoopAction::stop,LoopAction::stopAll}){
        Fixture f(block,MeterEventScope::acceptedPlayback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback);f.prepare();
        LoopMeterLifecycle owner(*f.engine,id);f.record(id);check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Graph transaction installs actual clip");
        f.command(LoopAction::stopImmediate,id);f.cycle();
        if(order){
            f.command(LoopAction::play,id);f.cycle();check(owner.consumeImmediatePlay(f.next(MeterGraphEventKind::immediatePlay,id)),"Old graph has accepted playback provenance");
            f.command(LoopAction::stopImmediate,id);f.cycle();check(owner.consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop,id)),"Old graph cleanup is explicit");
        }
        for(unsigned n=0;n<8;++n)f.cycle(); // Drain the old audible fade, not its independent message timer.
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,120,1,1,4,false});
        f.command(LoopAction::play,id);f.cycle();const auto intent=f.next(MeterGraphEventKind::queuedPlay,id);
        check(owner.consumeQueuedPlay(intent),"Graph transaction accepts real queued intent");
        if(late)for(unsigned n=0;n<1000&&f.view->tracks[id].state==LoopState::queued;++n)f.cycle();
        const auto old=owner.inspect();const auto cleanup=owner.cleanupRequest();
        if(order==2)check(owner.deliverCleanup(cleanup)&&owner.queuedIntentId()==intent.queueId,"Old cleanup before callback retains intent");
        check(!owner.requestQueuedGraph(intent.queueId+(1ull<<32),intent.bufferId),"Graph callback validates full-width queue before enqueue");
        check(owner.requestQueuedGraph(intent.queueId,intent.bufferId)&&!owner.requestQueuedGraph(intent.queueId,intent.bufferId),"One explicit graph callback transaction");
        const auto pending=owner.queuedWindowTransaction();check(pending.graph&&pending.status==QueuedWindowStatus::pending,"Graph enqueue is not graph acceptance");
        LoopCommand mirror{LoopAction::meterQueuedGraphRequest,id,0,f.control->generation(),0,0};
        mirror.meterGraphTicket=pending.ticket;mirror.meterQueueId=intent.queueId;mirror.meterBufferId=intent.bufferId;
        check(f.control->enqueueLoop(mirror),"Mirror explicit callback for paired PCM");
        if(order==3)check(owner.deliverCleanup(cleanup),"Old cleanup retires pending graph result");
        f.cycle();check(!owner.consumeQueuedWindowResult(f.next(MeterGraphEventKind::started,id)),"Raw start cannot impersonate graph acceptance");
        const auto accepted=f.next(MeterGraphEventKind::queuedGraphAccepted,id);
        if(order==3){
            check(!owner.consumeQueuedWindowResult(accepted)&&!owner.inspect().present,"Retired graph acceptance cannot resurrect after old cleanup");
            f.command(LoopAction::stopImmediate,id);f.cycle();check(owner.consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop,id))
                &&!owner.inspect().present&&!owner.cleanupRequest().serial,"Retired accepted callback stop is a detached no-op, not a user error");
            f.command(LoopAction::play,id);f.cycle();const auto next=f.next(MeterGraphEventKind::queuedPlay,id);
            check(owner.consumeQueuedPlay(next)&&owner.requestQueuedGraph(next.queueId,next.bufferId),"Detached owner recovers a new explicit callback after retired accepted result");
            mirror.generation=f.control->generation();mirror.meterGraphTicket=owner.queuedWindowTransaction().ticket;mirror.meterQueueId=next.queueId;mirror.meterBufferId=next.bufferId;
            check(f.control->enqueueLoop(mirror),"Mirror recovered callback");f.cycle();(void)f.next(MeterGraphEventKind::started,id);
            check(owner.consumeQueuedWindowResult(f.next(MeterGraphEventKind::queuedGraphAccepted,id))&&owner.inspect().present,"New callback recovers graph without resurrecting old result");
        }else{
            auto wrong=accepted;wrong.kind=MeterGraphEventKind::queuedWindowAccepted;check(!owner.consumeQueuedWindowResult(wrong),"Raw typed acceptance cannot impersonate graph result");
            wrong=accepted;++wrong.ticket;check(!owner.consumeQueuedWindowResult(wrong),"Wrong graph ticket rejects");
            wrong=accepted;wrong.bufferId+=(1ull<<32);check(!owner.consumeQueuedWindowResult(wrong),"Wrong full-width graph clip rejects");
            check(owner.consumeQueuedWindowResult(accepted)&&!owner.consumeQueuedWindowResult(accepted),"Exact graph acceptance commits once");
            check(owner.inspect().present&&owner.inspect().state==MeterLoopState::playing&&owner.inspect().analyser!=old.analyser
                &&owner.inspect().data!=old.data&&owner.binding().ticket==accepted.ticket&&!owner.queuedIntentId()&&!owner.queuedWindowReady(),"Accepted callback binds fresh graph and retires message intent");
            check(owner.cleanupRequest()==(order==2?MeterCleanupRequest{}:cleanup),"Queued graph birth preserves exact independent old deadline");
        }
        check(owner.request(),"Fresh queued graph may request its bound window");f.cycle();check(f.engine->popLoopMeterWindow(*f.packet),"Receive actual queued graph PCM");
        const double time=static_cast<double>(f.view->frame)/48000;
        check(owner.observe(*f.packet,*f.view,f.view->callbackClock,{time,time}),"Actual queued graph binding observes its own window");
        if(order==4)check(owner.deliverCleanup(cleanup)&&!owner.inspect().present,"Retained old cleanup tears down newly accepted graph");
        f.command(stop,id);f.cycle();
        const auto kind=stop==LoopAction::stopAll?MeterGraphEventKind::stopAll:stop==LoopAction::stop?MeterGraphEventKind::scheduledStop:MeterGraphEventKind::immediateStop;
        const auto stopped=f.next(kind,id);
        check(stop==LoopAction::stopAll?owner.consumeStopAll(stopped):stop==LoopAction::stop?owner.consumeScheduledStop(stopped):owner.consumeImmediateStop(stopped),"Post-callback stop routes to accepted graph origin");
        if(order==4)check(!owner.cleanupRequest().serial&&!owner.inspect().present,"Detached graph stop does not rearm cleanup");
        else check(owner.cleanupRequest().serial&&owner.cleanupRequest().delay==(stop==LoopAction::stopAll?.065:stop==LoopAction::stop?stopped.stopLeadSeconds+.05:.025),"Graph stop uses original deadline policy");
        if(stop==LoopAction::stop&&!late&&order!=3&&order!=4){
            const auto end=f.view->tracks[id].scheduledFrame;
            check(f.view->tracks[id].state==LoopState::queued&&f.view->tracks[id].stopPending,"Scheduled pre-grid stop retains audio gate");
            check(owner.request(),"Request scheduled queued silence");f.cycle();check(f.engine->popLoopMeterWindow(*f.packet)
                &&std::all_of(f.packet->samples.begin(),f.packet->samples.end(),[](float sample){return sample==0;}),"Future graph stop never renders source early");
            for(unsigned n=0;n<1000&&f.engine->clock()<=end;++n)f.cycle();
            check(f.view->tracks[id].state==LoopState::stopped,"Queued scheduled stop completes at its grid");
        }
        f.command(LoopAction::stopImmediate,id);f.cycle();check(owner.consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop,id)),"Repeated STOP retains accepted callback origin");
        f.command(LoopAction::stopAll,id);f.cycle();check(owner.consumeStopAll(f.next(MeterGraphEventKind::stopAll,id)),"Repeated panic retains accepted callback origin");
    }
    for(unsigned block:{64u,128u}){
        Fixture f(block,MeterEventScope::acceptedPlayback);LoopMeterLifecycle owner(*f.engine,19);
        f.record();check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled)),"Negative graph fixture installs clip");f.command(LoopAction::stopImmediate);f.cycle();
        const auto queue=[&]{for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,120,1,1,4,false});
            f.command(LoopAction::play);f.cycle();const auto q=f.next(MeterGraphEventKind::queuedPlay);check(owner.consumeQueuedPlay(q),"Negative graph fixture queues actual intent");return q;};
        auto q=queue();f.command(LoopAction::stopAll);
        check(owner.requestQueuedGraph(q.queueId,q.bufferId),"Queue graph callback behind panic");f.cycle();
        check(owner.consumeQueuedCancel(f.next(MeterGraphEventKind::queuedCancel))
            &&!owner.consumeQueuedWindowResult(f.next(MeterGraphEventKind::queuedGraphRejected))&&!owner.inspect().present,"Panic retires graph request before late typed rejection");
        q=queue();LoopCommand prior{LoopAction::meterQueuedWindowStart,19,0,f.engine->generation(),0,0};
        prior.meterGraphTicket=owner.queuedWindowTransaction().ticket+1;prior.meterQueueId=q.queueId;prior.meterBufferId=q.bufferId;
        check(f.engine->enqueueLoop(prior)&&owner.requestQueuedGraph(q.queueId,q.bufferId),"Stage graph request behind consumed raw reset");f.cycle();
        (void)f.next(MeterGraphEventKind::started);const auto rejected=f.next(MeterGraphEventKind::queuedGraphRejected);
        check(owner.consumeQueuedWindowResult(rejected)&&owner.queuedWindowTransaction().status==QueuedWindowStatus::rejected&&!owner.inspect().present,"Typed negative graph result terminates without birth");
        f.command(LoopAction::stopAll);f.cycle();check(owner.consumeQueuedCancel(f.next(MeterGraphEventKind::queuedCancel)),"Retire rejected graph intent");
        q=queue();check(owner.requestQueuedGraph(q.queueId,q.bufferId),"Fresh graph callback after rejection");
        watch=true;f.engine->process(nullptr,0,0);f.control->process(nullptr,0,0);watch=false;
        MeterGraphEvent event;check(!f.engine->popMeterGraphEvent(event)&&owner.queuedWindowTransaction().status==QueuedWindowStatus::pending,"Zero callback does not accept graph");
        f.command(LoopAction::clear);f.cycle();(void)f.next(MeterGraphEventKind::started);const auto retired=f.next(MeterGraphEventKind::queuedGraphAccepted);
        check(owner.consumeQueuedWindowResult(retired)&&owner.queuedWindowTransaction().status==QueuedWindowStatus::obsolete&&!owner.inspect().present,"Clear before delivery prevents accepted graph birth");
        check(owner.consumeClear(f.next(MeterGraphEventKind::cleared)),"Typed clear retires graph transaction");
        f.prepare();check(owner.reset()&&!owner.consumeQueuedWindowResult(retired),"Prepare rejects stale graph outcome");
        f.record();check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled)),"Record after graph prepare");f.command(LoopAction::stopImmediate);f.cycle();q=queue();
        for(unsigned n=0;n<255;++n)for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::volume,19,0,e->generation(),f.engine->clock()+1000000,1}),"Fill command FIFO before graph callback");
        check(!owner.requestQueuedGraph(q.queueId,q.bufferId)&&owner.queuedWindowTransaction().status==QueuedWindowStatus::idle&&!owner.inspect().present,"FIFO saturation leaves graph transaction unchanged");
        f.prepare();check(owner.reset()&&owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled)),"Prepare recovers graph FIFO saturation");q=queue();
        check(owner.requestQueuedGraph(q.queueId,q.bufferId),"Stage graph callback before outcome loss");
        for(unsigned n=0;n<254;++n){auto bad=prior;bad.action=LoopAction::meterQueuedGraphRequest;bad.generation=f.engine->generation();bad.meterGraphTicket=n+2;bad.meterQueueId=q.queueId+(1ull<<32);bad.meterBufferId=q.bufferId;
            check(f.engine->enqueueLoop(bad),"Fill graph outcome stream with real negative results");}
        f.cycle();f.cycle();check(!f.engine->meterGraphEventsCurrent()&&!owner.inspect().present&&!owner.requestQueuedGraph(q.queueId,q.bufferId),"Lost graph outcomes fail closed without birth");
        unsigned budget=255;check(!f.engine->popMeterGraphEvent(event,f.engine->generation(),budget)&&budget==0,"Lost graph stream drains bounded raw entries");
        f.prepare();check(owner.reset()&&owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled)),"Prepare recovers lost graph stream");q=queue();
        check(owner.requestQueuedGraph(q.queueId,q.bufferId),"Fresh generation requests graph after loss");f.cycle();(void)f.next(MeterGraphEventKind::started);
        check(owner.consumeQueuedWindowResult(f.next(MeterGraphEventKind::queuedGraphAccepted))&&owner.inspect().present,"Fresh generation accepts graph after loss");
    }
    check(allocations.load()==0,"Queued graph transaction has zero callback new/delete");
    std::cout<<"PASS: explicit queued graph transactions, fresh bindings, late callback after cleanup, retired-result recovery, old timer retention, pre/post-grid stop policies and paired 64/128 PCM\n";
}
void queuedWindowTransactionTests(){
    using namespace aion;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool attached:{false,true})for(bool late:{false,true}){
        Fixture f(block,MeterEventScope::acceptedPlayback);LoopMeterLifecycle owner(*f.engine,id);
        f.record(id);check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Transaction installs actual clip");
        f.command(LoopAction::stopImmediate,id);f.cycle();
        if(attached){
            f.command(LoopAction::play,id);f.cycle();check(owner.consumeImmediatePlay(f.next(MeterGraphEventKind::immediatePlay,id)),"Transaction uses an actual old graph");
            for(unsigned n=0;n<2048/block;++n){check(owner.request(),"Seed transaction history");f.cycle();check(f.engine->popLoopMeterWindow(*f.packet),"Read transaction history");
                const double t=static_cast<double>(f.view->frame)/48000;check(owner.observe(*f.packet,*f.view,f.view->callbackClock,{t,t}),"Observe transaction history");}
            f.command(LoopAction::stopImmediate,id);f.cycle();check(owner.consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop,id)),"Arm old transaction cleanup");
        }
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,999,1,1,4,false});
        const auto queue=[&]{f.command(LoopAction::play,id);f.cycle();auto e=f.next(MeterGraphEventKind::queuedPlay,id);check(owner.consumeQueuedPlay(e),"Accept transaction intent");return e;};
        auto intent=queue();
        if(late)for(unsigned n=0;n<1000&&f.view->tracks[id].state==LoopState::queued;++n)f.cycle();
        const auto graph=owner.inspect();const auto binding=owner.binding();const auto cleanup=owner.cleanupRequest();const auto held=reading(owner.value());
        check(!owner.requestQueuedWindow(intent.queueId+(1ull<<32),intent.bufferId)&&!owner.requestQueuedWindow(intent.queueId,intent.bufferId+(1ull<<32)),"Full-width wrong request identities reject before enqueue");
        check(owner.requestQueuedWindow(intent.queueId,intent.bufferId)&&!owner.requestQueuedWindow(intent.queueId,intent.bufferId),"One bounded pending window transaction");
        const auto request=owner.queuedWindowTransaction();check(request.status==QueuedWindowStatus::pending&&!owner.queuedWindowReady(),"Enqueue is not acceptance");
        watch=true;f.engine->process(nullptr,0,0,{},false);f.control->process(nullptr,0,0,{},false);watch=false;
        MeterGraphEvent zero;check(!f.engine->popMeterGraphEvent(zero)&&owner.queuedWindowTransaction().status==QueuedWindowStatus::pending,
            "Zero-sample callback leaves queued window work pending without an acceptance");
        check(owner.inspect().analyser==graph.analyser&&owner.inspect().data==graph.data&&owner.cleanupRequest()==cleanup&&owner.binding().ticket==binding.ticket,"Request does not create a graph, bind PCM or replace cleanup");
        f.cycle();const auto raw=f.next(MeterGraphEventKind::started,id);
        check(!owner.consumeQueuedWindowResult(raw),"Raw receipt alone cannot complete the explicit transaction");
        const auto accepted=f.next(MeterGraphEventKind::queuedWindowAccepted,id);
        auto wrong=accepted;wrong.queueId+=(1ull<<32);check(!owner.consumeQueuedWindowResult(wrong),"Wrong full-width result queue rejects");
        wrong=accepted;++wrong.ticket;check(!owner.consumeQueuedWindowResult(wrong),"Wrong result ticket rejects");
        wrong=accepted;++wrong.bufferId;check(!owner.consumeQueuedWindowResult(wrong),"Wrong result clip rejects");
        wrong=accepted;++wrong.generation;check(!owner.consumeQueuedWindowResult(wrong),"Wrong result generation rejects");
        check(owner.consumeQueuedWindowResult(accepted)&&owner.queuedWindowReady()&&!owner.consumeQueuedWindowResult(accepted),"Exact accepted result completes once");
        check(owner.queuedIntentId()==intent.queueId&&owner.inspect().state==MeterLoopState::queued&&owner.inspect().analyser==graph.analyser
            &&owner.inspect().data==graph.data&&owner.binding().ticket==binding.ticket&&owner.cleanupRequest()==cleanup&&reading(owner.value())==held,"Accepted reset retains queued graph/history/timer without analyser birth");
        check(!owner.request()&&!owner.start(0),"Rebased raw window cannot be read as an old analyser or bypass callback creation");
        f.command(LoopAction::stopAll,id);f.cycle();auto cancelled=f.next(MeterGraphEventKind::queuedCancel,id);
        check(cancelled.ticket==0&&owner.consumeQueuedCancel(cancelled)&&!owner.queuedWindowReady()&&owner.cleanupRequest()==cleanup,"Quarantined cancel routes safely even with an attached old graph");

        // The existing raw primitive can already have consumed this queue's
        // one reset. A raw notice must not impersonate the typed request result.
        intent=queue();LoopCommand prior{LoopAction::meterQueuedWindowStart,id,0,f.engine->generation(),0,0};
        prior.meterGraphTicket=accepted.ticket+1;prior.meterQueueId=intent.queueId;prior.meterBufferId=intent.bufferId;
        check(f.engine->enqueueLoop(prior)&&owner.requestQueuedWindow(intent.queueId,intent.bufferId),"Stage request behind an already-consumed raw callback");f.cycle();
        check(!owner.consumeQueuedWindowResult(f.next(MeterGraphEventKind::started,id)),"Unsolicited raw start cannot impersonate a typed accepted result");
        MeterGraphEvent rejected;check(f.engine->popMeterGraphEvent(rejected)&&rejected.kind==MeterGraphEventKind::queuedWindowRejected,
            "Typed rejection must terminate an invalid queued window request");
        check(owner.consumeQueuedWindowResult(rejected)&&owner.queuedWindowTransaction().status==QueuedWindowStatus::rejected&&!owner.queuedWindowReady(),"Audio rejection releases pending transaction without binding or birth");
        check(owner.inspect().analyser==graph.analyser&&owner.cleanupRequest()==cleanup&&reading(owner.value())==held,"Rejected reset retains original history and cleanup");
        f.command(LoopAction::clear,id);f.cycle();check(owner.consumeClear(f.next(MeterGraphEventKind::cleared,id)),"Clear unowned raw window after negative result");
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::off,120,1,4,4,false});
        f.record(id);check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Re-record recovers immutable clip ownership");f.command(LoopAction::stopImmediate,id);f.cycle();
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,999,1,1,4,false});
        intent=queue();check(owner.requestQueuedWindow(intent.queueId,intent.bufferId),"New intent recovers after negative result");
        check(!owner.consumeQueuedWindowResult(rejected)&&owner.queuedWindowTransaction().status==QueuedWindowStatus::pending,"Stale negative cannot retire a fresh pending request");
        check(owner.queuedWindowTransaction().ticket>rejected.ticket,"Rejected request tickets are never reused");
        f.cycle();(void)f.next(MeterGraphEventKind::started,id);const auto recovered=f.next(MeterGraphEventKind::queuedWindowAccepted,id);
        check(owner.consumeQueuedWindowResult(recovered)&&owner.queuedWindowReady(),"Fresh queue recovers actual audio acceptance");
        f.command(LoopAction::stopImmediate,id);f.cycle();check(owner.consumeQueuedStop(f.next(MeterGraphEventKind::queuedStop,id)),"Accepted transaction retains explicit queued STOP policy");

        intent=queue();check(owner.requestQueuedWindow(intent.queueId,intent.bufferId),"Stage request before clear/prepare");
        f.command(LoopAction::clear,id);f.cycle();(void)f.next(MeterGraphEventKind::started,id);const auto delayed=f.next(MeterGraphEventKind::queuedWindowAccepted,id);
        check(owner.consumeQueuedWindowResult(delayed)&&!owner.queuedWindowReady(),"A raw token cleared before delivery cannot become ready");
        check(owner.consumeClear(f.next(MeterGraphEventKind::cleared,id))&&!owner.consumeQueuedWindowResult(delayed),"Clear retires completed/retained outcomes");
        f.prepare();check(owner.reset()&&owner.queuedWindowTransaction().status==QueuedWindowStatus::idle&&!owner.consumeQueuedWindowResult(accepted),"Prepare resets transaction and rejects old accepted result");
    }
    for(unsigned block:{64u,128u}){
        Fixture f(block,MeterEventScope::acceptedPlayback);LoopMeterLifecycle owner(*f.engine,19);
        f.record();check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled)),"Overflow fixture installs clip");f.command(LoopAction::stopImmediate);f.cycle();
        const auto queue=[&]{for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,999,1,1,4,false});
            f.command(LoopAction::play);f.cycle();auto q=f.next(MeterGraphEventKind::queuedPlay);check(owner.consumeQueuedPlay(q),"Overflow fixture accepts queue");return q;};
        auto q=queue();
        for(unsigned n=0;n<255;++n)for(auto* e:{f.engine.get(),f.control.get()})
            check(e->enqueueLoop({LoopAction::volume,19,0,e->generation(),f.engine->clock()+1000000,1}),"Fill bounded performance FIFO");
        check(!owner.requestQueuedWindow(q.queueId,q.bufferId)&&owner.queuedWindowTransaction().status==QueuedWindowStatus::idle
            &&owner.queuedIntentId()==q.queueId&&!owner.inspect().present,"Full FIFO rejects without ticket/status/graph mutation");
        f.prepare();check(owner.reset()&&owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled)),"Prepare recovers saturated FIFO");q=queue();
        check(owner.requestQueuedWindow(q.queueId,q.bufferId),"Stage result before event-stream loss");
        for(unsigned n=0;n<254;++n){LoopCommand bad{LoopAction::meterQueuedWindowRequest,19,0,f.engine->generation(),0,0};
            bad.meterGraphTicket=n+2;bad.meterQueueId=q.queueId+(1ull<<32);bad.meterBufferId=q.bufferId;
            check(f.engine->enqueueLoop(bad),"Fill result stream with actual negative outcomes");}
        f.cycle();f.cycle();check(!f.engine->meterGraphEventsCurrent()&&!owner.queuedWindowReady()&&!owner.inspect().present,"Result overflow latches loss without false graph readiness");
        MeterGraphEvent event;unsigned budget=255;check(!f.engine->popMeterGraphEvent(event,f.engine->generation(),budget)&&!budget,"Lost transaction results retain bounded raw draining");
        f.prepare();check(owner.reset()&&owner.queuedWindowTransaction().status==QueuedWindowStatus::idle&&owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled)),"Fresh generation retires lost transaction");
        q=queue();check(owner.requestQueuedWindow(q.queueId,q.bufferId)&&owner.queuedWindowTransaction().ticket==1,"Fresh generation restarts request namespace safely");
        f.cycle();(void)f.next(MeterGraphEventKind::started);check(owner.consumeQueuedWindowResult(f.next(MeterGraphEventKind::queuedWindowAccepted))&&owner.queuedWindowReady(),"Healthy transaction recovers after prepare");
        f.command(LoopAction::stopAll);f.cycle();check(owner.consumeQueuedCancel(f.next(MeterGraphEventKind::queuedCancel)),"Retire recovered intent");
        q=queue();f.command(LoopAction::stopAll);check(owner.requestQueuedWindow(q.queueId,q.bufferId),"Stage conditional request behind panic");f.cycle();
        check(owner.consumeQueuedCancel(f.next(MeterGraphEventKind::queuedCancel))&&owner.queuedWindowTransaction().status==QueuedWindowStatus::obsolete,"Ordered panic retires in-flight request");
        check(!owner.consumeQueuedWindowResult(f.next(MeterGraphEventKind::queuedWindowRejected))&&!owner.queuedWindowReady(),"Late negative outcome cannot revive cancelled request");
    }
    check(allocations.load()==0,"Queued window transaction has zero tracked callback new/delete");
    std::cout<<"PASS: explicit queued window transaction, typed acceptance/rejection, full-width identity, raw-not-birth, attached quarantine routing, retry/clear/prepare and exact paired PCM at tracks 0/19, 64/128\n";
    std::cout<<"PASS: queued window transaction FIFO failure, bounded result overflow/loss and fresh-generation recovery at 64/128\n";
}
void queuedWindowTests(){
    using namespace aion;
    const auto request=[](Fixture& f,unsigned id,const MeterGraphEvent& queue,std::uint32_t ticket){
        LoopCommand command{LoopAction::meterQueuedWindowStart,id,0xffffffffu,f.engine->generation(),0,0};
        command.meterGraphTicket=ticket;command.meterQueueId=queue.queueId;command.meterBufferId=queue.bufferId;return command;
    };
    const auto begin=[](Fixture& f,unsigned id){
        f.record(id);(void)f.next(MeterGraphEventKind::bufferInstalled,id);
        f.command(LoopAction::stopImmediate,id);f.cycle();
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,123.456789,1,4,4,false});
        f.command(LoopAction::play,id);f.cycle();return f.next(MeterGraphEventKind::queuedPlay,id);
    };
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u}){
        Fixture f(block,MeterEventScope::acceptedPlayback);auto queued=begin(f,id);
        auto command=request(f,id,queued,1),wrong=command;
        wrong.meterGraphTicket=0;check(!f.engine->enqueueLoop(wrong),"Queued raw reset requires a nonzero integer ticket");
        wrong=command;wrong.frame=1;check(!f.engine->enqueueLoop(wrong),"Queued callback raw reset cannot be scheduled at a guessed future audio frame");
        wrong=command;wrong.meterQueueId=0;check(!f.engine->enqueueLoop(wrong),"Queued raw reset requires a queue identity");
        wrong=command;wrong.meterBufferId=0;check(!f.engine->enqueueLoop(wrong),"Queued raw reset requires a committed clip identity");
        wrong=command;++wrong.generation;check(!f.engine->enqueueLoop(wrong),"Queued raw reset rejects a foreign generation before enqueue");
        const auto reject=[&](LoopCommand c){
            check(f.engine->enqueueLoop(c),"Enqueue malformed conditional reset for audio-owned rejection");f.cycle();MeterGraphEvent event;
            check(!f.engine->popMeterGraphEvent(event)&&!f.engine->loopMeterGraphCurrent(f.engine->generation(),id,c.meterGraphTicket)
                &&f.view->lastRequestId==f.request&&!f.view->rejected,"Rejected conditional reset does not create a window or alter performance acknowledgements");
        };
        wrong=command;wrong.meterQueueId+=1ull<<32;reject(wrong);
        wrong=command;wrong.meterBufferId+=1ull<<32;reject(wrong);
        wrong=command;wrong.track=id?0:19;reject(wrong);
        check(f.engine->enqueueLoop(command),"Enqueue explicit queued callback raw reset");const auto frame=f.engine->clock();
        check(f.engine->requestLoopMeterWindow(id,f.engine->generation(),1),"Request conditional raw acceptance window");f.cycle();
        const auto accepted=f.next(MeterGraphEventKind::started,id);
        check(accepted.frame==frame&&accepted.ticket==1&&accepted.queueId==queued.queueId&&accepted.bufferId==queued.bufferId
            &&f.engine->loopMeterGraphCurrent(accepted.generation,id,1)&&f.view->tracks[id].state==LoopState::queued
            &&f.view->lastRequestId==f.request&&!f.view->rejected,"Only actual audio acceptance emits an exact conditional raw-window receipt without early playback");
        check(f.engine->popLoopMeterWindow(*f.packet)&&f.packet->windowGraphTicket==1&&f.packet->graphStartFrame==frame
            &&std::all_of(f.packet->samples.begin(),f.packet->samples.end(),[](float sample){return sample==0;}),"Queued raw acceptance has a fresh zero window before audible grid");
        // Even a higher ticket cannot repeat this queue's one accepted callback.
        command.meterGraphTicket=2;check(f.engine->enqueueLoop(command),"Enqueue duplicate callback with a newer raw ticket");f.cycle();MeterGraphEvent event;
        check(!f.engine->popMeterGraphEvent(event)&&f.engine->loopMeterGraphCurrent(accepted.generation,id,1),"Duplicate callback cannot reset the same queued intent twice");
        for(unsigned n=0;n<4000&&f.view->tracks[id].state==LoopState::queued;++n)f.cycle();
        check(f.view->tracks[id].state==LoopState::playing&&!f.engine->popMeterGraphEvent(event),"Audible grid does not synthesize another conditional reset or analyser birth");
        for(unsigned n=0;n<2048/block;++n)f.cycle();
        check(f.engine->requestLoopMeterWindow(id,f.engine->generation(),1),"Request live PCM after conditional reset");f.cycle();
        check(f.engine->popLoopMeterWindow(*f.packet)&&f.packet->windowGraphTicket==1
            &&std::any_of(f.packet->samples.begin(),f.packet->samples.end(),[](float sample){return sample!=0;}),"Accepted conditional reset collects actual post-grid PCM");
        check(f.engine->enqueueLoop(command),"Enqueue late duplicate after audible transition");f.cycle();
        check(!f.engine->popMeterGraphEvent(event)&&f.engine->loopMeterGraphCurrent(accepted.generation,id,1),"Late duplicate preserves healthy raw history");
        // Saturation and internal failures must never become user acknowledgements.
        for(unsigned n=0;n<255;++n)check(f.engine->enqueueLoop(command),"Fill existing bounded command FIFO with duplicate callbacks");
        check(!f.engine->enqueueLoop(command),"Conditional reset uses bounded existing command capacity");f.cycle();f.cycle();
        check(f.view->lastRequestId==f.request&&!f.view->rejected&&!f.engine->popMeterGraphEvent(event),"Bounded duplicate drain preserves user acknowledgements");
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool late:{false,true})for(auto stop:{LoopAction::stop,LoopAction::stopImmediate,LoopAction::stopAll,LoopAction::clear}){
        Fixture f(block,MeterEventScope::acceptedPlayback);const auto queued=begin(f,id);auto command=request(f,id,queued,1);
        if(late){for(unsigned n=0;n<4000&&f.view->tracks[id].state==LoopState::queued;++n)f.cycle();check(f.view->tracks[id].state==LoopState::playing,"Advance queued source for late cancellation race");}
        // Both commands are accepted by the producer. Only audio ownership can
        // reject the stale callback behind this still-unconsumed cancellation.
        f.command(stop,id);check(f.engine->enqueueLoop(command),"Enqueue delayed callback behind accepted stop/clear");f.cycle();
        MeterGraphEvent event;unsigned starts=0;while(f.engine->popMeterGraphEvent(event))if(event.kind==MeterGraphEventKind::started)++starts;
        check(!starts&&!f.engine->loopMeterGraphCurrent(f.engine->generation(),id,1)&&f.view->lastRequestId==f.request&&!f.view->rejected,
            "Pending callback cannot reset a cancelled queue before or after the audible grid");
        // Replace the queued identity; the old callback must not consume a new one.
        if(stop==LoopAction::clear){
            for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::off,123.456789,1,4,4,false});
            f.record(id);(void)f.next(MeterGraphEventKind::bufferInstalled,id);f.command(LoopAction::stopImmediate,id);f.cycle();
            for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,123.456789,1,4,4,false});
        }
        if(f.view->tracks[id].state==LoopState::stopping){for(unsigned n=0;n<4000&&f.view->tracks[id].state==LoopState::stopping;++n)f.cycle();}
        f.command(LoopAction::play,id);f.cycle();const auto newer=f.next(MeterGraphEventKind::queuedPlay,id);
        check(newer.queueId>queued.queueId,"New accepted intent has a fresh queue identity");
        check(f.engine->enqueueLoop(command),"Enqueue stale callback before newer valid callback");
        command=request(f,id,newer,1);check(f.engine->enqueueLoop(command),"Enqueue new queue callback without burning its ticket on old rejection");f.cycle();
        const auto accepted=f.next(MeterGraphEventKind::started,id);
        check(accepted.queueId==newer.queueId&&accepted.bufferId==newer.bufferId&&accepted.ticket==1&&!f.engine->popMeterGraphEvent(event),
            "Only the newer queue and clip can reset raw history after cancellation/replacement");
        command.meterGraphTicket=2;check(f.engine->enqueueLoop(command),"Queue callback before prepare barrier");f.prepare();f.cycle();
        while(f.engine->popMeterGraphEvent(event))check(event.kind==MeterGraphEventKind::bufferInstalled,"Prepare discards pending queued resets without resurrecting old windows");
        check(!f.engine->loopMeterGraphCurrent(f.engine->generation(),id,2),"Prepare invalidates old conditional reset ownership");
    }
    for(auto scope:{MeterEventScope::rawWindows,MeterEventScope::committedBuffers}){
        Fixture f(64,scope);f.record();MeterGraphEvent event;while(f.engine->popMeterGraphEvent(event)){}
        LoopCommand command{LoopAction::meterQueuedWindowStart,19,0xffffffffu,f.engine->generation(),0,0};command.meterGraphTicket=1;command.meterQueueId=1;command.meterBufferId=f.view->tracks[19].waveformId;
        check(f.engine->enqueueLoop(command),"Enqueue conditional reset in component scope");f.cycle();
        check(!f.engine->popMeterGraphEvent(event)&&!f.engine->loopMeterGraphCurrent(f.engine->generation(),19,1),"Component subscription cannot acquire queued playback ownership");
    }
    check(allocations.load()==0,"Conditional queued window callbacks have zero tracked new/delete");
    std::cout<<"PASS: conditional queued raw-window acceptance, full-width queue/clip validation, duplicate/saturation/cancellation/replacement/prepare guards, exact paired stereo PCM at tracks 0/19 and 64/128; no message graph birth inferred\n";
}
}
// STOP inside the grid's 30ms minimum lead schedules the following grid.
// Compare against an otherwise identical source stopped just after its start:
// both must emit exactly the same PCM, even though only one starts as stopping.
void queuedScheduledSourceTests(){
    using namespace aion;
    for(auto mode:{QueuedStartMode::sampleGrid,QueuedStartMode::messageCallback})for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true}){
        Fixture f(block,MeterEventScope::acceptedPlayback);
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,mode);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,mode);f.prepare();
        f.record(id);(void)f.next(MeterGraphEventKind::bufferInstalled,id);
        f.command(LoopAction::stopImmediate,id);f.cycle();
        for(auto* e:{f.engine.get(),f.control.get()}){
            check(e->enqueueLoop({LoopAction::startDelay,id,0,e->generation(),0,.217f}),"Set queued source Start offset");
            check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,reverse?1.f:0.f}),"Set queued source direction");
            e->configureSync({SyncMode::fixed,120,1,1,4,false});
        }
        f.command(LoopAction::play,id);f.cycle();const auto q=f.next(MeterGraphEventKind::queuedPlay,id);
        const auto grid=f.view->tracks[id].scheduledFrame;
        while(f.engine->clock()+block<grid-4800)f.cycle();
        for(auto* e:{f.engine.get(),f.control.get()}){
            LoopCommand callback{LoopAction::meterQueuedGraphRequest,id,0,e->generation(),0,0};
            callback.meterGraphTicket=1;callback.meterQueueId=q.queueId;callback.meterBufferId=q.bufferId;
            check(e->enqueueLoop(callback),"Enqueue real integer-identity queued graph callback");
        }
        f.cycle();(void)f.next(MeterGraphEventKind::started,id);
        (void)f.next(MeterGraphEventKind::queuedGraphAccepted,id);
        while(f.engine->clock()+block<grid-960)f.cycle();
        check(f.engine->enqueueLoop({LoopAction::stop,id,0,f.engine->generation(),0,0}),"Queue STOP within last 30ms before source start");
        f.cycle();const auto end=grid+24000;
        check(f.view->tracks[id].state==LoopState::queued&&f.view->tracks[id].stopPending&&f.view->tracks[id].scheduledFrame==grid,"Queued source retains its start gate while STOP waits for the following grid");
        while(f.engine->clock()<grid+960)f.cycle();
        check(f.view->tracks[id].state==LoopState::stopping&&f.view->tracks[id].scheduledFrame==end,"Queued source plays while waiting for following-grid STOP");
        check(f.control->enqueueLoop({LoopAction::stop,id,0,f.control->generation(),0,0}),"Reference source receives same STOP deadline after start");
        f.cycle();
        while(f.engine->clock()<=end+512)f.cycle();
        check(f.view->tracks[id].state==LoopState::stopped,"Queued source completes scheduled stop and tail");
    }
    check(allocations.load()==0,"Queued scheduled source callbacks perform no tracked allocation/deallocation");
    std::cout<<"PASS: queued source Start/reverse PCM equals normally started source through following-grid STOP at tracks 0/19 and 64/128, no callback new/delete\n";
}
void queuedLateSourceTests(){
    using namespace aion;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(unsigned outcome=0;outcome<6;++outcome){
        Fixture f(block,MeterEventScope::acceptedPlayback);
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);f.prepare();
        f.record(id);(void)f.next(MeterGraphEventKind::bufferInstalled,id);
        f.command(LoopAction::stopImmediate,id);for(unsigned n=0;n<8;++n)f.cycle();
        const auto revision=f.view->tracks[id].sourceRevision;
        for(auto* e:{f.engine.get(),f.control.get()}){
            e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
            check(e->enqueueLoop({LoopAction::startDelay,id,0,e->generation(),0,.217f}),"Late source Start offset");
            check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,reverse?1.f:0.f}),"Late source direction");
        }
        f.command(LoopAction::play,id);f.cycle();const auto q=f.next(MeterGraphEventKind::queuedPlay,id);
        const auto grid=f.view->tracks[id].scheduledFrame;
        // The control creates a fresh source only at the late callback, through
        // the existing immediate-source primitive, independently of the gate.
        check(f.control->enqueueLoop({LoopAction::stopImmediate,id,0,f.control->generation(),0,0}),"Keep comparison source stopped until callback");
        if(outcome==0)check(f.control->enqueueLoop({LoopAction::speed,id,0,f.control->generation(),0,.37f}),"Set fresh comparison source rate before callback");
        while(f.engine->clock()<grid+1104)f.cycle();
        check(f.view->tracks[id].state==LoopState::queued&&f.view->tracks[id].sourceRevision==revision,"Passing grid cannot create source or change queued state before callback");
        LoopCommand birth{LoopAction::meterQueuedGraphRequest,id,0,f.engine->generation(),0,0};
        birth.meterGraphTicket=1;birth.meterQueueId=q.queueId;birth.meterBufferId=q.bufferId;
        if(outcome==0){
            auto bad=birth;++bad.meterBufferId;check(f.engine->enqueueLoop(bad),"Deliver wrong late clip identity");f.cycle();
            (void)f.next(MeterGraphEventKind::queuedGraphRejected,id);
            check(f.view->tracks[id].state==LoopState::queued,"Rejected graph cannot admit queued audio");
            check(f.engine->enqueueLoop({LoopAction::speed,id,0,f.engine->generation(),0,.37f}),"Change rate immediately before fresh source construction");
            birth.meterGraphTicket=2;check(f.engine->enqueueLoop(birth),"Deliver valid late source callback");
            check(f.control->enqueueLoop({LoopAction::sourceRestart,id,0,f.control->generation(),0,0}),"Create comparison source at actual late frame");
            f.cycle();(void)f.next(MeterGraphEventKind::started,id);(void)f.next(MeterGraphEventKind::queuedGraphAccepted,id);
            check(f.view->tracks[id].state==LoopState::playing&&f.view->tracks[id].sourceRevision==revision+1,"Accepted callback creates exactly one fresh source");
            for(unsigned n=0;n<16;++n)f.cycle();
        }else{
            if(outcome==5){auto raw=birth;raw.action=LoopAction::meterQueuedWindowRequest;
                check(f.engine->enqueueLoop(raw),"Raw window reset before source birth");f.cycle();
                (void)f.next(MeterGraphEventKind::started,id);(void)f.next(MeterGraphEventKind::queuedWindowAccepted,id);
                check(f.view->tracks[id].state==LoopState::queued&&f.view->tracks[id].sourceRevision==revision,"Raw acceptance stays silent without source birth");
                birth.meterGraphTicket=2;
            }
            if(outcome==4)f.prepare();else {f.command(outcome==1?LoopAction::stopImmediate:outcome==2?LoopAction::stopAll:outcome==3?LoopAction::clear:LoopAction::stopImmediate,id);f.cycle();}
            check(f.engine->enqueueLoop(birth)==(outcome!=4),"Cancelled callback queues for ownership rejection; stale generation rejects before enqueue");f.cycle();
            check(f.view->tracks[id].state!=LoopState::playing,"Cancelled/clear/prepare callback cannot resurrect source");
            for(unsigned n=0;n<8;++n)f.cycle();
        }
    }
    check(allocations.load()==0,"Late-source processing performs zero tracked callback new/delete");
    std::cout<<"PASS: callback-controlled late source silence/state/phase/reverse, wrong/raw/cancel/clear/prepare guards at tracks 0/19 and 64/128; exact stereo comparison and no callback new/delete\n";
}
void queuedReplacementSpeedTests(bool overlapOnly=false){
    using namespace aion;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})
    for(unsigned sequence:{0u,1u,2u,3u,4u,5u})for(bool early:{false,true})for(auto action:{LoopAction::applyStart,LoopAction::retrigger})for(bool delayed:{false,true}){
        if(delayed&&sequence<4)continue;
        if(overlapOnly&&sequence<3)continue;
        if((sequence==2&&action!=LoopAction::retrigger)||(sequence>=3&&action!=LoopAction::applyStart))continue;
        const bool multiple=sequence!=0;
        Fixture f(block,MeterEventScope::acceptedPlayback);
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);f.prepare();
        f.record(id);(void)f.next(MeterGraphEventKind::bufferInstalled,id);f.command(LoopAction::stopImmediate,id);for(unsigned n=0;n<8;++n)f.cycle();
        for(auto* e:{f.engine.get(),f.control.get()}){e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
            check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,reverse?1.:0.}),"Replacement speed direction");}
        f.command(LoopAction::play,id);f.cycle();const auto q=f.next(MeterGraphEventKind::queuedPlay,id);const auto grid=f.view->tracks[id].scheduledFrame;
        for(auto* e:{f.engine.get(),f.control.get()}){LoopCommand birth{LoopAction::meterQueuedGraphRequest,id,0,e->generation(),0,0};
            birth.meterGraphTicket=1;birth.meterQueueId=q.queueId;birth.meterBufferId=q.bufferId;check(e->enqueueLoop(birth),"Replacement speed source admission");}
        f.cycle();(void)f.next(MeterGraphEventKind::started,id);(void)f.next(MeterGraphEventKind::queuedGraphAccepted,id);
        const auto edit=grid-(early?3840:(action==LoopAction::retrigger?1:960));while(f.engine->clock()+block<edit)f.cycle();
        const auto target=f.engine->clock()+(action==LoopAction::retrigger?240:2400);f.command(action,id);f.cycle();
        check(f.engine->enqueueLoop({LoopAction::speed,id,0,f.engine->generation(),0,.5}),"Speed after replacement has already been created");
        // The unchanged reference changes only the newest source's AudioParam;
        // its future initial-rate event still wins. The single-source control has
        // no speed edit; the double-source control receives only the write
        // required to capture .5 in source two. Exact stereo equality protects
        // the original source and both replacement rates.
        auto lastTarget=target;
        if(multiple){
            // Both engines capture .5 in source two. Only the primary receives
            // the later .25 write; neither earlier source may inherit it.
            check(f.control->enqueueLoop({LoopAction::speed,id,0,f.control->generation(),0,.5}),"Second source comparison control rate");
            if(sequence>=4){
                if(delayed&&sequence==4)while(f.engine->clock()+block<target-128)f.cycle();
                lastTarget=f.engine->clock()+2400;f.command(LoopAction::applyStart,id);f.cycle();
                for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::speed,id,0,e->generation(),0,2}),"Newest Apply receives the next control rate only");
                if(sequence==5){
                    if(delayed)while(f.engine->clock()+block<lastTarget-128)f.cycle();
                    lastTarget=f.engine->clock()+2400;f.command(LoopAction::applyStart,id);f.cycle();
                    for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::speed,id,0,e->generation(),0,1}),"Third Apply receives its own control rate only");
                }
            }
            const auto secondAction=sequence==2?LoopAction::applyStart:sequence>=3?LoopAction::retrigger:action;
            lastTarget=std::max(lastTarget,f.engine->clock()+(secondAction==LoopAction::retrigger?240:2400));
            f.command(secondAction,id);f.cycle();
            check(f.engine->enqueueLoop({LoopAction::speed,id,0,f.engine->generation(),0,.25}),"Speed targets the newest pending source");
        }
        while(f.engine->clock()<std::max({grid,target,lastTarget})+2048)f.cycle();
        check(f.view->tracks[id].playbackRate==(multiple?.25:.5)&&f.control->inspectAudioThread(id).track.playbackRate==(sequence==5?1:sequence==4?2:multiple?.5:1)
            &&!f.view->tracks[id].phaseApplyFrame&&!f.view->rejected,"UI metadata changes while scheduled source rate remains captured");
        for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::speed,id,0,e->generation(),0,.5}),"Explicit same-value speed write after source start");
        // Repeating the displayed value is an actual AudioParam write, not a
        // no-op: both sources now receive the same normal smoothed rate edit.
        for(unsigned n=0;n<32;++n)f.cycle();
        f.command(LoopAction::stopImmediate,id);for(unsigned n=0;n<8;++n)f.cycle();(void)f.next(MeterGraphEventKind::immediateStop,id);
        f.command(LoopAction::play,id);f.cycle();const auto fresh=f.next(MeterGraphEventKind::queuedPlay,id);
        for(auto* e:{f.engine.get(),f.control.get()}){LoopCommand birth{LoopAction::meterQueuedGraphRequest,id,0,e->generation(),0,0};birth.meterGraphTicket=2;birth.meterQueueId=fresh.queueId;birth.meterBufferId=fresh.bufferId;check(e->enqueueLoop(birth),"Fresh source uses current control rate");}
        for(unsigned n=0;n<400;++n)f.cycle();
        check(f.view->tracks[id].state==LoopState::playing&&f.view->tracks[id].playbackRate==.5,"Fresh source retains the speed control after captured-source playback");
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(unsigned sequence:{0u,1u,2u,3u,4u,5u})for(unsigned cancel=0;cancel<5;++cancel)for(bool active:{false,true}){
        if(active&&sequence!=5)continue;
        if(overlapOnly&&sequence<3)continue;
        const bool multiple=sequence!=0;
        Fixture f(block,MeterEventScope::acceptedPlayback);
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);f.prepare();
        f.record(id);(void)f.next(MeterGraphEventKind::bufferInstalled,id);f.command(LoopAction::stopImmediate,id);for(unsigned n=0;n<8;++n)f.cycle();
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
        f.command(LoopAction::play,id);f.cycle();const auto q=f.next(MeterGraphEventKind::queuedPlay,id);
        for(auto* e:{f.engine.get(),f.control.get()}){LoopCommand birth{LoopAction::meterQueuedGraphRequest,id,0,e->generation(),0,0};birth.meterGraphTicket=1;birth.meterQueueId=q.queueId;birth.meterBufferId=q.bufferId;check(e->enqueueLoop(birth),"Mixed cancellation source admission");}
        f.cycle();(void)f.next(MeterGraphEventKind::started,id);(void)f.next(MeterGraphEventKind::queuedGraphAccepted,id);
        const auto firstApplyAt=f.engine->clock()+2400;
        f.command(sequence==2?LoopAction::retrigger:LoopAction::applyStart,id);f.cycle();
        check(f.engine->enqueueLoop({LoopAction::speed,id,0,f.engine->generation(),0,.5}),"Mixed cancellation changes current control only");f.cycle();
        if(multiple){
            check(f.control->enqueueLoop({LoopAction::speed,id,0,f.control->generation(),0,.5}),"Canceled second source comparison rate");
            if(sequence>=4){
                f.command(LoopAction::applyStart,id);f.cycle();
                for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::speed,id,0,e->generation(),0,2}),"Cancellation chain captures separate source rate");
                if(sequence==5){
                    f.command(LoopAction::applyStart,id);f.cycle();
                    for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::speed,id,0,e->generation(),0,1}),"Canceled third Apply captures its own source rate");
                }
            }
            f.command(sequence>=3?LoopAction::retrigger:LoopAction::applyStart,id);f.cycle();
            check(f.engine->enqueueLoop({LoopAction::speed,id,0,f.engine->generation(),0,.25}),"Canceled newest source rate write");f.cycle();
        }
        if(active)while(f.engine->clock()<firstApplyAt+block)f.cycle(); // Retained intermediate PCM has started.
        if(cancel<3)f.command(cancel==0?LoopAction::stopImmediate:cancel==1?LoopAction::stopAll:LoopAction::clear,id);
        else if(cancel==3)f.prepare();
        else for(auto* e:{f.engine.get(),f.control.get()})check(e->retireQueuedSourceThrough(e->generation(),id,1),"Mixed cleanup fence");
        for(unsigned n=0;n<400;++n)f.cycle();
        check(f.view->tracks[id].state==(cancel==2?LoopState::empty:LoopState::stopped)&&!f.view->tracks[id].phaseApplyFrame&&!f.view->tracks[id].queuedSourceReady,"Canceled source rate events cannot resurrect after their old deadlines");
    }
    check(allocations.load()==0,"Scheduled replacement speed performs no callback new/delete");
    if(overlapOnly)std::cout<<"PASS: focused 16 single +32 two-Apply +32 three-Apply early/late overlap playback and 20 single +20 two-Apply +40 pending/active three-Apply cancellation cases, exact paired stereo at tracks 0/19, reverse and 64/128; no callback new/delete\n";
    else std::cout<<"PASS: 32 single + 32 double + 16 RTRG-to-Apply + 16 Apply-to-RTRG +32 two-Apply +32 three-Apply early/late chain overlap queued speed cases, exact old/replacement stereo PCM before/after grid, explicit same-value AudioParam write and fresh play at tracks 0/19, reverse and 64/128; 20 single + 20 double + 20 RTRG-to-Apply + 20 Apply-to-RTRG +20 two-Apply +40 pending/active three-Apply chain cancel/prepare/cleanup cases; no callback new/delete\n";
}
void queuedOverlapCapacityTests(){
    using namespace aion;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u}){
        Fixture f(block,MeterEventScope::acceptedPlayback);f.record(id);
        for(unsigned batch:{80u,48u}){
            for(unsigned n=0;n<batch;++n){f.command(LoopAction::applyStart,id);f.command(LoopAction::retrigger,id);}
            f.cycle();for(unsigned n=0;n<4;++n)f.cycle(); // Deliver 5ms starts, retain the older 50ms stops.
            check(!f.view->rejected,"All 128 bounded overlap reservations are accepted");
        }
        f.command(LoopAction::applyStart,id);f.command(LoopAction::retrigger,id);f.cycle();
        check(f.view->rejected==1&&f.view->tracks[id].phaseApplyFrame>f.engine->clock(),"Full overlap pool rejects RTRG without canceling the accepted Apply");
        f.command(LoopAction::stopImmediate,id);for(unsigned n=0;n<8;++n)f.cycle();
        check(f.view->tracks[id].state==LoopState::stopped&&!f.view->tracks[id].phaseApplyFrame,"Stop releases active and reserved overlap ownership");
        f.command(LoopAction::play,id);f.cycle();f.command(LoopAction::applyStart,id);f.command(LoopAction::retrigger,id);f.cycle();
        check(!f.view->rejected&&f.view->tracks[id].phaseApplyFrame>f.engine->clock(),"Released overlap slots can be reserved again with an accepted new source event");
        f.command(LoopAction::clear,id);for(unsigned n=0;n<64;++n)f.cycle();
        check(f.view->tracks[id].state==LoopState::empty&&!f.view->tracks[id].phaseApplyFrame,"Clear releases overlap clip leases and cannot resurrect deadlines");
    }
    check(allocations.load()==0,"Overlap saturation and recovery perform no callback new/delete");
    std::cout<<"PASS: 128 overlap slots, explicit saturation rejection without partial phase cancellation, stop/clear release and reuse at tracks 0/19, 64/128; no callback new/delete\n";
}
void queuedChainCapacityTests(){
    using namespace aion;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u}){
        Fixture f(block,MeterEventScope::acceptedPlayback);const auto other=id==0?19u:0u;f.record(id);f.record(other);
        // At most 96 commands in either block: both Apply births keep distinct
        // sample deadlines without exceeding the existing 128-command budget.
        const auto scheduled=[&](LoopAction action,std::uint64_t when){++f.request;
            for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({action,id,f.request,e->generation(),when,0}),"Queue sample-timed two-slot chain");};
        for(unsigned count:{32u,31u}){const auto base=f.engine->clock();
            for(unsigned n=0;n<count;++n){scheduled(LoopAction::applyStart,base+2*n);scheduled(LoopAction::applyStart,base+2*n+1);scheduled(LoopAction::retrigger,base+2*n+1);}
            f.cycle();check(!f.view->rejected&&f.view->lastRequestId==f.request,"126 chain reservations fit with independent intermediate deadlines");
        }
        f.command(LoopAction::applyStart,id);f.command(LoopAction::retrigger,id);f.cycle(); // 127 slots used.
        f.command(LoopAction::applyStart,id);f.cycle();const auto kept=f.engine->clock()+2400;
        f.command(LoopAction::applyStart,id);f.command(LoopAction::retrigger,id);f.cycle();
        check(f.view->rejected&&f.view->tracks[id].phaseApplyFrame==kept,"Two-slot chain rejects with one free slot and preserves both accepted Apply events");
        f.command(LoopAction::applyStart,other);f.command(LoopAction::retrigger,other);f.cycle();
        check(!f.view->rejected,"Rejected two-slot reservation leaves the final slot available to another track");
        f.command(LoopAction::stopAll,id);for(unsigned n=0;n<8;++n)f.cycle();
        check(f.view->tracks[id].state==LoopState::stopped&&f.view->tracks[other].state==LoopState::stopped&&!f.view->tracks[id].phaseApplyFrame,"Panic releases scheduled and active intermediate clip leases");
        f.command(LoopAction::play,id);f.cycle();f.command(LoopAction::applyStart,id);f.cycle();f.command(LoopAction::applyStart,id);f.command(LoopAction::retrigger,id);f.cycle();
        check(!f.view->rejected&&f.view->tracks[id].phaseApplyFrame>f.engine->clock(),"Two-slot chain recovers after panic");
        f.command(LoopAction::clear,id);for(unsigned n=0;n<64;++n)f.cycle();
        check(f.view->tracks[id].state==LoopState::empty&&!f.view->tracks[id].phaseApplyFrame,"Clear cancels every intermediate source without resurrection");
    }
    check(allocations.load()==0,"Atomic multi-slot chain reservation and recovery perform no callback new/delete");
    std::cout<<"PASS: two-slot chain atomic rejection with 127 occupied slots, surviving one-slot cross-track admission, panic/clear release and recovery at tracks 0/19, 64/128; no callback new/delete\n";
}
void queuedTripleCapacityTests(){
    using namespace aion;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u}){
        Fixture f(block,MeterEventScope::acceptedPlayback);const auto other=id==0?19u:0u;f.record(id);f.record(other);
        const auto scheduled=[&](LoopAction action,std::uint64_t when){++f.request;
            for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({action,id,f.request,e->generation(),when,0}),"Queue sample-timed three-slot chain");};
        // 24 / 18 chains reserve 126 slots. Each block stays within 96 commands;
        // distinct one-sample Apply deadlines require three source leases.
        for(unsigned count:{24u,18u}){const auto base=f.engine->clock();
            for(unsigned n=0;n<count;++n){scheduled(LoopAction::applyStart,base+3*n);scheduled(LoopAction::applyStart,base+3*n+1);scheduled(LoopAction::applyStart,base+3*n+2);scheduled(LoopAction::retrigger,base+3*n+2);}
            // At block 64 the first batch spans two callbacks; retain enough
            // time to dispatch its final commands before checking acceptance.
            f.cycle();if(count*3>block)f.cycle();
            check(!f.view->rejected&&f.view->lastRequestId==f.request,"126 three-slot reservations fit without command-budget overflow");
        }
        f.command(LoopAction::applyStart,id);f.cycle();f.command(LoopAction::applyStart,id);f.cycle();
        const auto kept=f.engine->clock()+2400;f.command(LoopAction::applyStart,id);f.command(LoopAction::retrigger,id);f.cycle();
        check(f.view->rejected&&f.view->tracks[id].phaseApplyFrame==kept,"Three-slot request rejects with two free slots and preserves accepted Apply deadlines");
        f.command(LoopAction::applyStart,other);f.cycle();f.command(LoopAction::applyStart,other);f.command(LoopAction::retrigger,other);f.cycle();
        check(!f.view->rejected,"Failed three-slot request leaves both slots available to a second track");
        f.command(LoopAction::stopAll,id);for(unsigned n=0;n<8;++n)f.cycle();
        check(f.view->tracks[id].state==LoopState::stopped&&f.view->tracks[other].state==LoopState::stopped&&!f.view->tracks[id].phaseApplyFrame,"Panic releases all three-source chain leases");
        f.command(LoopAction::play,id);f.cycle();for(unsigned n=0;n<3;++n){f.command(LoopAction::applyStart,id);f.cycle();}
        f.command(LoopAction::retrigger,id);f.cycle();
        check(!f.view->rejected&&f.view->tracks[id].phaseApplyFrame>f.engine->clock(),"Three-slot chain recovers after panic");
        f.command(LoopAction::clear,id);for(unsigned n=0;n<64;++n)f.cycle();
        check(f.view->tracks[id].state==LoopState::empty&&!f.view->tracks[id].phaseApplyFrame,"Clear releases all triple chain leases without resurrection");
    }
    check(allocations.load()==0,"Atomic three-slot reservation performs no callback new/delete");
    std::cout<<"PASS: three-slot atomic rejection with 126 occupied slots, two-slot cross-track admission, panic/clear release and recovery at tracks 0/19, 64/128; no callback new/delete\n";
}
void stoppingNormTests(){
    using namespace aion;unsigned cases=0;
    const auto setup=[](Fixture& f,unsigned id,bool reverse,bool unity,bool phase=false){
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);f.prepare();f.record(id);
        f.command(LoopAction::overdub,id);f.cycle();f.command(LoopAction::finish,id);f.cycle(true);
        if(unity){f.command(LoopAction::normalize,id);f.cycle(true);}
        f.command(LoopAction::stopImmediate,id);f.cycle();
        for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,reverse?1.:0.}),"Stopping NORM source direction");
        f.command(LoopAction::play,id);f.cycle();
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
        if(phase)f.command(LoopAction::applyStart,id);
        f.command(LoopAction::stop,id);f.cycle();check(f.view->tracks[id].state==LoopState::stopping&&f.view->tracks[id].undoCount==1,"NORM fixture has real STOP deadline and existing history");
    };
    const auto held=[](){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(heldWorkerAllocations.load()<2&&std::chrono::steady_clock::now()<end)std::this_thread::yield();
        check(heldWorkerAllocations.load()>=2,"Stopping NORM worker allocation held outside processing");};
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(bool unity:{false,true})for(bool delayed:{false,true})for(bool phase:{false,true}){
        Fixture f(block);setup(f,id,reverse,unity,phase);const auto before=f.view->tracks[id];
        if(delayed){heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);}
        f.command(LoopAction::normalize,id);f.cycle(!delayed);
        if(delayed){held();check(!f.view->rejected&&f.view->tracks[id].state==LoopState::finalizing,"Stopping NORM enters bounded worker path");
            while(f.engine->clock()<before.scheduledFrame+512)f.cycle();
            check(!f.view->tracks[id].scheduledFrame&&!f.view->tracks[id].phaseApplyFrame,"STOP and earlier phase deadlines resolve while NORM is pending");
            holdWorkerAllocation.store(false);f.cycle(true);
        }else{check(!f.view->rejected&&f.view->tracks[id].state==LoopState::stopping&&f.view->tracks[id].scheduledFrame==before.scheduledFrame,"NORM retains exact stopping state and deadline");
            while(f.engine->clock()<before.scheduledFrame+512)f.cycle();}
        const auto after=f.view->tracks[id];check(after.state==LoopState::stopped&&!after.scheduledFrame&&!after.finalizationFailed
            &&after.waveformId!=before.waveformId&&after.length==before.length&&after.undoCount==before.undoCount&&after.redoCount==before.redoCount,"Changed/no-op NORM commits without new history or post-STOP revival");
        f.command(LoopAction::undoGlobal,id);f.cycle();check(f.view->tracks[id].state==LoopState::stopped&&f.view->tracks[id].redoCount==1,"NORM preserves preexisting global history after STOP");++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true}){
        Fixture f(block);setup(f,id,reverse,false);const auto before=f.view->tracks[id];failWorkerAllocation.store(true);
        check(f.engine->enqueueLoop({LoopAction::normalize,id,++f.request,f.engine->generation(),0,0}),"Stopping NORM OOM observed engine only");f.cycle(true);
        check(!failWorkerAllocation.load()&&f.view->tracks[id].finalizationFailed&&f.view->tracks[id].waveformId==before.waveformId
            &&f.view->tracks[id].state==LoopState::stopping&&f.view->tracks[id].scheduledFrame==before.scheduledFrame&&f.view->tracks[id].undoCount==before.undoCount,"NORM OOM preserves original PCM/history/STOP");
        while(f.engine->clock()<before.scheduledFrame+512)f.cycle();++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(unsigned cancel=0;cancel<6;++cancel){
        Fixture f(block);setup(f,id,reverse,false);heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);f.command(LoopAction::normalize,id);f.cycle();held();
        if(cancel<4){f.command(cancel==0?LoopAction::stopImmediate:cancel==1?LoopAction::stopAll:cancel==2?LoopAction::clear:LoopAction::clearAll,id);f.cycle();}
        holdWorkerAllocation.store(false);
        if(cancel==4)f.prepare();else if(cancel==5)for(auto* e:{f.engine.get(),f.control.get()}){e->prepare(96000,81920);e->setMonitoring(false);e->setTarget(1);}else f.cycle(true);
        for(unsigned n=0;n<400;++n)f.cycle();
        check(f.view->tracks[id].state==((cancel==2||cancel==3||cancel==5)?LoopState::empty:LoopState::stopped)&&!f.view->tracks[id].scheduledFrame,"Stopping NORM cancel/prepare never revives playback");++cases;
    }
    check(allocations.load()==0,"Stopping NORM zero callback new/delete");
    std::cout<<"PASS: stopping NORM "<<cases<<" Engine 64/128, forward/reverse changed/no-op, exact STOP/phase deadlines, preserved history, OOM/clear/prepare and zero callback new/delete\n";
}

void queuedNormTests(){
    using namespace aion;
    const auto setup=[](Fixture& f,unsigned id,bool reverse,bool unity){
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);f.prepare();f.record(id);
        if(unity){f.command(LoopAction::normalize,id);f.cycle(true);}
        f.command(LoopAction::stopImmediate,id);for(unsigned n=0;n<8;++n)f.cycle();
        MeterGraphEvent discarded;while(f.engine->popMeterGraphEvent(discarded)){}
        for(auto* e:{f.engine.get(),f.control.get()}){
            e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
            check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,reverse?1.:0.}),"Queued NORM source direction");
        }
        f.command(LoopAction::play,id);f.cycle();const auto q=f.next(MeterGraphEventKind::queuedPlay,id);
        for(auto* e:{f.engine.get(),f.control.get()}){LoopCommand birth{LoopAction::meterQueuedGraphRequest,id,0,e->generation(),0,0};
            birth.meterGraphTicket=1;birth.meterQueueId=q.queueId;birth.meterBufferId=q.bufferId;check(e->enqueueLoop(birth),"Queued NORM graph request");}
        f.cycle();(void)f.next(MeterGraphEventKind::started,id);(void)f.next(MeterGraphEventKind::queuedGraphAccepted,id);
        check(f.view->tracks[id].state==LoopState::queued&&f.view->tracks[id].queuedSourceReady,"Queued NORM starts with a constructed inaudible source");
    };
    const auto waitHeld=[](){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(heldWorkerAllocations.load()<2&&std::chrono::steady_clock::now()<end)std::this_thread::yield();
        check(heldWorkerAllocations.load()>=2,"Queued NORM workers pause outside processing");};
    unsigned cases=0;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(bool unity:{false,true})for(unsigned stage=0;stage<3;++stage){
        Fixture f(block);setup(f,id,reverse,unity);const auto grid=f.view->tracks[id].scheduledFrame,buffer=f.view->tracks[id].waveformId;
        while(f.engine->clock()+block<grid-3840)f.cycle();
        if(stage){f.command(LoopAction::applyStart,id);f.cycle();if(stage==2){f.command(LoopAction::retrigger,id);f.cycle();}}
        const auto pending=f.view->tracks[id].phaseApplyFrame,serial=f.view->tracks[id].sourceRevision;
        heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);f.command(LoopAction::normalize,id);f.cycle();waitHeld();
        check(!f.view->rejected&&f.view->tracks[id].state==LoopState::finalizing&&f.view->tracks[id].phaseApplyFrame==(pending&&pending<f.engine->clock()?0:pending),"Queued NORM preserves accepted phase deadlines while worker runs");
        while(f.engine->clock()<grid+block){f.cycle();
            if(f.engine->clock()<=grid)check(f.view->outputPeak==0,"Worker-delayed queued NORM remains silent before the original grid");
        }
        check(!f.view->tracks[id].scheduledFrame&&!f.view->tracks[id].phaseApplyFrame&&f.view->tracks[id].sourceRevision>serial,"Original source or pending replacement starts without waiting for NORM");
        holdWorkerAllocation.store(false);f.cycle(true);
        check(f.view->tracks[id].state==LoopState::playing&&f.view->tracks[id].waveformId!=buffer&&!f.view->tracks[id].finalizationFailed
            &&!f.view->tracks[id].undoCount&&!f.view->tracks[id].redoCount,"Queued NORM commits after grid without resetting source/history");
        f.command(LoopAction::clear,id);for(unsigned n=0;n<64;++n)f.cycle();++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(unsigned cancel=0;cancel<6;++cancel){
        Fixture f(block);setup(f,id,reverse,false);
        if(cancel==5){f.command(LoopAction::stop,id);f.cycle();check(f.view->tracks[id].stopPending,"Queued NORM scheduled stop fixture");}
        heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);f.command(LoopAction::normalize,id);f.cycle();waitHeld();
        if(cancel==5)while(f.engine->clock()<=f.view->tracks[id].scheduledFrame||f.view->tracks[id].scheduledFrame)f.cycle();
        if(cancel<3){f.command(cancel==0?LoopAction::stopImmediate:cancel==1?LoopAction::stopAll:LoopAction::clear,id);f.cycle();}
        holdWorkerAllocation.store(false);
        if(cancel==3)f.prepare();
        else if(cancel==4)for(auto* e:{f.engine.get(),f.control.get()}){e->prepare(96000,81920);e->setMonitoring(false);e->setTarget(1);}
        else f.cycle(true);
        for(unsigned n=0;n<400;++n)f.cycle();
        check(f.view->tracks[id].state==((cancel==2||cancel==4)?LoopState::empty:LoopState::stopped)&&!f.view->tracks[id].scheduledFrame,"Queued NORM cancellation/prepare cannot resurrect source");++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true}){
        Fixture f(block);setup(f,id,reverse,false);const auto grid=f.view->tracks[id].scheduledFrame,buffer=f.view->tracks[id].waveformId;
        check(f.engine->enqueueLoop({LoopAction::normalize,id,++f.request,f.engine->generation(),0,0}),"Queued OOM NORM on observed engine only");
        failWorkerAllocation.store(true);f.cycle(true);
        check(!failWorkerAllocation.load()&&f.view->tracks[id].finalizationFailed&&f.view->tracks[id].waveformId==buffer
            &&f.view->tracks[id].state==LoopState::queued&&f.view->tracks[id].scheduledFrame==grid,"Queued NORM OOM preserves original grid/PCM");
        while(f.engine->clock()<grid+2048)f.cycle();++cases; // Exact stereo against the unedited engine.
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u}){
        Fixture f(block);setup(f,id,true,false);f.command(LoopAction::stopImmediate,id);f.cycle();f.command(LoopAction::play,id);f.cycle();
        const auto grid=f.view->tracks[id].scheduledFrame;
        f.command(LoopAction::normalize,id);f.cycle(true);
        check(!f.view->rejected&&f.view->tracks[id].state==LoopState::queued&&!f.view->tracks[id].queuedSourceReady
            &&f.view->tracks[id].scheduledFrame==grid&&f.view->outputPeak==0,"Pre-birth NORM keeps the original graphless queue and silence");++cases;
    }
    check(allocations.load()==0,"Queued NORM performs no callback new/delete");
    std::cout<<"PASS: queued-source NORM "<<cases<<" delayed changed/no-op, Apply/RTRG, stop/clear/same/new-rate prepare and exact OOM rollback cases; tracks 0/19, reverse, 64/128, no callback new/delete\n";
}

void queuedHistoryTests(){
    using namespace aion;
    const auto setup=[](Fixture& f,unsigned id,bool reverse,bool global,unsigned capacity=81920){
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        for(auto* e:{f.engine.get(),f.control.get()}){e->prepare(48000,capacity,{true,48000,0});e->setMonitoring(false);e->setTarget(1);}
        auto owner=std::make_unique<LoopMeterLifecycle>(*f.engine,id);
        f.command(LoopAction::record,id);for(unsigned n=0;n<(capacity==8192?4096u:512u)/f.block;++n)f.cycle();f.command(LoopAction::finish,id);f.cycle(true);
        check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"History initial PCM");
        if(global){f.command(LoopAction::overdub,id);f.cycle();f.command(LoopAction::finish,id);f.cycle(true);
            check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"History globally indexed edit");}
        f.command(LoopAction::stopImmediate,id);for(unsigned n=0;n<8;++n)f.cycle();
        if(!global){f.command(LoopAction::multiply,id);f.cycle(true);check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"History length-changing local edit");}
        for(auto* e:{f.engine.get(),f.control.get()}){
            e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
            check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,reverse?1.:0.}),"History direction");}
        f.command(LoopAction::play,id);f.cycle(false,{100,true});
        check(owner->consumeQueuedPlay(f.next(MeterGraphEventKind::queuedPlay,id)),"History initial queued receipt");return owner;
    };
    const auto birth=[](Fixture& f,LoopMeterLifecycle& owner,unsigned id,const QueuedMeterCallbackTime& expected){
        check(owner.requestQueuedGraphDue(expected,73,{73,expected.deadlineSeconds}),"History original callback requests once");
        const auto request=owner.queuedWindowTransaction();LoopCommand mirror{LoopAction::meterQueuedGraphRequest,id,0,f.control->generation(),0,0};
        mirror.meterGraphTicket=request.ticket;mirror.meterQueueId=request.queueId;mirror.meterBufferId=request.bufferId;
        check(f.control->enqueueLoop(mirror),"History paired callback");f.cycle();(void)f.next(MeterGraphEventKind::started,id);
        check(owner.consumeQueuedWindowResult(f.next(MeterGraphEventKind::queuedGraphAccepted,id)),"History accepts original receipt against restored PCM");
        check(!owner.requestQueuedGraphDue(expected,73,{73,expected.deadlineSeconds+1}),"History receipt is one-shot");
    };
    unsigned cases=0;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(bool global:{false,true})for(bool late:{false,true}){
        Fixture f(block);auto owner=setup(f,id,reverse,global);QueuedMeterCallbackTime expected;
        check(owner->readQueuedCallbackTime(73,{73,100},expected),"History original callback time");
        const auto grid=f.view->tracks[id].scheduledFrame;const auto identity=owner->inspect();
        const auto restore=[&](bool redo){const auto before=f.view->tracks[id];
            f.command(redo?(global?LoopAction::redoGlobal:LoopAction::redo):(global?LoopAction::undoGlobal:LoopAction::undo),id);f.cycle();
            const auto installed=f.next(MeterGraphEventKind::bufferInstalled,id);auto wrong=installed;wrong.queueId+=(1ull<<32);
            check(!owner->consumeBuffer(wrong)&&owner->consumeBuffer(installed),"History requires full-width explicit queue continuation");
            const auto& t=f.view->tracks[id];QueuedMeterCallbackTime current;
            check(!f.view->rejected&&t.state==LoopState::queued&&!t.queuedSourceReady&&t.scheduledFrame==grid&&f.view->outputPeak==0
                &&t.waveformId!=before.waveformId&&t.undoCount==(redo?1u:0u)&&t.redoCount==(redo?0u:1u)
                &&t.length==(global?512u:redo?1024u:512u),"History swaps exact storage/depth/length without starting or moving playback");
            check(owner->readQueuedCallbackTime(73,{73,100},current)&&current==expected&&owner->queuedCallbackBuffer()==expected.bufferId
                &&owner->committedBuffer()==installed.bufferId&&owner->inspect().data==identity.data,"History preserves exact original receipt and analyser/data");};
        restore(false);restore(true);restore(false);
        if(late){while(f.engine->clock()<grid+block){f.cycle();check(f.view->outputPeak==0,"Past-grid history remains silent before callback");}restore(true);}
        birth(f,*owner,id,expected);
        if(!late){const auto target=f.engine->clock()+240;f.command(global?LoopAction::redoGlobal:LoopAction::redo,id);f.cycle();
            check(!f.view->rejected&&owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id))
                &&f.view->tracks[id].redoCount==0&&f.view->tracks[id].undoCount==1&&f.view->tracks[id].phaseApplyFrame==target
                &&f.view->outputPeak==0,"After-birth queued history preserves the case with accepted 5ms replacement, restored depth and grid silence");}
        bool audible=false;while(f.engine->clock()<grid+2048){f.cycle();if(f.engine->clock()<=grid)check(f.view->outputPeak==0,"Restored source respects original grid silence");
            if(f.engine->clock()>grid+512)audible=audible||f.view->outputPeak>0;}
        check(f.view->tracks[id].state==LoopState::playing&&audible,"Original queued receipt starts restored source");
        f.command(LoopAction::clear,id);f.cycle();check(owner->consumeClear(f.next(MeterGraphEventKind::cleared,id)),"History clear retires restored ownership");++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(unsigned cancel=0;cancel<5;++cancel){
        Fixture f(block);auto owner=setup(f,id,true,false);QueuedMeterCallbackTime expected;check(owner->readQueuedCallbackTime(73,{73,100},expected),"History cancellation receipt");
        f.command(LoopAction::undo,id);f.cycle();check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"History before cancellation");
        if(cancel>=3){
            if(cancel==3)f.prepare();else for(auto* e:{f.engine.get(),f.control.get()}){e->prepare(96000,81920);e->setMonitoring(false);e->setTarget(1);}
            check(owner->reset(),"Prepared history owner resets");}
        else {f.command(cancel==0?LoopAction::stopImmediate:cancel==1?LoopAction::stopAll:LoopAction::clear,id);f.cycle();
            const auto event=f.next(cancel==0?MeterGraphEventKind::queuedStop:cancel==1?MeterGraphEventKind::queuedCancel:MeterGraphEventKind::cleared,id);
            check(cancel==0?owner->consumeQueuedStop(event):cancel==1?owner->consumeQueuedCancel(event):owner->consumeClear(event),"History consumes authoritative cancellation");}
        check(!owner->requestQueuedGraphDue(expected,73,{73,expected.deadlineSeconds+1}),"History old callback cannot resurrect canceled/cleared/prepared queue");++cases;
    }

    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u}){
        Fixture f(block);auto owner=setup(f,id,true,false,8192);QueuedMeterCallbackTime expected;
        check(owner->readQueuedCallbackTime(73,{73,100},expected),"History exhausted-capacity receipt");
        f.command(LoopAction::undo,id);f.cycle();check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"History frees current storage capacity");
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::off,137.3,1,1,4,false});
        const auto other=id?0u:19u;f.record(other);(void)f.next(MeterGraphEventKind::bufferInstalled,other);
        f.command(LoopAction::stopImmediate,other);for(unsigned n=0;n<8;++n)f.cycle();
        const auto before=f.view->tracks[id];f.command(LoopAction::redo,id);f.cycle();MeterGraphEvent event;
        check(f.view->rejected&&f.view->tracks[id].waveformId==before.waveformId&&f.view->tracks[id].length==before.length
            &&f.view->tracks[id].redoCount==before.redoCount&&!f.engine->popMeterGraphEvent(event),"Capacity failure preserves exact history/PCM without a false continuation");
        QueuedMeterCallbackTime current;check(owner->readQueuedCallbackTime(73,{73,100},current)&&current==expected,"Failed growing restore retains original timer");
        birth(f,*owner,id,expected);++cases;
    }

    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool saturated:{false,true}){
        Fixture f(block);auto owner=setup(f,id,false,false);QueuedMeterCallbackTime expected;
        check(owner->readQueuedCallbackTime(73,{73,100},expected),"History delayed-consumer receipt");
        f.command(LoopAction::undo,id);f.cycle(); // Keep bufferInstalled unconsumed on message owner.
        if(saturated){for(unsigned n=0;n<255;++n)f.command(LoopAction::mute,id);
            check(!owner->requestQueuedGraphDue(expected,73,{73,expected.deadlineSeconds})&&!owner->queuedWindowTransaction().ticket,"FIFO failure preserves history callback for retry");f.cycle();}
        check(owner->requestQueuedGraphDue(expected,73,{73,expected.deadlineSeconds}),"Old receipt requests callback before message sees history PCM");
        const auto request=owner->queuedWindowTransaction();
        check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id))&&owner->queuedWindowTransaction().ticket==request.ticket,"Ordered history continuation retains pending callback transaction");
        LoopCommand mirror{LoopAction::meterQueuedGraphRequest,id,0,f.control->generation(),0,0};mirror.meterGraphTicket=request.ticket;mirror.meterQueueId=request.queueId;mirror.meterBufferId=request.bufferId;
        check(f.control->enqueueLoop(mirror),"History delayed-consumer paired callback");f.cycle();(void)f.next(MeterGraphEventKind::started,id);
        check(owner->consumeQueuedWindowResult(f.next(MeterGraphEventKind::queuedGraphAccepted,id)),"History delayed buffer notice cannot obsolete its accepted source");++cases;
    }
    check(allocations.load()==0,"Queued history has no callback new/delete");
    std::cout<<"PASS: queued Undo/Redo "<<cases<<" local/global early/late, length/depth, original receipt/grid, full-width validation, accepted source-boundary replacement and stop/panic/clear/prepare cases; tracks 0/19, reverse, 64/128, exact paired stereo and no callback new/delete\n";
}


void postbirthHistoryTests(){
    using namespace aion;
    const auto setup=[](Fixture& f,unsigned id,bool reverse){
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);f.prepare();
        auto owner=std::make_unique<LoopMeterLifecycle>(*f.engine,id);f.record(id);check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Post-birth history seed");
        f.command(LoopAction::stopImmediate,id);for(unsigned n=0;n<8;++n)f.cycle();
        f.command(LoopAction::multiply,id);f.cycle(true);check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Post-birth history doubled seed");
        for(auto* e:{f.engine.get(),f.control.get()}){e->configureSync({SyncMode::fixed,137.3,1,4,4,false});
            check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,reverse?1.:0.}),"Post-birth direction");}
        f.command(LoopAction::play,id);f.cycle();const auto q=f.next(MeterGraphEventKind::queuedPlay,id);check(owner->consumeQueuedPlay(q)&&owner->requestQueuedGraph(q.queueId,q.bufferId),"Post-birth explicit graph request");
        const auto request=owner->queuedWindowTransaction();LoopCommand mirror{LoopAction::meterQueuedGraphRequest,id,0,f.control->generation(),0,0};
        mirror.meterGraphTicket=request.ticket;mirror.meterQueueId=request.queueId;mirror.meterBufferId=request.bufferId;check(f.control->enqueueLoop(mirror),"Post-birth paired graph");
        f.cycle();(void)f.next(MeterGraphEventKind::started,id);check(owner->consumeQueuedWindowResult(f.next(MeterGraphEventKind::queuedGraphAccepted,id)),"Post-birth owns constructed source");return owner;
    };
    unsigned cases=0;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(bool nearGrid:{false,true})for(unsigned sequence=0;sequence<6;++sequence){
        Fixture f(block);auto owner=setup(f,id,reverse);const auto grid=f.view->tracks[id].scheduledFrame;const auto identity=owner->inspect();
        if(nearGrid)while(f.engine->clock()+block<grid-48)f.cycle();
        if(sequence==2){f.command(LoopAction::applyStart,id);f.cycle();f.command(LoopAction::applyStart,id);f.cycle();}
        const auto target=f.engine->clock()+240;f.command(LoopAction::undo,id);f.cycle();
        check(!f.view->rejected&&f.view->tracks[id].length==512&&f.view->tracks[id].undoCount==0&&f.view->tracks[id].redoCount==1
            &&f.view->tracks[id].phaseApplyFrame==target&&owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"History swaps storage and admits exact 5ms source without resetting graph");
        if(sequence==1||sequence==4||sequence==5){f.command(LoopAction::redo,id);f.cycle();check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Rapid post-birth redo");
            if(sequence!=5){f.command(LoopAction::undo,id);f.cycle();check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Rapid post-birth undo");}}
        if(sequence==3||sequence==4||sequence==5){f.command(LoopAction::normalize,id);f.cycle(true);check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"NORM redirects matching forward buffers only");}
        bool audible=false;while(f.engine->clock()<grid+4096){f.cycle();if(f.engine->clock()<=grid)check(f.view->outputPeak==0,"History replacements retain original pre-grid silence");
            if(f.engine->clock()>grid+512)audible=audible||f.view->outputPeak>0;}
        check(audible&&owner->inspect().analyser==identity.analyser&&owner->inspect().data==identity.data,"History source and overlap remain audible without replacing analyser/data");
        f.command(LoopAction::clear,id);f.cycle();check(owner->consumeClear(f.next(MeterGraphEventKind::cleared,id)),"Post-birth clear");++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(unsigned cancel=0;cancel<5;++cancel){
        Fixture f(block);auto owner=setup(f,id,true);f.command(LoopAction::undo,id);f.cycle();check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"History before pending-source cancellation");
        if(cancel>=3){if(cancel==3)f.prepare();else for(auto* e:{f.engine.get(),f.control.get()}){e->prepare(96000,81920);e->setMonitoring(false);e->setTarget(1);}}
        else {f.command(cancel==0?LoopAction::stopImmediate:cancel==1?LoopAction::stopAll:LoopAction::clear,id);f.cycle();}
        for(unsigned n=0;n<128;++n)f.cycle();
        check(f.view->tracks[id].state==((cancel==2||cancel==4)?LoopState::empty:LoopState::stopped)&&!f.view->tracks[id].phaseApplyFrame,"Canceled history replacement never resurrects a source");++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool overlaps:{false,true}){
        Fixture f(block);auto owner=setup(f,id,false);
        if(overlaps){for(unsigned batch=0;batch<4;++batch){for(unsigned n=0;n<32;++n){f.command(LoopAction::applyStart,id);f.command(LoopAction::retrigger,id);}f.cycle();for(unsigned n=0;n<240/block+1;++n)f.cycle();}
            f.command(LoopAction::applyStart,id);f.cycle();}
        else for(unsigned n=0;n<127;++n)f.command(LoopAction::retrigger,id);
        const auto before=f.view->tracks[id];f.command(LoopAction::undo,id);f.cycle();MeterGraphEvent event;
        check(f.view->rejected&&f.view->tracks[id].waveformId==before.waveformId&&f.view->tracks[id].undoCount==before.undoCount
            &&f.view->tracks[id].redoCount==before.redoCount&&!f.engine->popMeterGraphEvent(event),"Full event/overlap reservations reject history atomically before swapping PCM/depth");
        f.command(LoopAction::clear,id);for(unsigned n=0;n<128;++n)f.cycle();check(f.view->tracks[id].state==LoopState::empty,"Full reservation clear releases source leases");++cases;
    }
    check(allocations.load()==0,"Post-birth history and source leases have no callback new/delete");
    std::cout<<"PASS: post-birth history "<<cases<<" 5ms/length/rapid/Apply/versioned NORM, grid silence, analyser/data, stop/clear/prepare and full event/overlap atomic rejection cases at tracks 0/19, reverse, 64/128; exact paired stereo and no callback new/delete\n";
}

// MERGE reads immutable stored PCM; it must not take ownership of the source's
// pending PLAY receipt, constructed graph, reversed snapshot or original grid.
void stoppingMergeTests(){
    using namespace aion;unsigned cases=0;
    const auto setup=[](Fixture& f,unsigned id,bool reverse,unsigned kind,bool clone=false){
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);f.prepare();
        f.record(19-id);f.command(LoopAction::stopImmediate,19-id);f.cycle();
        if(!clone){f.record(id);f.command(LoopAction::stopImmediate,id);f.cycle();}
        for(auto* e:{f.engine.get(),f.control.get()})for(unsigned track:{id,19-id})check(e->enqueueLoop({LoopAction::reverse,track,0,e->generation(),0,reverse?1.:0.}),"Stopping MERGE reverse source metadata");
        if(kind!=1){f.command(LoopAction::play,19-id);f.cycle();}
        if(kind!=0){f.command(LoopAction::play,id);f.cycle();}
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
        if(kind!=1){f.command(LoopAction::stop,19-id);f.cycle();check(f.view->tracks[19-id].state==LoopState::stopping,"Source has actual scheduled stop");}
        if(kind!=0){f.command(LoopAction::applyStart,id);f.command(LoopAction::stop,id);f.cycle();check(f.view->tracks[id].state==LoopState::stopping,"Destination has actual scheduled stop with pending phase");}
    };
    const auto merge=[](Fixture& f,unsigned id,bool paired=true){
        check(f.engine->enqueueLoop({LoopAction::merge,id,++f.request,f.engine->generation(),0,static_cast<double>(19-id)}),"Stopping MERGE observed command");
        if(paired)check(f.control->enqueueLoop({LoopAction::merge,id,f.request,f.control->generation(),0,static_cast<double>(19-id)}),"Stopping MERGE paired command");
    };
    const auto held=[](){const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(heldWorkerAllocations.load()<2&&std::chrono::steady_clock::now()<until)std::this_thread::yield();
        check(heldWorkerAllocations.load()>=2,"Stopping MERGE workers held outside audio");};
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(unsigned kind=0;kind<3;++kind)for(bool delayed:{false,true}){
        Fixture f(block);setup(f,id,reverse,kind);const auto before=f.view->tracks[id],source=f.view->tracks[19-id];
        const auto end=std::max(before.scheduledFrame,source.scheduledFrame);
        if(delayed){heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);}
        merge(f,id);f.cycle(!delayed);
        if(delayed){held();check(f.view->tracks[id].state==LoopState::finalizing,"Scheduled MERGE remains worker-owned");
            while(f.engine->clock()<end+512)f.cycle();
            check(!f.view->tracks[id].scheduledFrame&&!f.view->tracks[id].phaseApplyFrame,"Owned source/phase STOP deadlines resolve during worker");
            holdWorkerAllocation.store(false);f.cycle(true);
        }else{
            check(!f.view->rejected&&f.view->tracks[id].state==before.state&&f.view->tracks[id].scheduledFrame==before.scheduledFrame
                &&f.view->tracks[id].stopPending==before.stopPending,"MERGE retains exact destination stop state/deadline metadata");
            check(f.view->tracks[19-id].state==source.state&&f.view->tracks[19-id].scheduledFrame==source.scheduledFrame
                &&f.view->tracks[19-id].waveformId==source.waveformId,"MERGE retains source stop state/deadline and identity");
            check(f.view->tracks[id].mergeSerial,"Successful scheduled MERGE has current confirmation before STOP");
            while(f.engine->clock()<end+512)f.cycle();
        }
        const auto after=f.view->tracks[id];check(after.state==LoopState::stopped&&!after.scheduledFrame&&!after.finalizationFailed
            &&after.length==before.length&&after.waveformId!=before.waveformId&&after.undoCount==1,"Scheduled MERGE finishes stopped once, preserving committed PCM/history");
        if(kind!=0)check(after.mergeSerial==0,"Destination timed STOP cancels pending destructive confirmation");
        check(f.view->tracks[19-id].state==LoopState::stopped&&f.view->tracks[19-id].waveformId==source.waveformId,"Source timed STOP never mutates its stored PCM");
        f.command(LoopAction::undoGlobal,id);f.cycle();check(f.view->tracks[id].waveformId==before.waveformId,"Post-stop global Undo exact destination identity");
        f.command(LoopAction::redoGlobal,id);f.cycle();check(f.view->tracks[id].waveformId==after.waveformId&&!f.view->tracks[id].mergeSerial,"Post-stop global Redo exact identity, no revived confirmation");++cases;
    }
    // Read-only stopping sources are compared against an unedited paired engine.
    // A muted destination keeps source PCM comparison independent of the merge.
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(bool clone:{false,true}){
        Fixture f(block);setup(f,id,reverse,0,clone);for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::mute,id,0,e->generation(),0,1}),"Mute destination for independent source PCM oracle");
        const auto source=f.view->tracks[19-id];merge(f,id,false);f.cycle(true);const auto serial=f.view->tracks[id].mergeSerial;
        check(serial&&f.view->tracks[19-id].scheduledFrame==source.scheduledFrame,"Stopping source clone/mix retains exact deadline");
        for(unsigned n=0;n<8;++n)f.cycle();
        check(f.engine->enqueueLoop({LoopAction::clearMergedSource,id,++f.request,f.engine->generation(),0,static_cast<double>(serial)}),"Confirm exact stopping source owner");
        check(f.control->enqueueLoop({LoopAction::clear,19-id,f.request,f.control->generation(),0,0}),"Mirror explicit confirmed source clear");f.cycle();
        check(f.view->tracks[19-id].state==LoopState::empty,"Confirmed stopping source clear accepted");
        while(f.engine->clock()<source.scheduledFrame+1024)f.cycle();check(f.view->tracks[19-id].state==LoopState::empty,"Old scheduled stop cannot resurrect confirmed source");++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true}){
        Fixture f(block);setup(f,id,reverse,2);const auto before=f.view->tracks[id],source=f.view->tracks[19-id];
        failWorkerAllocation.store(true);merge(f,id,false);f.cycle(true);
        check(!failWorkerAllocation.load()&&f.view->tracks[id].finalizationFailed&&f.view->tracks[id].waveformId==before.waveformId
            &&f.view->tracks[id].state==LoopState::stopping&&f.view->tracks[id].scheduledFrame==before.scheduledFrame&&!f.view->tracks[id].undoCount&&!f.view->tracks[id].mergeSerial,"Scheduled MERGE OOM retains original PCM/stop/history");
        while(f.engine->clock()<std::max(before.scheduledFrame,source.scheduledFrame)+1024)f.cycle();check(f.view->tracks[id].state==LoopState::stopped,"OOM never loses timed STOP");++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(unsigned cancel=0;cancel<4;++cancel){
        Fixture f(block);setup(f,id,true,2);heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);merge(f,id);f.cycle();held();
        f.command(cancel==0?LoopAction::stopImmediate:cancel==1?LoopAction::stopAll:cancel==2?LoopAction::clear:LoopAction::clearAll,id);f.cycle();
        holdWorkerAllocation.store(false);f.cycle(true);for(unsigned n=0;n<400;++n)f.cycle();
        check(f.view->tracks[id].state==(cancel<2?LoopState::stopped:LoopState::empty)&&!f.view->tracks[id].mergeSerial&&!f.view->tracks[id].scheduledFrame,"Scheduled MERGE cancellation keeps STOP/DEL barriers");++cases;
    }
    check(allocations.load()==0,"Stopping MERGE zero callback new/delete");
    std::cout<<"PASS: scheduled-stop MERGE "<<cases<<" Engine 64/128, source/destination/both stop deadlines, history, exact independent source PCM, OOM/cancellation and zero callback new/delete\n";
}

void queuedMergeDestinationTests(){
    using namespace aion;unsigned cases=0;
    const auto setup=[](Fixture& f,unsigned id,bool reverse,bool sourceQueued=false){
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);f.prepare();
        f.record(19-id);f.command(LoopAction::stopImmediate,19-id);f.cycle();
        MeterGraphEvent event;while(f.engine->popMeterGraphEvent(event)){}
        auto owner=std::make_unique<LoopMeterLifecycle>(*f.engine,id);f.record(id);
        check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Queued destination owns initial buffer");
        f.command(LoopAction::stopImmediate,id);f.cycle();while(f.engine->popMeterGraphEvent(event)){}
        for(auto* e:{f.engine.get(),f.control.get()}){e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
            check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,reverse?1.:0.}),"Queued destination reverse metadata");}
        if(sourceQueued){f.command(LoopAction::play,19-id);f.cycle();(void)f.next(MeterGraphEventKind::queuedPlay,19-id);}
        f.command(LoopAction::play,id);f.cycle();const auto q=f.next(MeterGraphEventKind::queuedPlay,id);
        check(owner->consumeQueuedPlay(q),"Queued destination owns original PLAY receipt");return owner;
    };
    const auto birth=[](Fixture& f,unsigned id,LoopMeterLifecycle& owner){
        MeterGraphEvent q{};q.queueId=owner.queuedIntentId();q.bufferId=owner.queuedCallbackBuffer();
        check(owner.requestQueuedGraph(q.queueId,q.bufferId),"Queued destination source request");
        const auto request=owner.queuedWindowTransaction();LoopCommand mirror{LoopAction::meterQueuedGraphRequest,id,0,f.control->generation(),0,0};
        mirror.meterGraphTicket=request.ticket;mirror.meterQueueId=request.queueId;mirror.meterBufferId=request.bufferId;
        check(f.control->enqueueLoop(mirror),"Queued destination mirrored graph");f.cycle();
        (void)f.next(MeterGraphEventKind::started,id);check(owner.consumeQueuedWindowResult(f.next(MeterGraphEventKind::queuedGraphAccepted,id)),"Queued destination graph accepted");
    };
    const auto merge=[](Fixture& f,unsigned id,bool paired=true){
        check(f.engine->enqueueLoop({LoopAction::merge,id,++f.request,f.engine->generation(),0,static_cast<double>(19-id)}),"Queued destination MERGE observed");
        if(paired)check(f.control->enqueueLoop({LoopAction::merge,id,f.request,f.control->generation(),0,static_cast<double>(19-id)}),"Queued destination MERGE paired");
    };
    const auto held=[](){const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(heldWorkerAllocations.load()<2&&std::chrono::steady_clock::now()<until)std::this_thread::yield();
        check(heldWorkerAllocations.load()>=2,"Queued destination workers paused outside callback");};
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(bool graph:{false,true})for(bool delay:{false,true})for(bool sourceQueued:{false,true}){
        Fixture f(block);auto owner=setup(f,id,reverse,sourceQueued);MeterGraphEvent q{};q.queueId=owner->queuedIntentId();q.bufferId=owner->queuedCallbackBuffer();
        if(graph)birth(f,id,*owner);const auto before=f.view->tracks[id],source=f.view->tracks[19-id];const auto grid=before.scheduledFrame;
        if(delay&&graph){while(f.engine->clock()+block<grid-960)f.cycle();f.command(LoopAction::applyStart,id);f.cycle();
            check(f.view->tracks[id].phaseApplyFrame>grid,"Accepted pending phase occurs after grid during worker");}
        const auto revision=f.view->tracks[id].sourceRevision;
        if(delay){heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);}
        merge(f,id);f.cycle(!delay);
        if(delay){held();check(f.view->tracks[id].state==LoopState::finalizing,"Queued destination stays finalizing while held");
            if(!graph)birth(f,id,*owner);
            while(f.engine->clock()<grid+4096){f.cycle();if(f.engine->clock()<=grid)check(f.view->outputPeak==0,"MERGE cannot open source gate before grid");}
            check(!f.view->tracks[id].phaseApplyFrame&&f.view->tracks[id].sourceRevision>revision&&f.view->outputPeak>0,"Pending phase/grid advance without waiting for MERGE");
            holdWorkerAllocation.store(false);f.cycle(true);
        }
        const auto installed=f.next(MeterGraphEventKind::bufferInstalled,id);
        if(!graph&&!delay){check(installed.queueId==q.queueId,"MERGE explicitly retains original pre-birth PLAY receipt");
            auto wrong=installed;wrong.queueId+=(1ull<<32);check(!owner->consumeBuffer(wrong),"MERGE rejects mismatched full-width queue continuation");}
        check(owner->consumeBuffer(installed),"MERGE result is consumed by real meter owner");
        const auto after=f.view->tracks[id];check(!f.view->rejected&&!after.finalizationFailed&&after.length==before.length&&after.waveformId!=before.waveformId
            &&after.undoCount==1&&!after.redoCount&&after.mergeSerial&&f.view->nextUndo==static_cast<int>(id),"Queued destination commits once with local/global history");
        check(f.view->tracks[19-id].waveformId==source.waveformId&&f.view->tracks[19-id].length==source.length&&f.view->tracks[19-id].state==source.state
            &&f.view->tracks[19-id].scheduledFrame==source.scheduledFrame,"Queued destination MERGE preserves stopped/queued source receipt");
        if(!delay){check(after.state==LoopState::queued&&after.scheduledFrame==grid,"Queued MERGE retains grid after result");if(!graph)birth(f,id,*owner);}
        bool heard=false;while(f.engine->clock()<grid+8192){f.cycle();heard=heard||f.view->outputPeak>0;}
        check(heard&&f.view->tracks[id].state==LoopState::playing,"Queued destination remains audibly playing after grid");
        f.command(LoopAction::undoGlobal,id);f.cycle();check(f.view->tracks[id].waveformId==before.waveformId&&!f.view->tracks[id].mergeSerial,"Global Undo restores destination and invalidates completion");
        f.command(LoopAction::redoGlobal,id);f.cycle();check(f.view->tracks[id].waveformId==after.waveformId&&!f.view->tracks[id].mergeSerial,"Global Redo restores merged destination without stale clear");
        f.command(LoopAction::clear,id);f.cycle();++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(unsigned cancel=0;cancel<6;++cancel){
        Fixture f(block);auto owner=setup(f,id,reverse);birth(f,id,*owner);
        if(cancel==5){f.command(LoopAction::stop,id);f.cycle();check(f.view->tracks[id].stopPending,"Queued destination has accepted STOP");}
        heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);merge(f,id);f.cycle();held();
        if(cancel==5)while(f.engine->clock()<=f.view->tracks[id].scheduledFrame||f.view->tracks[id].scheduledFrame)f.cycle();
        if(cancel<3){f.command(cancel==0?LoopAction::stopImmediate:cancel==1?LoopAction::stopAll:LoopAction::clear,id);f.cycle();}
        holdWorkerAllocation.store(false);
        if(cancel==3)f.prepare();else if(cancel==4)for(auto* e:{f.engine.get(),f.control.get()}){e->prepare(96000,81920);e->setMonitoring(false);e->setTarget(1);}else f.cycle(true);
        for(unsigned n=0;n<400;++n)f.cycle();
        check(f.view->tracks[id].state==((cancel==2||cancel==4)?LoopState::empty:LoopState::stopped)&&!f.view->tracks[id].scheduledFrame&&!f.view->tracks[id].mergeSerial,"Queued MERGE clear/panic/prepare/STOP never resurrects source or confirmation");++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true}){
        Fixture f(block);auto owner=setup(f,id,reverse);birth(f,id,*owner);const auto before=f.view->tracks[id];
        failWorkerAllocation.store(true);merge(f,id,false);f.cycle(true);
        check(!failWorkerAllocation.load()&&f.view->tracks[id].finalizationFailed&&f.view->tracks[id].waveformId==before.waveformId
            &&f.view->tracks[id].state==LoopState::queued&&f.view->tracks[id].scheduledFrame==before.scheduledFrame&&!f.view->tracks[id].undoCount&&!f.view->tracks[id].mergeSerial,"Queued MERGE OOM preserves original source, PCM and history");
        while(f.engine->clock()<before.scheduledFrame+2048)f.cycle();++cases;
    }
    check(allocations.load()==0,"Queued destination MERGE zero callback new/delete");
    std::cout<<"PASS: queued-destination MERGE "<<cases<<" Engine 64/128 ownership/phase/grid, in-flight graph, history, OOM/cancellation and zero callback new/delete\n";
}

void queuedMergeSourceTests(){
    using namespace aion;
    unsigned cases=0;
    for(unsigned block:{64u,128u})for(unsigned source:{0u,19u})for(bool graph:{false,true})for(bool reverse:{false,true})for(bool clone:{false,true}){
        Fixture f(block);const unsigned dest=19-source;
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);f.prepare();
        const auto value=[&](LoopAction action,unsigned id,double v,bool paired=true){
            check(f.engine->enqueueLoop({action,id,++f.request,f.engine->generation(),0,v}),"Queued MERGE observed command");
            if(paired)check(f.control->enqueueLoop({action,id,f.request,f.control->generation(),0,v}),"Queued MERGE paired command");
        };
        if(!clone){f.record(dest);f.command(LoopAction::stopImmediate,dest);f.cycle();}
        MeterGraphEvent event;while(f.engine->popMeterGraphEvent(event)){}
        auto owner=std::make_unique<LoopMeterLifecycle>(*f.engine,source);
        value(LoopAction::mute,dest,1);f.record(source);
        check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,source)),"Queued MERGE source immutable buffer receipt");
        f.command(LoopAction::stopImmediate,source);f.cycle();
        while(f.engine->popMeterGraphEvent(event)){}
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,137.3,1,4,4,false});
        value(LoopAction::reverse,source,reverse?1:0);f.command(LoopAction::play,source);f.cycle();
        const auto q=f.next(MeterGraphEventKind::queuedPlay,source);check(owner->consumeQueuedPlay(q),"Queued MERGE source owns PLAY receipt");
        const auto grid=f.view->tracks[source].scheduledFrame;
        if(graph){
            while(f.engine->clock()+4096<grid)f.cycle();
            check(owner->requestQueuedGraph(q.queueId,q.bufferId),"Queued MERGE constructs source inside callback window");
            const auto request=owner->queuedWindowTransaction();LoopCommand mirror{LoopAction::meterQueuedGraphRequest,source,0,f.control->generation(),0,0};
            mirror.meterGraphTicket=request.ticket;mirror.meterQueueId=request.queueId;mirror.meterBufferId=request.bufferId;
            check(f.control->enqueueLoop(mirror),"Queued MERGE mirrored source construction");f.cycle();
            (void)f.next(MeterGraphEventKind::started,source);
            check(owner->consumeQueuedWindowResult(f.next(MeterGraphEventKind::queuedGraphAccepted,source)),"Queued MERGE constructed graph acknowledged");
        }
        const auto before=f.view->tracks[source];check(before.state==LoopState::queued,"Queued MERGE fixture is genuinely waiting");
        value(LoopAction::merge,dest,source,false);f.cycle(true);
        const auto merged=f.view->tracks[dest];
        check(!f.view->rejected&&merged.mergeSerial&&merged.length==512&&merged.state==LoopState::stopped
            &&merged.undoCount==(clone?0u:1u),"Queued source clone/mix commits stopped with expected history");
        check(f.view->tracks[source].waveformId==before.waveformId&&f.view->tracks[source].scheduledFrame==grid
            &&f.view->tracks[source].state==LoopState::queued,"MERGE preserves source identity and grid");
        if(!clone){value(LoopAction::undo,dest,0,false);f.cycle();value(LoopAction::redo,dest,0,false);f.cycle();
            check(f.view->tracks[dest].mergeSerial==0,"Queued source destination history invalidates confirmation");}
        if(!graph){
            while(f.engine->clock()+4096<grid)f.cycle();
            check(owner->requestQueuedGraph(q.queueId,q.bufferId),"Pre-birth MERGE retains original receipt");
            const auto request=owner->queuedWindowTransaction();LoopCommand mirror{LoopAction::meterQueuedGraphRequest,source,0,f.control->generation(),0,0};
            mirror.meterGraphTicket=request.ticket;mirror.meterQueueId=request.queueId;mirror.meterBufferId=request.bufferId;
            check(f.control->enqueueLoop(mirror),"Pre-birth MERGE mirrored original receipt");f.cycle();
        }
        bool audible=false;while(f.engine->clock()<grid+2048){f.cycle();audible=audible||f.view->outputPeak>0;}
        check(audible&&f.view->tracks[source].state==LoopState::playing,"Queued MERGE source resumes exact nonzero paired stereo at original grid");
        value(LoopAction::clearMergedSource,dest,static_cast<double>(merged.mergeSerial),false);
        if(clone)check(f.control->enqueueLoop({LoopAction::clear,source,0,f.control->generation(),0,0}),"Mirror explicit confirmed source clear");
        f.cycle();check(f.view->tracks[source].state==(clone?LoopState::empty:LoopState::playing),"Only current completion clears source after grid");
        ++cases;
    }
    check(allocations.load()==0,"Queued MERGE zero callback new/delete");
    std::cout<<"PASS: queued-source MERGE "<<cases<<" actual Engine 64/128, original PLAY receipt/grid, reversed source, paired stereo, clone/mix/history/confirmation and zero callback new/delete\n";
}

void postbirthMultiplyTests(){
    using namespace aion;
    const auto setup=[](Fixture& f,unsigned id,bool reverse,bool full=false){
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);f.prepare();
        if(full)for(auto* e:{f.engine.get(),f.control.get()}){e->prepare(48000,4096);e->setMonitoring(false);e->setTarget(1);}
        auto owner=std::make_unique<LoopMeterLifecycle>(*f.engine,id);
        if(full){f.command(LoopAction::record,id);for(unsigned n=0;n<4096/f.block;++n)f.cycle();f.command(LoopAction::finish,id);f.cycle(true);}else f.record(id);check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Post-birth history seed");
        f.command(LoopAction::stopImmediate,id);for(unsigned n=0;n<8;++n)f.cycle();

        for(auto* e:{f.engine.get(),f.control.get()}){e->configureSync({SyncMode::fixed,137.3,1,4,4,false});
            check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,reverse?1.:0.}),"Post-birth direction");}
        f.command(LoopAction::play,id);f.cycle();const auto q=f.next(MeterGraphEventKind::queuedPlay,id);check(owner->consumeQueuedPlay(q)&&owner->requestQueuedGraph(q.queueId,q.bufferId),"Post-birth explicit graph request");
        const auto request=owner->queuedWindowTransaction();LoopCommand mirror{LoopAction::meterQueuedGraphRequest,id,0,f.control->generation(),0,0};
        mirror.meterGraphTicket=request.ticket;mirror.meterQueueId=request.queueId;mirror.meterBufferId=request.bufferId;check(f.control->enqueueLoop(mirror),"Post-birth paired graph");
        f.cycle();(void)f.next(MeterGraphEventKind::started,id);check(owner->consumeQueuedWindowResult(f.next(MeterGraphEventKind::queuedGraphAccepted,id)),"Post-birth owns constructed source");return owner;
    };
    const auto waitHeld=[](){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(heldWorkerAllocations.load()<2&&std::chrono::steady_clock::now()<end)std::this_thread::yield();
        check(heldWorkerAllocations.load()>=2,"Post-birth MULT pauses workers outside callback");};
    unsigned cases=0;
    // Failed source construction must preserve the old clip AND deliver a
    // tempo edit to its surviving graph, just as a rejected MULT would.
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(double bpm:{93.75,211.7}){
        Fixture f(block);auto owner=setup(f,id,reverse);const auto before=f.view->tracks[id];const auto grid=before.scheduledFrame;
        heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);
        check(f.engine->enqueueLoop({LoopAction::multiply,id,++f.request,f.engine->generation(),0,0}),"Queue only observed failed MULT");f.cycle();
        for(unsigned n=0;heldWorkerAllocations.load()<1&&n<10000;++n)std::this_thread::sleep_for(std::chrono::milliseconds(1));
        check(heldWorkerAllocations.load()>=1,"Pause only failed MULT worker outside audio");
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,bpm,1,4,4,false});
        failWorkerAllocation.store(true);holdWorkerAllocation.store(false);
        watch=true;for(auto* e:{f.engine.get(),f.control.get()})e->process(nullptr,0,0,{},true);watch=false;
        f.cycle();check(f.view->tracks[id].finalizationFailed&&f.view->tracks[id].waveformId==before.waveformId
            &&f.view->tracks[id].length==before.length&&!f.view->tracks[id].undoCount,"Failed MULT keeps immutable PCM/history while accepting tempo metadata");
        bool heard=false;while(f.engine->clock()<grid+2048){f.cycle();if(f.engine->clock()>grid)heard=heard||f.view->outputPeak>0;}
        check(heard,"Rejected MULT tempo recovery remains audible");f.command(LoopAction::clear,id);f.cycle();++cases;
    }
    // A tempo edit during worker copying must equal the same edit after
    // immutable source construction, before its future initial-rate event.
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(bool near:{false,true})for(double bpm:{93.75,211.7}){
        Fixture f(block);auto owner=setup(f,id,reverse);const auto grid=f.view->tracks[id].scheduledFrame;
        if(near)while(f.engine->clock()+block<grid-3840)f.cycle();
        heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);f.command(LoopAction::multiply,id);f.cycle();waitHeld();
        const auto frame=f.engine->clock();f.engine->configureSync({SyncMode::fixed,bpm,1,4,4,false});
        holdWorkerAllocation.store(false);
        // Accept both actual worker results at the same sample frame. This is
        // offline fixture settlement, not a wait on a real-time callback.
        watch=true;for(auto* e:{f.engine.get(),f.control.get()})e->process(nullptr,0,0,{},true);watch=false;
        check(f.engine->clock()==frame&&f.control->clock()==frame,"Tempo worker settlement does not advance fixture audio time");
        f.control->configureSync({SyncMode::fixed,bpm,1,4,4,false});f.cycle();
        check(!f.view->rejected&&f.view->tracks[id].length==1024&&f.view->tracks[id].undoCount==1
            &&f.view->tracks[id].phaseApplyFrame==frame+240&&owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Tempo preserves MULT source deadline, PCM/history and original receipt");
        bool heard=false;while(f.engine->clock()<grid+2048){f.cycle();if(f.engine->clock()<=grid)check(f.view->outputPeak==0,"Worker-time tempo retains original volume grid");else heard=heard||f.view->outputPeak>0;}
        check(heard,"Worker-time tempo has exact nonzero paired stereo PCM");
        f.command(LoopAction::applyStart,id);for(unsigned n=0;n<4096/block;++n)f.cycle();
        check(f.view->outputPeak>0,"Following Apply uses current tempo metadata without source-snapshot rollback");
        f.command(LoopAction::clear,id);f.cycle();++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(bool late:{false,true})for(unsigned edits=0;edits<3;++edits){
        Fixture f(block);auto owner=setup(f,id,reverse);const auto grid=f.view->tracks[id].scheduledFrame;
        heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);f.command(LoopAction::multiply,id);f.cycle();waitHeld();
        const auto send=[&](Engine& engine,LoopAction action,double value){check(engine.enqueueLoop({action,id,++f.request,engine.generation(),0,value}),"Queue metadata during held MULT worker");};
        if(edits!=1)send(*f.engine,LoopAction::reverse,reverse?0.:1.);
        if(edits!=0)send(*f.engine,LoopAction::startDelay,.731);
        f.cycle();check(!f.view->rejected&&f.view->tracks[id].state==LoopState::finalizing
            &&f.view->tracks[id].reverse==(edits==1?reverse:!reverse)&&f.view->tracks[id].startDelay==(edits==0?0.:.731),"Worker-time metadata is accepted without changing current source");
        if(late)while(f.engine->clock()<grid+512)f.cycle();else for(unsigned n=0;n<4;++n)f.cycle();
        holdWorkerAllocation.store(false);const auto target=f.engine->clock()+240;f.cycle(true);
        check(f.view->tracks[id].phaseApplyFrame==target&&f.view->tracks[id].length==1024&&f.view->tracks[id].undoCount==1
            &&owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"MULT completion retains accepted source metadata with original history/5ms deadline");
        while(f.engine->clock()<std::max(grid+1024,target+512))f.cycle();
        // The following Apply must use the later accepted metadata, not restore
        // the MULT snapshot into user-visible state or lose the edit.
        if(edits!=1)send(*f.control,LoopAction::reverse,reverse?0.:1.);
        if(edits!=0)send(*f.control,LoopAction::startDelay,.731);
        f.command(LoopAction::applyStart,id);for(unsigned n=0;n<4096/block;++n)f.cycle();
        check(f.view->outputPeak>0,"Later Apply remains audible after worker-time metadata");
        f.command(LoopAction::clear,id);f.cycle();++cases;
    }
    // A completed MULT has constructed its 5ms replacement already. Later
    // metadata belongs to the next source, and a speed write before the future
    // initial-rate event must not retarget either the old or replacement voice.
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(bool near:{false,true})for(bool speed:{false,true}){
        Fixture f(block);auto owner=setup(f,id,reverse);const auto grid=f.view->tracks[id].scheduledFrame;
        if(near)while(f.engine->clock()+block<grid-48)f.cycle();
        const auto target=f.engine->clock()+240;f.command(LoopAction::multiply,id);f.cycle(true);
        check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id))&&f.engine->clock()<target,"Control fixture leaves pending 5ms MULT source");
        const auto send=[&](LoopAction action,double value){check(f.engine->enqueueLoop({action,id,++f.request,f.engine->generation(),0,value}),"Queue observed post-MULT control");};
        send(LoopAction::reverse,reverse?0.:1.);send(LoopAction::startDelay,.731);if(speed)send(LoopAction::speed,.5);
        f.cycle();check(!f.view->rejected&&f.view->tracks[id].reverse!=reverse&&f.view->tracks[id].startDelay==.731
            &&f.view->tracks[id].playbackRate==(speed?.5:1.),"Pending MULT accepts independent metadata/control without changing captured source");
        bool heard=false;while(f.engine->clock()<grid+2048){f.cycle();if(f.engine->clock()<=grid)check(f.view->outputPeak==0,"Post-MULT controls preserve original silence gate");else heard=heard||f.view->outputPeak>0;}
        check(heard,"Post-MULT captured source remains audible and exactly matches unchanged control engine");
        f.command(LoopAction::clear,id);f.cycle();++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(bool near:{false,true})for(unsigned sequence=0;sequence<4;++sequence){
        Fixture f(block);auto owner=setup(f,id,reverse);const auto grid=f.view->tracks[id].scheduledFrame;const auto identity=owner->inspect();
        if(near)while(f.engine->clock()+block<grid-48)f.cycle();
        if(sequence==2){f.command(LoopAction::applyStart,id);f.cycle();f.command(LoopAction::applyStart,id);f.cycle();}
        const auto target=f.engine->clock()+240;f.command(LoopAction::multiply,id);f.cycle(true);
        check(!f.view->rejected&&f.view->tracks[id].length==1024&&f.view->tracks[id].undoCount==1&&f.view->tracks[id].phaseApplyFrame==target
            &&owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Post-birth MULT installs doubled PCM/history with exact 5ms replacement");
        if(sequence==1){f.command(LoopAction::multiply,id);f.cycle(true);check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id))&&f.view->tracks[id].length==2048,"Rapid no-input doubles retain scheduled sources");}
        if(sequence==3){f.command(LoopAction::undo,id);f.cycle();check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Post-birth MULT Undo snapshot");
            f.command(LoopAction::normalize,id);f.cycle(true);check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"NORM after MULT Undo preserves logical source identities");}
        bool heard=false;while(f.engine->clock()<grid+4096){f.cycle();if(f.engine->clock()<=grid)check(f.view->outputPeak==0,"MULT preserves original pre-grid silence");else heard=heard||f.view->outputPeak>0;}
        check(heard&&owner->inspect().analyser==identity.analyser&&owner->inspect().data==identity.data,"MULT retains healthy graph/analyser and becomes audible");
        f.command(LoopAction::clear,id);f.cycle();check(owner->consumeClear(f.next(MeterGraphEventKind::cleared,id)),"Post-birth MULT releases source leases");++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(bool late:{false,true}){
        Fixture f(block);auto owner=setup(f,id,reverse);const auto grid=f.view->tracks[id].scheduledFrame,revision=f.view->tracks[id].sourceRevision;
        heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);f.command(LoopAction::multiply,id);f.cycle();waitHeld();
        if(late){bool heard=false;while(f.engine->clock()<grid+512){f.cycle();if(f.engine->clock()<=grid)check(f.view->outputPeak==0,"Delayed MULT keeps grid gate");else heard=heard||f.view->outputPeak>0;}check(heard,"Old constructed source plays while MULT worker is delayed");}
        else {check(f.view->tracks[id].queuedSourceReady&&f.view->tracks[id].length==512&&f.view->tracks[id].sourceRevision==revision,"Pending MULT preserves original source/length");for(unsigned n=0;n<8;++n)f.cycle();}
        holdWorkerAllocation.store(false);const auto target=f.engine->clock()+240;f.cycle(true);
        check(f.view->tracks[id].phaseApplyFrame==target&&f.view->tracks[id].length==1024&&owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Worker completion reserves replacement without changing old source early");
        while(f.engine->clock()<grid+2048)f.cycle();f.command(LoopAction::clear,id);f.cycle();++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool beforeCommit:{false,true})for(unsigned cancel=0;cancel<6;++cancel){
        Fixture f(block);auto owner=setup(f,id,true);
        if(beforeCommit){heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);}
        f.command(LoopAction::multiply,id);f.cycle(!beforeCommit);if(beforeCommit)waitHeld();else check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"MULT before pending-source cancel");
        if(cancel<3){f.command(cancel==0?LoopAction::stopImmediate:cancel==1?LoopAction::stopAll:LoopAction::clear,id);f.cycle();}
        if(cancel==5){const auto ticket=owner->binding().ticket;check(owner->teardown()&&f.control->retireQueuedSourceThrough(f.control->generation(),id,ticket),"Cleanup retires both constructed MULT sources");f.cycle();}
        holdWorkerAllocation.store(false);
        if(cancel==3)f.prepare();else if(cancel==4)for(auto* e:{f.engine.get(),f.control.get()}){e->prepare(96000,81920);e->setMonitoring(false);e->setTarget(1);}else f.cycle(true);
        for(unsigned n=0;n<128;++n)f.cycle();
        check(f.view->tracks[id].state==((cancel==2||cancel==4)?LoopState::empty:LoopState::stopped)&&!f.view->tracks[id].phaseApplyFrame,"Canceled MULT cannot revive pending/current sources");++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true}){
        Fixture f(block);auto owner=setup(f,id,reverse);const auto before=f.view->tracks[id];
        check(f.engine->enqueueLoop({LoopAction::multiply,id,++f.request,f.engine->generation(),0,0}),"OOM MULT only on observed engine");failWorkerAllocation.store(true);f.cycle(true);MeterGraphEvent event;
        check(!failWorkerAllocation.load()&&f.view->tracks[id].finalizationFailed&&f.view->tracks[id].waveformId==before.waveformId
            &&f.view->tracks[id].undoCount==before.undoCount&&f.view->tracks[id].scheduledFrame==before.scheduledFrame&&!f.view->tracks[id].phaseApplyFrame&&!f.engine->popMeterGraphEvent(event),"OOM keeps old source/grid/PCM/history atomically");
        while(f.engine->clock()<before.scheduledFrame+1024)f.cycle();++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool overlaps:{false,true}){
        Fixture f(block);auto owner=setup(f,id,false);
        if(overlaps){for(unsigned batch=0;batch<4;++batch){for(unsigned n=0;n<32;++n){f.command(LoopAction::applyStart,id);f.command(LoopAction::retrigger,id);}f.cycle();for(unsigned n=0;n<240/block+1;++n)f.cycle();}f.command(LoopAction::applyStart,id);f.cycle();}
        else for(unsigned n=0;n<127;++n)f.command(LoopAction::retrigger,id);
        const auto before=f.view->tracks[id];f.command(LoopAction::multiply,id);f.cycle(true);MeterGraphEvent event;
        check(f.view->tracks[id].finalizationFailed&&f.view->tracks[id].waveformId==before.waveformId&&f.view->tracks[id].undoCount==before.undoCount&&!f.engine->popMeterGraphEvent(event),"Full event/overlap reservation fails before PCM/history publication");
        f.command(LoopAction::clear,id);for(unsigned n=0;n<128;++n)f.cycle();check(f.view->tracks[id].state==LoopState::empty,"Failed MULT frees worker result and scheduled leases");++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true}){
        Fixture f(block);auto owner=setup(f,id,reverse,true);const auto before=f.view->tracks[id];f.command(LoopAction::multiply,id);f.cycle();
        check(f.view->rejected&&f.view->tracks[id].waveformId==before.waveformId&&f.view->tracks[id].undoCount==before.undoCount,"Full-pool post-birth MULT rejects before job admission");++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true}){
        Fixture f(block);auto owner=setup(f,id,reverse);
        // Input capture now has dedicated source/grid/PCM/ownership acceptance below.
        for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::speed,id,++f.request,e->generation(),0,.5}),"Set requested varispeed");f.cycle();
        f.command(LoopAction::multiply,id);f.cycle();check(f.view->rejected&&f.view->tracks[id].length==512&&!f.view->tracks[id].undoCount,"Original varispeed guard applies after source construction");++cases;
    }
    check(allocations.load()==0,"Post-birth MULT has no callback new/delete");
    std::cout<<"PASS: post-birth no-input Multiply "<<cases<<" 5ms/source-length/history/rapid/Apply/NORM and pending-source metadata/speed and held-worker Reverse/Start/tempo cases, original grid, delayed worker across grid, cancel/cleanup/prepare/OOM/full pool/event/overlap and input/varispeed guards; tracks 0/19, reverse, 64/128, exact paired stereo and zero callback new/delete\n";
}

void queuedMultiplyInputTests(bool gatedOnly=false){
    using namespace aion;
    const auto setup=[](Fixture& f,unsigned id,bool reverse,unsigned capacity=81920,SyncMode mode=SyncMode::fixed){
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        for(auto* e:{f.engine.get(),f.control.get()}){e->prepare(48000,capacity);e->configureSync({SyncMode::off,120,1,1,4,false});e->setMonitoring(false);e->setTarget(1);}
        auto owner=std::make_unique<LoopMeterLifecycle>(*f.engine,id);f.record(id);check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Input MULT seed clip");
        f.command(LoopAction::stopImmediate,id);for(unsigned n=0;n<8;++n)f.cycle();
        for(auto* e:{f.engine.get(),f.control.get()}){e->configureSync({mode,137.3,1,1,4,false});check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,reverse?1.:0.}),"Input MULT source direction");}
        f.command(LoopAction::play,id);f.cycle();const auto q=f.next(MeterGraphEventKind::queuedPlay,id);check(owner->consumeQueuedPlay(q)&&owner->requestQueuedGraph(q.queueId,q.bufferId),"Input MULT explicit graph birth");
        const auto request=owner->queuedWindowTransaction();LoopCommand mirror{LoopAction::meterQueuedGraphRequest,id,0,f.control->generation(),0,0};mirror.meterGraphTicket=request.ticket;mirror.meterQueueId=request.queueId;mirror.meterBufferId=request.bufferId;
        check(f.control->enqueueLoop(mirror),"Input MULT paired graph birth");f.cycle();(void)f.next(MeterGraphEventKind::started,id);check(owner->consumeQueuedWindowResult(f.next(MeterGraphEventKind::queuedGraphAccepted,id)),"Input MULT owns source");return owner;
    };
    const auto enqueue=[](Fixture& f,unsigned id,std::uint64_t at,bool finish=false){for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({finish?LoopAction::finish:LoopAction::multiply,id,++f.request,e->generation(),at,1}),"Timestamped input MULT action");};
    unsigned cases=0;
    if(!gatedOnly){
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(bool near:{false,true})for(bool pending:{false,true})for(unsigned take:{0u,127u,128u,513u,4096u}){
        Fixture f(block);auto owner=setup(f,id,reverse);const auto before=f.view->tracks[id];const auto grid=before.scheduledFrame;
        if(near)while(f.engine->clock()+block<grid-48)f.cycle();
        if(pending){f.command(LoopAction::applyStart,id);f.cycle();}
        const auto start=f.engine->clock()+19;const auto length=take==4096?static_cast<unsigned>(grid-start+257):take;
        enqueue(f,id,start);enqueue(f,id,start+length,pending);
        while(f.engine->clock()<start+length+block){f.cycle(true);
            if(f.engine->clock()<=grid)check(f.view->outputPeak==0,"Input MULT retains original pre-grid volume gate");
            if(f.engine->clock()>grid&&f.engine->clock()<=start+length)check(f.view->tracks[id].state==LoopState::multiplying,"Audible grid cannot finish active input capture");}
        const auto t=f.view->tracks[id];check(!f.view->rejected&&t.length==before.length+(length>=128?length:0)
            &&t.undoCount==(length>=128?1u:0u)&&(length<128?t.waveformId==before.waveformId:t.waveformId!=before.waveformId),"Exact timestamped append length, local history and short-take identity");
        if(length>=128)check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id))&&f.view->nextUndo==-1,"Input MULT preserves local-only history and analyser ownership");
        bool heard=false;while(f.engine->clock()<grid+2048){f.cycle();if(f.engine->clock()<=grid)check(f.view->outputPeak==0,"Committed input MULT retains original graph grid");else heard=heard||f.view->outputPeak>0;}
        check(heard,"Input MULT remains audible with exact paired stereo");
        if(length>=128){f.command(LoopAction::undo,id);f.cycle();check(f.view->tracks[id].waveformId==before.waveformId&&f.view->tracks[id].length==before.length,"Input MULT Undo restores original clip");
            f.command(LoopAction::redo,id);f.cycle();check(f.view->tracks[id].waveformId==t.waveformId&&f.view->tracks[id].length==t.length,"Input MULT Redo restores appended clip");}
        f.command(LoopAction::clear,id);f.cycle();++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(unsigned cancel=0;cancel<5;++cancel){
        Fixture f(block);auto owner=setup(f,id,true);enqueue(f,id,0);for(unsigned n=0;n<256/block;++n)f.cycle();
        check(f.view->tracks[id].state==LoopState::multiplying,"Input MULT cancellation begins in active capture");
        if(cancel==4){for(auto* e:{f.engine.get(),f.control.get()}){e->prepare(96000,81920);e->setMonitoring(false);e->setTarget(1);}}
        else {f.command(cancel==0?LoopAction::stopImmediate:cancel==1?LoopAction::stopAll:cancel==2?LoopAction::clear:LoopAction::clearAll,id);f.cycle(true);}
        for(unsigned n=0;n<128;++n)f.cycle();const auto t=f.view->tracks[id];
        check(cancel<2?(t.state==LoopState::stopped&&t.length==768&&t.undoCount==1):(t.state==LoopState::empty&&!t.length&&!t.undoCount),"Input MULT stop commits once; clear/prepare cancels PCM and pending source");++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u}){
        Fixture f(block);auto owner=setup(f,id,true);const auto before=f.view->tracks[id];
        check(f.engine->enqueueLoop({LoopAction::multiply,id,++f.request,f.engine->generation(),0,1}),"Observed OOM capture only");for(unsigned n=0;n<256/block;++n)f.cycle();
        failWorkerAllocation.store(true);check(f.engine->enqueueLoop({LoopAction::multiply,id,++f.request,f.engine->generation(),0,1}),"Observed OOM finish only");f.cycle(true);
        check(f.view->tracks[id].finalizationFailed&&f.view->tracks[id].waveformId==before.waveformId&&f.view->tracks[id].length==before.length&&!f.view->tracks[id].undoCount,"Input MULT OOM retains exact old source/PCM/history");
        while(f.engine->clock()<before.scheduledFrame+2048)f.cycle();f.command(LoopAction::clear,id);f.cycle();++cases;
        Fixture full(block);auto fullOwner=setup(full,id,false,8192);enqueue(full,id,0);for(unsigned n=0;n<9000/block+1;++n)full.cycle(true);
        check(full.view->tracks[id].length==8192&&full.view->tracks[id].undoCount==1&&full.view->poolFull,"Input MULT auto-finishes at exact shared memory boundary");full.command(LoopAction::clear,id);full.cycle();++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(unsigned cancel=0;cancel<3;++cancel){
        Fixture f(block);auto owner=setup(f,id,true);enqueue(f,id,0);for(unsigned n=0;n<256/block;++n)f.cycle();
        heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);enqueue(f,id,0);f.cycle();
        const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(heldWorkerAllocations.load()<2&&std::chrono::steady_clock::now()<end)std::this_thread::yield();
        check(heldWorkerAllocations.load()>=2,"Input finalization holds real workers outside audio");
        f.command(cancel==0?LoopAction::clear:cancel==1?LoopAction::clearAll:LoopAction::stopAll,id);f.cycle();
        holdWorkerAllocation.store(false);f.cycle(true);for(unsigned n=0;n<32;++n)f.cycle();const auto t=f.view->tracks[id];
        check(cancel<2?(t.state==LoopState::empty&&!t.length&&!t.undoCount):(t.state==LoopState::stopped&&t.length==768&&t.undoCount==1),"Held input finalizer obeys DEL/bank clear/panic without stale playback");++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u}){
        Fixture f(block);auto owner=setup(f,id,false);const auto before=f.view->tracks[id];
        for(unsigned n=0;n<127;++n)f.command(LoopAction::applyStart,id);f.cycle();enqueue(f,id,0);for(unsigned n=0;n<256/block;++n)f.cycle();enqueue(f,id,0);f.cycle(true);
        check(f.view->tracks[id].finalizationFailed&&f.view->tracks[id].waveformId==before.waveformId&&!f.view->tracks[id].undoCount,"Full phase FIFO rejects input replacement before PCM/history mutation");
        f.command(LoopAction::clear,id);for(unsigned n=0;n<128;++n)f.cycle();check(f.view->tracks[id].state==LoopState::empty,"Rejected input replacement releases phase/worker leases on clear");++cases;
    }
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true}){
        Fixture f(block);auto owner=setup(f,id,reverse,81920,reverse?SyncMode::dynamic:SyncMode::fixed);const auto before=f.view->tracks[id];const auto grid=before.scheduledFrame;
        f.command(LoopAction::retrigger,id);f.cycle();const auto active=f.engine->clock()+384;while(f.engine->clock()<active)f.cycle();
        check(f.view->tracks[id].state==LoopState::playing&&f.engine->clock()<grid&&f.view->outputPeak==0,"RTRG source is playing while original graph remains volume gated");
        enqueue(f,id,0);for(unsigned n=0;n<256/block;++n)f.cycle();const auto deadline=f.engine->clock()+240;enqueue(f,id,0);f.cycle(true);
        check(f.view->tracks[id].length==768&&f.view->tracks[id].undoCount==1&&f.view->tracks[id].phaseApplyFrame==deadline
            &&f.view->outputPeak==0&&owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Gated playing capture retains original 5ms replacement, local history and closed volume");
        bool heard=false;while(f.engine->clock()<grid+2048){f.cycle();if(f.engine->clock()>grid)heard=heard||f.view->outputPeak>0;else check(f.view->outputPeak==0,"Gated playing capture retains original grid");}
        check(heard,"Gated playing capture is audible with exact paired stereo");f.command(LoopAction::clear,id);f.cycle();++cases;
    }
    check(allocations.load()==0,"Input MULT has no callback new/delete");
    std::cout<<"PASS: queued-source input MULT "<<cases<<" timestamped short/append/grid-crossing/pending-source/Undo/Redo, local-only history, cancellation/prepare/OOM/full pool, tracks 0/19, reverse, 64/128, exact paired stereo and zero callback new/delete\n";
}

void prebirthMultiplyTests(){
    using namespace aion;
    const auto setup=[](Fixture& f,unsigned id,bool reverse,bool full=false){
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);f.prepare();
        if(full)for(auto* e:{f.engine.get(),f.control.get()}){e->prepare(48000,4096);e->setMonitoring(false);e->setTarget(1);}
        auto owner=std::make_unique<LoopMeterLifecycle>(*f.engine,id);
        if(full){f.command(LoopAction::record,id);for(unsigned n=0;n<4096/f.block;++n)f.cycle();f.command(LoopAction::finish,id);f.cycle(true);}else f.record(id);
        check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Queued MULT initial buffer");
        f.command(LoopAction::stopImmediate,id);for(unsigned n=0;n<8;++n)f.cycle();
        for(auto* e:{f.engine.get(),f.control.get()}){
            e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
            check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,reverse?1.:0.}),"Queued MULT direction");}
        f.command(LoopAction::play,id);f.cycle(false,{100,true});
        check(owner->consumeQueuedPlay(f.next(MeterGraphEventKind::queuedPlay,id)),"Queued MULT retains initial PLAY");return owner;
    };
    const auto waitHeld=[](){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(heldWorkerAllocations.load()<2&&std::chrono::steady_clock::now()<end)std::this_thread::yield();
        check(heldWorkerAllocations.load()>=2,"Queued MULT pauses both workers outside audio");};
    const auto request=[](Fixture& f,LoopMeterLifecycle& owner,unsigned id,const QueuedMeterCallbackTime& receipt,bool duplicate=false){
        check(owner.requestQueuedGraphDue(receipt,73,{73,receipt.deadlineSeconds}),"Queued MULT original due request");
        const auto transaction=owner.queuedWindowTransaction();LoopCommand command{LoopAction::meterQueuedGraphRequest,id,0,f.control->generation(),0,0};
        command.meterGraphTicket=transaction.ticket;command.meterQueueId=transaction.queueId;command.meterBufferId=transaction.bufferId;
        check(f.control->enqueueLoop(command),"Queued MULT paired callback");
        if(duplicate)for(auto* e:{f.engine.get(),f.control.get()}){command.generation=e->generation();check(e->enqueueLoop(command),"Queued MULT duplicate pending request");}
    };
    const auto attach=[](Fixture& f,LoopMeterLifecycle& owner,unsigned id){
        (void)f.next(MeterGraphEventKind::started,id);
        check(owner.consumeQueuedWindowResult(f.next(MeterGraphEventKind::queuedGraphAccepted,id)),"Queued MULT graph accepted only after PCM commit");};
    unsigned cases=0;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(unsigned stage=0;stage<4;++stage){
        Fixture f(block);auto owner=setup(f,id,reverse);QueuedMeterCallbackTime receipt;
        check(owner->readQueuedCallbackTime(73,{73,100},receipt),"Queued MULT original command deadline");
        const auto before=f.view->tracks[id];const auto identity=owner->inspect();
        if(stage>=2){heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);}
        f.command(LoopAction::multiply,id);f.cycle(stage<2);check(!f.view->rejected,"Queued MULT before birth accepted");
        if(stage>=2){waitHeld();
            if(stage==3)while(f.engine->clock()<before.scheduledFrame+block){f.cycle();check(f.view->outputPeak==0,"Worker past original grid stays silent");}
            request(f,*owner,id,receipt,true);f.cycle();MeterGraphEvent event;
            check(!f.engine->popMeterGraphEvent(event)&&!f.view->tracks[id].queuedSourceReady
                &&owner->queuedWindowTransaction().status==QueuedWindowStatus::pending,"In-flight MULT defers graph without false acceptance or old-buffer birth");
            if(stage==2){const auto transaction=owner->queuedWindowTransaction();LoopCommand wrong{LoopAction::meterQueuedGraphRequest,id,0,f.engine->generation(),0,0};
                wrong.meterGraphTicket=transaction.ticket+1;wrong.meterQueueId=transaction.queueId+(1ull<<32);wrong.meterBufferId=transaction.bufferId;
                for(auto* e:{f.engine.get(),f.control.get()}){wrong.generation=e->generation();check(e->enqueueLoop(wrong),"Invalid callback cannot replace occupied MULT slot");}f.cycle();
                check(!owner->consumeQueuedWindowResult(f.next(MeterGraphEventKind::queuedGraphRejected,id))&&owner->queuedWindowTransaction().status==QueuedWindowStatus::pending&&owner->queuedWindowTransaction().ticket==transaction.ticket&&owner->queuedWindowTransaction().queueId==transaction.queueId&&owner->queuedWindowTransaction().bufferId==transaction.bufferId,"Wrong full-width callback leaves pending receipt unchanged");}
            holdWorkerAllocation.store(false);f.cycle(true);
        }else if(stage==1)request(f,*owner,id,receipt);
        const auto installed=f.next(MeterGraphEventKind::bufferInstalled,id);
        check(installed.queueId==receipt.queueId&&installed.bufferId!=receipt.bufferId,"Doubled PCM retains immutable receipt identity");
        auto bad=installed;bad.queueId+=(1ull<<32);check(!owner->consumeBuffer(bad),"Full-width MULT continuation identity");
        check(owner->consumeBuffer(installed)&&owner->committedBuffer()==installed.bufferId&&owner->queuedCallbackBuffer()==receipt.bufferId,"MULT buffer install retains callback authority");
        check(owner->inspect().analyser==identity.analyser&&owner->inspect().data==identity.data,"MULT storage commit does not reset analyser");
        if(stage<2){QueuedMeterCallbackTime current;check(owner->readQueuedCallbackTime(73,{73,100},current)&&current==receipt,"MULT cannot postpone original deadline");
            check(f.view->tracks[id].scheduledFrame==before.scheduledFrame&&f.view->tracks[id].state==LoopState::queued,"MULT commit preserves queue/grid");
            if(stage==0)request(f,*owner,id,receipt);f.cycle();}
        attach(f,*owner,id);check(f.view->tracks[id].length==before.length*2&&f.view->tracks[id].undoCount==before.undoCount+1&&!f.view->tracks[id].redoCount,"MULT adds exactly one doubled history entry");
        check(!owner->requestQueuedGraphDue(receipt,73,{73,receipt.deadlineSeconds+1}),"MULT callback one-shot");
        bool audible=false;while(f.engine->clock()<before.scheduledFrame+2048){f.cycle();if(f.engine->clock()<=before.scheduledFrame)check(f.view->outputPeak==0,"MULT keeps original pre-grid silence");else audible=audible||f.view->outputPeak>0;}
        check(audible,"Doubled forward/reverse source audible after grid");
        f.command(LoopAction::clear,id);f.cycle();check(owner->consumeClear(f.next(MeterGraphEventKind::cleared,id)),"MULT source leases clear");++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(unsigned cancel=0;cancel<6;++cancel){
        Fixture f(block);auto owner=setup(f,id,reverse);QueuedMeterCallbackTime receipt;check(owner->readQueuedCallbackTime(73,{73,100},receipt),"MULT cancellation receipt");
        heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);f.command(LoopAction::multiply,id);f.cycle();waitHeld();
        request(f,*owner,id,receipt);f.cycle();
        if(cancel<3){f.command(cancel==0?LoopAction::stopImmediate:cancel==1?LoopAction::stopAll:LoopAction::clear,id);f.cycle();
            auto event=f.next(cancel==0?MeterGraphEventKind::queuedStop:cancel==1?MeterGraphEventKind::queuedCancel:MeterGraphEventKind::cleared,id);
            check(cancel==0?owner->consumeQueuedStop(event):cancel==1?owner->consumeQueuedCancel(event):owner->consumeClear(event),"Deferred MULT preserves stop/cancel/clear routing");}
        if(cancel==5){const auto transaction=owner->queuedWindowTransaction();check(owner->teardown(),"Cleanup retires in-flight MULT ticket");check(f.control->retireQueuedSourceThrough(f.control->generation(),id,transaction.ticket),"Mirror cleanup retirement on paired control");f.cycle();}
        holdWorkerAllocation.store(false);
        if(cancel==3){f.prepare();check(owner->reset(),"MULT same-rate prepare resets deferred slot");}
        else if(cancel==4){for(auto* e:{f.engine.get(),f.control.get()}){e->prepare(96000,81920);e->setMonitoring(false);e->setTarget(1);}check(owner->reset(),"MULT new-rate prepare resets deferred slot");}
        else f.cycle(true);
        MeterGraphEvent event;while(f.engine->popMeterGraphEvent(event)){
            if(event.kind==MeterGraphEventKind::bufferInstalled)(void)owner->consumeBuffer(event);
            else if(event.kind==MeterGraphEventKind::queuedGraphAccepted||event.kind==MeterGraphEventKind::queuedGraphRejected)(void)owner->consumeQueuedWindowResult(event);
            else check(cancel==5&&event.kind==MeterGraphEventKind::started,"Canceled MULT emits no unrelated graph birth");}
        for(unsigned n=0;n<400;++n){f.cycle();check(f.view->outputPeak==0,"Canceled deferred MULT never revives audio");}
        check(!owner->requestQueuedGraphDue(receipt,73,{73,receipt.deadlineSeconds+1}),"Canceled MULT cannot retarget old receipt");++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true}){
        Fixture f(block);auto owner=setup(f,id,reverse);QueuedMeterCallbackTime receipt;check(owner->readQueuedCallbackTime(73,{73,100},receipt),"MULT OOM receipt");const auto before=f.view->tracks[id];
        check(f.engine->enqueueLoop({LoopAction::multiply,id,++f.request,f.engine->generation(),0,0}),"MULT OOM observed engine only");failWorkerAllocation.store(true);f.cycle(true);
        MeterGraphEvent event;check(!failWorkerAllocation.load()&&f.view->tracks[id].finalizationFailed&&f.view->tracks[id].waveformId==before.waveformId
            &&f.view->tracks[id].undoCount==before.undoCount&&f.view->tracks[id].length==before.length&&!f.engine->popMeterGraphEvent(event),"MULT allocation failure preserves exact PCM/history/queue");
        request(f,*owner,id,receipt);f.cycle();attach(f,*owner,id);while(f.engine->clock()<before.scheduledFrame+1024)f.cycle();++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u}){
        Fixture f(block);auto owner=setup(f,id,false);QueuedMeterCallbackTime receipt;check(owner->readQueuedCallbackTime(73,{73,100},receipt),"MULT FIFO receipt");
        f.command(LoopAction::multiply,id);f.cycle(true);check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"MULT before full FIFO");
        for(unsigned n=0;n<255;++n)f.command(LoopAction::mute,id);
        check(!owner->requestQueuedGraphDue(receipt,73,{73,receipt.deadlineSeconds}),"MULT full FIFO must not consume receipt");f.cycle();f.cycle();request(f,*owner,id,receipt);f.cycle();attach(f,*owner,id);++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true}){
        Fixture f(block);auto owner=setup(f,id,reverse,true);QueuedMeterCallbackTime receipt;check(owner->readQueuedCallbackTime(73,{73,100},receipt),"Full-pool MULT receipt");const auto before=f.view->tracks[id];
        f.command(LoopAction::multiply,id);f.cycle();MeterGraphEvent event;
        check(f.view->rejected&&f.view->tracks[id].waveformId==before.waveformId&&f.view->tracks[id].length==before.length
            &&f.view->tracks[id].undoCount==before.undoCount&&f.view->tracks[id].scheduledFrame==before.scheduledFrame&&!f.engine->popMeterGraphEvent(event),"Full-pool queued MULT rejects atomically");
        request(f,*owner,id,receipt);f.cycle();attach(f,*owner,id);++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true}){
        Fixture f(block);auto owner=setup(f,id,reverse);QueuedMeterCallbackTime receipt;check(owner->readQueuedCallbackTime(73,{73,100},receipt),"MULT loss receipt");
        heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);f.command(LoopAction::multiply,id);f.cycle();waitHeld();request(f,*owner,id,receipt);f.cycle();
        for(unsigned n=0;n<260;++n){f.command(LoopAction::clear,id?0:19);if(n%120==119)f.cycle();}f.cycle();
        check(!f.engine->meterGraphEventsCurrent(),"Actual fact-stream overflow invalidates deferred MULT receipt");
        holdWorkerAllocation.store(false);f.cycle(true);
        const auto grid=f.view->tracks[id].scheduledFrame;while(f.engine->clock()<grid+1024){f.cycle();check(f.view->outputPeak==0&&!f.view->tracks[id].queuedSourceReady,"Lost deferred MULT callback cannot create a source");}
        check(!owner->requestQueuedGraphDue(receipt,73,{73,receipt.deadlineSeconds+1}),"Loss blocks late MULT receipt retry");MeterGraphEvent discarded;for(auto* e:{f.engine.get(),f.control.get()})(void)e->popMeterGraphEvent(discarded);f.prepare();check(owner->reset()&&f.engine->meterGraphEventsCurrent(),"Prepare recovers MULT stream loss");++cases;
    }
    check(allocations.load()==0,"Queued MULT callback new/delete");
    std::cout<<"PASS: pre-birth queued Multiply "<<cases<<" original grid/receipt/history, early/late in-flight worker and duplicate callback, forward/reverse, stop/panic/clear/cleanup/same/new-rate prepare, OOM, full pool, FIFO retry and event-stream loss; tracks 0/19, 64/128, exact paired stereo and no callback new/delete\n";
}

void prebirthNormTests(){
    using namespace aion;
    const auto setup=[](Fixture& f,unsigned id,bool reverse,bool attached=false){
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);f.prepare();
        auto owner=std::make_unique<LoopMeterLifecycle>(*f.engine,id);f.record(id);
        check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Pre-birth NORM installs initial PCM");
        f.command(LoopAction::stopImmediate,id);for(unsigned n=0;n<8;++n)f.cycle();
        if(attached){
            f.command(LoopAction::play,id);f.cycle();check(owner->consumeImmediatePlay(f.next(MeterGraphEventKind::immediatePlay,id)),"Attach old analyser before pre-birth NORM");
            f.command(LoopAction::stopImmediate,id);f.cycle();check(owner->consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop,id)),"Retain old cleanup before pre-birth NORM");
        }
        for(auto* e:{f.engine.get(),f.control.get()}){
            e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
            check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,reverse?1.:0.}),"Pre-birth source direction");
        }
        f.command(LoopAction::play,id);f.cycle(false,{100,true});
        check(owner->consumeQueuedPlay(f.next(MeterGraphEventKind::queuedPlay,id)),"Pre-birth accepted play receipt");return owner;
    };
    const auto waitHeld=[](){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(heldWorkerAllocations.load()<2&&std::chrono::steady_clock::now()<end)std::this_thread::yield();
        check(heldWorkerAllocations.load()>=2,"Pre-birth workers pause outside the callback");};
    const auto request=[](Fixture& f,LoopMeterLifecycle& owner,unsigned id,const QueuedMeterCallbackTime& expected){
        check(owner.requestQueuedGraphDue(expected,73,{73,expected.deadlineSeconds}),"Original due receipt survives NORM");
        const auto transaction=owner.queuedWindowTransaction();
        LoopCommand command{LoopAction::meterQueuedGraphRequest,id,0,f.control->generation(),0,0};
        command.meterGraphTicket=transaction.ticket;command.meterQueueId=transaction.queueId;command.meterBufferId=transaction.bufferId;
        check(f.control->enqueueLoop(command),"Mirror pre-birth conditional callback");
        check(transaction.bufferId==expected.bufferId&&!owner.requestQueuedGraphDue(expected,73,{73,expected.deadlineSeconds+1}),"One immutable original receipt, one-shot delivery");
    };
    const auto attach=[](Fixture& f,LoopMeterLifecycle& owner,unsigned id){
        f.cycle();(void)f.next(MeterGraphEventKind::started,id);
        check(owner.consumeQueuedWindowResult(f.next(MeterGraphEventKind::queuedGraphAccepted,id)),"NORM does not obsolete an accepted in-flight graph result");
    };
    unsigned cases=0;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(unsigned stage=0;stage<4;++stage){
        Fixture f(block);auto owner=setup(f,id,reverse);QueuedMeterCallbackTime expected;
        check(owner->readQueuedCallbackTime(73,{73,100},expected),"Read original pre-birth deadline");
        const auto grid=f.view->tracks[id].scheduledFrame,oldBuffer=owner->committedBuffer();const auto identity=owner->inspect();
        if(stage>=2){heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);}
        f.command(LoopAction::normalize,id);f.cycle(stage<2);check(!f.view->rejected,"NORM accepts a pending queue before graph creation");
        if(stage>=2)waitHeld();
        if(stage<2){
            const auto installed=f.next(MeterGraphEventKind::bufferInstalled,id);
            check(installed.queueId==expected.queueId&&installed.bufferId!=oldBuffer,"NORM publishes PCM separately from its retained play receipt");
            if(stage==1)request(f,*owner,id,expected); // The message owner has not observed the PCM install yet.
            auto bad=installed;bad.queueId+=(1ull<<32);check(!owner->consumeBuffer(bad)&&owner->committedBuffer()==oldBuffer,"Wrong full-width continuation cannot retarget the queue");
            check(owner->consumeBuffer(installed)&&owner->queuedCallbackBuffer()==expected.bufferId&&owner->committedBuffer()==installed.bufferId
                &&owner->inspect().analyser==identity.analyser&&owner->inspect().data==identity.data,"NORM preserves graph history and the original callback buffer identity");
            QueuedMeterCallbackTime current;check(owner->readQueuedCallbackTime(73,{73,100},current)&&current==expected,"All command-origin deadline fields remain exact after NORM");
            if(stage==0){
                f.command(LoopAction::normalize,id);f.cycle(true);check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Repeated no-op NORM retains the same receipt");
                check(owner->readQueuedCallbackTime(73,{73,100},current)&&current==expected,"Repeated NORM cannot postpone or replace the deadline");
                request(f,*owner,id,expected);
            }
        }else{
            if(stage==3)while(f.engine->clock()<grid+block){f.cycle();check(f.view->outputPeak==0&&!f.view->tracks[id].queuedSourceReady,"A passed grid and pending NORM do not create a source");}
            request(f,*owner,id,expected);
        }
        attach(f,*owner,id);const auto binding=owner->binding();
        while(f.engine->clock()<grid+block){f.cycle();if(f.engine->clock()<=grid)check(f.view->outputPeak==0,"Pre-birth NORM preserves pre-grid silence");}
        if(stage>=2){const auto revision=f.view->tracks[id].sourceRevision;holdWorkerAllocation.store(false);f.cycle(true);
            check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id))&&f.view->tracks[id].sourceRevision==revision,"Late NORM commit does not restart its already-created source");}
        check(f.view->tracks[id].state==LoopState::playing&&owner->binding().ticket==binding.ticket&&!f.view->tracks[id].undoCount&&!f.view->tracks[id].redoCount,"Source, graph and history survive pre-birth NORM");
        f.command(LoopAction::clear,id);f.cycle();check(owner->consumeClear(f.next(MeterGraphEventKind::cleared,id)),"Clear releases pre-birth NORM ownership");++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(unsigned cancel=0;cancel<5;++cancel){
        Fixture f(block);auto owner=setup(f,id,reverse);QueuedMeterCallbackTime expected;check(owner->readQueuedCallbackTime(73,{73,100},expected),"Cancellation seed receipt");
        heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);f.command(LoopAction::normalize,id);f.cycle();waitHeld();
        if(cancel<3){f.command(cancel==0?LoopAction::stopImmediate:cancel==1?LoopAction::stopAll:LoopAction::clear,id);f.cycle();
            auto event=f.next(cancel==0?MeterGraphEventKind::queuedStop:cancel==1?MeterGraphEventKind::queuedCancel:MeterGraphEventKind::cleared,id);
            check(cancel==0?owner->consumeQueuedStop(event):cancel==1?owner->consumeQueuedCancel(event):owner->consumeClear(event),"Cancellation retains the actual queued routing");}
        holdWorkerAllocation.store(false);
        if(cancel==3){f.prepare();check(owner->reset(),"Same-rate prepare retires old receipt");}
        else if(cancel==4){for(auto* e:{f.engine.get(),f.control.get()}){e->prepare(96000,81920);e->setMonitoring(false);e->setTarget(1);}check(owner->reset(),"New-rate prepare retires old receipt");}
        else f.cycle(true);
        for(unsigned n=0;n<400;++n)f.cycle();
        MeterGraphEvent remaining;while(f.engine->popMeterGraphEvent(remaining))
            check(remaining.kind==MeterGraphEventKind::bufferInstalled&&owner->consumeBuffer(remaining),"Canceled NORM's remaining buffer facts cannot create a graph");
        check(!owner->requestQueuedGraphDue(expected,73,{73,expected.deadlineSeconds+1})&&f.view->tracks[id].state==((cancel==2||cancel==4)?LoopState::empty:LoopState::stopped),"NORM completion cannot resurrect a canceled or prepared queue");++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true}){
        Fixture f(block);auto owner=setup(f,id,reverse);QueuedMeterCallbackTime expected;check(owner->readQueuedCallbackTime(73,{73,100},expected),"OOM seed receipt");
        const auto buffer=owner->committedBuffer();check(f.engine->enqueueLoop({LoopAction::normalize,id,++f.request,f.engine->generation(),0,0}),"Pre-birth OOM edit only on observed engine");
        failWorkerAllocation.store(true);f.cycle(true);check(!failWorkerAllocation.load()&&f.view->tracks[id].finalizationFailed&&f.view->tracks[id].waveformId==buffer&&owner->committedBuffer()==buffer,"OOM retains old PCM and receipt");
        request(f,*owner,id,expected);attach(f,*owner,id);const auto grid=f.view->tracks[id].scheduledFrame;while(f.engine->clock()<grid+2048)f.cycle();++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u}){
        Fixture f(block);auto owner=setup(f,id,true);QueuedMeterCallbackTime expected;check(owner->readQueuedCallbackTime(73,{73,100},expected),"Saturation seed receipt");
        f.command(LoopAction::normalize,id);f.cycle(true);check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Saturation NORM continuation");
        const auto before=owner->queuedWindowTransaction();for(unsigned n=0;n<255;++n)f.command(LoopAction::mute,id);
        check(!owner->requestQueuedGraphDue(expected,73,{73,expected.deadlineSeconds})&&owner->queuedWindowTransaction().ticket==before.ticket,"A full command FIFO cannot consume the preserved NORM receipt");
        f.cycle();f.cycle();request(f,*owner,id,expected);attach(f,*owner,id);
        f.command(LoopAction::stopImmediate,id);f.cycle();check(owner->consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop,id)),"NORM's new PCM routes the accepted graph stop");
        f.command(LoopAction::play,id);f.cycle(false,{101,true});check(owner->consumeQueuedPlay(f.next(MeterGraphEventKind::queuedPlay,id)),"Replacement queued intent after NORM");
        check(!owner->requestQueuedGraphDue(expected,73,{73,expected.deadlineSeconds+1}),"An old NORM timer cannot retarget the replacement intent");++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u}){
        Fixture f(block);auto owner=setup(f,id,false);QueuedMeterCallbackTime expected;check(owner->readQueuedCallbackTime(73,{73,100},expected),"Boolean guard seed receipt");
        f.command(LoopAction::normalize,id);f.cycle(true);check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Boolean guard NORM continuation");
        check(owner->setBuffer(true)&&!owner->queuedCallbackBuffer()&&!owner->requestQueuedGraphDue(expected,73,{73,expected.deadlineSeconds}),"A bare buffer boolean cannot inherit NORM's receipt authority");++cases;
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true}){
        Fixture f(block);auto owner=setup(f,id,reverse,true);QueuedMeterCallbackTime expected;check(owner->readQueuedCallbackTime(73,{73,100},expected),"Attached analyser seed deadline");
        const auto identity=owner->inspect();const auto cleanup=owner->cleanupRequest();
        f.command(LoopAction::normalize,id);f.cycle(true);check(owner->consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Attached analyser NORM continuation");
        check(identity.analyser&&owner->inspect().analyser==identity.analyser&&owner->inspect().data==identity.data&&owner->cleanupRequest()==cleanup,"NORM retains old analyser/data and exact cleanup request");
        check(owner->deliverCleanup(cleanup)&&!owner->inspect().present,"Old cleanup precedes the delayed graph callback");
        request(f,*owner,id,expected);attach(f,*owner,id);
        check(owner->inspect().present&&owner->inspect().analyser!=identity.analyser&&!owner->deliverCleanup(cleanup),"Fresh callback survives old cleanup and obsolete timer delivery");
        const auto grid=f.view->tracks[id].scheduledFrame;while(f.engine->clock()<grid+1024)f.cycle();++cases;
    }
    check(allocations.load()==0,"Pre-birth NORM routing has no callback new/delete");
    std::cout<<"PASS: pre-birth NORM "<<cases<<" before/in-flight callback, repeated no-op, delayed worker/late callback, exact original deadline, stop/clear/prepare and OOM cases at tracks 0/19, reverse, 64/128; no callback new/delete\n";
}
void normPendingTests(){
    using namespace aion;
    const auto waitHeld=[](unsigned count){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(heldWorkerAllocations.load()<count&&std::chrono::steady_clock::now()<end)std::this_thread::yield();
        check(heldWorkerAllocations.load()>=count,"NORM workers pause outside audio processing");};
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(unsigned stage:{0u,1u,2u})for(bool unity:{false,true}){
        Fixture f(block,MeterEventScope::acceptedPlayback);f.record(id);
        if(unity){f.command(LoopAction::normalize,id);f.cycle(true);}
        for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,reverse?1.:0.}),"Initial NORM source direction");
        f.command(LoopAction::sourceRestart,id);f.cycle();
        for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,reverse?0.:1.}),"Pending reversed snapshot direction");
        const auto first=f.engine->clock()+2400;f.command(LoopAction::applyStart,id);f.cycle();
        for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,reverse?1.:0.}),"Second pending source direction");
        const auto second=f.engine->clock()+2400;f.command(LoopAction::applyStart,id);f.cycle();
        if(stage){f.command(LoopAction::retrigger,id);f.cycle();}
        if(stage==2)while(f.engine->clock()<first+block)f.cycle();
        const auto serial=f.view->tracks[id].sourceRevision,buffer=f.view->tracks[id].waveformId;
        heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);
        f.command(LoopAction::normalize,id);f.cycle();waitHeld(2);
        check(f.view->tracks[id].state==LoopState::finalizing&&!f.view->rejected,"NORM is accepted without canceling source ownership");
        // Keep both immutable PCM jobs pending through the original source
        // deadlines. Pausing a test allocator never blocks the audio callback.
        while(f.engine->clock()<second+block)f.cycle();
        check(!f.view->tracks[id].phaseApplyFrame&&f.view->tracks[id].sourceRevision>=serial+(stage==0?2u:stage==1?1u:0u),"Source events execute while NORM remains on the worker");
        holdWorkerAllocation.store(false);f.cycle(true);
        check(f.view->tracks[id].state==LoopState::playing&&f.view->tracks[id].waveformId!=buffer&&!f.view->tracks[id].finalizationFailed
            &&!f.view->tracks[id].undoCount&&!f.view->tracks[id].redoCount,"NORM commit preserves playing state and history without retriggering");
        if(!stage){f.command(LoopAction::retrigger,id);f.cycle();}
        for(unsigned n=0;n<48;++n)f.cycle();
        f.command(LoopAction::clear,id);for(unsigned n=0;n<64;++n)f.cycle();
        check(f.view->tracks[id].state==LoopState::empty&&!f.view->tracks[id].phaseApplyFrame,"Clear releases scheduled reversed leases after NORM");
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(unsigned cancel=0;cancel<5;++cancel){
        Fixture f(block,MeterEventScope::acceptedPlayback);f.record(id);
        for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,1}),"NORM cancellation pins reversed future PCM");
        f.command(LoopAction::applyStart,id);f.cycle();f.command(LoopAction::applyStart,id);f.cycle();f.command(LoopAction::retrigger,id);f.cycle();
        heldWorkerAllocations.store(0);holdWorkerAllocation.store(true);f.command(LoopAction::normalize,id);f.cycle();waitHeld(2);
        if(cancel<3){f.command(cancel==0?LoopAction::stopImmediate:cancel==1?LoopAction::stopAll:LoopAction::clear,id);f.cycle();}
        holdWorkerAllocation.store(false);
        if(cancel==3)f.prepare();
        else if(cancel==4)for(auto* e:{f.engine.get(),f.control.get()}){e->prepare(96000,81920);e->setMonitoring(false);e->setTarget(1);}
        else f.cycle(true);
        for(unsigned n=0;n<128;++n)f.cycle();
        if(f.view->tracks[id].state!=((cancel==2||cancel==4)?LoopState::empty:LoopState::stopped)||f.view->tracks[id].phaseApplyFrame)
            std::cerr<<"NORM cancellation "<<block<<"/"<<id<<"/"<<cancel<<", state "<<static_cast<unsigned>(f.view->tracks[id].state)<<", phase "<<f.view->tracks[id].phaseApplyFrame<<", rejected "<<f.view->rejected<<'\n';
        check(f.view->tracks[id].state==((cancel==2||cancel==4)?LoopState::empty:LoopState::stopped)&&!f.view->tracks[id].phaseApplyFrame,"Canceled worker NORM cannot resurrect stopped/cleared/prepared sources");
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true}){
        Fixture f(block,MeterEventScope::acceptedPlayback);f.record(id);
        for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,reverse?1.:0.}),"OOM future source direction");
        f.command(LoopAction::applyStart,id);f.cycle();const auto target=f.engine->clock()+2400;f.command(LoopAction::applyStart,id);f.cycle();
        const auto buffer=f.view->tracks[id].waveformId;
        check(f.engine->enqueueLoop({LoopAction::normalize,id,++f.request,f.engine->generation(),0,0}),"OOM NORM on observed engine only");
        failWorkerAllocation.store(true);f.cycle(true);
        check(!failWorkerAllocation.load()&&f.view->tracks[id].finalizationFailed&&f.view->tracks[id].waveformId==buffer
            &&f.view->tracks[id].phaseApplyFrame==target,"NORM OOM preserves old PCM and all accepted source deadlines");
        while(f.engine->clock()<target+2048)f.cycle(); // Exact stereo against an engine with no NORM attempt.
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u}){
        Fixture f(block,MeterEventScope::acceptedPlayback);f.record(id);
        for(unsigned batch=0;batch<4;++batch){
            for(unsigned n=0;n<32;++n){f.command(LoopAction::applyStart,id);f.command(LoopAction::retrigger,id);}
            f.cycle();check(!f.view->rejected,"Full-pool NORM fixture admits each bounded command batch");
            // Retire the 5ms phase events before the next batch so the 127-event
            // queue is not the limiting resource. All 50ms leases remain live.
            for(unsigned n=0;n<240/block+1;++n)f.cycle();
        }
        f.command(LoopAction::normalize,id);f.cycle(true);
        check(!f.view->rejected&&f.view->tracks[id].state==LoopState::playing,"NORM commits with all 128 overlap reservations occupied");
        f.command(LoopAction::applyStart,id);f.command(LoopAction::retrigger,id);f.cycle();
        check(f.view->rejected&&f.view->tracks[id].phaseApplyFrame>f.engine->clock(),"NORM cannot discard overlap reservations to admit another RTRG");
        f.command(LoopAction::clear,id);for(unsigned n=0;n<64;++n)f.cycle();
        check(f.view->tracks[id].state==LoopState::empty,"Full-pool NORM clear releases all forward PCM leases");
    }
    check(allocations.load()==0,"NORM source ownership performs no callback new/delete");
    std::cout<<"PASS: NORM pending/active Apply/RTRG at tracks 0/19, reverse, 64/128; 48 changed/no-op delayed-worker playback, 20 cancellation/same/new-rate prepare, 8 exact OOM rollback and 4 full-pool cases; no callback new/delete\n";
}
void meterOverlapTests(){queuedReplacementSpeedTests(true);queuedOverlapCapacityTests();queuedChainCapacityTests();queuedTripleCapacityTests();}
void queuedEarlySpeedTests(){
    using namespace aion;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})for(float speed:{.25f,.5f,2.f}){
        Fixture f(block,MeterEventScope::acceptedPlayback);
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);f.prepare();
        f.record(id);(void)f.next(MeterGraphEventKind::bufferInstalled,id);
        f.command(LoopAction::stopImmediate,id);for(unsigned n=0;n<8;++n)f.cycle();
        for(auto* e:{f.engine.get(),f.control.get()}){
            e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
            check(e->enqueueLoop({LoopAction::startDelay,id,0,e->generation(),0,.217}),"Early speed captured Start");
            check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,reverse?1.:0.}),"Early speed captured direction");
        }
        f.command(LoopAction::play,id);f.cycle();const auto q=f.next(MeterGraphEventKind::queuedPlay,id);
        const auto grid=f.view->tracks[id].scheduledFrame,waveform=f.view->tracks[id].waveformId;
        for(auto* e:{f.engine.get(),f.control.get()}){LoopCommand birth{LoopAction::meterQueuedGraphRequest,id,0,e->generation(),0,0};
            birth.meterGraphTicket=1;birth.meterQueueId=q.queueId;birth.meterBufferId=q.bufferId;check(e->enqueueLoop(birth),"Early speed explicit source admission");}
        f.cycle();(void)f.next(MeterGraphEventKind::started,id);(void)f.next(MeterGraphEventKind::queuedGraphAccepted,id);
        while(f.engine->clock()+block<grid-3840)f.cycle();
        check(f.control->enqueueLoop({LoopAction::speed,id,0,f.control->generation(),0,speed}),"Comparison changes speed 80ms before grid");
        while(f.engine->clock()+block<grid)f.cycle();
        const bool scheduledStop=speed==.5f;
        if(scheduledStop)f.command(LoopAction::stop,id); // Inside the minimum lead: stop at the following grid.
        check(f.engine->enqueueLoop({LoopAction::speed,id,0,f.engine->generation(),0,speed}),"Primary changes speed within one block of grid");
        // Equality begins at the first output sample, including source smoothing:
        // both sources are still inaudible when their AudioParam is updated.
        for(unsigned n=0;n<32;++n)f.cycle();
        check(f.view->tracks[id].state==(scheduledStop?LoopState::stopping:LoopState::playing)&&f.view->tracks[id].waveformId==waveform&&f.view->tracks[id].length==512
            &&f.view->tracks[id].playbackRate==speed&&!f.view->tracks[id].phaseApplyFrame&&!f.view->rejected,"Early speed retains clip/source and produces exact stereo PCM without replacement");
        if(scheduledStop){const auto stopAt=f.view->tracks[id].scheduledFrame;check(stopAt>grid,"Queued speed edit retains the later normal STOP deadline");
            while(f.engine->clock()<stopAt+512)f.cycle();
            check(f.view->tracks[id].state==LoopState::stopped&&!f.view->tracks[id].scheduledFrame,"Early speed source stops at its retained deadline without resurrection");}

    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(unsigned cancel=0;cancel<5;++cancel){
        Fixture f(block,MeterEventScope::acceptedPlayback);
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);f.prepare();
        f.record(id);(void)f.next(MeterGraphEventKind::bufferInstalled,id);f.command(LoopAction::stopImmediate,id);for(unsigned n=0;n<8;++n)f.cycle();
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
        f.command(LoopAction::play,id);f.cycle();const auto q=f.next(MeterGraphEventKind::queuedPlay,id);
        for(auto* e:{f.engine.get(),f.control.get()}){LoopCommand birth{LoopAction::meterQueuedGraphRequest,id,0,e->generation(),0,0};
            birth.meterGraphTicket=1;birth.meterQueueId=q.queueId;birth.meterBufferId=q.bufferId;check(e->enqueueLoop(birth),"Speed cancellation source admission");
            check(e->enqueueLoop({LoopAction::speed,id,0,e->generation(),0,.25}),"Speed before cancellation");}
        f.cycle();(void)f.next(MeterGraphEventKind::started,id);(void)f.next(MeterGraphEventKind::queuedGraphAccepted,id);
        if(cancel<3)f.command(cancel==0?LoopAction::stopImmediate:cancel==1?LoopAction::stopAll:LoopAction::clear,id);
        else if(cancel==3)f.prepare();
        else for(auto* e:{f.engine.get(),f.control.get()})check(e->retireQueuedSourceThrough(e->generation(),id,1),"Speed source cleanup fence");
        for(unsigned n=0;n<400;++n)f.cycle();
        check(f.view->tracks[id].state==(cancel==2?LoopState::empty:LoopState::stopped)&&!f.view->tracks[id].queuedSourceReady,"Speed edits cannot survive source cancellation or restore its old grid");
    }
    check(allocations.load()==0,"Early speed processing performs no callback new/delete");
    std::cout<<"PASS: 24 early speed stereo equality cases at first grid sample (80ms versus one-block edit), reverse, tracks 0/19 and 64/128; 8 later scheduled STOP cases and 20 cancellation/prepare/cleanup cases, no callback new/delete\n";
}
void queuedEarlyRestartTests(){
    using namespace aion;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})
    for(bool beforeGrid:{false,true})for(auto action:{LoopAction::applyStart,LoopAction::quantizeStart,LoopAction::nudgeStart,LoopAction::retrigger}){
        Fixture f(block,MeterEventScope::acceptedPlayback);
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);f.prepare();
        f.command(LoopAction::record,id);for(unsigned n=0;n<8192/block;++n)f.cycle();f.command(LoopAction::finish,id);f.cycle(true);
        (void)f.next(MeterGraphEventKind::bufferInstalled,id);
        f.command(LoopAction::stopImmediate,id);for(unsigned n=0;n<8;++n)f.cycle();
        for(auto* e:{f.engine.get(),f.control.get()}){
            e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
            check(e->enqueueLoop({LoopAction::startDelay,id,0,e->generation(),0,.217}),"Early restart initial Start");
            check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,reverse?1.:0.}),"Early restart initial direction");
        }
        f.command(LoopAction::play,id);f.cycle();const auto q=f.next(MeterGraphEventKind::queuedPlay,id);
        const auto grid=f.view->tracks[id].scheduledFrame;
        for(auto* e:{f.engine.get(),f.control.get()}){
            LoopCommand birth{LoopAction::meterQueuedGraphRequest,id,0,e->generation(),0,0};
            birth.meterGraphTicket=1;birth.meterQueueId=q.queueId;birth.meterBufferId=q.bufferId;
            check(e->enqueueLoop(birth),"Early restart requires explicit graph callback");
        }
        f.cycle();(void)f.next(MeterGraphEventKind::started,id);(void)f.next(MeterGraphEventKind::queuedGraphAccepted,id);
        const auto edit=grid-(beforeGrid?3840:(action==LoopAction::retrigger?1:960));
        while(f.engine->clock()+block<edit)f.cycle();
        const auto target=f.engine->clock()+(action==LoopAction::retrigger?240:2400);
        for(auto* e:{f.engine.get(),f.control.get()}){
            check(e->enqueueLoop({LoopAction::startDelay,id,0,e->generation(),0,.731}),"Early restart edited Start");
            check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,reverse?0.:1.}),"Early restart new source direction");
        }
        // Independent baseline: the existing, validated PLAYING restart path.
        // Its preparatory source is audible only in the comparison engine; compare
        // PCM after replacement transitions settle, plus full reference-source PCM
        // from the first sample in the zero-envelope JS oracle.
        check(f.control->enqueueLoop({LoopAction::stopImmediate,id,0,f.control->generation(),0,0})
            &&f.control->enqueueLoop({LoopAction::sourceRestart,id,0,f.control->generation(),0,0}),"Comparison uses ordinary playing source restart");
        for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({action,id,0,e->generation(),0,action==LoopAction::nudgeStart?(reverse?-1.:1.):0.}),"Schedule early Apply/Q/nudge");
        const bool scheduledStop=!beforeGrid&&(action==LoopAction::quantizeStart||action==LoopAction::retrigger);
        if(scheduledStop)f.command(LoopAction::stop,id);
        bool audible=false;auto end=std::max(grid,target)+4096;std::uint64_t stopAt=0;
        while(f.engine->clock()<end){
            const auto begin=f.engine->clock();std::array<float,128> l{},r{},cl{},cr{};float* channels[]={l.data(),r.data()},*other[]={cl.data(),cr.data()};
            watch=true;f.engine->process(channels,2,static_cast<int>(block));f.control->process(other,2,static_cast<int>(block));watch=false;
            while(f.engine->popView(*f.view)){}
            for(unsigned i=0;i<block;++i){
                if(begin+i<grid)check(l[i]==0&&r[i]==0,"Early replacement advances silently behind original graph volume gate");
                if(begin+i>=std::max(grid,target)+960){check(l[i]==cl[i]&&r[i]==cr[i],"Early restart matches ordinary source stereo PCM after bounded transition, across old grid");audible=audible||l[i]!=0||r[i]!=0;}
            }
            if(f.engine->clock()<=std::min(grid,target))check(f.view->tracks[id].state==LoopState::queued&&f.view->tracks[id].phaseApplyFrame==target,"Early replacement retains future deadline before activation");
            if(scheduledStop&&!stopAt&&f.engine->clock()>grid){stopAt=f.view->tracks[id].scheduledFrame;check(stopAt>target,"Following-grid STOP stays later than replacement");end=stopAt+512;}
            if(f.engine->clock()>target){
                const auto expected=scheduledStop?(f.engine->clock()>stopAt?LoopState::stopped:LoopState::stopping):LoopState::playing;
                check(f.view->tracks[id].state==expected&&f.view->tracks[id].scheduledFrame==(expected==LoopState::stopping?stopAt:0)&&!f.view->tracks[id].phaseApplyFrame,"Replacement consumes its own deadline and retains later STOP exactly once");
            }
        }
        check(audible&&f.view->tracks[id].length==8192,"Early replacement remains audible without changing stored clip");
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(unsigned cancel=0;cancel<8;++cancel)for(bool rtrg:{false,true}){
        Fixture f(block,MeterEventScope::acceptedPlayback);
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);f.prepare();
        if(cancel==6){f.command(LoopAction::record,id);for(unsigned n=0;n<8192/block;++n)f.cycle();f.command(LoopAction::finish,id);f.cycle(true);}else f.record(id);
        (void)f.next(MeterGraphEventKind::bufferInstalled,id);f.command(LoopAction::stopImmediate,id);for(unsigned n=0;n<8;++n)f.cycle();
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
        f.command(LoopAction::play,id);f.cycle();const auto q=f.next(MeterGraphEventKind::queuedPlay,id);
        if(cancel!=5){
            for(auto* e:{f.engine.get(),f.control.get()}){LoopCommand birth{LoopAction::meterQueuedGraphRequest,id,0,e->generation(),0,0};
                birth.meterGraphTicket=1;birth.meterQueueId=q.queueId;birth.meterBufferId=q.bufferId;check(e->enqueueLoop(birth),"Cancellation fixture admits early graph");}
            f.cycle();(void)f.next(MeterGraphEventKind::started,id);(void)f.next(MeterGraphEventKind::queuedGraphAccepted,id);
        }
        if(cancel==7){f.command(LoopAction::stop,id);f.cycle();}
        for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::startDelay,id,0,e->generation(),0,.731}),"Cancellation fixture Start");
        const auto target=f.engine->clock()+((rtrg&&cancel!=6)?240:2400);f.command(rtrg&&cancel!=6?LoopAction::retrigger:LoopAction::applyStart,id);f.cycle();
        check(f.view->tracks[id].phaseApplyFrame==((cancel==5||cancel==7)?0:target),"Unadmitted/stopping queued source cannot schedule a restart");
        if(cancel==6){
            const auto fullDeadline=f.engine->clock()+2400;
            for(unsigned n=0;n<126;++n)f.command(LoopAction::applyStart,id);
            if(rtrg)f.command(LoopAction::retrigger,id);
            f.command(LoopAction::quantizeStart,id);
            for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::nudgeStart,id,0,e->generation(),0,1}),"Queue nudge against full early-source phase FIFO");
            f.cycle();f.cycle();
            check(f.view->rejected&&f.view->tracks[id].startDelay==.731&&f.view->tracks[id].phaseApplyFrame==fullDeadline,"Full phase FIFO rejects Q/nudge with metadata and accepted deadlines intact");
            f.command(LoopAction::stopAll,id);
        }else if(cancel<3)f.command(cancel==0?LoopAction::stopImmediate:cancel==1?LoopAction::stopAll:LoopAction::clear,id);
        else if(cancel==3)f.prepare();
        else if(cancel==4){
            check(f.engine->retireQueuedSourceThrough(f.engine->generation(),id,1),"Retained cleanup retires pending early restart");
            check(f.control->enqueueLoop({LoopAction::stopImmediate,id,0,f.control->generation(),0,0}),"Comparison cancels pending restart");
        }
        for(unsigned n=0;n<400;++n)f.cycle();
        check(f.view->tracks[id].state==(cancel==2?LoopState::empty:cancel==5?LoopState::queued:LoopState::stopped)&&!f.view->tracks[id].phaseApplyFrame,"Cancel/clear/prepare/cleanup and unadmitted source cannot resurrect a phase event");
    }
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(auto cancel:{LoopAction::stopImmediate,LoopAction::stopAll,LoopAction::clear,LoopAction::meterGraphStart})for(bool rtrg:{false,true}){
        Fixture f(block,MeterEventScope::acceptedPlayback);
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);f.prepare();
        f.record(id);(void)f.next(MeterGraphEventKind::bufferInstalled,id);f.command(LoopAction::stopImmediate,id);for(unsigned n=0;n<8;++n)f.cycle();
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
        f.command(LoopAction::play,id);f.cycle();const auto q=f.next(MeterGraphEventKind::queuedPlay,id);const auto grid=f.view->tracks[id].scheduledFrame;
        for(auto* e:{f.engine.get(),f.control.get()}){LoopCommand birth{LoopAction::meterQueuedGraphRequest,id,0,e->generation(),0,0};
            birth.meterGraphTicket=1;birth.meterQueueId=q.queueId;birth.meterBufferId=q.bufferId;check(e->enqueueLoop(birth),"Gated cancellation admits actual source");}
        f.cycle();(void)f.next(MeterGraphEventKind::started,id);(void)f.next(MeterGraphEventKind::queuedGraphAccepted,id);
        f.command(rtrg?LoopAction::retrigger:LoopAction::applyStart,id);const auto target=f.engine->clock()+(rtrg?240:2400);
        while(f.engine->clock()<target+512)f.cycle();
        check(f.engine->clock()<grid&&f.view->tracks[id].state==LoopState::playing,"Source is advancing before retained graph volume start");
        // The sentinel case drives the independent retained-cleanup fence.
        if(cancel==LoopAction::meterGraphStart){for(auto* e:{f.engine.get(),f.control.get()})check(e->retireQueuedSourceThrough(e->generation(),id,1),"Cleanup fence closes an advancing silent source");}
        else f.command(cancel,id);
        f.cycle();
        while(f.engine->clock()<grid+512){
            std::array<float,128> l{},r{};float* channels[]={l.data(),r.data()};
            watch=true;f.engine->process(channels,2,static_cast<int>(block));watch=false;
            check(std::all_of(l.begin(),l.end(),[](float v){return v==0;})&&std::all_of(r.begin(),r.end(),[](float v){return v==0;}),"Cancel while graph is silent cannot leak a raw-source tail or reopen at its old grid");
        }
    }
    check(allocations.load()==0,"Early restart processing performs no callback new/delete");
    std::cout<<"PASS: 64 early Apply/Q/nudge/RTRG source cases before/after original grid, exact post-transition stereo PCM, no old-grid resurrection; 64 cancel/prepare/cleanup/unadmitted/stopping/full-phase-FIFO cases plus 32 silent-source tail cancellations at tracks 0/19 and 64/128, no callback new/delete\n";
}
void queuedEarlyControlTests(){
    using namespace aion;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool reverse:{false,true})
    for(unsigned edit=0;edit<3;++edit)for(bool scheduledStop:{false,true}){
        Fixture f(block,MeterEventScope::acceptedPlayback);
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);f.prepare();
        f.record(id);(void)f.next(MeterGraphEventKind::bufferInstalled,id);
        f.command(LoopAction::stopImmediate,id);for(unsigned n=0;n<8;++n)f.cycle();
        for(auto* e:{f.engine.get(),f.control.get()}){
            e->configureSync({SyncMode::fixed,137.3,1,1,4,false});
            check(e->enqueueLoop({LoopAction::startDelay,id,0,e->generation(),0,.111}),"Initial queued Start metadata");
        }
        f.command(LoopAction::play,id);f.cycle();const auto q=f.next(MeterGraphEventKind::queuedPlay,id);
        const auto grid=f.view->tracks[id].scheduledFrame;
        for(auto* e:{f.engine.get(),f.control.get()}){
            // Controls changed before birth belong to this new source.
            check(e->enqueueLoop({LoopAction::startDelay,id,0,e->generation(),0,.217}),"Set Start between intent and actual source birth");
            check(e->enqueueLoop({LoopAction::reverse,id,0,e->generation(),0,reverse?1.:0.}),"Set direction at actual source birth");
            LoopCommand birth{LoopAction::meterQueuedGraphRequest,id,0,e->generation(),0,0};
            birth.meterGraphTicket=1;birth.meterQueueId=q.queueId;birth.meterBufferId=q.bufferId;
            check(e->enqueueLoop(birth),"Create explicit early source");
        }
        f.cycle();(void)f.next(MeterGraphEventKind::started,id);(void)f.next(MeterGraphEventKind::queuedGraphAccepted,id);
        while(f.engine->clock()+block<grid-960)f.cycle();
        const double nextDelay=edit==1?.217:.731;const bool nextReverse=edit==0?reverse:!reverse;
        check(f.engine->enqueueLoop({LoopAction::startDelay,id,0,f.engine->generation(),0,nextDelay}),"Edit Start after source construction");
        check(f.engine->enqueueLoop({LoopAction::reverse,id,0,f.engine->generation(),0,nextReverse?1.:0.}),"Edit Reverse after source construction");
        if(scheduledStop)f.command(LoopAction::stop,id);
        f.cycle();
        if(scheduledStop)(void)f.next(MeterGraphEventKind::scheduledStop,id);
        check(f.view->tracks[id].state==LoopState::queued&&f.view->tracks[id].startDelay==nextDelay&&f.view->tracks[id].reverse==nextReverse,
            "Early edits update visible metadata while keeping the future-source gate");
        while(f.engine->clock()<grid+2048)f.cycle();
        check(f.view->tracks[id].state==(scheduledStop?LoopState::stopping:LoopState::playing),"Early source starts with optional following-grid STOP intact");
        // Pair against an unedited source through the grid, then give the control
        // the new metadata only for the next source. Neither timing nor PCM may
        // differ from either independent baseline.
        check(f.control->enqueueLoop({LoopAction::startDelay,id,0,f.control->generation(),0,nextDelay}),"Apply next-source Start to comparison");
        check(f.control->enqueueLoop({LoopAction::reverse,id,0,f.control->generation(),0,nextReverse?1.:0.}),"Apply next-source direction to comparison");
        f.command(LoopAction::stopImmediate,id);for(unsigned n=0;n<8;++n)f.cycle();
        MeterGraphEvent event;while(f.engine->popMeterGraphEvent(event)){}
        f.command(LoopAction::play,id);f.cycle();const auto fresh=f.next(MeterGraphEventKind::queuedPlay,id);
        for(auto* e:{f.engine.get(),f.control.get()}){
            LoopCommand birth{LoopAction::meterQueuedGraphRequest,id,0,e->generation(),0,0};
            birth.meterGraphTicket=2;birth.meterQueueId=fresh.queueId;birth.meterBufferId=fresh.bufferId;
            check(e->enqueueLoop(birth),"Next source captures newly accepted controls");
        }
        f.cycle();(void)f.next(MeterGraphEventKind::started,id);(void)f.next(MeterGraphEventKind::queuedGraphAccepted,id);
        const auto nextGrid=f.view->tracks[id].scheduledFrame;
        while(f.engine->clock()<nextGrid+1024)f.cycle();
    }
    check(allocations.load()==0,"Early source control snapshots use no callback new/delete");
    std::cout<<"PASS: early callback captures Start/direction, pre-grid metadata edits preserve exact current stereo PCM and apply on next play, following-grid STOP intact; 48 cases at tracks 0/19 and 64/128, no callback new/delete\n";
}
void queuedCleanupSourceTests(){
    using namespace aion;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(unsigned order=0;order<7;++order){
        Fixture f(block,MeterEventScope::acceptedPlayback);
        f.engine=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);
        f.control=std::make_unique<Engine>(MeterEventScope::acceptedPlayback,QueuedStartMode::messageCallback);f.prepare();
        LoopMeterLifecycle owner(*f.engine,id);f.record(id);
        check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Cleanup source installs actual clip");
        f.command(LoopAction::stopImmediate,id);f.command(LoopAction::play,id);f.cycle();
        check(owner.consumeImmediatePlay(f.next(MeterGraphEventKind::immediatePlay,id)),"Cleanup source binds original immediate graph");
        f.command(LoopAction::stopImmediate,id);f.cycle();
        check(owner.consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop,id)),"Cleanup source retains real old STOP timer");
        const auto cleanup=owner.cleanupRequest();
        for(unsigned n=0;n<8;++n)f.cycle();
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,120,1,1,4,false});
        f.command(LoopAction::play,id);f.cycle();const auto q=f.next(MeterGraphEventKind::queuedPlay,id);
        check(owner.consumeQueuedPlay(q),"Cleanup source queues replacement without cancelling old timer");
        const auto grid=f.view->tracks[id].scheduledFrame;
        if(order!=1)while(f.engine->clock()<grid+512)f.cycle();
        if(order==3)check(owner.deliverCleanup(cleanup),"Cleanup before callback keeps future callback alive");
        check(owner.requestQueuedGraph(q.queueId,q.bufferId),"Cleanup source requests explicit replacement callback");
        const auto pending=owner.queuedWindowTransaction();
        LoopCommand mirror{LoopAction::meterQueuedGraphRequest,id,0,f.control->generation(),0,0};
        mirror.meterGraphTicket=pending.ticket;mirror.meterQueueId=q.queueId;mirror.meterBufferId=q.bufferId;
        check(f.control->enqueueLoop(mirror),"Mirror source callback before cleanup");
        if(order==2)check(owner.deliverCleanup(cleanup),"Cleanup fences callback still in FIFO");
        // From this boundary audio intentionally differs from the unretired control.
        const auto render=[&](bool silent){
            std::array<float,128> l{},r{};float* channels[]={l.data(),r.data()};
            watch=true;f.engine->process(channels,2,static_cast<int>(block));watch=false;
            while(f.engine->popView(*f.view)){}
            const bool zero=std::all_of(l.begin(),l.end(),[](float v){return v==0;})&&std::all_of(r.begin(),r.end(),[](float v){return v==0;});
            if(zero!=silent)std::cerr<<"Cleanup case "<<order<<", block "<<block<<", track "<<id<<", frame "<<f.engine->clock()<<", expected silence "<<silent<<", state "<<static_cast<unsigned>(f.view->tracks[id].state)<<'\n';
            check(zero==silent,"Cleanup cuts exact stereo PCM without residual STOP tail or source resurrection");
        };
        render(order==1||order==2);(void)f.next(MeterGraphEventKind::started,id);
        const auto accepted=f.next(MeterGraphEventKind::queuedGraphAccepted,id);
        if(order==2)check(!owner.consumeQueuedWindowResult(accepted),"Cleanup retires pending typed result");
        else check(owner.consumeQueuedWindowResult(accepted),"Cleanup source accepts graph before retained timer");
        const auto length=f.view->tracks[id].length,undo=f.view->tracks[id].undoCount;
        if(order==3){
            check(!owner.deliverCleanup(cleanup),"Already delivered timer cannot cut a later fresh callback");render(false);
        }else if(order==5){
            const auto oldGeneration=f.engine->generation();
            check(owner.deliverCleanup(cleanup),"Publish retirement immediately before prepare");
            f.engine->prepare(48000,81920,{true,48000,0});
            check(!owner.deliverCleanup(cleanup)&&!f.engine->retireQueuedSourceThrough(oldGeneration,id,pending.ticket),"Prepare rejects old owner and retirement generation");
            check(owner.reset(),"Cleanup owner resets after prepare");render(true);
            check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Prepare retains clip through typed buffer receipt");
            // Prepare resets master gain to zero; drain its existing stop tail
            // before restoring gain to isolate the new generation's source.
            for(unsigned n=0;n<8;++n)render(true);
            f.engine->configureSync({SyncMode::fixed,120,1,1,4,false});f.engine->setTarget(1);
            check(f.engine->enqueueLoop({LoopAction::play,id,0,f.engine->generation(),0,0}),"New-generation queued PLAY reuses low graph tickets safely");render(true);
            const auto fresh=f.next(MeterGraphEventKind::queuedPlay,id);
            check(owner.consumeQueuedPlay(fresh)&&owner.requestQueuedGraph(fresh.queueId,fresh.bufferId),"New generation callback after stale retirement mailbox");
            render(true);(void)f.next(MeterGraphEventKind::started,id);
            check(owner.consumeQueuedWindowResult(f.next(MeterGraphEventKind::queuedGraphAccepted,id)),"New-generation source accepted");
            while(f.engine->clock()+block<f.view->tracks[id].scheduledFrame)render(true);
            // The grid may be inside this callback; test steady playback after it.
            std::array<float,128> l{},r{};float* channels[]={l.data(),r.data()};
            watch=true;f.engine->process(channels,2,static_cast<int>(block));watch=false;render(false);
        }else{
            if(order==4){
                for(unsigned n=0;n<255;++n)check(f.engine->enqueueLoop({LoopAction::volume,id,0,f.engine->generation(),f.engine->clock()+1000000,1}),"Fill performance FIFO before cleanup");
                check(!f.engine->enqueueLoop({LoopAction::volume,id,0,f.engine->generation(),0,1}),"Cleanup tests a genuinely full FIFO");
            }
            if(order==6){
                LoopCommand raw{LoopAction::meterGraphStart,id,0,f.engine->generation(),0,0};raw.meterGraphTicket=pending.ticket+1;
                check(f.engine->enqueueLoop(raw),"Raw reset must not hide source identity from cleanup");render(false);(void)f.next(MeterGraphEventKind::started,id);
            }
            if(order!=2)check(owner.deliverCleanup(cleanup),"Retained old timer retires replacement source");
            check(!owner.deliverCleanup(cleanup),"Duplicate cleanup is rejected");
            for(unsigned n=0;n<(order==1?400u:8u);++n)render(true);
            check(f.view->tracks[id].state==LoopState::stopped&&f.view->tracks[id].length==length&&f.view->tracks[id].undoCount==undo,"Cleanup stops pending/playing source while preserving clip and edit history");
            if(order!=4){
                f.engine->configureSync({SyncMode::off,120,1,1,4,false});
                check(f.engine->enqueueLoop({LoopAction::play,id,0,f.engine->generation(),0,0}),"Fresh immediate PLAY after cleanup");render(false);
                check(owner.consumeImmediatePlay(f.next(MeterGraphEventKind::immediatePlay,id))&&!owner.deliverCleanup(cleanup),"Fresh graph supersedes stale timer");render(false);
            }
        }
    }
    check(allocations.load()==0,"Cleanup retirement uses no callback new/delete");
    std::cout<<"PASS: retained cleanup cuts replacement source PCM, pending/future callback retirement, fresh callback survival, raw-reset identity, FIFO overflow and prepare guards at tracks 0/19, 64/128; no callback new/delete\n";
}
void meterQueuedTests(){
    queuedReplacementSpeedTests();
    queuedOverlapCapacityTests();
    queuedChainCapacityTests();
    queuedTripleCapacityTests();
    queuedHistoryTests();postbirthHistoryTests();stoppingMergeTests();queuedMergeDestinationTests();queuedMergeSourceTests();postbirthMultiplyTests();queuedMultiplyInputTests();prebirthMultiplyTests();prebirthNormTests();queuedNormTests();stoppingNormTests();normPendingTests();
    queuedEarlySpeedTests();
    queuedEarlyRestartTests();
    queuedEarlyControlTests();
    queuedCleanupSourceTests();
    queuedLateSourceTests();
    queuedScheduledSourceTests();
    stopCallbackClockTests();
    queuedDueDeliveryTests();
    queuedCallbackClockTests();
    queuedGraphTransactionTests();
    queuedWindowTransactionTests();
    queuedWindowTests();
    using namespace aion;
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u})for(bool attached:{false,true})for(auto action:{LoopAction::stopImmediate,LoopAction::stop}){
        Fixture f(block,MeterEventScope::acceptedPlayback);LoopMeterLifecycle owner(*f.engine,id);
        f.record(id);check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Late STOP NOW commits actual clip");
        f.command(LoopAction::stopImmediate,id);f.cycle();
        if(attached){
            f.command(LoopAction::play,id);f.cycle();check(owner.consumeImmediatePlay(f.next(MeterGraphEventKind::immediatePlay,id)),"Late STOP NOW establishes an actual old graph");
            check(owner.request(),"Request old graph PCM before queued STOP NOW");f.cycle();
            const double t=static_cast<double>(f.view->frame)/48000;
            check(f.engine->popLoopMeterWindow(*f.packet)&&owner.observe(*f.packet,*f.view,f.view->callbackClock,{t,t}),"Observe actual old graph PCM");
            f.command(LoopAction::stopImmediate,id);f.cycle();check(owner.consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop,id)),"Arm cleanup before queued STOP NOW");
        }
        const auto old=owner.cleanupRequest();const auto identity=owner.inspect();const auto held=reading(owner.value());
        for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::fixed,999,1,1,4,false});
        f.command(LoopAction::play,id);f.cycle();const auto queued=f.next(MeterGraphEventKind::queuedPlay,id);
        check(owner.consumeQueuedPlay(queued),"Accept pending intent before late STOP NOW");
        const auto rejectGenericStart=[&]{
            const auto graph=owner.inspect();const auto binding=owner.binding();const auto cleanup=owner.cleanupRequest();
            const auto history=reading(owner.value());const auto delay=owner.queuedCallbackDelay();
            check(!owner.start(0)&&!owner.start(f.engine->clock()+4096),"Generic graph start must reject a real pending queued intent");
            const auto after=owner.inspect();const auto bound=owner.binding();
            check(after.state==graph.state&&after.present==graph.present&&after.destroyed==graph.destroyed&&after.buffer==graph.buffer
                &&after.analyser==graph.analyser&&after.data==graph.data&&after.teardownDelay==graph.teardownDelay
                &&bound.generation==binding.generation&&bound.track==binding.track&&bound.ticket==binding.ticket
                &&bound.analyser==binding.analyser&&bound.data==binding.data&&owner.cleanupRequest()==cleanup
                &&owner.queuedIntentId()==queued.queueId&&owner.queuedCallbackDelay()==delay&&reading(owner.value())==history,
                "Rejected unconditional start preserves the complete queued owner and cleanup");
            f.cycle();MeterGraphEvent extra;
            check(!f.engine->popMeterGraphEvent(extra),"Rejected generic start must not enqueue an unconditional raw reset");
            if(binding.ticket)check(f.engine->loopMeterGraphCurrent(binding.generation,id,binding.ticket),"Rejected generic start preserves the old raw window token");
        };
        rejectGenericStart();
        for(unsigned n=0;n<1000&&f.view->tracks[id].state==LoopState::queued;++n)f.cycle();
        check(f.view->tracks[id].state==LoopState::playing&&owner.inspect().state==MeterLoopState::queued,"Audible grid leaves message intent pending");
        rejectGenericStart();
        if(action==LoopAction::stop)for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({SyncMode::off,999,1,1,4,false});
        f.command(action,id);f.cycle();MeterGraphEvent stopped;
        check(f.engine->popMeterGraphEvent(stopped)&&stopped.kind==MeterGraphEventKind::queuedStop,
            action==LoopAction::stop?"Pending unsynced STOP must emit queuedStop after audible grid":"Late STOP NOW must emit queuedStop for the pending intent after audible grid");
        check(f.view->tracks[id].state==LoopState::stopped,"Both immediate STOP paths stop actual playback after the grid");
        check(stopped.queueId==queued.queueId&&stopped.bufferId==queued.bufferId,"Late STOP NOW retains exact queue and clip identity");
        auto wrong=stopped;++wrong.queueId;check(!owner.consumeQueuedStop(wrong)&&owner.queuedIntentId()==queued.queueId,"Wrong late STOP NOW cannot retire pending intent");
        check(owner.consumeQueuedStop(stopped)&&!owner.queuedIntentId()&&owner.queuedCallbackDelay()==-1&&!owner.consumeQueuedStop(stopped),"Late STOP NOW retires pending intent exactly once");
        check(owner.inspect().analyser==identity.analyser&&owner.inspect().data==identity.data&&reading(owner.value())==held,"Late STOP NOW retains original analyser/data/history");
        if(attached){const auto current=owner.cleanupRequest();check(current.serial&&current.serial!=old.serial&&current.delay==.025&&!owner.deliverCleanup(old),"Late STOP NOW replaces old cleanup with 25ms and rejects obsolete timer");}
        else check(!owner.inspect().present&&!owner.cleanupRequest().serial,"Graphless late STOP NOW tears down without a timer");
        f.command(LoopAction::stopAll,id);f.cycle();check(!f.engine->popMeterGraphEvent(stopped),"STOP ALL after late STOP NOW must not emit orphan queued cancellation");
    }
    std::cout<<"PASS: post-grid pending STOP NOW, graphless teardown and attached 25ms replacement, exact identities and no orphan cancellation at tracks 0/19, 64/128\n";
    std::cout<<"PASS: post-grid pending normal STOP after sync off, graphless/attached cleanup and no orphan cancellation at tracks 0/19, 64/128\n";
    std::cout<<"PASS: generic graph start rejects pending queue before/after grid without mutation or raw reset at tracks 0/19, 64/128\n";
    auto stream=std::make_unique<MeterGraphEvents>();stream->reset(3);
    stream->publish(MeterGraphEventKind::queuedPlay,19,0,42,7,9007199254740993ull,0,0xffffffffffffffffull);
    MeterGraphEvent wide;check(stream->pop(wide,3)&&wide.queueId==0xffffffffffffffffull&&wide.bufferId==9007199254740993ull&&wide.frame==42,"Queued identity remains full-width in the actual bounded stream");
    for(unsigned block:{64u,128u})for(unsigned id:{0u,19u}){
        Fixture f(block,MeterEventScope::acceptedPlayback);LoopMeterLifecycle owner(*f.engine,id);
        f.record(id);check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Queue fixture commits actual clip");
        f.command(LoopAction::stopImmediate,id);f.cycle();
        const auto sync=[&](SyncMode mode){for(auto* e:{f.engine.get(),f.control.get()})e->configureSync({mode,120,1,4,4,false});};
        sync(SyncMode::fixed);
        const auto queue=[&]{const auto frame=f.engine->clock();f.command(LoopAction::play,id);f.cycle();auto event=f.next(MeterGraphEventKind::queuedPlay,id);
            check(event.frame==frame&&event.queueId&&event.bufferId==owner.committedBuffer()&&f.view->tracks[id].state==LoopState::queued,"Actual accepted queued PLAY supplies exact frame, queue identity and committed clip");return event;};
        auto queued=queue();const auto first=queued;
        auto wrong=queued;wrong.queueId=0;check(!owner.consumeQueuedPlay(wrong)&&!owner.queuedIntentId(),"Zero queued identity cannot mutate owner");
        wrong=queued;++wrong.ticket;check(!owner.consumeQueuedPlay(wrong),"Queue wrong raw identity rejects");
        wrong=queued;++wrong.bufferId;check(!owner.consumeQueuedPlay(wrong),"Queue wrong clip rejects");
        wrong=queued;++wrong.generation;check(!owner.consumeQueuedPlay(wrong),"Queue wrong generation rejects");
        for(double delay:{-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
            wrong=queued;wrong.queueDelaySeconds=delay;check(!owner.consumeQueuedPlay(wrong)&&owner.queuedCallbackDelay()==-1,"Invalid queued callback delay cannot change accepted intent");
        }
        check(owner.consumeQueuedPlay(queued)&&owner.queuedIntentId()==queued.queueId&&owner.inspect().state==MeterLoopState::queued&&!owner.inspect().present&&!owner.inspect().data&&!owner.cleanupRequest().serial&&!owner.request(),"Accepted first queue creates no analyser, data or cleanup");
        check(owner.queuedCallbackDelay()==queued.queueDelaySeconds&&queued.queueDelaySeconds>0,"Owner retains accepted unrounded callback delay without starting a timer");
        check(!owner.consumeQueuedPlay(queued)&&!owner.consumeImmediatePlay(queued),"Duplicate queue and wrong birth route reject");
        f.command(LoopAction::stopAll,id);f.cycle();auto cancel=f.next(MeterGraphEventKind::queuedCancel,id);
        wrong=cancel;wrong.queueDelaySeconds=.1;check(!owner.consumeQueuedCancel(wrong)&&owner.queuedIntentId()==queued.queueId,"Cancellation cannot carry a fabricated callback delay");
        check(cancel.queueId==queued.queueId&&!cancel.ticket&&owner.consumeQueuedCancel(cancel)&&!owner.queuedIntentId()&&owner.inspect().state==MeterLoopState::stopped&&!owner.cleanupRequest().serial&&!owner.consumeQueuedCancel(cancel),"Actual queued panic cancels first intent without creating cleanup");
        check(owner.queuedCallbackDelay()==-1,"Cancellation hides the obsolete queued callback delay");
        for(auto action:{LoopAction::stop,LoopAction::stopImmediate}){
            queued=queue();check(queued.queueId>first.queueId&&owner.consumeQueuedPlay(queued),"New queued intent does not reuse cleared identity");
            wrong=cancel;wrong.serial=queued.serial+1;check(!owner.consumeQueuedCancel(wrong)&&owner.queuedIntentId()==queued.queueId,"Old cancellation cannot cancel newer queue even with a fresh serial");
            f.command(action,id);f.cycle();auto stopped=f.next(MeterGraphEventKind::queuedStop,id);
            check(stopped.queueId==queued.queueId&&owner.consumeQueuedStop(stopped)&&!owner.queuedIntentId()&&!owner.inspect().present&&!owner.cleanupRequest().serial,"Actual queued STOP without a volume graph tears down without a timer");
        }
        sync(SyncMode::off);f.command(LoopAction::play,id);f.cycle();auto played=f.next(MeterGraphEventKind::immediatePlay,id);check(owner.consumeImmediatePlay(played),"Actual graph before queued history checks");
        for(unsigned n=0;n<2048/block;++n){check(owner.request(),"Request queued-history PCM");f.cycle();check(f.engine->popLoopMeterWindow(*f.packet),"Receive queued-history PCM");
            const double t=static_cast<double>(f.view->frame)/48000;check(owner.observe(*f.packet,*f.view,f.view->callbackClock,{t,t}),"Seed queued history from actual PCM");}
        const auto identity=owner.inspect();const auto held=reading(owner.value());check(owner.value().peak>0,"Queued history is nonzero");
        f.command(LoopAction::stopImmediate,id);f.cycle();check(owner.consumeImmediateStop(f.next(MeterGraphEventKind::immediateStop,id)),"Arm old graph timer before queue");
        const auto old=owner.cleanupRequest();sync(SyncMode::fixed);queued=queue();check(owner.consumeQueuedPlay(queued)&&owner.cleanupRequest()==old&&owner.inspect().analyser==identity.analyser&&reading(owner.value())==held,"Queue retains older stop timer and analyser/history without a birth");
        f.command(LoopAction::stopAll,id);f.cycle();cancel=f.next(MeterGraphEventKind::queuedCancel,id);
        check(owner.consumeQueuedCancel(cancel)&&owner.cleanupRequest()==old&&owner.inspect().state==MeterLoopState::stopped&&owner.inspect().data==identity.data&&reading(owner.value())==held,"Queued panic retains the original old cleanup and held history");
        queued=queue();check(owner.consumeQueuedPlay(queued),"Queue again before explicit stop");
        f.command(LoopAction::stop,id);f.cycle();auto stopped=f.next(MeterGraphEventKind::queuedStop,id);
        check(owner.consumeQueuedStop(stopped)&&owner.cleanupRequest().serial!=old.serial&&owner.cleanupRequest().delay==.025&&!owner.deliverCleanup(old)&&owner.inspect().analyser==identity.analyser,"Queued explicit STOP replaces an attached old timer with fresh 25ms policy");
        const auto current=owner.cleanupRequest();queued=queue();check(owner.consumeQueuedPlay(queued)&&owner.deliverCleanup(current)&&owner.queuedIntentId()==queued.queueId&&!owner.inspect().present&&owner.inspect().data==identity.data,"Old cleanup does not infer cancellation of the separate queued intent");
        check(!owner.start(0)&&owner.queuedIntentId()==queued.queueId&&!owner.inspect().present&&owner.inspect().data==identity.data,
            "Generic start cannot bypass pending identity after old teardown changes message state");
        f.command(LoopAction::stopAll,id);f.cycle();cancel=f.next(MeterGraphEventKind::queuedCancel,id);
        check(!owner.consumeQueuedCancel(cancel)&&owner.queuedIntentId()==queued.queueId&&!owner.cleanupRequest().serial,"After old teardown changed state, queued-cancel mapping fails closed until graph-lead lifecycle is implemented");
        f.command(LoopAction::clear,id);f.cycle();check(owner.consumeClear(f.next(MeterGraphEventKind::cleared,id))&&!owner.queuedIntentId()&&owner.inspect().data==identity.data&&!owner.consumeQueuedPlay(queued),"Clear invalidates queue while retaining detached original history");
        sync(SyncMode::off);f.record(id);check(owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Commit fresh queue clip");f.command(LoopAction::stopImmediate,id);f.cycle();sync(SyncMode::fixed);
        queued=queue();check(owner.consumeQueuedPlay(queued)&&queued.queueId>first.queueId,"Clear preserves queued identity high-water");
        for(unsigned n=0;n<128;++n){f.command(LoopAction::stopAll,id);f.command(LoopAction::play,id);f.cycle();}
        check(!f.engine->meterGraphEventsCurrent()&&!owner.consumeQueuedCancel(cancel)&&!owner.consumeQueuedPlay(queued),"Lost queue stream rejects retained lifecycle facts");
        MeterGraphEvent event;unsigned budget=255;check(!f.engine->popMeterGraphEvent(event,f.engine->generation(),budget)&&!budget,"Lost queue stream retains bounded raw drain");
        f.prepare();check(owner.reset()&&!owner.queuedIntentId()&&!owner.consumeQueuedPlay(queued)&&owner.consumeBuffer(f.next(MeterGraphEventKind::bufferInstalled,id)),"Prepare removes old queued state and permits fresh committed-buffer ownership");
        sync(SyncMode::fixed);queued=queue();check(queued.queueId==1&&owner.consumeQueuedPlay(queued),"Fresh generation may restart queued identity without aliasing old receipts");
    }
    check(allocations.load()==0,"Queued intent/cancel callbacks have zero tracked new/delete");
    std::cout<<"PASS: actual queued PLAY/cancel/STOP receipts, independent full-width identities, no analyser birth, retained versus replaced cleanup, detached boundary rejection, clear/prepare/loss and stale guards at tracks 0/19, 64/128; exact paired stereo PCM and zero tracked callback new/delete\n";
}
void meterBufferTests(){
    using namespace aion;
    // Buffer metadata is independent of current raw-window ownership and retains
    // full-width immutable clip IDs, even above JavaScript's integer precision.
    auto stream=std::make_unique<MeterGraphEvents>();stream->reset(7);
    stream->publish(MeterGraphEventKind::started,19,0xffffffffu,10,2);
    stream->publish(MeterGraphEventKind::bufferInstalled,19,0,11,2,0xffffffffffffffffull);
    MeterGraphEvent event;check(stream->pop(event,7)&&stream->pop(event,7)&&event.bufferId==0xffffffffffffffffull
        &&stream->owns(7,19,0xffffffffu),"Buffer installation preserves full-width ID and existing window token");
    stream->reset(9);stream->publish(MeterGraphEventKind::started,19,0xffffffffu,0,0);check(stream->pop(event,9),"Seed concurrent buffer stream");
    std::atomic<unsigned> ready{0},ack{0};
    std::thread producer([&]{watch=true;for(unsigned n=1;n<=2000;++n){
        stream->publish(MeterGraphEventKind::bufferInstalled,n%20,0,0xffffffffffffffffull-n,n,9007199254740993ull+n);
        ready.store(n,std::memory_order_release);while(ack.load(std::memory_order_acquire)!=n)std::this_thread::yield();}watch=false;});
    watch=true;for(unsigned n=1;n<=2000;++n){while(ready.load(std::memory_order_acquire)!=n)std::this_thread::yield();
        check(stream->pop(event,9)&&event.kind==MeterGraphEventKind::bufferInstalled&&event.serial==n+1&&event.track==n%20
            &&event.frame==0xffffffffffffffffull-n&&event.bufferId==9007199254740993ull+n&&event.clearEpoch==n,
            "Concurrent buffer stream preserves coherent full-width ordered metadata");ack.store(n,std::memory_order_release);}watch=false;producer.join();
    check(stream->owns(9,19,0xffffffffu),"Concurrent installed buffers do not change raw window ownership");
    for(unsigned block:{64u,128u}){
        Fixture f(block);LoopMeterLifecycle owner(*f.engine,19);
        f.record();auto installed=f.next(MeterGraphEventKind::bufferInstalled);const auto first=installed.bufferId;
        check(first&&first==f.view->tracks[19].waveformId&&installed.frame==512&&installed.generation==f.engine->generation(),"Capture buffer receipt describes actual accepted clip and sample frame");
        check(owner.consumeBuffer(installed)&&owner.committedBuffer()==first&&owner.inspect().buffer&&!owner.inspect().present&&!owner.binding().ticket,
            "Accepted buffer notification does not invent an analyser or enqueue a graph reset");
        check(!owner.consumeBuffer(installed),"Duplicate committed buffer receipt rejects");
        check(owner.start(f.engine->clock()),"Explicit graph notification remains separate from buffer ownership");
        for(unsigned n=0;n<2048/block;++n){check(owner.request(),"Request seeded graph window");f.cycle();check(f.engine->popLoopMeterWindow(*f.packet),"Receive real buffer fixture PCM");
            const double now=static_cast<double>(f.view->frame)/48000;check(owner.observe(*f.packet,*f.view,f.view->callbackClock,{now,now}),"Seed graph history with actual Engine PCM");}
        (void)f.next(MeterGraphEventKind::started);
        const auto identity=owner.inspect();const auto held=reading(owner.value());check(owner.value().peak>0,"Buffer fixture has nonzero meter history");
        f.command(LoopAction::overdub);for(unsigned n=0;n<512/block;++n)f.cycle();f.command(LoopAction::finish);f.cycle(true);
        installed=f.next(MeterGraphEventKind::bufferInstalled);const auto edited=installed.bufferId;
        check(edited&&edited!=first&&owner.consumeBuffer(installed)&&owner.committedBuffer()==edited
            &&owner.inspect().analyser==identity.analyser&&owner.inspect().data==identity.data&&reading(owner.value())==held
            &&f.engine->loopMeterGraphCurrent(f.engine->generation(),19,owner.binding().ticket),
            "Accepted edit changes only committed buffer ownership, retaining analyser/window/history");
        auto malformed=installed;malformed.serial+=100;malformed.bufferId=0;check(!owner.consumeBuffer(malformed)&&owner.committedBuffer()==edited,"Malformed installation leaves current buffer unchanged");
        f.command(LoopAction::undo);f.cycle();installed=f.next(MeterGraphEventKind::bufferInstalled);
        check(installed.bufferId==first&&owner.consumeBuffer(installed),"Undo publishes actual restored immutable clip identity");
        f.command(LoopAction::redo);f.cycle();installed=f.next(MeterGraphEventKind::bufferInstalled);
        check(installed.bufferId==edited&&owner.consumeBuffer(installed),"Redo restores the prior edited buffer ID with a fresh serial");
        f.command(LoopAction::multiply);f.cycle(true);installed=f.next(MeterGraphEventKind::bufferInstalled);
        check(owner.consumeBuffer(installed)&&f.view->tracks[19].length==1024,"Multiply publishes accepted doubled clip identity");
        f.command(LoopAction::substitute);for(unsigned n=0;n<512/block;++n)f.cycle();f.command(LoopAction::finish);f.cycle(true);
        installed=f.next(MeterGraphEventKind::bufferInstalled);check(owner.consumeBuffer(installed),"Substitute publishes accepted replacement clip identity");
        ++f.request;for(auto* e:{f.engine.get(),f.control.get()})check(e->enqueueLoop({LoopAction::merge,0,f.request,e->generation(),0,19}),"Queue paired clone merge");
        f.cycle(true);const auto cloned=f.next(MeterGraphEventKind::bufferInstalled,0);
        check(cloned.bufferId&&cloned.bufferId==f.view->tracks[0].waveformId&&f.view->tracks[0].state==LoopState::stopped,"Clone merge publishes actual stopped destination buffer");
        f.command(LoopAction::clear,0);f.cycle();check(f.next(MeterGraphEventKind::cleared,0).bufferId==cloned.bufferId,"Clone clear identifies its installed buffer");
        f.command(LoopAction::normalize);f.cycle(true);installed=f.next(MeterGraphEventKind::bufferInstalled);
        check(owner.consumeBuffer(installed)&&installed.bufferId==f.view->tracks[19].waveformId&&reading(owner.value())==held,"Successful NORM publishes buffer installation without resetting analyser history");
        const auto normalized=owner.committedBuffer();
        check(f.engine->enqueueLoop({LoopAction::normalize,19,++f.request,f.engine->generation(),0,0}),"Queue worker failure only on observed engine");
        failWorkerAllocation.store(true);f.cycle(true);
        check(!failWorkerAllocation.load()&&f.view->tracks[19].finalizationFailed&&f.view->tracks[19].waveformId==normalized
            &&!f.engine->popMeterGraphEvent(event)&&owner.committedBuffer()==normalized,"Failed worker finalization emits no successful buffer installation");
        f.command(LoopAction::clear);f.cycle();const auto cleared=f.next(MeterGraphEventKind::cleared);
        auto wrongBuffer=cleared;++wrongBuffer.bufferId;
        check(!owner.consumeClear(wrongBuffer)&&owner.committedBuffer()==normalized&&owner.inspect().analyser==identity.analyser&&reading(owner.value())==held,
            "Wrong removed buffer identity cannot clear attached graph/history even with fresh serial");
        check(cleared.bufferId==normalized&&owner.consumeClear(cleared)&&!owner.inspect().buffer&&!owner.inspect().data&&!owner.committedBuffer(),
            "Clear names the actual removed buffer after edits and resets attached graph despite intervening buffer notices");
        check(!owner.consumeClear(cleared),"Duplicate typed clear rejects");
        f.record();installed=f.next(MeterGraphEventKind::bufferInstalled);check(owner.consumeBuffer(installed),"New stopped/playing clip can replace cleared ownership without inferred graph");
        check(!owner.consumeClear(cleared)&&owner.committedBuffer()==installed.bufferId&&owner.inspect().buffer,"Old clear cannot remove a newer accepted clip");
        // No graph/window ticket was created for track zero. Its actual buffer
        // clear must still be routable from the accepted immutable clip identity.
        LoopMeterLifecycle unbound(*f.engine,0);f.record(0);auto zero=f.next(MeterGraphEventKind::bufferInstalled,0);
        check(unbound.consumeBuffer(zero)&&!unbound.binding().ticket,"Accept track-zero clip without a graph");
        f.command(LoopAction::clear,0);f.cycle();zero=f.next(MeterGraphEventKind::cleared,0);
        check(!zero.retiredTicket&&zero.bufferId&&unbound.consumeClear(zero)&&!unbound.inspect().buffer,"Buffer identity routes actual clear with retired window ticket zero");
        f.prepare();check(owner.reset(),"Prepare resets buffer receipt owner");auto seeded=f.next(MeterGraphEventKind::bufferInstalled);
        check(seeded.serial==1&&seeded.frame==0&&seeded.bufferId==installed.bufferId&&owner.consumeBuffer(seeded),"Retained prepare explicitly hands off the committed clip under fresh generation");
        check(!owner.consumeBuffer(installed),"Old-generation installation cannot overwrite prepared owner");
        for(unsigned n=0;n<256;++n){f.command(LoopAction::clear,0);f.cycle();}
        check(!f.engine->meterGraphEventsCurrent()&&!owner.consumeBuffer(seeded)&&owner.committedBuffer()==seeded.bufferId,"Stream loss rejects buffer metadata without mutating retained ownership");
        unsigned budget=255;check(!f.engine->popMeterGraphEvent(event,f.engine->generation(),budget)&&!budget&&!f.engine->meterGraphEventsCurrent(),"Buffer stream loss drain remains bounded and cannot repair health");
        f.prepare();check(owner.reset(),"Fresh prepare resets lost buffer owner");seeded=f.next(MeterGraphEventKind::bufferInstalled);
        check(owner.consumeBuffer(seeded)&&f.engine->meterGraphEventsCurrent(),"Fresh prepared handoff recovers buffer ownership after loss");
        f.command(LoopAction::record,0);for(unsigned n=0;n<512/block;++n)f.cycle();
        f.command(LoopAction::finish,0);f.command(LoopAction::clear,0);f.cycle();f.cycle(true);
        check(f.next(MeterGraphEventKind::cleared,0).bufferId==0&&!f.engine->popMeterGraphEvent(event)&&f.view->tracks[0].state==LoopState::empty,
            "Clear before worker-result acceptance emits no installation for the cancelled take");
        f.command(LoopAction::play,0);f.cycle();
        check(f.view->rejected&&!f.engine->popMeterGraphEvent(event),"Rejected empty play emits no buffer success receipt");
    }
    check(allocations.load()==0,"Committed buffer notification callbacks have zero tracked new/delete");
    std::cout<<"PASS: accepted capture/OD/SUB/MULT/MERGE/NORM/Undo/Redo buffer receipts, worker failure/cancellation, full-width identity, retained prepare, typed clear without graph, stale/lost rejection and exact paired 64/128 stereo PCM; zero tracked callback new/delete\n";
}
