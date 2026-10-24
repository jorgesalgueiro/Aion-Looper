#include "core/MeterWindow.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
int main(){
    unsigned channels,frames,block;
    if(!(std::cin>>channels>>frames>>block)||channels<1||channels>2||frames>1000000||(block!=64&&block!=128))return 2;
    aion::MeterWindow window;
    for(unsigned offset=0;offset<frames;offset+=block)for(unsigned i=offset;i<std::min(frames,offset+block);++i){
        float left,right;if(!(std::cin>>left))return 3;
        if(channels==1)window.pushMono(left);else {if(!(std::cin>>right))return 3;window.pushStereo(left,right);}
    }
    std::array<float,aion::MeterWindow::size> output{};window.copy(output);
    for(float sample:output)std::cout<<std::setprecision(17)<<sample<<' ';
}
