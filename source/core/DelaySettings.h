// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace aion {
struct DelayControl {std::string_view name;double initial,minimum,maximum,step;};
inline constexpr std::array<DelayControl,7> delayControls{{
 {"time",.375,.01,4,.01},{"repetitions",4,0,10,1},
 {"damp",3000,200,20000,100},{"mix",.4,0,1,.01},
 {"panSpeed",0,0,5,.1},{"panDepth",.8,0,1,.1},{"sync",0,0,1,1}
}};
using DelayValues=std::array<double,7>;
constexpr DelayValues delayDefaults() noexcept {
 DelayValues v{};for(unsigned p=0;p<v.size();++p)v[p]=delayControls[p].initial;return v;
}
inline constexpr std::array<std::string_view,9> delayPresetNames{
 "default","slapback","dotted8th","dub","infinite","ping-pong",
 "short-ambience","tape-echo","rhythmic"};
struct DelaySettings {
 DelayValues values=delayDefaults();bool enabled=false;std::uint8_t preset=0;
 bool operator==(const DelaySettings&) const noexcept=default;
};
bool validDelaySettings(const DelaySettings&) noexcept;
bool applyDelayPreset(DelaySettings&,unsigned) noexcept;
bool setDelayControl(DelaySettings&,unsigned,double) noexcept;
}
