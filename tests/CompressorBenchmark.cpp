// SPDX-License-Identifier: AGPL-3.0-only
// Manual Release kernel measurement, not a hard deadline test.
#include "core/Compressor.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>
int main(){
    constexpr unsigned warm=512,callbacks=2048,tracks=20;constexpr double rate=48000;
    for(unsigned block:{64u,128u})for(bool automation:{false,true}){
        auto filters=std::make_unique<std::array<aion::Compressor,tracks>>();
        auto values=aion::compressorDefaults();values[6]=.65f;
        for(auto& filter:*filters)if(!filter.prepare(rate,values))return 2;
        std::array<float,128> left{},right{},ol{},orr{};
        for(unsigned i=0;i<128;++i){left[i]=static_cast<float>(.7*std::sin(i*.13));right[i]=static_cast<float>(.3*std::cos(i*.17));}
        std::vector<double> times(callbacks);double checksum=0;
        for(unsigned callback=0;callback<warm+callbacks;++callback){
            const auto begin=std::chrono::steady_clock::now();
            for(auto& filter:*filters){
                if(automation&&!(callback%16)){
                    auto next=values;next[0]=callback%32?-12.f:-24.f;next[1]=callback%32?2.f:8.f;next[5]=callback%32?2.f:4.f;next[6]=callback%32?.2f:.8f;
                    if(!filter.setTargets(next))return 3;
                }
                filter.process(left.data(),right.data(),2,ol.data(),orr.data(),block);checksum+=ol[block-1]+orr[block-1];
            }
            const auto end=std::chrono::steady_clock::now();
            if(callback>=warm)times[callback-warm]=std::chrono::duration<double,std::micro>(end-begin).count();
        }
        std::sort(times.begin(),times.end());double sum=0;unsigned overruns=0;const double deadline=block/rate*1e6;
        for(double t:times){sum+=t;overruns+=t>deadline;}
        std::cout<<"{\"scope\":\"20 stereo compressor kernels, excludes production routing/callback/device/roundtrip\",\"rate\":"<<rate
            <<",\"block\":"<<block<<",\"automated\":"<<(automation?"true":"false")<<",\"warmCallbacks\":"<<warm<<",\"measuredCallbacks\":"<<callbacks
            <<",\"meanUs\":"<<sum/callbacks<<",\"p99Us\":"<<times[callbacks*99/100]<<",\"maximumUs\":"<<times.back()<<",\"deadlineUs\":"<<deadline
            <<",\"observedOverruns\":"<<overruns<<",\"checksum\":"<<checksum<<"}\n";
    }
}
