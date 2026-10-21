#include "core/Engine.h"
#include <iomanip>
#include <memory>
#include <iostream>
int main(int argc,char** argv){
 if(argc==2&&std::string(argv[1])=="continuous"){
  auto engine=std::make_unique<aion::Engine>();engine->prepare(8000,8192);
  aion::MidiSettings settings;settings.enabled=true;settings.bindings[33]=aion::midiBinding(2,11);settings.bindings[53]=aion::midiBinding(2,12);if(!engine->configureMidi(settings))return 2;
  unsigned status,a,b,size;while(std::cin>>status>>a>>b>>size){if(status>255||a>255||b>255||size>255)return 2;
   const aion::MidiEvent e{0,static_cast<std::uint8_t>(status),static_cast<std::uint8_t>(a),static_cast<std::uint8_t>(b),static_cast<std::uint8_t>(size)};
   engine->process(nullptr,0,1,{&e,1});aion::EngineView view{},latest{};while(engine->popView(view))latest=view;
   std::cout<<std::setprecision(17)<<latest.tracks[0].volume<<' '<<latest.tracks[0].pan<<'\n';
  }return 0;
 }
aion::MidiSettings s;unsigned status,a,b,size;while(std::cin>>status>>a>>b>>size){if(status>255||a>255||b>255||size>255)return 2;std::cout<<aion::MidiController::targetFor(s,{0,static_cast<std::uint8_t>(status),static_cast<std::uint8_t>(a),static_cast<std::uint8_t>(b),static_cast<std::uint8_t>(size)})<<'\n';}}
