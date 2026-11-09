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
        check(e->enqueue({0,1})&&e->enqueueLoop({LoopAction::record,0,1,e->generation()}),"live compressor seed record");
    }
    std::array<float,128> dl{},dr{},wl{},wr{};float* d[]{dl.data(),dr.data()},* w[]{wl.data(),wr.data()};
    std::uint64_t frame=0;
    auto render=[&](bool input){
        for(unsigned i=0;i<block;++i){dl[i]=wl[i]=input?static_cast<float>(.13*std::sin((frame+i)*.073)):0;dr[i]=wr[i]=input?static_cast<float>(.13*std::sin((frame+i)*.073)):0;}
        watch=true;dry->process(d,2,block,{},true);wet->process(w,2,block,{},true);watch=false;frame+=block;
        const auto a=dry->inspectAudioThread().track,b=wet->inspectAudioThread().track;
        check(a.state==b.state&&a.position==b.position&&a.length==b.length&&a.undoCount==b.undoCount&&a.redoCount==b.redoCount&&a.scheduledFrame==b.scheduledFrame&&a.sourceRevision==b.sourceRevision,
              "live compressor must not alter transport, history, capture length, source or STOP timing");
    };
    auto paired=[&](LoopAction op,unsigned request){for(auto* e:{dry.get(),wet.get()}){LoopCommand command{op,0,request,e->generation()};command.value=1;check(e->enqueueLoop(command),"live compressor paired action");}};
    for(unsigned n=0;n<4096/block;++n)render(true);
    paired(LoopAction::finish,2);render(false);paired(LoopAction::play,3);
    for(unsigned n=0;n<1024/block;++n)render(false);
    if(action==LoopAction::stop){
        SyncSettings sync;sync.mode=SyncMode::fixed;sync.bpm=120;sync.bars=1;
        for(auto* e:{dry.get(),wet.get()})e->configureSync(sync);
    }
    paired(action,4);
    CompressorSettings settings{compressorPresetValues(8),true,8};
    LoopCommand eq{LoopAction::compressor,0,5,wet->generation()};eq.compressor=settings;
    check(wet->enqueueLoop(eq),"live compressor enabled in special state");
    const auto origin=frame;Compressor oracle,frozen;
    check(oracle.prepare(48000,settings.values,origin)&&frozen.prepare(48000,settings.values,origin),"live compressor independent render oracles");
    const auto expected=action==LoopAction::overdub?LoopState::overdubbing:action==LoopAction::substitute?LoopState::substituting:action==LoopAction::multiply?LoopState::multiplying:LoopState::stopping;
    double error=0,changed=0;std::uint64_t deadline=0;
    for(unsigned offset=0;offset<2304;offset+=block){
        if(offset==256){settings.preset=0;settings.values[0]=-36;settings.values[5]=6;eq.compressor=settings;eq.compressorMask=(1u<<0)|(1u<<5);eq.requestId=6;check(wet->enqueueLoop(eq)&&oracle.setTarget(0,-36)&&oracle.setTarget(5,6),"live XY controls");}
        if(offset==1024||offset==1280){settings.enabled=offset==1280;eq.compressor=settings;eq.compressorMask=0;eq.requestId=offset;check(wet->enqueueLoop(eq),"live bypass/re-enable");if(settings.enabled)check(oracle.prepare(48000,settings.values,frame),"live re-enable uses current global clock");}
        if(offset==1536){settings.values=compressorPresetValues(10);settings.preset=10;eq.compressor=settings;eq.compressorMask=compressorAllParameters;eq.requestId=1536;check(wet->enqueueLoop(eq)&&oracle.setTargets(settings.values),"live original preset");}
        render(action!=LoopAction::stop);
        const auto actual=wet->inspectAudioThread().track;
        check(actual.state==expected&&actual.compressor==settings,"actual audio-owned state and live compressor targets acknowledged");
        if(action==LoopAction::stop){if(!deadline)deadline=actual.scheduledFrame;check(deadline>frame&&actual.scheduledFrame==deadline,"compressor retains pending STOP deadline");}
        std::array<float,128> ol{},orr{},fl{},fr{};
        // The compressor precedes the equal-power centre pan. Recover the mono
        // pre-pan input, then reapply pan to the independent output oracle.
        constexpr float centre=.7071067811865475f;
        std::array<float,128> il{},ir{};for(unsigned i=0;i<block;++i)il[i]=ir[i]=dl[i]/centre;
        frozen.process(il.data(),ir.data(),2,fl.data(),fr.data(),block);
        for(unsigned i=0;i<block;++i){fl[i]=std::clamp(fl[i]*centre,-1.f,1.f);fr[i]=std::clamp(fr[i]*centre,-1.f,1.f);}
        if(settings.enabled){oracle.process(il.data(),ir.data(),2,ol.data(),orr.data(),block);for(unsigned i=0;i<block;++i){ol[i]=std::clamp(ol[i]*centre,-1.f,1.f);orr[i]=std::clamp(orr[i]*centre,-1.f,1.f);}}
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
            render(false);std::array<float,128> il{},ir{},ol{},orr{};constexpr float centre=.7071067811865475f;for(unsigned i=0;i<block;++i)il[i]=ir[i]=dl[i]/centre;oracle.process(il.data(),ir.data(),2,ol.data(),orr.data(),block);for(unsigned i=0;i<block;++i){ol[i]=std::clamp(ol[i]*centre,-1.f,1.f);orr[i]=std::clamp(orr[i]*centre,-1.f,1.f);}
            for(unsigned i=0;i<block;++i){
                const auto at=frame-block+i;
                if(at<deadline)check(std::abs(wl[i]-ol[i])<=1e-6&&std::abs(wr[i]-orr[i])<=1e-6,"live compressor playback continues until the original scheduled STOP sample");
                if(at>=deadline+240)check(dl[i]==0&&dr[i]==0&&wl[i]==0&&wr[i]==0,"scheduled STOP and bounded tail cannot be revived by compressor");
            }
        }
        check(wet->inspectAudioThread().track.state==LoopState::stopped,"scheduled STOP still completes");
    }else{
        paired(LoopAction::finish,7);render(false);
        check(wet->inspectAudioThread().track.state==LoopState::playing,"live compressor capture finalizes normally");
        const auto track=wet->inspectAudioThread().track;
        check(track.length==(action==LoopAction::multiply?6400u:4096u)&&track.undoCount==1,"live compressor adds no extra history or capture frames");
    }
    auto exported=[&](Engine& engine){
        check(engine.enqueueExport({0,engine.generation(),engine.inspectAudioThread().track.waveformId}),"live compressor dry export request");render(false);
        WavExport out;bool ready=false;
        for(unsigned n=0;n<5000&&!ready;++n){ready=engine.popExport(out);if(!ready)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        check(ready&&!out.bytes.empty(),"live compressor worker export complete");return out.bytes;
    };
    const auto dryBytes=exported(*dry),wetBytes=exported(*wet);
    check(dryBytes==wetBytes,"live compressor edits/toggles/presets never enter stored PCM or capture feedback");
    std::cout<<"live compressor state "<<static_cast<unsigned>(expected)<<" / "<<block<<": peak error "<<error<<", audible edit "<<changed<<'\n';
}
static void midiCompressorControls(unsigned block,unsigned parameter){
    using namespace aion;
    auto gui=std::make_unique<Engine>(),midi=std::make_unique<Engine>();
    for(auto* e:{gui.get(),midi.get()}){e->prepare(48000,32768);e->setMonitoring(false);check(e->enqueue({0,1})&&e->enqueueLoop({LoopAction::record,0,1,e->generation()}),"compressor MIDI capture fixture");}
    std::array<float,128> gl{},gr{},ml{},mr{};float* g[]{gl.data(),gr.data()},* m[]{ml.data(),mr.data()};std::uint64_t frame=0;
    auto render=[&](bool input,std::span<const MidiEvent> events={}){
        for(unsigned i=0;i<block;++i){gl[i]=ml[i]=input?static_cast<float>(.13*std::sin((frame+i)*.073)):0;gr[i]=mr[i]=input?static_cast<float>(.13*std::sin((frame+i)*.073)):0;}
        watch=true;gui->process(g,2,block,{},true);midi->process(m,2,block,events,true);watch=false;frame+=block;
    };
    for(unsigned n=0;n<2048/block;++n)render(true);
    for(auto* e:{gui.get(),midi.get()})check(e->enqueueLoop({LoopAction::finish,0,2,e->generation()}),"compressor MIDI finish fixture");render(false);
    for(auto* e:{gui.get(),midi.get()}){
        check(e->enqueueLoop({LoopAction::play,0,3,e->generation()}),"compressor MIDI play fixture");
        LoopCommand c{LoopAction::compressor,0,4,e->generation()};c.compressor={compressorPresetValues(8),true,8};check(e->enqueueLoop(c),"compressor MIDI initial filter");
    }
    render(false);
    MidiSettings mapping;mapping.enabled=true;mapping.bindings.fill(0);mapping.learnTarget=static_cast<int>(MidiSettings::compressorBase+parameter);mapping.learnToken=1;mapping.learnGeneration=midi->generation();
    check(midi->configureMidi(mapping),"compressor MIDI learn configured");
    std::array<CompressorSettings,20> expected{};expected[0]={compressorPresetValues(8),true,8};
    unsigned request=10;double error=0;float peak=0;
    auto target=[&](unsigned track,unsigned value,unsigned offset){
        auto& settings=expected[track];settings.values[parameter]=compressorMidiValue(parameter,value);settings.preset=0;
        LoopCommand c{LoopAction::compressor,track,request++,gui->generation()};c.frame=frame+offset;c.compressor=settings;c.compressorMask=1u<<parameter;
        check(gui->enqueueLoop(c),"sample-accurate GUI reference for MIDI compressor");
    };
    auto compare=[&]{
        for(unsigned i=0;i<block;++i){error=std::max({error,std::abs(static_cast<double>(gl[i])-ml[i]),std::abs(static_cast<double>(gr[i])-mr[i])});peak=std::max(peak,std::abs(ml[i]));}
        for(unsigned track:{0u,19u})check(midi->inspectAudioThread(track).track.compressor==expected[track],"compressor MIDI updates only selected tab, including preset-to-Custom");
        const auto a=gui->inspectAudioThread().track,b=midi->inspectAudioThread().track;
        check(a.state==b.state&&a.position==b.position&&a.length==b.length&&a.undoCount==b.undoCount,"compressor MIDI cannot enter transport/held/history paths");
    };
    // Learn consumes zero at 17; the following packet at 18 must work before any
    // editor/message acknowledgement, using the same global coefficient clock.
    const std::array<MidiEvent,2> burst{{{17,176,11,0,3},{18,191,11,127,3}}};target(0,127,18);render(false,burst);compare();
    MidiLearnResult learned;check(midi->popMidiLearn(learned)&&learned.target==MidiSettings::compressorBase+parameter&&midi->midiLearnCurrent(learned),"compressor MIDI captured result");
    mapping.bindings[MidiSettings::compressorBase+parameter]=learned.binding;mapping.learnTarget=-1;++mapping.learnToken;check(midi->configureMidi(mapping),"compressor MIDI accepted mapping handoff");
    for(unsigned n=0;n<128;++n){
        if(n==32||n==96){mapping.eqTrack=n==32?19:0;check(midi->configureMidi(mapping),"compressor MIDI selected tab changes");}
        target(mapping.eqTrack,n,3);const MidiEvent event{3,static_cast<std::uint8_t>(176+n%16),11,static_cast<std::uint8_t>(n),3};render(false,{&event,1});compare();
    }
    check(error<=1e-6&&peak>.001f,"learned and preassigned compressor CC PCM matches sample-timed GUI, at 64/128");
    mapping.bindings[MidiSettings::compressorBase+parameter]=midiBinding(2,11,16);check(midi->configureMidi(mapping),"compressor MIDI channel filter");
    const MidiEvent ignored{1,176,11,0,3};render(false,{&ignored,1});compare();
    std::cout<<"compressor MIDI "<<compressorParameters[parameter].name<<" / "<<block<<": same-block learn, all CC values/channels, selected-tab isolation, channel filter, PCM peak error "<<error<<'\n';
}


