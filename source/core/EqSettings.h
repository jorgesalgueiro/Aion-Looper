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
inline constexpr std::uint32_t eqAllParameters=(1u<<24)-1;
}
