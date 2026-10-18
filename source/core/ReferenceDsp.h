// Derived from the preserved Ascii-Looper DSP; see resources/ui/LICENSE.
#pragma once
#include <span>
namespace aion::reference {
// Offline finalization kernels. Full-clip scans MUST NOT run in processBlock.
void removeDc(std::span<float> data) noexcept;
bool crossfade(std::span<float> data, double sampleRate, double seconds = 0.015) noexcept;
bool mixCircular(std::span<float> base, std::span<const float> take, double sampleRate,
                 double offsetSeconds, double feedback = 0.8) noexcept;
}
