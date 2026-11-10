// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ArpModulation.h"
namespace aion {
// Internal render targets, not the original UI/preset/persistence representation.
// The factory maps repetitions to feedback, unlike the raw worklet's .18 default.
// Organic new-instance voicing and seed selection remain product integration.
constexpr ArpModValues arpFactoryTargets() noexcept {
 auto values=arpModDefaults();values[1]=static_cast<float>(2.0/11.1);return values;
}
struct ArpRenderSettings {
 ArpModValues values=arpFactoryTargets();
 std::uint32_t seed=123456789; // Explicit DSP identity, not a product random policy.
 bool enabled=false;
 bool operator==(const ArpRenderSettings&) const noexcept = default;
};
inline bool validArpRenderSettings(const ArpRenderSettings& settings) noexcept {return validArpModValues(settings.values);}
inline constexpr std::uint32_t arpAllParameters=(1u<<12)-1;
}
