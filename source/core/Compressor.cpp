/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
// Derived DSP portions retain the BSD-3-Clause upstream notice above.
// Copyright 2022 The Chromium Authors. AudioParam timeline adaptation also
// retains the BSD-style license in third_party/notices/Chromium-BSD.txt.
// Adaptation: fixed stereo storage, continuous subdivision clock, validated
// controls, original AudioParam automation and dry/wet/makeup routing.
#include "Compressor.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace aion {
namespace {
float db(float linear) noexcept { return 20*std::log10(linear); }
float linear(float decibels) noexcept { return std::pow(10.0f,.05f*decibels); }
float finite(float x,float fallback) noexcept { return std::isfinite(x)?x:fallback; }
float flush(float x) noexcept { return std::abs(x)<std::numeric_limits<float>::min()?0:x; }
constexpr float piOverTwo=std::numbers::pi_v<float>/2;
constexpr float z1=.09f,z2=.16f,z3=.42f,z4=.98f;
constexpr std::array<float,5> releaseBases{{
    .9999999999999998f*z1+1.8432219684323923e-16f*z2-1.9373394351676423e-16f*z3+8.824516011816245e-18f*z4,
    -1.5788320352845888f*z1+2.3305837032074286f*z2-.9141194204840429f*z3+.1623677525612032f*z4,
    .5334142869106424f*z1-1.272736789213631f*z2+.9258856042207512f*z3-.18656310191776226f*z4,
    .08783463138207234f*z1-.1694162967925622f*z2+.08588057951595272f*z3-.00429891410546283f*z4,
    -.042416883008123074f*z1+.1115693827987602f*z2-.09764676325265872f*z3+.028494263462021576f*z4
}};
}
bool validCompressorValues(const CompressorValues& values) noexcept {
    for(std::size_t i=0;i<values.size();++i)
        if(!std::isfinite(values[i])||values[i]<compressorParameters[i].minimum||values[i]>compressorParameters[i].maximum)return false;
    return true;
}
bool compressorPreset(unsigned id,CompressorValues& values) noexcept {
    constexpr std::array<CompressorValues,11> presets{{
        {-16,4,10,.01f,.15f,2,1},{-20,4,15,.005f,.1f,3,1},{-24,6,5,.015f,.1f,4,1},
        {-18,3,20,.02f,.2f,2.5f,1},{-18,2.5f,10,.01f,.2f,3,1},{-12,1.5f,30,.05f,.5f,1.5f,1},
        {-30,12,0,.002f,.05f,6,1},{-32,8,5,.005f,.8f,5,1},{-30,10,0,.005f,.1f,8,1},
        {-10,1.5f,30,.1f,.6f,1,.8f},{-20,6,3,.03f,.15f,3,1}
    }};
    if(!id||id>presets.size())return false;
    auto result=presets[id-1];
    // JavaScript calculates using decimal Number literals before Float32 params.
    constexpr std::array<double,11> ratios{{4,4,6,3,2.5,1.5,12,8,10,1.5,6}};
    const double makeup=std::min(25.0,(1-1/ratios[id-1])*std::abs(double(result[0]))-1);
    result[5]=static_cast<float>(std::pow(10.0,makeup/20));
    values=result;return true;
}
bool Compressor::prepare(double sampleRate,const CompressorValues& values) noexcept {
    if(!std::isfinite(sampleRate)||sampleRate<8000||sampleRate>384000||!validCompressorValues(values))return false;
    rate=static_cast<float>(sampleRate);target=values;prepared=true;
    delayFrames=std::min(1023u,static_cast<unsigned>(.006f*rate));
    meteringRelease=static_cast<float>(1-std::exp(-1/(.325f*double(rate))));
    controlStep=static_cast<float>(1-std::exp(-128/(double(rate)*.05f)));
    const float c=static_cast<float>(1-std::exp(-1/(double(rate)*.05f)));
    smoothingSteps={c,c*(2-c),c*((c-3)*c+3),c*(c*((4-c)*c-6)+4)};
    reset();return true;
}
void Compressor::reset() noexcept {
    for(auto& channel:delay)channel.fill(0);
    frame=0;changedAt.fill(0);audioRates={};deferredBoundary.fill(false);
    for(std::size_t i=0;i<start.size();++i)last[i]=start[i]=automationTarget(i);
    detectorAverage=0;compressorGain=1;meteringGain=1;maxAttackDiff=-1;
    readIndex=0;writeIndex=delayFrames;threshold=kneeDb=ratio=-1;
    if(prepared)configure(target);
}
float Compressor::audioRate(std::size_t p) noexcept {
    const float dest=automationTarget(p);
    auto& state=audioRates[p-5];
    if(frame==changedAt[p]||!(frame%128)){
        state.base=frame==changedAt[p]?start[p]:last[p]+(dest-last[p])*smoothingSteps[0];
        // The browser ends the asymptotic automation at its documented practical
        // convergence threshold, checked once per render quantum.
        if(double(frame-changedAt[p])>10*.05f*double(rate)||
           std::abs(dest-state.base)<4.539992976248485e-05f*(dest==0?1:std::abs(state.base)))state.base=dest;
        state.phase=0;
    }
    if(state.phase==0){
        const float delta=dest-state.base;
        state.lane={state.base,state.base+delta*smoothingSteps[0],state.base+delta*smoothingSteps[1],state.base+delta*smoothingSteps[2]};
        state.next=state.base+delta*smoothingSteps[3];
    }
    const float value=state.lane[state.phase++];
    if(state.phase==4){state.phase=0;state.base=state.next;}
    // original smoothSetParam timestamps use currentTime=frame/rate. A binary
    // round-trip just above the integer frame leaves that one a-rate sample on
    // the former event (ceil boundary); subsequent new-event samples retain
    // the original fractional-time exponential phase. Do not move the anchor.
    return frame==changedAt[p]&&deferredBoundary[p-5]?oldBoundary[p-5]:value;
}
void Compressor::retarget(std::size_t p,float value) noexcept {
    if(p>=5){
        oldBoundary[p-5]=last[p]+(automationTarget(p)-last[p])*smoothingSteps[0];
        deferredBoundary[p-5]=(double(frame)/double(rate))*double(rate)>double(frame);
    }
    start[p]=last[p];changedAt[p]=frame;
    if(p<7)target[p]=value;
}
bool Compressor::setTargets(const CompressorValues& values) noexcept {
    if(!prepared||!validCompressorValues(values))return false;
    for(std::size_t p=0;p<values.size();++p)if(values[p]!=target[p]){
        if(p==6)retarget(7,1-values[p]);
        retarget(p,values[p]);
    }
    return true;
}
bool Compressor::setTarget(std::size_t parameter,float value) noexcept {
    if(!prepared||parameter>=target.size()||!std::isfinite(value)||value<compressorParameters[parameter].minimum||value>compressorParameters[parameter].maximum)return false;
    if(parameter==6)retarget(7,1-value);
    retarget(parameter,value);return true;
}
float Compressor::kneeCurve(float x,float k) const noexcept {
    if(x<linearThreshold)return x;
    return linearThreshold+(1-static_cast<float>(std::exp(double(-k*(x-linearThreshold)))))/k;
}
float Compressor::saturate(float x,float k) const noexcept {
    if(x<kneeThreshold)return kneeCurve(x,k);
    return linear(dbYKneeThreshold+slope*(db(x)-dbKneeThreshold));
}
float Compressor::kAtSlope(float desired) const noexcept {
    const float x=linear(threshold+kneeDb);
    float x2=1,dbX2=0;
    if(!(x<linearThreshold)){x2=static_cast<float>(x*1.001);dbX2=db(x2);}
    float minK=.1f,maxK=10000,k=5,s=1;
    for(int i=0;i<15;++i){
        if(!(x<linearThreshold))s=(db(kneeCurve(x2,k))-db(kneeCurve(x,k)))/(dbX2-(threshold+kneeDb));
        if(s<desired)maxK=k;else minK=k;
        k=std::sqrt(minK*maxK);
    }
    return k;
}
void Compressor::configure(const CompressorValues& values) noexcept {
    if(values[0]!=threshold||values[2]!=kneeDb||values[1]!=ratio){
        threshold=values[0];linearThreshold=linear(threshold);kneeDb=values[2];
        ratio=values[1];slope=1/ratio;knee=kAtSlope(slope);
        dbKneeThreshold=threshold+kneeDb;kneeThreshold=linear(dbKneeThreshold);
        dbYKneeThreshold=db(kneeCurve(kneeThreshold,knee));
    }
    postGain=std::pow(1/saturate(1,knee),.6f);
    attackFrames=std::max(.001f,values[3])*rate;
    const float releaseFrames=rate*values[4];
    for(unsigned i=0;i<5;++i)releaseCurve[i]=releaseFrames*releaseBases[i];
    satReleaseFrames=.0025f*rate;
}
void Compressor::envelope() noexcept {
    detectorAverage=finite(detectorAverage,1);
    scaledDesired=std::asin(detectorAverage)/piOverTwo;
    const bool releasing=scaledDesired>compressorGain;
    float diff=scaledDesired==0?(releasing?-1.0f:1.0f):db(compressorGain/scaledDesired);
    if(releasing){
        maxAttackDiff=-1;diff=finite(diff,-1);
        float x=.25f*(std::clamp(diff,-12.0f,0.0f)+12);
        const float x2=x*x,x3=x2*x,x4=x2*x2;
        const float frames=releaseCurve[0]+releaseCurve[1]*x+releaseCurve[2]*x2+releaseCurve[3]*x3+releaseCurve[4]*x4;
        envelopeRate=linear(5/frames);
    }else{
        diff=finite(diff,1);
        if(maxAttackDiff==-1||maxAttackDiff<diff)maxAttackDiff=diff;
        const float x=.25f/std::max(.5f,maxAttackDiff);
        envelopeRate=1-std::pow(x,1/attackFrames);
    }
}
void Compressor::process(const float* left,const float* right,unsigned channels,
                         float* outLeft,float* outRight,std::size_t frames) noexcept {
    for(std::size_t i=0;i<frames;++i){
        const float l=finite(channels&&left?left[i]:0,0);
        const float r=finite(channels>1?(right?right[i]:0):l,0);
        if(!prepared){outLeft[i]=l;outRight[i]=r;continue;}
        // Compressor AudioParams are k-rate: .value at a suspended retarget is
        // the preceding quantum's first value. GainNode params are a-rate and
        // expose the preceding rendered sample. Preserve both anchor clocks.
        for(std::size_t p=0;p<last.size();++p){
            if(p>=5)last[p]=audioRate(p);
            else if(!(frame%128)){
                const auto age=frame-changedAt[p];
                if(age<128)last[p]=static_cast<float>(target[p]+(double(start[p])-target[p])*std::exp(-double(age)/(double(rate)*.05f)));
                else last[p]+=(target[p]-last[p])*controlStep;
                if(double(age)>10*.05f*double(rate)||std::abs(target[p]-last[p])<4.539992976248485e-05f*(target[p]==0?1:std::abs(last[p])))last[p]=target[p];
            }
        }
        if(!(frame%128)){CompressorValues values{};std::copy_n(last.begin(),7,values.begin());configure(values);}
        if(!(frame%32))envelope();
        delay[0][writeIndex]=l;delay[1][writeIndex]=r;
        const float input=std::max(std::abs(l),std::abs(r));
        const float shaped=saturate(input,knee);
        const float attenuation=input<=.0001f?1:shaped/input;
        const float dbAttenuation=std::max(2.0f,-db(attenuation));
        const float releaseRate=linear(dbAttenuation/satReleaseFrames)-1;
        detectorAverage+=(attenuation-detectorAverage)*(attenuation>detectorAverage?releaseRate:1);
        detectorAverage=finite(std::min(1.0f,detectorAverage),1);
        if(envelopeRate<1)compressorGain+=(scaledDesired-compressorGain)*envelopeRate;
        else compressorGain=std::min(1.0f,compressorGain*envelopeRate);
        const float warped=static_cast<float>(std::sin(double(piOverTwo*compressorGain)));
        const float gain=postGain*warped;
        const float realGain=db(warped);
        if(realGain<meteringGain)meteringGain=realGain;
        else meteringGain+=(realGain-meteringGain)*meteringRelease;
        // Each original GainNode and summing junction rounds to Float32.
        const float wetL=delay[0][readIndex]*gain,wetR=delay[1][readIndex]*gain;
        const float makeupL=wetL*last[5],makeupR=wetR*last[5];
        const float dryL=l*last[7],dryR=r*last[7];
        const float mixL=makeupL*last[6],mixR=makeupR*last[6];
        outLeft[i]=finite(dryL+mixL,0);outRight[i]=finite(dryR+mixR,0);
        readIndex=(readIndex+1)&1023;writeIndex=(writeIndex+1)&1023;
        ++frame;
        if(!(frame%32)){detectorAverage=flush(detectorAverage);compressorGain=flush(compressorGain);}
    }
}
}
