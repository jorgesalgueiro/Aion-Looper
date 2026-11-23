// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace aion {
struct DelayControl {std::string_view name;double initial,minimum,maximum,step;};
enum class DelayRepeatMode : std::uint8_t { feedback, exact };
inline constexpr unsigned delayMaximumRepeats=10;
inline constexpr std::array<DelayControl,7> delayControls{{
 {"time",.375,.01,4,.01},{"repetitions",4,0,10,1},
 {"damp",3000,200,20000,100},{"mix",.4,0,1,.01},
 {"panSpeed",0,0,5,.1},{"panDepth",.8,0,1,.1},{"sync",0,0,1,1}
}};
using DelayValues=std::array<double,7>;
inline constexpr std::array<unsigned,6> delayMidiControls{0,1,2,3,4,5};
constexpr double delayMidiValue(unsigned slider,unsigned cc) noexcept {
 const auto& p=delayControls[delayMidiControls[slider]];return p.minimum+(cc/127.0)*(p.maximum-p.minimum);
}
constexpr DelayValues delayDefaults() noexcept {
 DelayValues v{};for(unsigned p=0;p<v.size();++p)v[p]=delayControls[p].initial;return v;
}
inline constexpr std::array<std::string_view,9> delayPresetNames{
 "default","slapback","dotted8th","dub","infinite","ping-pong",
 "short-ambience","tape-echo","rhythmic"};
struct DelaySettings {
 DelayValues values=delayDefaults();bool enabled=false;std::uint8_t preset=0;
 // Additive native controls. Legacy values/presets retain feedback semantics.
 DelayRepeatMode repeatMode=DelayRepeatMode::feedback;
 unsigned repeatCount=1;
 bool operator==(const DelaySettings&) const noexcept=default;
};
bool validDelaySettings(const DelaySettings&) noexcept;
bool applyDelayPreset(DelaySettings&,unsigned) noexcept;
bool setDelayControl(DelaySettings&,unsigned,double) noexcept;
bool setDelayRepeats(DelaySettings&,DelayRepeatMode,unsigned count) noexcept;
}
