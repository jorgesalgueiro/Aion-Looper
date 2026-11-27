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
// Machine plate family: smooth, bright, big, vocal, drum, dark, shimmer.
constexpr std::array<ReverbValues,7> platePresets{{
 {4,14000,70,.02,.35,.5},{3,18000,60,.01,.4,.6},{7,12000,50,.03,.45,.5},
 {2.5,13000,100,.02,.3,.8},{1.5,16000,120,.005,.25,.7},{5,6000,80,.04,.4,.4},
 {8,16000,120,.05,.5,.6}
}};
}
bool validReverbSettings(const ReverbSettings& s) noexcept {
 if(static_cast<unsigned>(s.room)>=reverbRooms.size())return false;
 if(s.engine!=ReverbEngine::convolution&&s.engine!=ReverbEngine::machine)return false;
 for(unsigned p=0;p<s.values.size();++p)if(!std::isfinite(s.values[p])||s.values[p]<reverbControls[p].minimum||s.values[p]>reverbControls[p].maximum)return false;
 for(unsigned p=0;p<s.plate.size();++p)if(!std::isfinite(s.plate[p])||s.plate[p]<plateControls[p].minimum||s.plate[p]>plateControls[p].maximum)return false;
 if(s.engine==ReverbEngine::machine){
  if(s.preset>platePresets.size())return false;
  return !s.preset||s.plate==platePresets[s.preset-1];
 }
 if(s.preset>presets.size())return false;
 return !s.preset||(s.room==presets[s.preset-1].room&&s.values==presets[s.preset-1].values);
}
bool setReverbControl(ReverbSettings& s,unsigned p,double value) noexcept {
 if(!validReverbSettings(s))return false;
 if(s.engine==ReverbEngine::machine)return setPlateControl(s,p,value);
 if(p>=s.values.size()||!std::isfinite(value)||value<reverbControls[p].minimum||value>reverbControls[p].maximum)return false;
 s.values[p]=value==0?0:value;s.preset=0;return true;
}
bool setPlateControl(ReverbSettings& s,unsigned p,double value) noexcept {
 if(!validReverbSettings(s)||s.engine!=ReverbEngine::machine||p>=s.plate.size()||!std::isfinite(value)||value<plateControls[p].minimum||value>plateControls[p].maximum)return false;
 s.plate[p]=value==0?0:value;s.preset=0;return true;
}
bool setReverbRoom(ReverbSettings& s,ReverbRoom room) noexcept {
 if(!validReverbSettings(s)||s.engine!=ReverbEngine::convolution||static_cast<unsigned>(room)>=reverbRooms.size())return false;
 const auto& r=reverbRooms[static_cast<unsigned>(room)];s.room=room;s.values[2]=r.duration;s.values[3]=r.decay;s.preset=0;return true;
}
bool applyReverbPreset(ReverbSettings& s,unsigned id) noexcept {
 if(!validReverbSettings(s)||!id)return false;
 if(id<=presets.size()){
  const auto& p=presets[id-1];s.engine=ReverbEngine::convolution;s.values=p.values;s.room=p.room;s.preset=static_cast<std::uint8_t>(id);return true;
 }
 if(id<=presets.size()+platePresets.size()){
  s.engine=ReverbEngine::machine;s.plate=platePresets[id-presets.size()-1];s.preset=static_cast<std::uint8_t>(id-presets.size());return true;
 }
 return false;
}
bool applyPlatePreset(ReverbSettings& s,unsigned id) noexcept {
 if(!id||id>platePresets.size())return false;
 return applyReverbPreset(s,static_cast<unsigned>(presets.size())+id);
}
}
