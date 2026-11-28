// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <atomic>
#include <cstddef>
#include <memory>
#include <span>
namespace aion {
// Native plate-reverb engine ("Machine" mode). A dense eight-line feedback
// network with input diffusion, damping, low cut and gentle delay modulation
// for a smooth, bright, large tail. The dry-aware carver listens to the dry
// signal and applies dynamic peaking cuts to the wet path where the dry has
// energy, so a lush reverb does not smear consonants. Original implementation;
// not derived from any proprietary product algorithm.
struct PlateSettings {
 bool enabled=false;
 float decay=2.5f;      // seconds, 0.1..10
 float damping=12000.f; // Hz, 500..20000
 float lowCut=80.f;     // Hz, 20..500
 float predelay=0.02f;  // seconds, 0..0.1
 float mix=0.35f;       // 0..1
 float unmask=0.5f;     // 0..1 depth of dry-aware carving
};
bool validPlateSettings(const PlateSettings&) noexcept;
class PlateReverb {
public:
 PlateReverb() noexcept;
 ~PlateReverb();
 PlateReverb(const PlateReverb&)=delete;
 PlateReverb& operator=(const PlateReverb&)=delete;
 // Allocation only here (worker thread). Cancellation
 // preserves the previous prepared processor.
 bool prepare(double sampleRate,const PlateSettings&,const std::atomic<bool>* cancelled=nullptr) noexcept;
 // Live controls; no allocation, no FFT/graph rebuild. Invalid edits reject atomically.
 bool set(const PlateSettings&) noexcept;
 // Zero-latency scalar block path. One audio owner, no locks.
 bool process(std::span<const float> left,std::span<const float> right,
              std::span<float> outLeft,std::span<float> outRight) noexcept;
 void reset() noexcept; // Fixed metadata/filter work; delay memory is invalidated lazily.
 const PlateSettings& settings() const noexcept;
 double sampleRate() const noexcept;
 static std::size_t preparationBytes(double,const PlateSettings&) noexcept;
private:
 struct State;
 std::unique_ptr<State> state;
};
}
