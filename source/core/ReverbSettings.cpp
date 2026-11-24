// SPDX-License-Identifier: AGPL-3.0-only
#include "ReverbSettings.h"
#include <cmath>
namespace aion {
namespace {
struct Preset {ReverbRoom room;ReverbValues values;};
using R=ReverbRoom;
// Resolved original room-first, then explicit preset overrides. Controls stay
// double precision; room labels do not claim physical spring/plate modelling.
constexpr std::array<Preset,17> presets{{
 {R::small,{1,.2,.5,.8,12000,0}},{R::studio,{1,.25,1.2,1.8,9000,.01}},
 {R::medium,{1,.35,1,1,8000,.015}},{R::large,{1,.45,2,1.5,6000,.03}},
 {R::hall,{1,.5,2.5,1.8,5500,.04}},{R::plate,{1,.4,1.5,1.2,13000,.005}},
 {R::plate,{1.1,.45,2,1.2,18000,.008}},{R::spring,{1,.3,.8,2.5,7000,0}},
 {R::cathedral,{1.2,.6,3,2,4500,.06}},{R::large,{.9,1,2,1.5,3500,.02}},
 {R::small,{1,.15,.4,.8,10000,.005}},{R::cathedral,{1.1,.8,4.5,2.4,3000,.08}},
 {R::hall,{1.15,.7,3.5,2.8,1500,.07}},{R::hall,{1,.55,2.8,1.8,2500,.09}},
 {R::medium,{1,.2,.8,1,10000,.02}},{R::large,{1,.35,1.5,1.5,4000,.03}},
 {R::cathedral,{1.3,.65,3.5,2,8000,.05}}
}};
}
bool validReverbSettings(const ReverbSettings& s) noexcept {
 if(static_cast<unsigned>(s.room)>=reverbRooms.size()||s.preset>presets.size())return false;
 for(unsigned p=0;p<s.values.size();++p)if(!std::isfinite(s.values[p])||s.values[p]<reverbControls[p].minimum||s.values[p]>reverbControls[p].maximum)return false;
 return !s.preset||(s.room==presets[s.preset-1].room&&s.values==presets[s.preset-1].values);
}
bool setReverbControl(ReverbSettings& s,unsigned p,double value) noexcept {
 if(!validReverbSettings(s)||p>=s.values.size()||!std::isfinite(value)||value<reverbControls[p].minimum||value>reverbControls[p].maximum)return false;
 s.values[p]=value==0?0:value;s.preset=0;return true;
}
bool setReverbRoom(ReverbSettings& s,ReverbRoom room) noexcept {
 if(!validReverbSettings(s)||static_cast<unsigned>(room)>=reverbRooms.size())return false;
 const auto& r=reverbRooms[static_cast<unsigned>(room)];s.room=room;s.values[2]=r.duration;s.values[3]=r.decay;s.preset=0;return true;
}
bool applyReverbPreset(ReverbSettings& s,unsigned id) noexcept {
 if(!validReverbSettings(s)||!id||id>presets.size())return false;
 const auto& p=presets[id-1];s.values=p.values;s.room=p.room;s.preset=static_cast<std::uint8_t>(id);return true;
}
}
