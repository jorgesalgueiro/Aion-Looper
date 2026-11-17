// SPDX-License-Identifier: AGPL-3.0-only
#include "DriveSettings.h"
#include <cmath>
namespace aion {
namespace {
constexpr std::array<DistortionControl,5> overdrive{{{"drive",8,0,50,1},{"",0,0,0,0},{"tone",6000,100,10000,10},{"volume",.9,0,1.5,.01},{"mix",1,0,1,.01}}};
constexpr std::array<DistortionControl,5> fuzz{{{"gain",45,1,50,1},{"bias",0,-.5,.5,.01},{"tone",3000,100,10000,10},{"volume",.5,0,1.5,.01},{"mix",1,0,1,.01}}};
constexpr DrivePreset distortionPresets[]{
 {"default",{35,.5,5000,.8,1}},{"tube-warmth",{20,.5,3000,.9,.8}},
 {"analog-edge",{50,.5,6000,.75,1}},{"console-slam",{80,.5,12000,.6,1}},
 {"bit-crush",{60,.8,4000,.7,.9}},{"tape-saturate",{15,.3,5500,.95,.7}}};
constexpr DrivePreset overdrivePresets[]{
 {"Clean Boost",{3,0,8000,1,1}},{"American Sparkle",{6,0,7000,1,1}},
 {"British Crunch",{15,0,4000,.9,1}},{"Thick Overdrive",{25,0,1000,.9,1}},
 {"Blues Breaker",{10,0,3500,.95,1}},{"Lead Boost",{20,0,5000,.85,1}}};
constexpr DrivePreset fuzzPresets[]{
 {"Vintage Fuzz",{25,0,5000,.5,1}},{"Big Muff",{45,0,800,.8,1}},
 {"Velcro Sputter",{50,.4,5000,.9,1}},{"Drone Fuzz",{50,-.3,400,.8,1}},
 {"Fuzz Face",{35,.1,3000,.7,.9}}};
bool kindValid(DriveKind k){return static_cast<unsigned>(k)<3;}
}
std::span<const DistortionControl,5> driveControls(DriveKind k)noexcept{
 return k==DriveKind::overdrive?std::span<const DistortionControl,5>(overdrive):k==DriveKind::fuzz?std::span<const DistortionControl,5>(fuzz):std::span<const DistortionControl,5>(distortionControls);
}
std::span<const DrivePreset> drivePresets(DriveKind k)noexcept{
 if(k==DriveKind::distortion)return distortionPresets;
 if(k==DriveKind::overdrive)return overdrivePresets;
 if(k==DriveKind::fuzz)return fuzzPresets;
 return {};
}
DriveSettings driveDefaults(DriveKind k)noexcept{DriveSettings s;const auto controls=driveControls(k);for(unsigned p=0;p<5;++p)s.values[p]=controls[p].initial;return s;}
bool validDriveSettings(DriveKind k,const DriveSettings& s)noexcept{
 if(!kindValid(k))return false;
 const auto controls=driveControls(k);
 for(unsigned p=0;p<5;++p)if(!std::isfinite(s.values[p])||s.values[p]<controls[p].minimum||s.values[p]>controls[p].maximum)return false;
 const auto presets=drivePresets(k);return s.preset<=presets.size()&&(!s.preset||s.values==presets[s.preset-1].values);
}
bool setDriveControl(DriveKind k,DriveSettings& s,unsigned p,double value)noexcept{
 if(!kindValid(k)||p>=5||driveControls(k)[p].name.empty())return false;
 auto next=s;next.preset=0;next.values[p]=value;if(!validDriveSettings(k,next))return false;s=next;return true;
}
bool applyDrivePreset(DriveKind k,DriveSettings& s,unsigned p)noexcept{
 const auto presets=drivePresets(k);if(!p||p>presets.size())return false;auto next=s;next.values=presets[p-1].values;next.preset=static_cast<std::uint8_t>(p);if(!validDriveSettings(k,next))return false;s=next;return true;
}
}
