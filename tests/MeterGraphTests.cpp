#include "core/LoopMeterGraph.h"
#include "core/LoopMeterObservation.h"
#include "core/LoopMeterLifecycle.h"
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
void graphEventTests(){
    using namespace aion;
    auto stream=std::make_unique<MeterGraphEvents>();MeterGraphEvent event{};event.ticket=999;
    watch=true;
    check(!stream->current(0)&&!stream->pop(event,1)&&event.ticket==999,"Unprepared event stream rejects without output mutation");
    stream->reset(7);
    for(unsigned n=0;n<255;++n)stream->publish(MeterGraphEventKind::started,n%20,n+1,9007199254740993ull+n,0xffffffffffffffffull);
    check(stream->current(7),"All 255 event slots are usable");
    stream->publish(MeterGraphEventKind::cleared,0,0,42,9);
    check(!stream->current(7)&&!stream->pop(event,7)&&event.ticket==999,"One lost event invalidates all retained receipts and leaves output untouched");
    stream->publish(MeterGraphEventKind::started,0,256,43,9);
    check(!stream->current(7)&&!stream->pop(event,7),"Draining a lost stream never silently repairs it");
    stream->reset(8);stream->publish(MeterGraphEventKind::started,19,0xffffffffu,0xffffffffffffffffull,9007199254740993ull);
    check(stream->pop(event,8)&&event.serial==1&&event.generation==8&&event.track==19&&event.ticket==0xffffffffu&&event.frame==0xffffffffffffffffull&&event.clearEpoch==9007199254740993ull,"Fresh generation preserves full integer precision");
    stream->publish(MeterGraphEventKind::cleared,0,0,1,2);stream->reset(9);stream->publish(MeterGraphEventKind::started,1,1,3,4);
    check(stream->pop(event,9)&&event.generation==9&&event.serial==1&&event.track==1&&!stream->pop(event,9),"Prepare discards old queued generations without resetting SPSC ownership");
    watch=false;
    for(unsigned block:{64u,128u}){
        auto engine=std::make_unique<Engine>(),control=std::make_unique<Engine>();auto view=std::make_unique<EngineView>();
        for(auto* e:{engine.get(),control.get()}){e->prepare(48000,81920);e->setMonitoring(false);e->setTarget(1);e->configureSync({SyncMode::off,120,1,4,4,false});
            e->enqueueLoop({LoopAction::record,19,1,e->generation(),0,0});e->enqueueLoop({LoopAction::finish,19,2,e->generation(),4096,0});}
        std::array<float,128> l{},r{},cl{},cr{};float* channels[]={l.data(),r.data()},*cc[]={cl.data(),cr.data()};
        const auto cycle=[&](bool offline=false){l.fill(.3f);r.fill(.2f);cl=l;cr=r;engine->process(channels,2,static_cast<int>(block),{},offline);control->process(cc,2,static_cast<int>(block),{},offline);check(l==cl&&r==cr,"Runtime receipt publication preserves bit-identical paired Engine stereo PCM");while(engine->popView(*view)){};};
        for(unsigned n=0;n<8192/block;++n)cycle(true);
        while(engine->popMeterGraphEvent(event)){};
        watch=true;
        const auto begin=engine->clock();const auto start=[&](unsigned ticket,std::uint64_t frame){LoopCommand command{LoopAction::meterGraphStart,19,0,engine->generation(),frame,0};command.meterGraphTicket=ticket;check(engine->enqueueLoop(command),"Queue explicit native window reset");};
        start(1,begin+block+17);
        for(auto* e:{engine.get(),control.get()})check(e->enqueueLoop({LoopAction::volume,19,3,e->generation(),0,.6}),"Ordinary volume behind future reset");
        cycle();check(!engine->popMeterGraphEvent(event)&&view->lastRequestId==3,"No receipt before actual sample reset; UI progresses");
        cycle();check(engine->popMeterGraphEvent(event)&&event.kind==MeterGraphEventKind::started&&event.frame==begin+block+17&&event.track==19&&event.ticket==1&&event.generation==engine->generation(),"Audio producer reports actual sample-17 reset, not enqueue/callback time");
        start(1,0);cycle();check(!engine->popMeterGraphEvent(event),"Rejected duplicate reset emits no success receipt");
        for(auto* e:{engine.get(),control.get()})e->enqueueLoop({LoopAction::retrigger,19,4,e->generation(),0,0});cycle();
        check(!engine->popMeterGraphEvent(event),"Healthy RTRG never invents a new analyser/window receipt");
        const auto clearFrame=engine->clock()+5;
        for(auto* e:{engine.get(),control.get()})e->enqueueLoop({LoopAction::clear,19,5,e->generation(),clearFrame,0});cycle();
        check(engine->popMeterGraphEvent(event)&&event.kind==MeterGraphEventKind::cleared&&event.frame==clearFrame&&event.ticket==0&&event.track==19&&event.clearEpoch==view->tracks[19].clearEpoch,"DEL reports exact native clear epoch/frame");
        start(2,0);cycle();check(!engine->popMeterGraphEvent(event),"No committed clip means no accepted reset receipt");
        // A bounded telemetry failure cannot change performance acknowledgements.
        for(unsigned n=0;n<255;++n)for(auto* e:{engine.get(),control.get()})check(e->enqueueLoop({LoopAction::clear,0,10+n,e->generation(),0,0}),"Fill ordinary command queue for event saturation");
        cycle();cycle();check(engine->meterGraphEventsCurrent(),"Native producer fills exactly 255 event slots");
        for(auto* e:{engine.get(),control.get()}){e->enqueueLoop({LoopAction::clear,0,300,e->generation(),0,0});e->enqueueLoop({LoopAction::volume,19,301,e->generation(),0,.4});}cycle();
        check(!engine->meterGraphEventsCurrent()&&view->lastRequestId==301&&!view->rejected&&view->tracks[19].volume==.4f,"Lost notification latches invalidity while performance commands still succeed");
        event.ticket=777;check(!engine->popMeterGraphEvent(event)&&event.ticket==777&&!engine->meterGraphEventsCurrent(),"Lost native stream rejects its queued receipts without output mutation or silent recovery");
        watch=false;for(auto* e:{engine.get(),control.get()})e->prepare(48000,81920);watch=true;
        check(engine->meterGraphEventsCurrent()&&!engine->popMeterGraphEvent(event),"Prepare alone recovers stream under a fresh generation");
        for(auto* e:{engine.get(),control.get()})e->enqueueLoop({LoopAction::clearAll,0,1,e->generation(),0,0});cycle();
        for(unsigned n=0;n<20;++n)check(engine->popMeterGraphEvent(event)&&event.serial==n+1&&event.track==n&&event.generation==engine->generation()&&event.kind==MeterGraphEventKind::cleared,"Clear All produces explicit ordered per-track facts in fresh generation");
        check(!engine->popMeterGraphEvent(event),"No extra inferred lifecycle events");watch=false;
    }
    std::cout<<"PASS: actual window reset/clear event producer, exact sample-17 64/128 receipts, duplicate/RTRG rejection, ordered Clear All, latched overflow/prepare recovery and identical PCM; zero tracked new/delete\n";
}
void deferredStartTests(){
    using namespace aion;
    auto pending=std::make_unique<MeterGraphStarts>();MeterGraphStart output{};output.ticket=999;
    watch=true;
    check(!pending->popDue(0,output)&&output.ticket==999,"Empty deferred start leaves destination untouched");
    check(pending->push(1000,9,0,10)&&pending->push(500,8,1,20)&&pending->push(1000,7,0,11),"Schedule out-of-order frame notifications");
    check(!pending->popDue(499,output)&&pending->popDue(500,output)&&output.ticket==20,"Earliest frame wins independently of arrival");
    check(pending->popDue(1000,output)&&output.ticket==10&&pending->popDue(1000,output)&&output.ticket==11,"Same-frame graph starts preserve FIFO order");
    pending->clear();
    for(unsigned n=0;n<MeterGraphStarts::capacity;++n)check(pending->push(1000+n%7,123,n%2,n+1),"Deferred start capacity is fully usable");
    check(!pending->push(1,0,0,999)&&pending->size()==MeterGraphStarts::capacity,"Full deferred storage rejects without eviction or growth");
    pending->discardIf([](const MeterGraphStart& start){return start.track==0;});check(pending->size()==128,"Clear frees deferred capacity without touching other tracks");
    MeterGraphStart previous{};while(pending->popDue(2000,output)){check(output.track==1&&(output.frame>previous.frame||(output.frame==previous.frame&&output.order>previous.order)),"Compaction retains sample and FIFO priority");previous=output;}
    pending->clear();const auto maximum=std::numeric_limits<std::uint64_t>::max();
    check(pending->push(maximum,maximum,19,0xffffffffu)&&!pending->popDue(maximum-1,output)&&pending->popDue(maximum,output)&&output.clearEpoch==maximum&&output.ticket==0xffffffffu,"Deferred frame/epoch/ticket fields retain full integer precision");
    pending->clear();watch=false;
    for(unsigned block:{64u,128u}){
        auto engine=std::make_unique<Engine>();auto view=std::make_unique<EngineView>();engine->prepare(48000,8192);
        watch=true;
        for(unsigned n=0;n<255;++n){LoopCommand command{LoopAction::meterGraphStart,0,0,engine->generation(),1000000,0};command.meterGraphTicket=n+1;check(engine->enqueueLoop(command),"Fill input FIFO with future telemetry");}
        engine->process(nullptr,0,static_cast<int>(block));while(engine->popView(*view)){}
        engine->process(nullptr,0,static_cast<int>(block));while(engine->popView(*view)){}
        for(unsigned n=256;n<=257;++n){LoopCommand command{LoopAction::meterGraphStart,0,0,engine->generation(),1000000,0};command.meterGraphTicket=n;check(engine->enqueueLoop(command),"Deferred capacity boundary remains bounded");}
        check(engine->enqueueLoop({LoopAction::volume,0,41,engine->generation(),0,.5}),"Volume follows full deferred storage");engine->process(nullptr,0,static_cast<int>(block));
        check(engine->popView(*view)&&view->lastRequestId==41&&!view->rejected&&view->tracks[0].volume==.5f,"Saturated deferred telemetry cannot starve or reject UI commands");
        watch=false;
    }
    std::cout<<"PASS: fixed deferred graph-start heap ordering/precision/compaction/full capacity, bounded 64/128 ingress and UI progress; zero tracked new/delete\n";
}
void lifecycleOwnerTests(){
    using namespace aion;
    const auto bindingFields=[](LoopMeterBinding b){return std::array<std::uint64_t,5>{b.generation,b.track,b.ticket,b.analyser,b.data};};
    const auto graphFields=[](LoopMeterGraphView v){return std::array<double,7>{static_cast<double>(v.state),v.buffer?1.:0.,v.present?1.:0.,v.destroyed?1.:0.,static_cast<double>(v.analyser),static_cast<double>(v.data),v.teardownDelay};};
    for(unsigned block:{64u,128u}){
        auto engine=std::make_unique<Engine>(),control=std::make_unique<Engine>();
        auto packet=std::make_unique<MeterWindowEvent>();auto view=std::make_unique<EngineView>();
        for(auto* e:{engine.get(),control.get()}){e->prepare(48000,81920,{true,48000,0});e->configureSync({SyncMode::off,120,1,4,4,false});e->setMonitoring(false);e->setTarget(1);
            e->enqueueLoop({LoopAction::record,19,1,e->generation(),0,0});e->enqueueLoop({LoopAction::finish,19,2,e->generation(),4096,0});}
        std::array<float,128> left{},right{},cl{},cr{};float* channels[]={left.data(),right.data()},*cc[]={cl.data(),cr.data()};
        for(unsigned at=0;at<8192;at+=block){left.fill(.3f);right.fill(.2f);cl=left;cr=right;engine->process(channels,2,static_cast<int>(block),{},true);control->process(cc,2,static_cast<int>(block),{},true);while(engine->popView(*view)){};}
        LoopMeterLifecycle owner(*engine,19);LoopMeterLifecycle invalid(*engine,20);
        watch=true;
        check(!invalid.setBuffer(true)&&!invalid.start(0)&&!invalid.request(),"Invalid lifecycle owner cannot address another track");
        check(owner.setBuffer(true)&&owner.start(engine->clock()+block+17),"Explicit owner enqueues a future graph start");
        for(auto* e:{engine.get(),control.get()})check(e->enqueueLoop({LoopAction::volume,19,4,e->generation(),0,.5}),"UI volume command follows future meter notification");
        bool requireImmediateVolume=true;
        const auto capture=[&]{
            check(owner.request(),"Active owned graph requests only its own ticket");
            left.fill(0);right.fill(0);cl=left;cr=right;
            engine->process(channels,2,static_cast<int>(block));control->process(cc,2,static_cast<int>(block));
            check(engine->popLoopMeterWindow(*packet),"Owned actual window delivered");while(engine->popView(*view)){};
            if(requireImmediateVolume){check(view->tracks[19].volume==.5f&&view->lastRequestId==4,"Future meter notification must not postpone ordinary UI volume command");requireImmediateVolume=false;}
            check(left==cl&&right==cr,"Graph owner notifications preserve bit-identical actual Engine stereo PCM");
            const auto time=static_cast<double>(view->frame)/view->sampleRate;
            return owner.observe(*packet,*view,view->callbackClock,{time,time});
        };
        const auto empty=readingTuple(owner.value());
        check(!capture()&&readingTuple(owner.value())==empty,"Enqueue before audio reset never observes old rolling PCM");
        check(capture()&&owner.value().peak>.01,"Audio-owner receipt activates the lifecycle observation");
        const auto first=bindingFields(owner.binding());const auto stable=readingTuple(owner.value());
        const auto observedTime=static_cast<double>(view->frame)/view->sampleRate;
        check(owner.setBuffer(false)&&!owner.request()&&!owner.observe(*packet,*view,view->callbackClock,{observedTime,observedTime})&&readingTuple(owner.value())==stable,"Removed buffer rejects requests and observations without follower mutation");
        check(owner.setBuffer(true),"Restore committed buffer notification without changing graph ownership");
        check(!owner.start(engine->clock())&&!owner.reset()&&bindingFields(owner.binding())==first&&readingTuple(owner.value())==stable,"Duplicate play and same-generation reset cannot reuse tickets or erase history");
        const auto source=packet->sourceRevision;
        for(auto* e:{engine.get(),control.get()})e->enqueueLoop({LoopAction::retrigger,19,3,e->generation(),e->clock(),0});
        unsigned calls=0;do{check(capture(),"Healthy RTRG observations retain the existing owner");}while(packet->sourceRevision==source&&++calls<100);
        check(packet->sourceRevision!=source&&bindingFields(owner.binding())==first,"Actual source replacement retains analyser/data/ticket");
        check(owner.stop(1),"Immediate stop requests original cleanup");const auto oldTimer=owner.cleanupRequest();
        check(oldTimer.delay==.025&&owner.start(engine->clock()),"Play while stopping cancels cleanup and queues fresh window");
        const auto second=bindingFields(owner.binding());
        check(!owner.deliverCleanup(oldTimer)&&bindingFields(owner.binding())==second&&owner.inspect().present,"Cancelled timer cannot detach replacement graph");
        check(capture(),"Fresh replacement graph recovers on actual window acknowledgement");
        check(owner.stop(2),"Queue saturation fixture has pending cleanup");
        const auto superseded=owner.cleanupRequest();check(owner.stop(2.01)&&!owner.deliverCleanup(superseded),"Repeated stop supersedes the prior cleanup token even on the same graph");
        auto wrongDelay=owner.cleanupRequest();wrongDelay.delay+=.001;check(!owner.deliverCleanup(wrongDelay),"Malformed cleanup receipt cannot change graph ownership");
        const auto beforeGraph=graphFields(owner.inspect());const auto beforeReading=readingTuple(owner.value());const auto beforeBinding=bindingFields(owner.binding());const auto beforeTimer=owner.cleanupRequest();
        for(unsigned n=0;n<255;++n)check(engine->enqueueLoop({LoopAction::volume,19,100+n,engine->generation(),engine->clock(),.5}),"Fill existing performance queue exactly");
        check(!owner.start(engine->clock())&&graphFields(owner.inspect())==beforeGraph&&readingTuple(owner.value())==beforeReading&&bindingFields(owner.binding())==beforeBinding&&owner.cleanupRequest()==beforeTimer,"Full command queue rejects graph creation atomically including cleanup and follower");
        check(capture()&&capture(),"Existing graph remains observable while bounded command budget drains");
        check(owner.start(engine->clock())&&owner.binding().ticket==second[2]+1&&capture(),"Queue recovery spends one fresh ticket, never the rejected candidate");
        check(owner.stop(3,4)&&owner.setState(MeterLoopState::queued),"Original queued-during-scheduled-stop fixture");const auto retainedTimer=owner.cleanupRequest();
        check(owner.start(engine->clock())&&owner.cleanupRequest()==retainedTimer,"Queued->play preserves original timer rather than guarding it by the new graph ticket");
        check(capture(),"Queued replacement graph acknowledges its own window");const auto retained=readingTuple(owner.value());const auto retainedData=owner.inspect().data;
        check(owner.deliverCleanup(retainedTimer)&&!owner.inspect().present&&owner.inspect().data==retainedData&&readingTuple(owner.value())==retained&&!owner.request(),"Original retained timer detaches current graph and preserves data/holds");
        check(!owner.deliverCleanup(retainedTimer)&&owner.clear()&&owner.inspect().data==retainedData&&readingTuple(owner.value())==retained,"Repeated callback rejected; detached clear retains original data/history quirk");
        check(owner.setBuffer(true)&&owner.start(engine->clock())&&capture(),"Fresh graph after detached clear uses a new ticket");
        check(owner.stop(5),"Prepare invalidation has outstanding timer");const auto staleTimer=owner.cleanupRequest();const auto staleBinding=bindingFields(owner.binding());const auto staleReading=readingTuple(owner.value());
        watch=false;for(auto* e:{engine.get(),control.get()})e->prepare(48000,81920,{true,48000,0});watch=true;
        check(!owner.request()&&!owner.start(0)&&!owner.deliverCleanup(staleTimer)&&bindingFields(owner.binding())==staleBinding,"Changed Engine generation freezes old owner without follower mutation");
        const auto staleTime=static_cast<double>(view->frame)/view->sampleRate;
        check(!owner.observe(*packet,*view,view->callbackClock,{staleTime,staleTime})&&readingTuple(owner.value())==staleReading,"Old coherent packet/clock cannot revive a stale owner after prepare");
        check(owner.reset()&&owner.binding().ticket==0&&owner.value().peak==0&&!owner.deliverCleanup(staleTimer),"Explicit context reset invalidates graph and old timer");
        for(auto* e:{engine.get(),control.get()})e->enqueueLoop({LoopAction::play,19,1,e->generation(),0,0});
        check(owner.setBuffer(true)&&owner.start(0)&&owner.binding().ticket==1&&capture(),"Prepared owner restarts ticket sequence only under a new Engine generation");
        check(owner.stop(6)&&owner.cleanupRequest().serial>staleTimer.serial,"Timer serials never recycle across prepare");
        auto forged=staleTimer;forged.generation=engine->generation();check(!owner.deliverCleanup(forged),"Old serial cannot masquerade as a new-generation timer");
        check(owner.destroyGraph()&&!owner.request(),"Destroyed graph cannot request PCM");
        check(owner.clear()&&owner.value().peak==0,"Attached clear resets follower under the original policy");
        // A DEL followed by a fresh take must not inherit a deferred reset from
        // the deleted graph, even after a new committed clip exists at its due frame.
        const auto cycle=[&](bool offline=false){
            left.fill(.2f);right.fill(.1f);cl=left;cr=right;engine->requestLoopMeterWindow(19);
            engine->process(channels,2,static_cast<int>(block),{},offline);control->process(cc,2,static_cast<int>(block),{},offline);
            check(left==cl&&right==cr,"Deferred cancellation preserves exact control PCM");
            check(engine->popLoopMeterWindow(*packet)&&packet->windowGraphTicket==0,"Cancelled graph start never resets a replacement/paused source");
        };
        LoopCommand future{LoopAction::meterGraphStart,19,0,engine->generation(),engine->clock()+5*block,0};future.meterGraphTicket=1000;check(engine->enqueueLoop(future),"Queue old graph reset before DEL");
        for(auto* e:{engine.get(),control.get()})e->enqueueLoop({LoopAction::clear,19,10,e->generation(),0,0});cycle();
        for(auto* e:{engine.get(),control.get()})e->enqueueLoop({LoopAction::record,19,11,e->generation(),0,0});cycle();
        for(auto* e:{engine.get(),control.get()})e->enqueueLoop({LoopAction::finish,19,12,e->generation(),0,0});cycle(true);
        check(packet->waveformId!=0,"Replacement clip is committed before stale reset deadline");for(unsigned n=0;n<5;++n)cycle();
        future.frame=engine->clock()+2*block;future.meterGraphTicket=1001;check(engine->enqueueLoop(future),"Queue future graph reset before UI Stop All");
        for(auto* e:{engine.get(),control.get()})e->enqueueLoop({LoopAction::stopAll,19,13,e->generation(),0,0});for(unsigned n=0;n<4;++n)cycle();
        MidiSettings settings;settings.enabled=true;for(auto* e:{engine.get(),control.get()})check(e->configureMidi(settings),"Enable zero-sample MIDI Stop cancellation");
        future.frame=engine->clock()+3*block;future.meterGraphTicket=1002;check(engine->enqueueLoop(future),"Queue future reset before MIDI Stop");cycle();
        const MidiEvent stop{0,252,0,0,1};for(auto* e:{engine.get(),control.get()})e->process(nullptr,0,0,{&stop,1});
        for(unsigned n=0;n<5;++n)cycle();
        watch=false;
    }
    std::cout<<"PASS: lifecycle owner actual Engine queue/receipt, atomic saturation/recovery, retained RTRG, cancelled/replaced/queued cleanup tokens, context reset and identical 64/128 PCM; zero tracked new/delete\n";
}
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
    graphEventTests();
    deferredStartTests();
    lifecycleOwnerTests();
    check(allocations.load()==0,"Graph lifecycle/read composition has zero tracked new/delete");
    std::cout<<"PASS: explicit meter graph lifetime, stale/short-window rejection, detached versus attached clear, stop timer requests, fresh play and supplied PCM 64/128; zero tracked new/delete\n";
}
