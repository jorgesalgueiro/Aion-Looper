// SPDX-License-Identifier: AGPL-3.0-only
#include "ArpSettings.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace aion {
namespace {
constexpr std::array<ArpControlValues,8> presets{{
 {{.4,6,.5,0,1,0,1,1,.35,0,.8}},{{.4,6,.5,0,2,0,1,1,.35,0,.8}},
 {{.3,5,.6,0,0,0,1,1,.35,0,.8}},{{.5,9,.5,1,1,0,1,1,.35,0,.8}},
 {{.4,8,.5,0,4,1,1,1,.35,.8,.8}},{{.33,7,.5,0,4,1,1,1,.35,0,.8}},
 {{.35,5,.55,0,3,0,1,1,.3,.3,.7}},{{.25,8,.45,0,5,0,1,2,.4,0,.8}}
}};
bool validControl(unsigned p,double value,bool widget) noexcept {
 if(p>=arpControls.size()||!std::isfinite(value))return false;
 const auto& d=arpControls[p];
 return value>=d.minimum&&value<=(widget?d.widgetMaximum:d.maximum)&&
        ((p!=3&&p!=5)||value==0||value==1);
}
}
bool validArpSettings(const ArpSettings& settings) noexcept {
 if(settings.preset>presets.size())return false;
 for(unsigned p=0;p<settings.values.size();++p)if(!validControl(p,settings.values[p],false))return false;
 return !settings.preset||settings.values==presets[settings.preset-1];
}
bool applyArpPreset(ArpSettings& settings,unsigned id) noexcept {
 if(!id||id>presets.size())return false;
 settings.values=presets[id-1];settings.preset=static_cast<std::uint8_t>(id);return true;
}
bool setArpControl(ArpSettings& settings,unsigned p,double value) noexcept {
 if(!validArpSettings(settings)||!validControl(p,value,true))return false;
 settings.values[p]=value;settings.preset=0;return true;
}
bool compileArpSettings(const ArpSettings& settings,double bpm,std::uint32_t seed,ArpRenderSettings& output) noexcept {
 if(!validArpSettings(settings)||!std::isfinite(bpm)||std::abs(bpm)>std::numeric_limits<float>::max())return false;
 ArpRenderSettings candidate;candidate.enabled=settings.enabled;candidate.seed=seed;
 for(unsigned p=0;p<settings.values.size();++p){
  const auto value=settings.values[p];
  // The factory uses truthiness for these two flags, including stored -0.
  candidate.values[arpControls[p].renderIndex]=static_cast<float>(p==1?value/11.1:(p==3||p==5)?(value?1.:0.):value);
 }
 // AudioParam nominal BPM range. Transport state and stored BPM are not changed.
 candidate.values[6]=static_cast<float>(std::clamp(bpm,30.,300.));
 if(!validArpRenderSettings(candidate))return false;
 output=candidate;return true;
}
bool arpRenderMask(std::uint32_t controls,std::uint32_t& output) noexcept {
 if(controls&~arpAllControls)return false;
 std::uint32_t result=controls?(1u<<6):0;
 for(unsigned p=0;p<arpControls.size();++p)if(controls&(1u<<p))result|=1u<<arpControls[p].renderIndex;
 output=result;return true;
}
}
