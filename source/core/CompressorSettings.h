// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "core/Compressor.h"
#include <cmath>
namespace aion {
// Stable original parameter/preset order. Zero is Custom/untouched defaults.
// Missing legacy session data must decode to these disabled settings.
struct CompressorSettings {
    CompressorValues values=compressorDefaults();
    bool enabled=false;
    std::uint8_t preset=0;
    bool operator==(const CompressorSettings&) const noexcept = default;
};
inline CompressorValues compressorPresetValues(unsigned id) noexcept {
    auto values=compressorDefaults();(void)compressorPreset(id,values);return values;
}
inline bool validCompressorSettings(const CompressorSettings& settings) noexcept {
    if(!validCompressorValues(settings.values)||settings.preset>compressorPresetNames.size())return false;
    if(!settings.preset)return true;
    CompressorValues expected{};
    return compressorPreset(settings.preset,expected)&&settings.values==expected;
}
inline constexpr std::uint32_t compressorAllParameters=(1u<<7)-1;
// Exact original range markup bounds, kept as decimal doubles before Float32
// conversion. Using the float DSP descriptors here changes release CC values.
inline constexpr std::array<std::array<double,2>,7> compressorMidiRanges{{
    {-100,0},{1,20},{0,40},{0,1},{.01,1},{0,20},{0,1}
}};
inline float compressorMidiValue(unsigned parameter,unsigned value) noexcept {
    if(parameter>=compressorMidiRanges.size()||value>127)return 0;
    const auto& range=compressorMidiRanges[parameter];
    return static_cast<float>((value/127.0)*(range[1]-range[0])+range[0]);
}
// Original display only: its symmetric quadratic knee is deliberately NOT
// used as the DSP transfer curve. UI/CLI can share this pure snapshot function.
inline double compressorDisplayCurveDb(double input,const CompressorValues& values) noexcept {
    const double threshold=values[0],ratio=values[1],knee=values[2],width=knee/2;
    if(knee>0&&input>=threshold-width&&input<=threshold+width)
        return input+((1/ratio-1)*std::pow(input-threshold+width,2))/(4*width);
    if(input>threshold+width)return threshold+(input-threshold)/ratio;
    return input;
}
}
