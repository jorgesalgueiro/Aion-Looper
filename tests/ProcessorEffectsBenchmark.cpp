// SPDX-License-Identifier: AGPL-3.0-only
// Development-only production processBlock observation, not device/GUI latency.
#include "plugin/Processor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <vector>
#ifdef AION_GPROF_CORE
#include <sys/gmon.h>
#endif
#ifdef AION_DRIVE_SNAPSHOT_RUN
extern "C" const char* aionDriveSnapshotIdentity();
#endif
#ifdef _WIN32
#include <malloc.h>
#endif
namespace {
thread_local bool audioWatch=false;
std::atomic<unsigned> heapCalls{0};
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
void allocation(){if(audioWatch)++heapCalls;}
}
void* operator new(std::size_t n){allocation();if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{allocation();std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
void* operator new(std::size_t n,std::align_val_t a){
 allocation();void* p=nullptr;
#ifdef _WIN32
 p=_aligned_malloc(n?n:1,static_cast<std::size_t>(a));
#else
 if(posix_memalign(&p,static_cast<std::size_t>(a),n?n:1))p=nullptr;
#endif
 if(p)return p;throw std::bad_alloc();
}
void operator delete(void* p,std::align_val_t)noexcept{allocation();
#ifdef _WIN32
 _aligned_free(p);
#else
 std::free(p);
#endif
}
void* operator new[](std::size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete[](void* p,std::align_val_t a)noexcept{::operator delete(p,a);}
void operator delete(void* p,std::size_t,std::align_val_t a)noexcept{::operator delete(p,a);}
void operator delete[](void* p,std::size_t,std::align_val_t a)noexcept{::operator delete(p,a);}
int main(int argc,char** argv){
#ifdef AION_GPROF_CORE
 moncontrol(0); // Do not attribute capture/setup/teardown to the playback loop.
#endif
 juce::ScopedJuceInitialiser_GUI gui;
 const bool quick=argc==3&&juce::String(argv[1])=="--quick";
 const int quickBlock=quick?juce::String(argv[2]).getIntValue():0;
 const bool single=argc==5&&juce::String(argv[1])=="--case";
 const int selectedRate=single?juce::String(argv[2]).getIntValue():0,selectedBlock=single?juce::String(argv[3]).getIntValue():0,selectedTracks=single?juce::String(argv[4]).getIntValue():0;
 check(argc==1||(quick&&(quickBlock==64||quickBlock==128))||(single&&(selectedRate==48000||selectedRate==96000)&&(selectedBlock==64||selectedBlock==128)&&(selectedTracks==10||selectedTracks==20)),"Usage: aion_processor_effects_benchmark [--quick 64|128 | --case 48000|96000 64|128 10|20]");
 const unsigned warm=quick?16:256,measured=quick?64:1024;
 for(double rate:{48000.,96000.})for(int block:{64,128})for(unsigned tracks:{10u,20u}){
  if(quick&&(rate!=48000||block!=quickBlock||tracks!=10))continue;
  if(single&&(rate!=selectedRate||block!=selectedBlock||tracks!=static_cast<unsigned>(selectedTracks)))continue;
  auto p=std::make_unique<AionProcessor>();p->setPlayConfigDetails(2,2,rate,block);p->prepareToPlay(rate,block);
  juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;midi.ensureSize(4096);
  auto snapshot=[&]{return p->request({juce::JSON::parse(R"({"operation":"snapshot"})")});};
  auto process=[&](bool input){
   const auto frame=p->inspectAudioThread().frame;
   for(int i=0;i<block;++i){audio.setSample(0,i,input?static_cast<float>(.13*std::sin((frame+i)*.13)):0);audio.setSample(1,i,input?static_cast<float>(.07*std::cos((frame+i)*.17)):0);}
   p->processBlock(audio,midi);midi.clear();
  };
  process(false);auto state=snapshot();const auto generation=state["generation"];
  auto request=[&](juce::var r){
   auto* o=r.getDynamicObject();check(o!=nullptr,"Request object");o->setProperty("generation",generation);o->setProperty("expiresAt",static_cast<double>(juce::Time::currentTimeMillis()+4000));
   const auto reply=p->request({r});if(!static_cast<bool>(reply["ok"])){std::cerr<<juce::JSON::toString(reply)<<'\n';check(false,"Production fixture command rejected");}
  };
  auto command=[&](const char* operation,const char* action,unsigned track){auto* o=new juce::DynamicObject();o->setProperty("operation",operation);o->setProperty("action",action);o->setProperty("track",static_cast<int>(track));return juce::var(o);};
  for(const auto* id:{"masterGain","inputMonitor","autoPlayAfterRecord","syncMode","metronomeVolume"}){
   auto* o=new juce::DynamicObject();o->setProperty("operation","setParameter");o->setProperty("id",id);o->setProperty("value",juce::String(id)=="masterGain"?1:0);request(juce::var(o));
  }
  auto enableMidi=command("midiConfig","enable",0);enableMidi.getDynamicObject()->setProperty("enabled",true);request(enableMidi);
  // Offline completion is SETUP ONLY. Every timed callback below is realtime.
  p->setNonRealtime(true);process(false);snapshot();
  for(unsigned track=0;track<tracks;++track){
   request(command("loop","record",track));for(int n=0;n<1024/block;++n)process(true);
   request(command("loop","finish",track));process(false);snapshot();
   check(p->inspectAudioThread(track).track.state==aion::LoopState::stopped,"Capture finalized before timing");
   request(command("loop","play",track));auto volume=command("loop","volume",track);volume.getDynamicObject()->setProperty("value",.5/tracks);request(volume);
  }
  process(false);snapshot();p->setNonRealtime(false);
  std::array<std::uint64_t,20> clips{};
  for(unsigned i=0;i<tracks;++i){const auto t=p->inspectAudioThread(i).track;check(t.state==aion::LoopState::playing&&t.length==1024&&t.waveformId,"Every loaded loop plays committed PCM");clips[i]=t.waveformId;}
  for(unsigned scenario=0;scenario<3;++scenario){
   const bool effects=scenario!=0;const unsigned events=scenario==2?32:0;
   for(unsigned track=0;track<tracks;++track){
    auto chain=command("signalChain","set",track);chain.getDynamicObject()->setProperty("order","QCTFO");request(chain);
    for(const auto* effect:{"equalizer","compressor"}){
     auto preset=command(effect,"preset",track);preset.getDynamicObject()->setProperty("preset",juce::String(effect)=="equalizer"?"Radio":"drums");request(preset);
     auto enabled=command(effect,"enable",track);enabled.getDynamicObject()->setProperty("enabled",effects);request(enabled);
    }
    for(const auto name:aion::driveNames){auto enabled=command("drive","enable",track);enabled.getDynamicObject()->setProperty("effect",juce::String(name.data()));enabled.getDynamicObject()->setProperty("enabled",effects);request(enabled);}
   }
   // Drain initial settings through the real callback/message request paths.
   for(unsigned i=0;i<3;++i){process(false);snapshot();}
   const int midiBefore=static_cast<int>(snapshot()["midi"]["messages"]);
   std::vector<double> times(measured);double checksum=0,serviceUs=0;heapCalls=0;
#ifdef AION_GPROF_CORE
   // One continuous window preserves the sampling interval. This includes the
   // fixture's MIDI/input/checksum and periodic off-audio snapshot service too;
   // the profile is attribution, never a clean callback latency measurement.
   moncontrol(1);
#endif
   for(unsigned n=0;n<warm+measured;++n){
    audio.clear();midi.clear();
    for(unsigned event=0;event<events;++event)midi.addEvent(juce::MidiMessage::controllerEvent(1+event%16,119,static_cast<int>((n+event)%128)),static_cast<int>(event*block/events));
    const auto begin=std::chrono::steady_clock::now();audioWatch=true;p->processBlock(audio,midi);audioWatch=false;
    const auto us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count();
    if(n>=warm)times[n-warm]=us;
    if(n%16==0){const auto before=std::chrono::steady_clock::now();state=snapshot();serviceUs+=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-before).count();}
    for(int i=0;i<block;++i){const float l=audio.getSample(0,i),r=audio.getSample(1,i);check(std::isfinite(l)&&std::isfinite(r),"Finite production callback PCM");if(n>=warm)checksum+=std::abs(l)+std::abs(r);}
   }
#ifdef AION_GPROF_CORE
   moncontrol(0);
#endif
   snapshot();midi.clear();process(false);state=snapshot(); // Fresh final telemetry, outside timing.
   check(static_cast<int>(state["midi"]["messages"])-midiBefore==static_cast<int>(events*(warm+measured)),"MIDI traffic really traversed the production router");
   check(static_cast<int>(state["midi"]["overflows"])==0&&static_cast<int>(state["midi"]["actions"])==0,"Unmapped CC traffic never overflows or triggers transport");
   check(!p->inspectCallbackClockAudioThread().nonRealtime,"Timed workload is not offline processing");
   check(heapCalls.load()==0&&checksum>0,"Nonzero PCM and no callback C++ heap operations, including aligned forms");
   for(unsigned i=0;i<tracks;++i){const auto t=p->inspectAudioThread(i).track;auto expected=aion::defaultDriveBank();for(auto& drive:expected)drive.enabled=effects;
    check(t.state==aion::LoopState::playing&&t.length==1024&&t.waveformId==clips[i]&&t.volume==static_cast<float>(.5/tracks),"Timing/MIDI preserves real playback and clip owners");
    check(t.equalizer==aion::EqSettings{aion::eqPresetValues(8),effects,8}&&t.compressor==aion::CompressorSettings{aion::compressorPresetValues(3),effects,3}&&t.drives==expected&&t.signalChain==aion::normalizedSignalChain("QCTFO"),"Reported effects and editable chain really match audio-owned settings");
   }
   std::sort(times.begin(),times.end());double sum=0;unsigned over=0;const double deadline=block/rate*1e6;for(double t:times){sum+=t;over+=t>deadline;}
   std::cout<<"{\"driveImplementation\":\""
#ifdef AION_DRIVE_SNAPSHOT_RUN
    <<aionDriveSnapshotIdentity()
#else
    <<"current"
#endif
    <<"\""
#ifdef AION_GPROF_CORE
    <<",\"instrumentation\":\"gprof core; timing invalid for capacity; profile includes fixture and snapshot service\""
#endif
    <<",\"scope\":\"Production processBlock/APVTS/MidiBuffer, synthetic stereo captures; excludes device, editor, message dispatch timers and roundtrip\",\"rate\":"<<rate<<",\"block\":"<<block<<",\"tracks\":"<<tracks<<",\"effects\":"<<(effects?"true":"false")<<",\"midiEventsPerCallback\":"<<events<<",\"warmCallbacks\":"<<warm<<",\"measuredCallbacks\":"<<measured<<",\"meanUs\":"<<sum/measured<<",\"p99Us\":"<<times[measured*99/100]<<",\"maximumUs\":"<<times.back()<<",\"deadlineUs\":"<<deadline<<",\"observedOverruns\":"<<over<<",\"callbackCxxHeapCalls\":"<<heapCalls.load()<<",\"offAudioSnapshotServiceTotalUs\":"<<serviceUs<<",\"checksum\":"<<checksum<<"}"<<std::endl;
  }
 }
}

