// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace aion {
struct ReverbControl {std::string_view name;double initial,minimum,maximum,step;};
inline constexpr std::array<ReverbControl,6> reverbControls{{
 {"volume",1,.1,3,.01},{"mix",.15,0,1,.01},{"duration",1.2,.1,5,.1},
 {"decay",1.8,.1,5,.1},{"damp",20000,500,20000,100},{"predelay",0,0,.25,.005}
}};
enum class ReverbRoom:std::uint8_t {small,studio,medium,large,cathedral,spring,plate,hall};
struct ReverbRoomInfo {std::string_view name;double duration,decay;};
inline constexpr std::array<ReverbRoomInfo,8> reverbRooms{{
 {"small",.5,.8},{"studio",1.2,1.8},{"medium",1,1},{"large",2,1.5},
 {"cathedral",3,2},{"spring",.8,2.5},{"plate",1.5,1.2},{"hall",2.5,1.8}
}};
using ReverbValues=std::array<double,6>;
constexpr ReverbValues reverbDefaults() noexcept {ReverbValues v{};for(unsigned p=0;p<v.size();++p)v[p]=reverbControls[p].initial;return v;}
inline constexpr std::array<std::string_view,17> reverbPresetNames{
 "small","studio","medium","large","hall","plate","bright-plate","spring",
 "cathedral","washed-out","tight-drum","ambient-cloud","dark-cave","dub-space",
 "vocal-room","garage","shimmer-hall"};
struct ReverbSettings {
 ReverbValues values=reverbDefaults();ReverbRoom room=ReverbRoom::studio;
 bool enabled=false;std::uint8_t preset=0;
 bool operator==(const ReverbSettings&)const noexcept=default;
};
bool validReverbSettings(const ReverbSettings&) noexcept;
bool setReverbControl(ReverbSettings&,unsigned,double) noexcept;
bool setReverbRoom(ReverbSettings&,ReverbRoom) noexcept;
bool applyReverbPreset(ReverbSettings&,unsigned) noexcept;
}
