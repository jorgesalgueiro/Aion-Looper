// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "core/Equalizer.h"
namespace aion {
// Stored preset IDs are append-only. Zero means Custom/default, not bypass.
inline constexpr std::array<std::string_view,10> eqPresetNames{
    "Drums","Low End Clean","Vocal Presence","De-Ess & Clean","Guitar",
    "Master Air","Bass","Radio","Synth Pad","Telephone"};
struct EqSettings {
    EqValues values=eqDefaults();
    bool enabled=false;
    std::uint8_t preset=0;
    bool operator==(const EqSettings&) const noexcept = default;
};
EqValues eqPresetValues(unsigned id) noexcept;
bool validEqSettings(const EqSettings&) noexcept;
// Original range-widget bounds, rather than the wider stored/DSP domain.
inline constexpr std::array<std::array<double,2>,24> eqMidiRanges{{
    {20,500},{40,1000},{-24,24},{60,4000},{-24,24},{.1,10},
    {100,4000},{-24,24},{.1,10},{200,6000},{-24,24},{.1,10},
    {400,8000},{-24,24},{.1,10},{1000,12000},{-24,24},{.1,10},
    {2000,16000},{-24,24},{.1,10},{1500,20000},{-24,24},{3000,22000}}};
inline float eqMidiValue(unsigned parameter,unsigned value) noexcept {
    if(parameter>=eqMidiRanges.size()||value>127)return 0;
    const auto& range=eqMidiRanges[parameter];
    return static_cast<float>((value/127.0)*(range[1]-range[0])+range[0]);
}
inline constexpr std::uint32_t eqAllParameters=(1u<<24)-1;
}
