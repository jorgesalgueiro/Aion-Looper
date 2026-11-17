// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Equalizer.h"
#include "EqScalarReference.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

int main(){
    std::size_t samples=0;
    auto require=[](bool good){if(!good){std::cerr<<"EQ scalar equivalence failed\n";std::exit(1);}};
    for(double rate:{8000.,22050.,32000.,44100.,48000.,96000.,192000.,384000.}){
        aion::SmoothedEqualizer native;
        aion::eq_scalar_reference::SmoothedEqualizer scalar;
        auto values=aion::eqDefaults();
        for(unsigned p:{2u,4u,7u,10u,13u,16u,19u,22u})values[p]=p%2?12.f:-12.f;
        require(native.prepare(rate,values,37)&&scalar.prepare(rate,values,37));
        std::array<float,257> l{},r{},a{},b{},c{},d{};
        for(unsigned iteration=0;iteration<640;++iteration){
            const unsigned count=std::array<unsigned,7>{1,37,64,128,129,256,257}[iteration%7];
            if(iteration%9==0){
                const auto p=(iteration/9)%values.size();
                const auto& spec=aion::eqParameters[p];
                const float value=iteration%18?spec.minimum:spec.maximum;
                require(native.setTarget(p,value)&&scalar.setTarget(p,value));
            }
            if(iteration==200||iteration==400){native.reset();scalar.reset();}
            for(unsigned i=0;i<count;++i){
                l[i]=static_cast<float>(.23*std::sin((samples+i)*.071));
                r[i]=static_cast<float>(.19*std::cos((samples+i)*.113));
            }
            if(iteration%31==0)l[0]=std::numeric_limits<float>::quiet_NaN();
            if(iteration%43==0)r[count-1]=std::numeric_limits<float>::infinity();
            const bool missingL=iteration%13==0,missingR=iteration%17==0;
            native.process(missingL?nullptr:l.data(),missingR?nullptr:r.data(),a.data(),b.data(),count);
            scalar.process(missingL?nullptr:l.data(),missingR?nullptr:r.data(),c.data(),d.data(),count);
            for(unsigned i=0;i<count;++i){require(a[i]==c[i]&&b[i]==d[i]);samples+=2;}
        }
    }
    std::cout<<"PASS: "<<samples<<" stereo channel samples exactly equal to frozen 80433e4 EQ, including automation, reset, absent channels and bad-lane recovery\n";
}
