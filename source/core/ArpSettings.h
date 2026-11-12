// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ArpRenderSettings.h"
namespace aion {
// Original control/preset representation. Keep JS decimal values as doubles;
// repetitions / 11.1 happens BEFORE Float32 conversion at the DSP boundary.
struct ArpControlParameter {std::string_view name;double initial,minimum,maximum,widgetMaximum;unsigned renderIndex;};
inline constexpr std::array<ArpControlParameter,11> arpControls{{
 {"time",.4,.01,4,2,0},{"repetitions",2,0,10,10,1},{"mix",.5,0,1,1,2},
 {"stay",0,0,1,1,3},{"scale",2,0,19,19,4},{"sync",0,0,1,1,5},
 {"amplitude",1,0,2,1.5,7},{"range",1,.25,3,3,8},{"humanize",.35,0,1,1,9},
 {"panSpeed",0,0,5,5,10},{"panDepth",.8,0,1,1,11}
}};
using ArpControlValues=std::array<double,arpControls.size()>;
constexpr ArpControlValues arpControlDefaults() noexcept {
 ArpControlValues result{};for(unsigned p=0;p<result.size();++p)result[p]=arpControls[p].initial;return result;
}
inline constexpr std::array<std::string_view,8> arpPresetNames{
 "maj-chord","min-chord","angel-oct","frozen","swirling","sync-trip","pentatonic-rise","chromatic-descent"};
inline constexpr std::array<std::string_view,20> arpScaleNames{
 "Octaves","Major","Minor","PentMaj","PentMin","WholeTone","Diminish","Octatonic","Aeolian","Ionian",
 "Maj7","Min7","4ths/5ths","SuperOct","FifthsUp","Chrom","Micro1","Micro2","Wide","Tritones"};
struct ArpSettings {
 ArpControlValues values=arpControlDefaults();
 bool enabled=false;
 std::uint8_t preset=0; // Zero is Custom/reference defaults; original names map to 1..8.
 bool operator==(const ArpSettings&) const noexcept = default;
};
inline constexpr std::uint32_t arpAllControls=(1u<<11)-1;
bool validArpSettings(const ArpSettings&) noexcept;
bool applyArpPreset(ArpSettings&,unsigned id) noexcept; // Failure leaves the old object intact.
bool setArpControl(ArpSettings&,unsigned parameter,double value) noexcept;
bool compileArpSettings(const ArpSettings&,double bpm,std::uint32_t seed,ArpRenderSettings&) noexcept;
// Every original parameter edit, even pan or an equal-value edit, retargets BPM.
// A zero mask denotes no parameter gesture, e.g. a standalone enabled toggle.
bool arpRenderMask(std::uint32_t controlMask,std::uint32_t& renderMask) noexcept;
}
