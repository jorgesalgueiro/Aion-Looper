// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Compressor.h"
#include "CompressorScalarReference.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

int main(){
    auto require=[](bool ok){if(!ok){std::cerr<<"Compressor scalar PCM/envelope equivalence failed\n";std::exit(1);}};
    std::size_t samples=0,meters=0;
    for(double rate:{8000.,22050.,32000.,44100.,48000.,96000.,192000.,384000.}){
        for(unsigned preset=1;preset<=11;++preset){
            aion::Compressor native;
            aion::compressor_scalar_reference::Compressor scalar;
            aion::CompressorValues values{};require(aion::compressorPreset(preset,values));
            require(native.prepare(rate,values,37)&&scalar.prepare(rate,values,37));
            std::array<float,257> l{},r{},a{},b{},c{},d{};
            std::size_t frame=0;
            for(unsigned iteration=0;iteration<320;++iteration){
                const unsigned count=std::array<unsigned,7>{1,31,64,128,129,256,257}[iteration%7];
                if(iteration%11==0){
                    const auto p=(iteration/11)%values.size();const auto& spec=aion::compressorParameters[p];
                    const float v=iteration%22?spec.minimum:spec.maximum;
                    require(native.setTarget(p,v)&&scalar.setTarget(p,v));
                    // Repeated gestures must retain their accepted automation anchor.
                    require(native.setTarget(p,v)&&scalar.setTarget(p,v));
                }
                if(iteration==170){native.reset(129);scalar.reset(129);}
                if(iteration==230){require(native.prepare(rate,values)&&scalar.prepare(rate,values));}
                for(unsigned i=0;i<count;++i){
                    const double level=iteration%8<2?0:iteration%8<5?.95:.003;
                    l[i]=static_cast<float>(level*std::sin((frame+i)*.071));
                    r[i]=static_cast<float>(level*.7*std::cos((frame+i)*.113));
                }
                if(iteration%41==0)l[0]=std::numeric_limits<float>::quiet_NaN();
                if(iteration%43==0)r[count-1]=std::numeric_limits<float>::infinity();
                const unsigned channels=iteration%19==0?0:iteration%17==0?1:2;
                const float* left=iteration%29==0?nullptr:l.data();
                const float* right=iteration%23==0?nullptr:r.data();
                native.process(left,right,channels,a.data(),b.data(),count);
                scalar.process(left,right,channels,c.data(),d.data(),count);
                for(unsigned i=0;i<count;++i){require(a[i]==c[i]&&b[i]==d[i]);samples+=2;}
                require(native.reductionDb()==scalar.reductionDb());++meters;frame+=count;
            }
        }
    }
    std::cout<<"PASS: "<<samples<<" channel samples and "<<meters<<" gain-reduction observations exactly equal to frozen 5908944 compressor, 11 presets, 8 rates, variable blocks, automation, reset, silence/attack/release and malformed inputs\n";
}
