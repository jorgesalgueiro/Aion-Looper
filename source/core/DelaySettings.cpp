// SPDX-License-Identifier: AGPL-3.0-only
#include "DelaySettings.h"
#include <cmath>
namespace aion {
namespace {
constexpr std::array<DelayValues,9> presets{{
 {.375,4,3000,.4,0,.8,0},{.11,1,6000,.30,0,.8,0},
 {.45,5,2500,.45,0,.8,0},{.6,8,600,.6,.1,.8,0},
 {.25,10,18000,.65,0,.8,0},{.4,6,4000,.5,1.5,.9,0},
 {.08,3,1500,.25,0,.8,0},{.35,5,1800,.4,.2,.6,0},
 {.25,4,3500,.35,.5,.7,0}
}};
bool validValues(const DelayValues& v) noexcept {
 for(unsigned p=0;p<v.size();++p)
  if(!std::isfinite(v[p])||v[p]<delayControls[p].minimum||v[p]>delayControls[p].maximum)return false;
 return v[6]==0||v[6]==1;
}
}
bool validDelaySettings(const DelaySettings& s) noexcept {
 return validValues(s.values)&&s.preset<=presets.size()&&(!s.preset||s.values==presets[s.preset-1]);
}
bool applyDelayPreset(DelaySettings& s,unsigned id) noexcept {
 if(!validDelaySettings(s)||!id||id>presets.size())return false;
 s.values=presets[id-1];s.preset=static_cast<std::uint8_t>(id);return true;
}
bool setDelayControl(DelaySettings& s,unsigned p,double value) noexcept {
 if(!validDelaySettings(s)||p>=delayControls.size()||!std::isfinite(value)
  ||value<delayControls[p].minimum||value>delayControls[p].maximum
  ||(p==6&&value!=0&&value!=1))return false;
 s.values[p]=value==0?0:value;s.preset=0;return true;
}
}
