#include "plugin/LoopMeterCleanupTimer.h"
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
            check(e->enqueueLoop({aion::LoopAction::record,19,1,e->generation(),0,0})&&e->enqueueLoop({aion::LoopAction::finish,19,2,e->generation(),4096,0}),"Prepare actual recorded timer PCM");
        }
        for(unsigned n=0;n<8192/block;++n)cycle(true);
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
class Runner final : private juce::Timer {
public:
    explicit Runner(bool& watch):audioWatch(watch){begin(64);startTimer(5);}
    ~Runner() override {stopTimer();}
private:
    bool& audioWatch;std::unique_ptr<Fixture> f;unsigned stage=0;
    double started=0,expectedDelay=0;std::uint64_t frame=0,data=0;std::array<double,7> held{};
    aion::MeterCleanupRequest obsolete{};
    void capture(){started=wall();frame=f->engine->clock();data=f->owner->inspect().data;held=reading(f->owner->value());}
    void stopped(){check(!f->owner->inspect().present&&!f->timer->pending()&&f->owner->inspect().data==data
        &&reading(f->owner->value())==held&&f->engine->clock()==frame,"Real timer detaches analyser but retains data/history with frozen audio clock");}
    void begin(unsigned block){
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
            if(f->block==64){begin(128);break;}
            stopTimer();juce::MessageManager::getInstance()->stopDispatchLoop();break;
        }
    }
};
}

void meterCleanupTimerTests(bool& audioWatch,std::atomic<int>& allocations){
    std::mutex mutex;std::condition_variable changed;bool finished=false;
    std::thread watchdog([&]{std::unique_lock lock(mutex);if(!changed.wait_for(lock,std::chrono::seconds(30),[&]{return finished;})){
        std::cerr<<"Native cleanup timer message-loop watchdog expired"<<std::endl;std::_Exit(1);}});
    {Runner runner(audioWatch);juce::MessageManager::getInstance()->runDispatchLoop();}
    {std::lock_guard lock(mutex);finished=true;}changed.notify_one();watchdog.join();
    check(allocations.load()==0,"Native timer actual Engine callbacks have zero tracked new/delete");
    std::cout<<"PASS: real JUCE message-loop cleanup at 64/128, 25/50ms, late delivery, cancellation/replacement, queued-play retention, prepare/destruction and exact PCM; zero tracked callback new/delete\n";
}
