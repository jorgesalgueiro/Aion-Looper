// SPDX-License-Identifier: AGPL-3.0-only
// Manual warmed Engine measurement, NOT a timing assertion or device latency.
#include "core/Engine.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>
int main(){
    using namespace aion;using Clock=std::chrono::steady_clock;
    for(unsigned block:{64u,128u}){
        Engine engine;engine.prepare(48000,262144);engine.setMonitoring(false);engine.setAutoPlay(true);engine.enqueue({0,1});
        std::array<float,128> left{},right{};float* channels[]{left.data(),right.data()};EngineView view{};std::uint32_t request=0;
        auto process=[&](bool input){for(unsigned i=0;i<block;++i){left[i]=input?static_cast<float>(.002*std::sin((engine.clock()+i)*.071)):0;right[i]=input?static_cast<float>(.003*std::cos((engine.clock()+i)*.113)):0;}engine.process(channels,2,block,{},true);while(engine.popView(view)){};};
        for(unsigned track=0;track<20;++track){
            if(!engine.enqueueLoop({LoopAction::record,track,++request,engine.generation()}))return 2;
            for(unsigned n=0;n<2048/block;++n)process(true);
            if(!engine.enqueueLoop({LoopAction::finish,track,++request,engine.generation()}))return 2;process(false);
        }
        for(const auto& t:view.tracks)if(t.state!=LoopState::playing)return 3;
        std::array<EqSettings,20> bank{};for(auto& s:bank){s.enabled=true;s.values[2]=3;s.values[22]=-3;for(unsigned p=0;p<6;++p){s.values[4+p*3]=p%2?-3.f:3.f;s.values[5+p*3]=2;}}
        if(!engine.enqueueEqBank(bank,request+1))return 4;request+=20;
        for(unsigned n=0;n<512;++n)process(false);
        for(bool targets:{false,true}){
            std::vector<double> times;times.reserve(2048);unsigned overruns=0;const double deadline=block/48000.0*1e6;
            for(unsigned n=0;n<2048;++n){
                if(targets&&n%16==0){for(auto& s:bank)for(unsigned p=0;p<6;++p)s.values[4+p*3]=(n%32?1.f:-1.f)*(p%2?-3.f:3.f);
                    if(!engine.enqueueEqBank(bank,request+1))return 5;request+=20;}
                left.fill(0);right.fill(0);const auto begin=Clock::now();engine.process(channels,2,block);const auto end=Clock::now();
                while(engine.popView(view)){};const auto elapsed=std::chrono::duration<double,std::micro>(end-begin).count();times.push_back(elapsed);if(elapsed>deadline)++overruns;
            }
            std::sort(times.begin(),times.end());std::cout<<"Engine 20 stereo loops / 10 EQ bands / "<<block<<" / "<<(targets?"targets":"steady")<<": p50 "<<times[1024]<<"us, p99 "<<times[2027]<<"us, max "<<times.back()<<"us; deadline "<<deadline<<"us, observed overruns "<<overruns<<"/2048\n";
        }
    }
}
