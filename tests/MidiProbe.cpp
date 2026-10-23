#include "core/Engine.h"
#include <iomanip>
#include <memory>
#include <iostream>
#include <chrono>
#include <thread>
#include <cstdlib>
int main(int argc,char** argv){
 if(argc==2&&std::string(argv[1])=="panic-repress"){
  auto engine=std::make_unique<aion::Engine>();engine->prepare(8000,8192);aion::MidiSettings settings;settings.enabled=true;if(!engine->configureMidi(settings))return 2;
  aion::EngineView latest{},view{};const auto process=[&](unsigned n,std::span<const aion::MidiEvent> events={}){engine->process(nullptr,0,static_cast<int>(n),events);while(engine->popView(view))latest=view;};
  unsigned request=0;const auto command=[&](aion::LoopAction action){if(!engine->enqueueLoop({action,0,++request,engine->generation(),0,0}))std::exit(2);process(1);};
  command(aion::LoopAction::record);process(512);command(aion::LoopAction::finish);
  for(unsigned n=0;n<10000&&engine->finalizationPending();++n){process(0);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  if(engine->finalizationPending())return 2;command(aion::LoopAction::stopAll);
  unsigned status,a,b,size,panics=0;while(std::cin>>status>>a>>b>>size){if(status>255||a>127||b>127||size>3)return 2;
   const aion::MidiEvent event{0,static_cast<std::uint8_t>(status),static_cast<std::uint8_t>(a),static_cast<std::uint8_t>(b),static_cast<std::uint8_t>(size)};process(1,{&event,1});if(status==252)++panics;
   std::cout<<latest.midi.actions-panics<<'\n';
  }return 0;
 }
 if((argc==2&&std::string(argv[1])=="modes")||(argc==3&&std::string(argv[1])=="reassigned-mode")){
  auto engine=std::make_unique<aion::Engine>();engine->prepare(8000,8192);aion::MidiSettings settings;settings.enabled=true;if(!engine->configureMidi(settings))return 2;
  if(argc==3){
   const std::string value=argv[2];if(value!="30"&&value!="31"&&value!="32")return 2;const unsigned target=value=="30"?30:value=="31"?31:32;
   const aion::MidiEvent held{0,144,36,100,3};engine->process(nullptr,0,1,{&held,1});aion::EngineView discard{};while(engine->popView(discard)){}
   settings.bindings[0]=0;settings.bindings[target]=aion::midiBinding(1,36);++settings.revision;if(!engine->configureMidi(settings))return 2;
  }
  unsigned status,a,b,size;while(std::cin>>status>>a>>b>>size){if(status>255||a>127||b>127||size>3)return 2;
   const aion::MidiEvent e{0,static_cast<std::uint8_t>(status),static_cast<std::uint8_t>(a),static_cast<std::uint8_t>(b),static_cast<std::uint8_t>(size)};
   engine->process(nullptr,0,1,{&e,1});aion::EngineView view{},latest{};while(engine->popView(view))latest=view;
   std::cout<<latest.midi.mode<<' '<<latest.midi.sustain<<'\n';
  }return 0;
 }
 if(argc==2&&std::string(argv[1])=="stale-note"){
  auto engine=std::make_unique<aion::Engine>();engine->prepare(8000,131072);aion::MidiSettings settings;settings.enabled=true;if(!engine->configureMidi(settings))return 2;
  std::uint64_t frame;unsigned status,a,b,size;
  while(std::cin>>frame>>status>>a>>b>>size){
   if(frame<engine->clock()||status>255||a>127||b>127||size>3)return 2;
   aion::EngineView latest{},view{};const auto process=[&](unsigned n,std::span<const aion::MidiEvent> events={}){engine->process(nullptr,0,static_cast<int>(n),events);while(engine->popView(view))latest=view;};
   while(engine->clock()<frame)process(static_cast<unsigned>(std::min<std::uint64_t>(128,frame-engine->clock())));
   const aion::MidiEvent event{0,static_cast<std::uint8_t>(status),static_cast<std::uint8_t>(a),static_cast<std::uint8_t>(b),static_cast<std::uint8_t>(size)};process(1,{&event,1});std::cout<<latest.midi.actions<<'\n';
  }return 0;
 }
 if(argc==2&&(std::string(argv[1])=="continuous"||std::string(argv[1])=="learn-volume")){
  auto engine=std::make_unique<aion::Engine>();engine->prepare(8000,8192);
  aion::MidiSettings settings;settings.enabled=true;settings.bindings[33]=aion::midiBinding(2,11);settings.bindings[53]=aion::midiBinding(2,12);if(std::string(argv[1])=="learn-volume"){settings.bindings[33]=settings.bindings[53]=0;settings.learnTarget=33;settings.learnToken=1;settings.learnGeneration=engine->generation();}if(!engine->configureMidi(settings))return 2;
  unsigned status,a,b,size;while(std::cin>>status>>a>>b>>size){if(status>255||a>255||b>255||size>255)return 2;
   const aion::MidiEvent e{0,static_cast<std::uint8_t>(status),static_cast<std::uint8_t>(a),static_cast<std::uint8_t>(b),static_cast<std::uint8_t>(size)};
   engine->process(nullptr,0,1,{&e,1});aion::EngineView view{},latest{};while(engine->popView(view))latest=view;
   std::cout<<std::setprecision(17)<<latest.tracks[0].volume<<' '<<latest.tracks[0].pan<<'\n';
  }return 0;
 }
aion::MidiSettings s;unsigned status,a,b,size;while(std::cin>>status>>a>>b>>size){if(status>255||a>255||b>255||size>255)return 2;std::cout<<aion::MidiController::targetFor(s,{0,static_cast<std::uint8_t>(status),static_cast<std::uint8_t>(a),static_cast<std::uint8_t>(b),static_cast<std::uint8_t>(size)})<<'\n';}}
