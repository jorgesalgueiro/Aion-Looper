#include "core/MidiController.h"
#include <iostream>
int main(){aion::MidiSettings s;unsigned status,a,b,size;while(std::cin>>status>>a>>b>>size){if(status>255||a>255||b>255||size>255)return 2;std::cout<<aion::MidiController::targetFor(s,{0,static_cast<std::uint8_t>(status),static_cast<std::uint8_t>(a),static_cast<std::uint8_t>(b),static_cast<std::uint8_t>(size)})<<'\n';}}
