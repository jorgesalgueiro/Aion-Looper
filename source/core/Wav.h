#pragma once
#include <cstdint>
#include <span>
#include <vector>
namespace aion {
// Worker/message thread only. Matches the original AudioEngine.bufferToWAV encoder.
std::vector<std::uint8_t> encodeWav(std::span<const float> left, std::span<const float> right, double sampleRate, unsigned channels=2);
struct WavExport { std::vector<std::uint8_t> bytes; };
}
