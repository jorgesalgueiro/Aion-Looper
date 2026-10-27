#include "core/LoopMeterLifecycle.h"
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
extern thread_local bool watch;
extern std::atomic<int> allocations;
extern std::atomic<bool> failWorkerAllocation;
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
    void cycle(bool offline=false){
        std::array<float,128> l{},r{},cl{},cr{};
        for(unsigned i=0;i<block;++i){l[i]=.3f*static_cast<float>(std::sin((engine->clock()+i)*.037));r[i]=-.5f*l[i];}cl=l;cr=r;
        float* channels[]={l.data(),r.data()},*other[]={cl.data(),cr.data()};
        watch=true;engine->process(channels,2,static_cast<int>(block),{},offline);control->process(other,2,static_cast<int>(block),{},offline);watch=false;
        check(l==cl&&r==cr,"Buffer receipts preserve exact paired stereo PCM at 64/128");
        while(engine->popView(*view)){}
    }
    void record(unsigned id=19){command(aion::LoopAction::record,id);for(unsigned n=0;n<512/block;++n)cycle();command(aion::LoopAction::finish,id);cycle(true);}
    aion::MeterGraphEvent next(aion::MeterGraphEventKind kind,unsigned id=19){
        aion::MeterGraphEvent event;check(engine->popMeterGraphEvent(event)&&event.kind==kind&&event.track==id,"Actual audio owner emits the expected ordered buffer/window fact");return event;
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
        f.command(LoopAction::stopAll,id);f.cycle();check(!f.engine->popMeterGraphEvent(event),"Audible queued transition cannot recover quarantined panic graph origin");
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
}
namespace {
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
void meterQueuedTests(){
    queuedWindowTests();
    using namespace aion;
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
