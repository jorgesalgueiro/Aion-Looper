// SPDX-License-Identifier: AGPL-3.0-only
#include "Equalizer.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <numbers>

namespace aion {
double effectiveEqFrequency(float requested,double sampleRate) noexcept {
    return std::min(static_cast<double>(requested),sampleRate*.5*(1-1e-6));
}
namespace {
bool validPeak(float frequency,float q,double rate) noexcept {
    return std::isfinite(frequency)&&frequency>=10&&frequency<=22000&&std::isfinite(q)&&q>=.1f&&q<=10&&std::isfinite(rate)&&rate>=8000&&rate<=384000;
}
double peakIntegrator(float frequency,double rate) noexcept {
    const double w=std::numbers::pi/rate*effectiveEqFrequency(frequency,rate);
    return std::min(1000.0,std::sin(w)/std::cos(w));
}
}
bool eqPeakRange(float frequency,float q,double rate,double& low,double& high) noexcept {
    if(!validPeak(frequency,q,rate))return false;
    const double width=1/static_cast<double>(q),upper=(std::sqrt(4+width*width)+width)*.5;
    const double g=peakIntegrator(frequency,rate);
    low=rate/std::numbers::pi*std::atan(g/upper);
    high=rate/std::numbers::pi*std::atan(g*upper);return true;
}
bool eqPeakQ(float frequency,double edge,double rate,float& q) noexcept {
    if(!validPeak(frequency,1,rate)||!std::isfinite(edge)||edge<0||edge>=rate*.5)return false;
    const double w=std::numbers::pi/rate*edge,r=(std::sin(w)/std::cos(w))/peakIntegrator(frequency,rate);
    const double denominator=std::abs(1-r*r);
    q=denominator==0?10.f:static_cast<float>(std::clamp(r/denominator,.1,10.0));return true;
}
bool validEqValues(const EqValues& values) noexcept {
    for(std::size_t i=0;i<values.size();++i)
        if(!std::isfinite(values[i])||values[i]<eqParameters[i].minimum||values[i]>eqParameters[i].maximum)return false;
    return true;
}
bool Equalizer::calculate(double sr,const EqValues& p,Configuration& next) noexcept {
    if(!std::isfinite(sr)||sr<8000||sr>384000||!validEqValues(p))return false;
    std::array<double,10> frequencies{p[0],p[1],p[3],p[6],p[9],p[12],p[15],p[18],p[21],p[23]};
    const std::array<double,10> gains{0,p[2],p[4],p[7],p[10],p[13],p[16],p[19],p[22],0};
    const std::array<double,10> qualities{.707,.707,p[5],p[8],p[11],p[14],p[17],p[20],.707,.707};
    for(unsigned b=0;b<10;++b){
        next.bypass[b]=b!=0&&b!=9&&std::abs(gains[b])<.01;
        if(next.bypass[b])continue;
        const double a=std::pow(10.0,gains[b]/40.0);
        const double w=(std::numbers::pi/sr)*effectiveEqFrequency(static_cast<float>(frequencies[b]),sr);
        double g=std::sin(w)/std::cos(w),k=0,m0=0,m1=0,m2=0;
        if(g>1000)g=1000;
        if(b==0){k=1/qualities[b];m0=1;m1=-k;m2=-1;}
        else if(b==1){g/=std::sqrt(a);k=1/qualities[b];m0=1;m1=k*(a-1);m2=a*a-1;}
        else if(b==8){g*=std::sqrt(a);k=1/qualities[b];m0=a*a;m1=k*(1-a)*a;m2=1-a*a;}
        else if(b==9){k=1/qualities[b];m2=1;}
        else{k=1/(qualities[b]*a);m0=1;m1=k*(a*a-1);}
        const double a1=1/(1+g*(g+k)),a2=g*a1,a3=g*a2;
        next.coefficients[b]={a1,a2,a3,m0,m1,m2};
    }
    return true;
}
void Equalizer::commit(const Configuration& next) noexcept {
    for(unsigned b=0;b<10;++b)
        if(next.bypass[b]&&!configuration.bypass[b])states[b]={};
    configuration=next;
}
bool Equalizer::prepare(double sr,const EqValues& values) noexcept {
    Configuration next{};
    if(!calculate(sr,values,next))return false;
    rate=sr;configuration=next;reset();prepared=true;return true;
}
bool Equalizer::configure(const EqValues& values) noexcept {
    Configuration next{};
    if(!prepared||!calculate(rate,values,next))return false;
    commit(next);return true;
}
void Equalizer::reset() noexcept { states={}; }
bool Equalizer::magnitude(double frequency,double& amplitude) const noexcept {
    if(!prepared||!std::isfinite(frequency)||frequency<0||frequency>rate*.5)return false;
    const double w=2*std::numbers::pi/rate*frequency;
    const std::complex<double> z{std::cos(w),std::sin(w)};
    std::complex<double> response{1,0};
    for(unsigned b=0;b<10;++b){
        if(configuration.bypass[b])continue;
        const auto& c=configuration.coefficients[b];
        // State-space response of precisely the running SVF recurrence:
        // s[n+1]=A*s[n]+B*x[n], y[n]=C*s[n]+D*x[n].
        const double a11=2*c.a1-1,a12=-2*c.a2,a21=2*c.a2,a22=1-2*c.a3;
        const auto determinant=(z-a11)*(z-a22)-a12*a21;
        const auto one=((z-a22)*(2*c.a2)+a12*(2*c.a3))/determinant;
        const auto two=(a21*(2*c.a2)+(z-a11)*(2*c.a3))/determinant;
        const double d=c.m0+c.m1*c.a2+c.m2*c.a3,c1=c.m1*c.a1+c.m2*c.a2,c2=-c.m1*c.a2+c.m2*(1-c.a3);
        response*=d+c1*one+c2*two;
    }
    const double result=std::abs(response);if(!std::isfinite(result))return false;
    amplitude=result;return true;
}
void Equalizer::process(const float* left,const float* right,float* outLeft,float* outRight,std::size_t frames) noexcept {
    using namespace stereo_detail;
    for(std::size_t i=0;i<frames;++i){
        Frame sample(left?left[i]:0,right?right[i]:0);
        if(prepared){
            for(unsigned b=0;b<10;++b){
                if(configuration.bypass[b])continue;
                auto& s=states[b];const auto& c=configuration.coefficients[b];
                // Independent lanes retain scalar operation order and 1e-9 flush.
                const Frame v3=sample-s.two;
                const Frame v1=Frame(c.a1)*s.one+Frame(c.a2)*v3;
                const Frame v2=s.two+Frame(c.a2)*s.one+Frame(c.a3)*v3;
                s.one=Frame(2)*v1-s.one;s.two=Frame(2)*v2-s.two;
                s.one=select(less(absolute(s.one),Frame(1e-9)),Frame(0),s.one);
                s.two=select(less(absolute(s.two),Frame(1e-9)),Frame(0),s.two);
                sample=Frame(c.m0)*sample+Frame(c.m1)*v1+Frame(c.m2)*v2;
            }
            // A bad channel resets only its own history; retain the healthy tail.
            const auto valid=lessEqual(absolute(sample),Frame(std::numeric_limits<float>::max()));
            if(!both(valid)){
                for(auto& state:states){state.one=select(valid,state.one,Frame(0));state.two=select(valid,state.two,Frame(0));}
                sample=select(valid,sample,Frame(0));
            }
        }
        if(outLeft)outLeft[i]=static_cast<float>(sample.lane(0));
        if(outRight)outRight[i]=static_cast<float>(sample.lane(1));
    }
}

double SmoothedEqualizer::valueAt(std::size_t i) const noexcept {
    const double destination=target[i];
    if(start[i]==destination)return destination;
    return destination+(start[i]-destination)*std::exp(-static_cast<double>(frame-changedAt[i])/(rate*.05));
}
EqValues SmoothedEqualizer::currentValues() const noexcept {
    EqValues values{};for(std::size_t i=0;i<values.size();++i)values[i]=static_cast<float>(valueAt(i));return values;
}
bool SmoothedEqualizer::prepare(double sr,const EqValues& values,std::uint64_t firstFrame) noexcept {
    if(!filter.prepare(sr,values))return false;
    rate=sr;target=last=values;frame=firstFrame;changedAt.fill(frame);
    for(std::size_t i=0;i<start.size();++i)start[i]=values[i];
    initialized=true;return true;
}
bool SmoothedEqualizer::setTarget(std::size_t i,float value) noexcept {
    if(!initialized||i>=target.size()||!std::isfinite(value)||value<eqParameters[i].minimum||value>eqParameters[i].maximum)return false;
    // AudioParam.value is the LAST rendered sample, not the mathematical next
    // sample at currentTime. Actual OfflineAudioContext retargeting exposes this
    // one-sample difference. Multiple updates before rendering retain the anchor.
    const auto elapsed=frame-changedAt[i];
    const double previous=elapsed?static_cast<double>(target[i])+(start[i]-target[i])*std::exp(-static_cast<double>(elapsed-1)/(rate*.05)):start[i];
    start[i]=static_cast<float>(previous);changedAt[i]=frame;target[i]=value;return true;
}
bool SmoothedEqualizer::setTargets(const EqValues& values) noexcept {
    if(!initialized||!validEqValues(values))return false;
    for(std::size_t i=0;i<values.size();++i)(void)setTarget(i,values[i]);
    return true;
}
void SmoothedEqualizer::reset() noexcept {
    if(initialized)(void)prepare(rate,target);
}
void SmoothedEqualizer::process(const float* left,const float* right,float* outLeft,float* outRight,std::size_t frames) noexcept {
    std::size_t offset=0;
    while(offset<frames){
        if(frame%128==0){
            const auto values=currentValues();
            if(values!=last&&filter.configure(values))last=values;
        }
        const auto count=std::min(frames-offset,static_cast<std::size_t>(128-frame%128));
        filter.process(left?left+offset:nullptr,right?right+offset:nullptr,outLeft?outLeft+offset:nullptr,outRight?outRight+offset:nullptr,count);
        offset+=count;frame+=count;
    }
}
}
