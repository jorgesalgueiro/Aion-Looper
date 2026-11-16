// SPDX-License-Identifier: AGPL-3.0-only
#include "DistortionSettings.h"
#include <cmath>

namespace aion {
namespace {
constexpr std::array<DistortionValues,6> presets{{
    {35,.5,5000,.8,1},{20,.5,3000,.9,.8},{50,.5,6000,.75,1},
    {80,.5,12000,.6,1},{60,.8,4000,.7,.9},{15,.3,5500,.95,.7}
}};
bool validValues(const DistortionValues& values) noexcept {
    for(unsigned p=0;p<values.size();++p)
        if(!std::isfinite(values[p])||values[p]<distortionControls[p].minimum
            ||values[p]>distortionControls[p].maximum)return false;
    return true;
}
}
bool validDistortionSettings(const DistortionSettings& s) noexcept {
    return validValues(s.values)&&s.preset<=presets.size()
        &&(!s.preset||s.values==presets[s.preset-1]);
}
bool applyDistortionPreset(DistortionSettings& s,unsigned preset) noexcept {
    if(!validDistortionSettings(s)||!preset||preset>presets.size())return false;
    s.values=presets[preset-1];s.preset=static_cast<std::uint8_t>(preset);
    return true;
}
bool setDistortionControl(DistortionSettings& s,unsigned p,double value) noexcept {
    if(!validDistortionSettings(s)||p>=distortionControls.size()||!std::isfinite(value)
        ||value<distortionControls[p].minimum||value>distortionControls[p].maximum)return false;
    s.values[p]=value==0?0:value;s.preset=0;return true;
}
bool prepareDistortionCurve(const DistortionSettings& s,DistortionCurve& out) noexcept {
    if(!validDistortionSettings(s))return false;
    // Preserve the reference's double arithmetic before Float32 conversion.
    // Amount zero still applies tanh; it is NOT an effect bypass.
    const double k=s.values[0]*.5,drive=1+k*.5;
    const double driveNeg=drive*(1-s.values[1]*.5);
    for(unsigned i=0;i<out.size();++i){
        const double x=i*2./(out.size()-1)-1;
        out[i]=static_cast<float>(x>=0?std::tanh(drive*x):std::tanh(driveNeg*x));
    }
    out[(out.size()-1)/2]=0;return true;
}
}
