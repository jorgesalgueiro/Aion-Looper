// SPDX-License-Identifier: AGPL-3.0-only
// Manual Release measurement; not a CTest timing gate or a production callback.
#include "core/Equalizer.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

int main(){
    constexpr unsigned warm=512,callbacks=2048,tracks=20;constexpr double rate=48000;
    for(unsigned block:{64,128})for(bool automation:{false,true}){
        auto filters=std::make_unique<std::array<aion::SmoothedEqualizer,tracks>>();
        auto p=aion::eqDefaults();p[2]=3;p[22]=-3;
        for(unsigned i=0;i<6;++i){p[4+i*3]=i%2?3.f:-3.f;p[5+i*3]=1.7f;}
        for(auto& filter:*filters)if(!filter.prepare(rate,p))return 2;
        std::array<float,128> left{},right{},outLeft{},outRight{};
        for(unsigned i=0;i<128;++i){left[i]=static_cast<float>(.1*std::sin(i*.13));right[i]=static_cast<float>(.1*std::cos(i*.17));}
        std::vector<double> durations(callbacks);double checksum=0;
        for(unsigned callback=0;callback<warm+callbacks;++callback){
            const auto start=std::chrono::steady_clock::now();
            for(auto& filter:*filters){
                if(automation&&callback%16==0){auto next=p;for(unsigned i=0;i<6;++i)next[4+i*3]+=(callback%32?2.f:-2.f);if(!filter.setTargets(next))return 3;}
                filter.process(left.data(),right.data(),outLeft.data(),outRight.data(),block);
                checksum+=outLeft[block-1]+outRight[block-1];
            }
            const auto end=std::chrono::steady_clock::now();
            if(callback>=warm)durations[callback-warm]=std::chrono::duration<double,std::micro>(end-start).count();
        }
        std::sort(durations.begin(),durations.end());double sum=0;unsigned overruns=0;const double deadline=block/rate*1e6;
        for(double duration:durations){sum+=duration;overruns+=duration>deadline;}
        std::cout<<"{\"scope\":\"20 stereo EQ kernels, all ten bands active; excludes production routing, callback/device/round-trip latency\",\"rate\":"<<rate
          <<",\"block\":"<<block<<",\"automated\":"<<(automation?"true":"false")<<",\"warmCallbacks\":"<<warm<<",\"measuredCallbacks\":"<<callbacks
          <<",\"meanUs\":"<<sum/callbacks<<",\"p99Us\":"<<durations[callbacks*99/100]<<",\"maximumUs\":"<<durations.back()<<",\"deadlineUs\":"<<deadline
          <<",\"observedOverruns\":"<<overruns<<",\"checksum\":"<<checksum<<"}\n";
    }
}
