// SPDX-License-Identifier: AGPL-3.0-only AND BSD-3-Clause
// Resampling coefficients/phase conventions adapted from Blink UpSampler and
// DownSampler, Copyright (C) 2013 Google Inc. All rights reserved.
// Original license: third_party/notices/WebAudio-Resampling-BSD.txt.
// Audited source snapshot: chromium/chromium
// 538c07e4d3aa078bdd8ae5ef92a0c910ce4b465e, renderer/platform/audio/{up,down}_sampler.cc.
// Native causal direct convolution avoids render-quantum buffering and keeps
// subdivision behavior identical at 64/128 and irregular callback sizes.
#include "DistortionWaveShaper.h"
#include <cmath>
#include <limits>
#include <numbers>

namespace aion {
namespace {
float finiteFloat(double value) noexcept {
    constexpr double limit=std::numeric_limits<float>::max();
    return static_cast<float>(value>limit?limit:value<-limit?-limit:value);
}
}
void DistortionWaveShaper::prepare() noexcept {
    constexpr double pi=std::numbers::pi;
    for(unsigned i=0;i<128;++i){
        const double s=pi*(static_cast<double>(i)-64+.5),x=(i+.5)/128.;
        upKernel[i]=static_cast<float>((std::sin(s)/s)*(.42-.5*std::cos(2*pi*x)+.08*std::cos(4*pi*x)));
        const double odd=2.*i+1,t=.5*pi*(odd-128),y=odd/256.;
        downKernel[i]=static_cast<float>((.5*std::sin(t)/t)*(.42-.5*std::cos(2*pi*y)+.08*std::cos(4*pi*y)));
    }
    reset();prepared=true;
}
void DistortionWaveShaper::reset() noexcept {channels={};}
std::array<float,2> DistortionWaveShaper::Up::process(float input,const std::array<float,128>& kernel) noexcept {
    history[index]=input;
    double sum=0;
    for(unsigned k=0;k<128;++k)sum+=static_cast<double>(history[(index-k)&127])*kernel[k];
    const std::array<float,2> out{history[(index-64)&127],finiteFloat(sum)};
    index=(index+1)&127;return out;
}
float DistortionWaveShaper::Down::process(const std::array<float,2>& input,const std::array<float,128>& kernel) noexcept {
    // Reduced odd kernel is shifted one output sample; its current input is
    // the previous high-rate odd sample. Keep it until AFTER convolution.
    double sum=0;
    for(unsigned k=0;k<128;++k)sum+=static_cast<double>(odd[(index-1-k)&127])*kernel[k];
    const float out=finiteFloat(sum)+.5f*even[(index-64)&127];
    even[index]=input[0];odd[index]=input[1];index=(index+1)&127;return out;
}
float DistortionWaveShaper::lookup(const DistortionCurve& curve,float input) noexcept {
    if(std::isnan(input))return 0;
    float result;
    if(input<=-1)result=curve.front();
    else if(input>=1)result=curve.back();
    else {
        // Match the separate Float32 add/scale/interpolation operations of
        // WebAudio's vector curve lookup; no tanh evaluation on audio.
        const float position=(input+1.f)*2048.f;
        const unsigned first=static_cast<unsigned>(position);
        const unsigned second=first<4096?first+1:4096;
        const float fraction=position-static_cast<float>(first);
        const float difference=curve[second]-curve[first];
        const float product=fraction*difference;
        result=curve[first]+product;
    }
    return std::isfinite(result)?result:0;
}
bool DistortionWaveShaper::process(const DistortionCurve& curve,const float* left,const float* right,unsigned count,
                                  float* outLeft,float* outRight,std::size_t frames) noexcept {
    if(!prepared||count>2||frames>maxProcessFrames)return false;
    if(!frames)return true;
    if(!outLeft||!outRight||outLeft==outRight||(count&&!left)||(count==2&&!right))return false;
    for(std::size_t i=0;i<frames;++i){
        const float l=count?left[i]:0,r=count==2?right[i]:l;
        const std::array<float,2> input{std::isfinite(l)?l:0,std::isfinite(r)?r:0};
        for(unsigned ch=0;ch<2;++ch){
            auto& state=channels[ch];const auto twice=state.up1.process(input[ch],upKernel);
            std::array<float,2> down{};
            for(unsigned j=0;j<2;++j){auto four=state.up2.process(twice[j],upKernel);
                for(auto& value:four)value=lookup(curve,value);
                down[j]=state.down2.process(four,downKernel);
            }
            const float value=state.down1.process(down,downKernel);
            (ch?outRight:outLeft)[i]=value;
        }
    }
    return true;
}
}
