// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace aion {
struct DistortionControl {std::string_view name;double initial,minimum,maximum,step;};
inline constexpr std::array<DistortionControl,5> distortionControls{{
    {"amount",40,0,100,1},{"shape",.5,0,1,.01},{"tone",4000,200,20000,100},
    {"volume",.8,0,1.5,.01},{"mix",1,0,1,.01}
}};
using DistortionValues=std::array<double,distortionControls.size()>;
constexpr DistortionValues distortionDefaults() noexcept {
    DistortionValues values{};
    for(unsigned p=0;p<values.size();++p)values[p]=distortionControls[p].initial;
    return values;
}
inline constexpr std::array<std::string_view,6> distortionPresetNames{
    "default","tube-warmth","analog-edge","console-slam","bit-crush","tape-saturate"};
struct DistortionSettings {
    DistortionValues values=distortionDefaults();
    bool enabled=false;
    std::uint8_t preset=0; // Custom/untouched defaults; NOT the named "default" preset.
    bool operator==(const DistortionSettings&) const noexcept=default;
};
bool validDistortionSettings(const DistortionSettings&) noexcept;
// One-based preset IDs; failures preserve the complete old settings. Applying
// a preset preserves enable. An accepted manual edit clears the preset name,
// including equal-value edits. GUI steps do not quantize continuous MIDI values.
bool applyDistortionPreset(DistortionSettings&,unsigned preset) noexcept;
bool setDistortionControl(DistortionSettings&,unsigned parameter,double value) noexcept;

using DistortionCurve=std::array<float,4097>;
// Worker/prepare-time operation, NOT an audio-callback control setter. One owner
// prepares this fixed-storage table before publishing it immutably. Invalid
// settings leave the destination untouched. This is only makeDistortionCurve;
// it does not implement WaveShaper interpolation/4x oversampling, tone or mixing,
// and does not enable the product's T control or define a session schema.
bool prepareDistortionCurve(const DistortionSettings&,DistortionCurve&) noexcept;
}
