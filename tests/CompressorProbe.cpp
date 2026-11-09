// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Compressor.h"
#include "core/CompressorSettings.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
int main(){
    aion::Compressor comp;std::string operation;std::cout<<std::setprecision(9);
    while(std::cin>>operation){
        if(operation=="model"){
            std::cout<<std::setprecision(17)<<"{\"midi\":[";
            for(unsigned p=0;p<7;++p){if(p)std::cout<<',';std::cout<<'[';for(unsigned cc=0;cc<128;++cc){if(cc)std::cout<<',';std::cout<<aion::compressorMidiValue(p,cc);}std::cout<<']';}
            std::cout<<"],\"curves\":[";
            for(unsigned id=0;id<=11;++id){if(id)std::cout<<',';auto values=aion::compressorDefaults();if(id&&!aion::compressorPreset(id,values))return 2;
                std::cout<<'[';for(unsigned x=0;x<=320;++x){if(x)std::cout<<',';std::cout<<aion::compressorDisplayCurveDb(-60+(x/320.0)*60,values);}std::cout<<']';}
            std::cout<<"]}\n"<<std::setprecision(9);
        }else if(operation=="presets"){
            std::cout<<'[';
            for(unsigned id=1;id<=11;++id){
                aion::CompressorValues p{};if(!aion::compressorPreset(id,p))return 2;
                if(id>1)std::cout<<',';
                std::cout<<"{\"name\":\""<<aion::compressorPresetNames[id-1]<<"\",\"values\":[";
                for(unsigned i=0;i<7;++i){if(i)std::cout<<',';std::cout<<p[i];}std::cout<<"]}";
            }std::cout<<"]\n";
        }else if(operation=="prepare"){
            double rate;aion::CompressorValues p{};if(!(std::cin>>rate))return 2;
            for(auto& v:p)if(!(std::cin>>v))return 2;
            std::cout<<(comp.prepare(rate,p)?"ok":"rejected")<<'\n';
        }else if(operation=="target"){
            unsigned parameter;float value;if(!(std::cin>>parameter>>value))return 2;
            std::cout<<(comp.setTarget(parameter,value)?"ok":"rejected")<<'\n';
        }else if(operation=="render"){
            unsigned frames,channels,block;
            if(!(std::cin>>frames>>channels>>block)||frames>1000000||channels>2||!block||block>4096)return 2;
            std::vector<float> left(frames),right(frames),ol(frames),orr(frames);
            for(unsigned i=0;i<frames;++i){if(channels&&!(std::cin>>left[i]))return 2;if(channels>1&&!(std::cin>>right[i]))return 2;}
            for(unsigned i=0;i<frames;i+=block)comp.process(channels?left.data()+i:nullptr,channels>1?right.data()+i:nullptr,channels,ol.data()+i,orr.data()+i,std::min(block,frames-i));
            std::cout<<"pcm";for(unsigned i=0;i<frames;++i)std::cout<<' '<<ol[i]<<' '<<orr[i];std::cout<<'\n';
        }else return 2;
    }
}