// Owner-approved persistent envelope: source restarts/history replacements do
// not restart the compressor or EQ clock. Compare the audible output, not flags.
static void persistentChain(unsigned block){
 using namespace aion;
 auto dry=std::make_unique<Engine>(),wet=std::make_unique<Engine>();
 for(auto* e:{dry.get(),wet.get()}){e->prepare(48000,65536);e->setMonitoring(false);check(e->enqueue({0,1})&&e->enqueueLoop({LoopAction::record,0,1,e->generation()}),"persistent chain seed");}
 std::array<float,128> dl{},dr{},wl{},wr{};float* d[]{dl.data(),dr.data()},*w[]{wl.data(),wr.data()};std::uint64_t frame=0;
 auto render=[&](bool input){for(unsigned i=0;i<block;++i)dl[i]=dr[i]=wl[i]=wr[i]=input?static_cast<float>(.1*std::sin((frame+i)*.073)):0;watch=true;dry->process(d,2,block,{},true);wet->process(w,2,block,{},true);watch=false;frame+=block;};
 auto action=[&](LoopAction action,float value=1){for(auto*e:{dry.get(),wet.get()}){LoopCommand c{action,0,2,e->generation()};c.value=value;check(e->enqueueLoop(c),"paired restart/history control");}};
 for(unsigned n=0;n<4096/block;++n)render(true);action(LoopAction::finish);render(false);action(LoopAction::play);
 for(auto*e:{dry.get(),wet.get()}){LoopCommand c{LoopAction::equalizer,0,3,e->generation()};c.equalizer={eqPresetValues(8),true,8};check(e->enqueueLoop(c),"EQ before compressor");}
 for(unsigned n=0;n<1024/block;++n)render(false);
 CompressorSettings settings{compressorPresetValues(3),true,3};LoopCommand c{LoopAction::compressor,0,4,wet->generation()};c.compressor=settings;check(wet->enqueueLoop(c),"connect after EQ");
 Compressor oracle;check(oracle.prepare(48000,settings.values,frame),"persistent oracle global clock");double error=0;float peak=0;
 for(unsigned offset=0;offset<8192;offset+=block){
  if(offset==512)action(LoopAction::retrigger);
  if(offset==1024)action(LoopAction::reverse);
  if(offset==1536)action(LoopAction::overdub);
  if(offset==2560)action(LoopAction::finish);
  if(offset==3072)action(LoopAction::undo);
  if(offset==3584)action(LoopAction::redo);
  if(offset==4096)action(LoopAction::applyStart,.25f);
  render(offset>=1536&&offset<2560);
  constexpr float centre=.7071067811865475f;std::array<float,128> il{},ir{},ol{},orr{};
  for(unsigned i=0;i<block;++i)il[i]=ir[i]=dl[i]/centre;
  oracle.process(il.data(),ir.data(),2,ol.data(),orr.data(),block);
  for(unsigned i=0;i<block;++i){const float expected=std::clamp(ol[i]*centre,-1.f,1.f);error=std::max(error,std::abs(double(wl[i])-expected));check(wl[i]==wr[i],"mono fixture retains equal stereo output");peak=std::max(peak,std::abs(wl[i]));}
  const auto a=dry->inspectAudioThread().track,b=wet->inspectAudioThread().track;
  check(a.state==b.state&&a.waveformId==b.waveformId&&a.length==b.length&&a.position==b.position&&a.undoCount==b.undoCount&&a.redoCount==b.redoCount,"compressor leaves source/history/clock unchanged");
 }
 std::cout<<"persistent EQ -> compressor "<<block<<": peak error "<<error<<'\n';check(error<=1e-6&&peak>.001f,"restart/reverse/OD/Undo/Redo/phase retain continuous compressor envelope after EQ");
 action(LoopAction::stopAll);for(unsigned n=0;n<1024/block;++n)render(false);
 for(unsigned i=0;i<block;++i)check(wl[i]==0&&wr[i]==0,"panic drains without delayed wet revival");
 action(LoopAction::clear);render(false);for(unsigned n=0;n<1024/block;++n)render(false);
 for(unsigned i=0;i<block;++i)check(wl[i]==0&&wr[i]==0,"DEL clears lookahead without stale output");
}
int main(){
 using namespace aion;
 for(unsigned block:{64u,128u})for(auto action:{LoopAction::overdub,LoopAction::substitute,LoopAction::multiply,LoopAction::stop})liveStateControls(block,action);
 for(unsigned block:{64u,128u}){for(unsigned p=0;p<7;++p)midiCompressorControls(block,p);persistentChain(block);}
 for(unsigned block:{64u,128u}){
  auto e=std::make_unique<Engine>();e->prepare(48000,65536);e->setMonitoring(false);
  std::array<float,128> l{},r{};float* audio[]{l.data(),r.data()};
  auto render=[&]{l.fill(0);r.fill(0);watch=true;e->process(audio,2,block,{},true);watch=false;};
  std::array<EqSettings,20> eq{};std::array<CompressorSettings,20> comp{};
  eq[19]={eqPresetValues(8),true,8};comp[19]={compressorPresetValues(8),true,8};
  unsigned accepted=0;while(e->enqueueLoop({LoopAction::volume,0,10,e->generation()}))++accepted;
  check(accepted==255&&!e->enqueueEffectsBank(eq,comp,100),"full queue rejects combined restore atomically");
  for(unsigned n=0;n<5;++n)render();
  check(e->inspectAudioThread(19).track.equalizer==EqSettings{}&&e->inspectAudioThread(19).track.compressor==CompressorSettings{},"rejected bank publishes neither EQ nor compressor prefix");
  check(e->enqueueEffectsBank(eq,comp,100),"combined restore accepted after drain");render();
  check(e->inspectAudioThread(19).track.equalizer==eq[19]&&e->inspectAudioThread(19).track.compressor==comp[19],"both restored on expanded track");
  LoopCommand c{LoopAction::compressor,19,200,e->generation()};c.compressor=comp[19];c.compressor.values[0]=-70;c.compressor.preset=0;c.compressorMask=0;
  check(e->enqueueLoop(c),"invalid mask enters bounded queue");render();check(e->inspectAudioThread(19).track.compressor==comp[19],"unmasked mutation rolls back entire settings");
  c.compressorMask=compressorAllParameters;c.compressorClearEpoch=1;check(e->enqueueLoop(c),"stale epoch enters queue");render();check(e->inspectAudioThread(19).track.compressor==comp[19],"stale ownership cannot change compressor");
  check(e->enqueueLoop({LoopAction::clear,19,201,e->generation()}),"DEL accepted");render();check(e->inspectAudioThread(19).track.compressor==CompressorSettings{},"DEL resets compressor settings and runtime");
  // Short arming cancellation retains selected effects; explicit deletion does not.
  auto bank=std::make_unique<LoopBank>();bank->prepare(262144,0,48000);SyncSettings sync;sync.mode=SyncMode::fixed;sync.bars=1;bank->configureSync(sync);
  for(unsigned i=0;i<64;++i)bank->tick({});
  check(bank->setCompressor(0,comp[19],compressorAllParameters,1,0),"configure before ARM");
  for(auto action:{LoopAction::finish,LoopAction::stop,LoopAction::stopAll}){
   watch=true;check(bank->apply(LoopAction::record,0),"arm with compressor");check(bank->trackView(0).state==LoopState::armed,"armed grid");check(bank->apply(action,0),"cancel ARM");bank->tick({});watch=false;
   check(bank->trackView(0).state==LoopState::empty&&bank->trackView(0).compressor==comp[19],"cancel preserves configured compressor");
  }
 }
 check(operations==0,"callback new/delete prohibited");
 std::cout<<"PASS connected compressor: live OD/SUB/MULT/STOP PCM, raw export, MIDI offsets/learn, combined restore atomicity, epoch/mask rollback and no callback new/delete\n";
}
