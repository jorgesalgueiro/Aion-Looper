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
    Fixture(unsigned size,bool& audioWatch):block(size),watch(audioWatch){prepare();owner=std::make_unique<aion::LoopMeterLifecycle>(*engine,19);owner->setBuffer(true);timer=std::make_unique<aion::LoopMeterCleanupTimer>(*owner);}
    void cycle(bool offline=false){
        std::array<float,128> l{},r{},cl{},cr{};
        for(unsigned i=0;i<block;++i)l[i]=r[i]=1.8f*static_cast<float>(std::sin((engine->clock()+i)*.037));
        cl=l;cr=r;float* channels[]={l.data(),r.data()},*cc[]={cl.data(),cr.data()};
        watch=true;engine->process(channels,2,static_cast<int>(block),{},offline);control->process(cc,2,static_cast<int>(block),{},offline);watch=false;
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
class Runner final : private juce::Timer {
public:
    explicit Runner(bool& watch):audioWatch(watch){begin(64);startTimer(5);}
    ~Runner() override {stopTimer();check(!cleanupWorker.joinable(),"Cleanup worker must finish before owner destruction");}
private:
    bool& audioWatch;std::unique_ptr<Fixture> f;std::unique_ptr<ProcessorMeterFixture> processorMeters;
    std::unique_ptr<ProcessorShutdownFixture> shutdown;unsigned stage=0;
    double started=0,expectedDelay=0;std::uint64_t frame=0,data=0;std::array<double,7> held{};
    aion::MeterCleanupRequest obsolete{};
    std::thread cleanupWorker;
    std::atomic<bool> cleanupDestroyed{false};
    std::atomic<std::uint32_t> cleanupPrepared{0};
    void capture(){started=wall();frame=f->engine->clock();data=f->owner->inspect().data;held=reading(f->owner->value());}
    void stopped(){check(!f->owner->inspect().present&&!f->timer->pending()&&f->owner->inspect().data==data
        &&reading(f->owner->value())==held&&f->engine->clock()==frame,"Real timer detaches analyser but retains data/history with frozen audio clock");}
    void begin(unsigned block){
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
            cleanupPrepared.store(f->engine->generation(),std::memory_order_release);f->timer->synchronise();
            check(!f->timer->pending()&&f->owner->deliverCleanup(obsolete),"Generation cancellation retains explicit owner metadata without rescheduling");
            if(f->block==64){begin(128);break;}
            stopTimer();juce::MessageManager::getInstance()->stopDispatchLoop();break;
        }
    }
};
}

void meterCleanupTimerTests(bool& audioWatch,std::atomic<int>& allocations){
    clearDispatcherTests(audioWatch);
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
}
