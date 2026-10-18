// Derived from the preserved Ascii-Looper DSP; see resources/ui/LICENSE.
#include "core/ReferenceDsp.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
namespace aion::reference {
bool normalize(std::span<float> left,std::span<float> right) noexcept {
    double peak=0;
    for(auto channel:{left,right})for(float value:channel)peak=std::max(peak,std::abs(static_cast<double>(value)));
    if(peak<=0.0001||std::abs(peak-1.0)<=0.0001)return false;
    const double ratio=1.0/peak;
    for(auto channel:{left,right})for(float& value:channel)value=static_cast<float>(static_cast<double>(value)*ratio);
    return true;
}
void removeDc(std::span<float> data) noexcept {
    if(data.empty())return;
    double sum=0;for(float sample:data)sum+=sample;
    const double mean=sum/static_cast<double>(data.size());
    for(float& sample:data)sample=static_cast<float>(static_cast<double>(sample)-mean);
}
bool crossfade(std::span<float> data,double sampleRate,double seconds) noexcept {
    if(!std::isfinite(sampleRate)||sampleRate<=0||!std::isfinite(seconds)||seconds<0)return false;
    const auto length=static_cast<std::int64_t>(data.size());
    if(length<4)return true;
    const auto fade=static_cast<std::int64_t>(std::min(std::floor(sampleRate*(seconds==0?0.015:seconds)),std::floor(length*0.2)));
    if(fade<2)return true;
    const auto search=std::min(static_cast<std::int64_t>(std::floor(fade*0.25)),std::int64_t{256});
    std::int64_t head=0,tail=length;
    for(std::int64_t i=1;i<search&&i<length;++i)if(data[i-1]<=0&&data[i]>0){head=i;break;}
    for(std::int64_t i=length-2;i>length-1-search&&i>0;--i)if(data[i-1]<=0&&data[i]>0){tail=i;break;}
    const auto overlap=std::min(fade,tail-head);
    if(overlap<2)return true;
    const auto start=tail-overlap;
    constexpr double halfPi=std::numbers::pi/2;
    for(std::int64_t i=0;i<overlap;++i){
        const double progress=static_cast<double>(i)/overlap;
        const double in=std::sqrt(std::sin(halfPi*progress)),out=std::sqrt(std::cos(halfPi*progress));
        data[head+i]=static_cast<float>(data[head+i]*in+data[start+i]*out);
    }
    const auto edge=std::min(std::int64_t{8},static_cast<std::int64_t>(std::floor(overlap*0.1)));
    for(std::int64_t i=0;i<edge;++i){
        const double gain=0.5-0.5*std::cos(std::numbers::pi*i/edge);
        data[i]=static_cast<float>(data[i]*gain);data[length-1-i]=static_cast<float>(data[length-1-i]*gain);
    }
    return true;
}
bool mixCircular(std::span<float> base,std::span<const float> take,double sampleRate,double offset,double feedback) noexcept {
    if(!std::isfinite(sampleRate)||sampleRate<=0||!std::isfinite(offset)||!std::isfinite(feedback)||feedback<0||feedback>1)return false;
    if(base.empty()||take.empty())return true;
    const double start=std::floor(offset*sampleRate);
    if(!std::isfinite(start))return false;
    auto position=static_cast<std::size_t>(std::fmod(std::fmod(start,static_cast<double>(base.size()))+base.size(),static_cast<double>(base.size())));
    const auto fade=static_cast<std::size_t>(std::max(1.0,std::min(std::floor(sampleRate*0.01),std::floor(take.size()*0.1))));
    constexpr double halfPi=std::numbers::pi/2;
    for(std::size_t i=0;i<take.size();++i){
        double old=feedback,added=1;
        std::size_t fadeIndex=0;bool fading=false;
        if(i<fade){fadeIndex=i;fading=true;}
        else if(i>=take.size()-fade){fadeIndex=take.size()-1-i;fading=true;}
        if(fading){
            const double progress=static_cast<double>(fadeIndex)/fade;
            // Reference stores these tables in Float32Array before mixing in JS doubles.
            old=static_cast<float>(feedback+(1-feedback)*std::cos(progress*halfPi));
            added=static_cast<float>(std::sin(progress*halfPi));
        }
        double sum=base[position]*old+take[i]*added;
        if(sum>0.95)sum=0.95+0.05*std::tanh((sum-0.95)*10);
        else if(sum< -0.95)sum= -0.95+0.05*std::tanh((sum+0.95)*10);
        base[position]=static_cast<float>(sum);
        if(++position==base.size())position=0;
    }
    return true;
}
}
