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
    explicit Fixture(unsigned size):block(size){prepare();}
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
