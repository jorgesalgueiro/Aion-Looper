// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Engine.h"
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <string_view>
#include <new>
#include <thread>
#include <chrono>
#include <vector>
static thread_local bool watch=false;
static unsigned operations=0;
void* operator new(std::size_t n){if(watch)++operations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watch)++operations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
static void check(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
// Owner-approved extension: original effect controls store values but skip their
// running graph in these states. Keep this separate from the unchanged-JS oracle.
static void liveStateControls(unsigned block,aion::LoopAction action){
    using namespace aion;
    auto dry=std::make_unique<Engine>(),wet=std::make_unique<Engine>();
    for(auto* e:{dry.get(),wet.get()}){
        e->prepare(48000,65536);e->setMonitoring(false);
        check(e->enqueue({0,1})&&e->enqueueLoop({LoopAction::record,0,1,e->generation()}),"live EQ seed record");
    }
    std::array<float,128> dl{},dr{},wl{},wr{};float* d[]{dl.data(),dr.data()},* w[]{wl.data(),wr.data()};
    std::uint64_t frame=0;
    auto render=[&](bool input){
        for(unsigned i=0;i<block;++i){dl[i]=wl[i]=input?static_cast<float>(.13*std::sin((frame+i)*.073)):0;dr[i]=wr[i]=input?static_cast<float>(.11*std::cos((frame+i)*.039)):0;}
        watch=true;dry->process(d,2,block,{},true);wet->process(w,2,block,{},true);watch=false;frame+=block;
        const auto a=dry->inspectAudioThread().track,b=wet->inspectAudioThread().track;
        check(a.state==b.state&&a.position==b.position&&a.length==b.length&&a.undoCount==b.undoCount&&a.redoCount==b.redoCount&&a.scheduledFrame==b.scheduledFrame&&a.sourceRevision==b.sourceRevision,
              "live EQ must not alter transport, history, capture length, source or STOP timing");
    };
    auto paired=[&](LoopAction op,unsigned request){for(auto* e:{dry.get(),wet.get()}){LoopCommand command{op,0,request,e->generation()};command.value=1;check(e->enqueueLoop(command),"live EQ paired action");}};
    for(unsigned n=0;n<4096/block;++n)render(true);
    paired(LoopAction::finish,2);render(false);paired(LoopAction::play,3);
    for(unsigned n=0;n<1024/block;++n)render(false);
    if(action==LoopAction::stop){
        SyncSettings sync;sync.mode=SyncMode::fixed;sync.bpm=120;sync.bars=1;
        for(auto* e:{dry.get(),wet.get()})e->configureSync(sync);
    }
    paired(action,4);
    EqSettings settings{eqPresetValues(8),true,8};
    LoopCommand eq{LoopAction::equalizer,0,5,wet->generation()};eq.equalizer=settings;
    check(wet->enqueueLoop(eq),"live EQ enabled in special state");
    const auto origin=frame;SmoothedEqualizer oracle,frozen;
    check(oracle.prepare(48000,settings.values,origin)&&frozen.prepare(48000,settings.values,origin),"live EQ independent render oracles");
    const auto expected=action==LoopAction::overdub?LoopState::overdubbing:action==LoopAction::substitute?LoopState::substituting:action==LoopAction::multiply?LoopState::multiplying:LoopState::stopping;
    double error=0,changed=0;std::uint64_t deadline=0;
    for(unsigned offset=0;offset<2304;offset+=block){
        if(offset==256){settings.preset=0;settings.values[3]=1800;settings.values[4]=-12;eq.equalizer=settings;eq.eqMask=(1u<<3)|(1u<<4);eq.requestId=6;check(wet->enqueueLoop(eq)&&oracle.setTarget(3,1800)&&oracle.setTarget(4,-12),"live XY controls");}
        if(offset==1024||offset==1280){settings.enabled=offset==1280;eq.equalizer=settings;eq.eqMask=0;eq.requestId=offset;check(wet->enqueueLoop(eq),"live bypass/re-enable");if(settings.enabled)check(oracle.prepare(48000,settings.values,frame),"live re-enable uses current global clock");}
        if(offset==1536){settings.values=eqPresetValues(10);settings.preset=10;eq.equalizer=settings;eq.eqMask=eqAllParameters;eq.requestId=1536;check(wet->enqueueLoop(eq)&&oracle.setTargets(settings.values),"live original preset");}
        render(action!=LoopAction::stop);
        const auto actual=wet->inspectAudioThread().track;
        check(actual.state==expected&&actual.equalizer==settings,"actual audio-owned state and live EQ targets acknowledged");
        if(action==LoopAction::stop){if(!deadline)deadline=actual.scheduledFrame;check(deadline>frame&&actual.scheduledFrame==deadline,"EQ retains pending STOP deadline");}
        std::array<float,128> ol{},orr{},fl{},fr{};
        frozen.process(dl.data(),dr.data(),fl.data(),fr.data(),block);
        if(settings.enabled)oracle.process(dl.data(),dr.data(),ol.data(),orr.data(),block);
        else {ol=dl;orr=dr;}
        for(unsigned i=0;i<block;++i){
            error=std::max({error,std::abs(static_cast<double>(wl[i])-ol[i]),std::abs(static_cast<double>(wr[i])-orr[i])});
            if(!settings.enabled)check(wl[i]==dl[i]&&wr[i]==dr[i],"live bypass is exact, without extra gain dip");
            if(offset>=256&&offset<1024)changed=std::max({changed,std::abs(static_cast<double>(wl[i])-fl[i]),std::abs(static_cast<double>(wr[i])-fr[i])});
        }
    }
    check(error<=1e-6&&changed>.001,"live controls affect nonzero PCM with correct smoothing, not merely metadata");
    if(action==LoopAction::stop){
        while(frame<deadline+512){
            render(false);std::array<float,128> ol{},orr{};oracle.process(dl.data(),dr.data(),ol.data(),orr.data(),block);
            for(unsigned i=0;i<block;++i){
                const auto at=frame-block+i;
                if(at<deadline)check(std::abs(wl[i]-ol[i])<=1e-6&&std::abs(wr[i]-orr[i])<=1e-6,"live EQ playback continues until the original scheduled STOP sample");
                if(at>=deadline+240)check(dl[i]==0&&dr[i]==0&&wl[i]==0&&wr[i]==0,"scheduled STOP and bounded tail cannot be revived by EQ");
            }
        }
        check(wet->inspectAudioThread().track.state==LoopState::stopped,"scheduled STOP still completes");
    }else{
        paired(LoopAction::finish,7);render(false);
        check(wet->inspectAudioThread().track.state==LoopState::playing,"live EQ capture finalizes normally");
        const auto track=wet->inspectAudioThread().track;
        check(track.length==(action==LoopAction::multiply?6400u:4096u)&&track.undoCount==1,"live EQ adds no extra history or capture frames");
    }
    auto exported=[&](Engine& engine){
        check(engine.enqueueExport({0,engine.generation(),engine.inspectAudioThread().track.waveformId}),"live EQ dry export request");render(false);
        WavExport out;bool ready=false;
        for(unsigned n=0;n<5000&&!ready;++n){ready=engine.popExport(out);if(!ready)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        check(ready&&!out.bytes.empty(),"live EQ worker export complete");return out.bytes;
    };
    const auto dryBytes=exported(*dry),wetBytes=exported(*wet);
    check(dryBytes==wetBytes,"live EQ edits/toggles/presets never enter stored PCM or capture feedback");
    std::cout<<"live EQ state "<<static_cast<unsigned>(expected)<<" / "<<block<<": peak error "<<error<<", audible edit "<<changed<<'\n';
}
int main(int argc,char** argv){
    using namespace aion;
    const bool reference=argc==2&&std::string_view(argv[1])=="--reference";
    std::cout<<std::setprecision(9);
    SpscQueue<int,8> q;const std::array<int,5> five{1,2,3,4,5};int v=0;
    check(q.pushBatch(five),"batch fits");check(!q.pushBatch(five),"full batch rejects");
    for(int i=1;i<=3;++i)check(q.pop(v)&&v==i,"ordered prefix");
    check(q.pushBatch(five),"wrapped batch fits");
    for(int expected:{4,5,1,2,3,4,5})check(q.pop(v)&&v==expected,"wrapped order/no rejected prefix");
    check(!q.pop(v),"batch empties");
    for(unsigned id=0;id<=10;++id)check(validEqSettings({eqPresetValues(id),true,static_cast<std::uint8_t>(id)}),"original preset valid");
    auto malformed=EqSettings{};malformed.preset=1;check(!validEqSettings(malformed),"mismatched preset rejected");
    {
        auto bank=std::make_unique<LoopBank>();bank->prepare(262144,0,48000);
        SyncSettings clock;clock.mode=SyncMode::fixed;clock.bars=1;bank->configureSync(clock);
        for(unsigned i=0;i<64;++i)bank->tick({});
        const EqSettings settings{eqPresetValues(8),true,8};check(bank->setEqualizer(0,settings,eqAllParameters,1,0),"Configure EQ before arming");
        for(auto action:{LoopAction::finish,LoopAction::stop,LoopAction::stopAll}){
            watch=true;check(bank->apply(LoopAction::record,0),"Arm for EQ cancellation");check(bank->trackView(0).state==LoopState::armed,"Waiting for recording grid");
            check(bank->apply(action,0),"Cancel armed take");bank->tick({});watch=false;
            check(bank->trackView(0).state==LoopState::empty&&bank->trackView(0).equalizer==settings,"Finish/stop/panic cancel take without erasing configured EQ");
        }
        watch=true;check(bank->apply(LoopAction::clear,0),"Explicit DEL");watch=false;
        check(bank->trackView(0).equalizer==EqSettings{},"DEL intentionally resets EQ");
    }
    {
        // Same-rate Engine prepare stops playback BEFORE resetting its timeline.
        // Retaining the former filter frame here shifts later automation by a
        // partial worklet quantum despite apparently correct saved settings.
        auto dry=std::make_unique<LoopBank>(),wet=std::make_unique<LoopBank>();
        for(auto* bank:{dry.get(),wet.get()}){
            bank->prepare(8192,0,48000);bank->setAutoPlay(false);check(bank->apply(LoopAction::record,0),"reprepare seed");
            for(unsigned i=0;i<512;++i)bank->tick({static_cast<float>(.1*std::sin(i*.073)),static_cast<float>(.15*std::cos(i*.041))},true);
            check(bank->apply(LoopAction::finish,0),"reprepare seed finish");bank->completeOfflineEdits();check(bank->apply(LoopAction::play,0),"reprepare seed play");
        }
        EqSettings s{eqPresetValues(8),true,8};check(wet->setEqualizer(0,s,eqAllParameters,1,0),"reprepare initial EQ");
        for(unsigned i=0;i<2085;++i){dry->tick({});wet->tick({});}
        for(auto* bank:{dry.get(),wet.get()}){check(bank->apply(LoopAction::stopAll,0),"prepare panic");bank->resetTimeline();}
        SmoothedEqualizer oracle;check(oracle.prepare(48000,s.values),"reprepare oracle");
        for(unsigned i=0;i<1024;++i){dry->tick({});wet->tick({});float a=0,b=0;oracle.process(nullptr,nullptr,&a,&b,1);}
        for(auto* bank:{dry.get(),wet.get()})check(bank->apply(LoopAction::play,0),"reprepare resumed source");
        double error=0;
        for(unsigned i=0;i<2048;++i){
            if(i==37){s.values[4]=-6;s.preset=0;check(wet->setEqualizer(0,s,1u<<4,2,0)&&oracle.setTarget(4,-6),"non-quantum target after timeline reset");}
            watch=true;const auto d=dry->tick({}),w=wet->tick({});watch=false;float l=0,r=0;oracle.process(&d.left,&d.right,&l,&r,1);
            error=std::max({error,std::abs(static_cast<double>(w.left)-l),std::abs(static_cast<double>(w.right)-r)});
        }
        check(error<=1e-6,"same-rate prepare resets the actual EQ render clock, not only stored settings");
    }
    for(unsigned block:{64u,128u}){
        // A deterministic production Engine capture. The bypass signal is also
        // the oracle's input; the original pre-pan stereo chain is linear within
        // the retained tiny-state flush tolerance. EQ must not enter stored PCM.
        Engine dry,wet;dry.prepare(48000,65536);wet.prepare(48000,65536);
        for(auto* e:{&dry,&wet}){e->setMonitoring(false);check(e->enqueue({0,1}),"master");check(e->enqueueLoop({LoopAction::record,0,1,e->generation()}),"record");}
        std::array<float,128> dl{},dr{},wl{},wr{};float* d[]{dl.data(),dr.data()};float* w[]{wl.data(),wr.data()};
        std::uint64_t frame=0;EngineView view{},dryView{};
        auto render=[&](bool input){for(unsigned i=0;i<block;++i){dl[i]=wl[i]=input?static_cast<float>(.13*std::sin((frame+i)*.073)):0;dr[i]=wr[i]=input?static_cast<float>(.11*std::cos((frame+i)*.039)):0;}
            watch=true;dry.process(d,2,block,{},true);wet.process(w,2,block,{},true);watch=false;frame+=block;
            while(dry.popView(dryView)){}while(wet.popView(view)){};};
        for(unsigned i=0;i<2048/block;++i)render(true);
        for(auto* e:{&dry,&wet})check(e->enqueueLoop({LoopAction::finish,0,2,e->generation()}),"finish");render(false);
        for(auto* e:{&dry,&wet})check(e->enqueueLoop({LoopAction::play,0,3,e->generation()}),"play");
        EqSettings settings{eqPresetValues(8),true,8};LoopCommand c{LoopAction::equalizer,0,4,wet.generation()};c.equalizer=settings;
        check(wet.enqueueLoop(c),"EQ queued");
        const auto origin=frame;SmoothedEqualizer oracle;check(oracle.prepare(48000,settings.values,origin),"oracle prepared");
        std::vector<float> originalPcm,processedPcm;originalPcm.reserve(16384);processedPcm.reserve(16384);
        double error=0;float peak=0;
        // Unchanged rebuild/scheduledFade measured in the actual browser has
        // flat settled gain: the second schedule cancels the first endpoint.
        for(unsigned n=0;n<8192/block;++n){
            if(n*block==2048){settings.preset=0;settings.values[3]=900;settings.values[4]=-6;c.equalizer=settings;c.eqMask=(1u<<3)|(1u<<4);c.requestId=8;check(wet.enqueueLoop(c),"sample-timed XY edit");check(oracle.setTarget(3,900)&&oracle.setTarget(4,-6),"oracle XY targets");}
            if(n*block==4096){settings.values=eqPresetValues(10);settings.preset=10;c.equalizer=settings;c.eqMask=eqAllParameters;c.requestId=9;check(wet.enqueueLoop(c),"sample-timed preset");check(oracle.setTargets(settings.values),"oracle preset targets");}
            render(false);std::array<float,128> ol{},orr{};oracle.process(dl.data(),dr.data(),ol.data(),orr.data(),block);
            for(unsigned i=0;i<block;++i){
                error=std::max(error,std::abs(static_cast<double>(wl[i])-ol[i]));error=std::max(error,std::abs(static_cast<double>(wr[i])-orr[i]));peak=std::max(peak,std::abs(wl[i]));
                if(reference){originalPcm.push_back(dl[i]);originalPcm.push_back(dr[i]);processedPcm.push_back(wl[i]);processedPcm.push_back(wr[i]);}}}
        check(error<=1e-6&&peak>.001f,"connected pre-pan EQ + actual original flat toggle gain PCM");
        if(reference){
            std::cout<<"{\"block\":"<<block<<",\"origin\":"<<origin<<",\"dry\":[";
            for(unsigned i=0;i<originalPcm.size();++i){if(i)std::cout<<',';std::cout<<originalPcm[i];}
            std::cout<<"],\"wet\":[";for(unsigned i=0;i<processedPcm.size();++i){if(i)std::cout<<',';std::cout<<processedPcm[i];}std::cout<<"]}\n";
        }
        check(view.tracks[0].equalizer==settings,"audio settings acknowledged");
        auto exported=[&](Engine& engine,const EngineView& current){
            check(engine.enqueueExport({0,engine.generation(),current.tracks[0].waveformId}),"stored PCM export accepted");render(false);
            WavExport result;bool ready=false;
            for(unsigned n=0;n<5000&&!ready;++n){ready=engine.popExport(result);if(!ready)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
            check(ready&&!result.bytes.empty(),"worker export complete");return result.bytes;
        };
        check(exported(dry,dryView)==exported(wet,view),"EQ never changes captured PCM/dry WAV bytes");
        for(auto action:{LoopAction::overdub,LoopAction::substitute}){
            for(auto* e:{&dry,&wet})check(e->enqueueLoop({action,0,30,e->generation()}),"OD/SUB capture with EQ");
            for(unsigned n=0;n<2048/block;++n)render(true);
            for(auto* e:{&dry,&wet})check(e->enqueueLoop({LoopAction::finish,0,31,e->generation()}),"OD/SUB finish with EQ");render(false);
            check(exported(dry,dryView)==exported(wet,view),"EQ playback is excluded from OD/SUB capture/feedback and dry export");
        }
        settings.enabled=false;c.equalizer=settings;c.eqMask=0;c.requestId=32;check(wet.enqueueLoop(c),"bypass on playing source");
        for(unsigned n=0;n<2048/block;++n){render(false);for(unsigned i=0;i<block;++i)check(dl[i]==wl[i]&&dr[i]==wr[i],"Bypass has no invented down/up output dip");}
        settings.enabled=true;c.equalizer=settings;c.requestId=33;check(wet.enqueueLoop(c),"re-enable on playing source");
        Equalizer fresh;check(fresh.prepare(48000,settings.values),"fresh original EQ node");
        for(unsigned n=0;n<2048/block;++n){render(false);std::array<float,128> l{},r{};fresh.process(dl.data(),dr.data(),l.data(),r.data(),block);
            for(unsigned i=0;i<block;++i)check(std::abs(wl[i]-l[i])<=1e-6&&std::abs(wr[i]-r[i])<=1e-6,"Re-enable reconstructs filters without an invented gain envelope");}
        // Superseded ownership, mismatched masks and queue overflow are rejected.
        c.requestId=5;c.eqClearEpoch=view.tracks[0].clearEpoch+1;c.equalizer.values[4]=2;c.equalizer.preset=0;check(wet.enqueueLoop(c),"stale command enters bounded queue");render(false);
        while(wet.popView(view)){}check(view.rejected&&view.tracks[0].equalizer==settings,"stale clear epoch preserves settings");
        c.eqClearEpoch=view.tracks[0].clearEpoch;c.eqMask=0;check(wet.enqueueLoop(c),"bad mask queued");render(false);while(wet.popView(view)){}check(view.rejected&&view.tracks[0].equalizer==settings,"unmasked value cannot change state");
        check(wet.enqueueLoop({LoopAction::stopAll,0,6,wet.generation()}),"panic");render(false);
        for(unsigned n=0;n<64;++n){render(false);for(unsigned i=0;i<block;++i)if((n+1)*block+i>=240)check(wl[i]==0&&wr[i]==0,"panic has no EQ tail revival after existing 5ms stop tail");}
        check(wet.enqueueLoop({LoopAction::clear,0,7,wet.generation()}),"clear");render(false);while(wet.popView(view)){}check(view.tracks[0].equalizer==EqSettings{},"DEL resets effect settings");
        std::array<EqSettings,20> bank{};bank[19]=settings;
        unsigned accepted=0;LoopCommand filler{LoopAction::volume,1,20,wet.generation()};while(wet.enqueueLoop(filler))++accepted;
        check(accepted>20&&!wet.enqueueEqBank(bank,100),"full queue rejects entire bank");
        for(unsigned n=0;n<8;++n)render(false);while(wet.popView(view)){}check(view.tracks[19].equalizer==EqSettings{},"failed bank publishes no prefix");
        check(wet.enqueueEqBank(bank,100),"bank fits after drain");render(false);while(wet.popView(view)){}check(view.tracks[19].equalizer==settings,"bank restores expanded final track");
        std::cout<<"connected EQ "<<block<<" frames: peak error "<<error<<'\n';
    }
    for(unsigned block:{64u,128u})for(auto action:{LoopAction::overdub,LoopAction::substitute,LoopAction::multiply,LoopAction::stop})liveStateControls(block,action);
    check(operations==0,"callback new/delete prohibited");
    std::cout<<"PASS batch atomicity, connected EQ/actual toggle gain, live OD/SUB/MULT/STOP controls and dry capture, epoch/mask rollback, panic/clear, full-queue bank, zero callback new/delete\n";
}
