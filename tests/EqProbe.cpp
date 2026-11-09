// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Equalizer.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

int main(){
    aion::Equalizer eq;aion::SmoothedEqualizer smooth;bool smoothed=false;std::string operation;std::cout<<std::setprecision(9);
    while(std::cin>>operation){
        if(operation=="prepare"||operation=="configure"||operation=="smooth"){
            double rate=0;if(operation!="configure"&&!(std::cin>>rate))return 2;
            aion::EqValues values{};for(auto& value:values)if(!(std::cin>>value))return 2;
            if(operation!="configure")smoothed=operation=="smooth";
            const bool accepted=operation=="configure"?(smoothed?smooth.setTargets(values):eq.configure(values)):(smoothed?smooth.prepare(rate,values):eq.prepare(rate,values));
            std::cout<<(accepted?"ok":"rejected")<<'\n';
        }else if(operation=="reset"){if(smoothed)smooth.reset();else eq.reset();std::cout<<"ok\n";}
        else if(operation=="target"){
            unsigned parameter;float value;if(!(std::cin>>parameter>>value))return 2;
            std::cout<<(smoothed&&smooth.setTarget(parameter,value)?"ok":"rejected")<<'\n';
        }
        else if(operation=="render"){
            unsigned frames,channels,block;
            if(!(std::cin>>frames>>channels>>block)||frames>1000000||channels>2||!block||block>4096)return 2;
            std::vector<float> left(frames),right(frames),outLeft(frames),outRight(frames);
            for(unsigned i=0;i<frames;++i){if(channels>0&&!(std::cin>>left[i]))return 2;if(channels>1&&!(std::cin>>right[i]))return 2;}
            for(unsigned i=0;i<frames;i+=block){
                const auto count=std::min(block,frames-i);
                if(smoothed)smooth.process(channels?left.data()+i:nullptr,channels>1?right.data()+i:nullptr,outLeft.data()+i,outRight.data()+i,count);
                else eq.process(channels?left.data()+i:nullptr,channels>1?right.data()+i:nullptr,outLeft.data()+i,outRight.data()+i,count);
            }
            std::cout<<"pcm";for(unsigned i=0;i<frames;++i)std::cout<<' '<<outLeft[i]<<' '<<outRight[i];std::cout<<'\n';
        }else return 2;
    }
}
