// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ArpDelay.h"

namespace aion {
// DSP-only parameter order: the original ten worklet AudioParams, then the
// factory's Oscillator frequency and Gain gain. Not new product parameter IDs.
using ArpModValues=std::array<float,12>;
constexpr ArpModValues arpModDefaults() noexcept {
 ArpModValues v{};const auto worklet=arpDefaults();
 for(std::size_t p=0;p<worklet.size();++p)v[p]=worklet[p];
 v[11]=.8f;return v;
}
bool validArpModValues(const ArpModValues&) noexcept;
struct ArpControlFrame {
 ArpModValues parameters{}; // actual a-rate values for this sample
 ArpValues worklet{}; // first a-rate value latched on the 128-frame clock
 float pan=0; // sine LFO * smoothed pan depth, as a StereoPanner AudioParam
};
// Fixed-storage, single-owner automation and post-worklet stereo pan. No delay
// kernel/streaming idle policy, GUI/CLI/state integration or organic voicing yet.
// Prepare/reset start a fresh oscillator; ordinary controls preserve its phase.
// Repeated setTarget is a gesture; setTargets retargets changed fields only.
class ArpModulation {
public:
 bool prepare(double rate,const ArpModValues& =arpModDefaults(),std::uint64_t firstFrame=0) noexcept;
 bool setTarget(std::size_t parameter,float value) noexcept;
 bool setTargets(const ArpModValues&) noexcept;
 void reset(std::uint64_t firstFrame=0) noexcept;
 bool next(ArpControlFrame&) noexcept;
 static void panStereo(float left,float right,float pan,float& outLeft,float& outRight) noexcept;
 const ArpModValues& targets() const noexcept{return target;}
 std::uint64_t position() const noexcept{return frame;}
private:
 float audioRate(std::size_t) noexcept;
 float oscillator(float frequency) noexcept;
 void retarget(std::size_t,float) noexcept;
 struct Ramp {std::array<float,4> lane{};float base=0,next=0;unsigned phase=0,scalarFrom=128;};
 ArpModValues target=arpModDefaults(),last{},start{},oldBoundary{};
 std::array<Ramp,12> ramps{};
 std::array<std::uint64_t,12> changedAt{};
 std::array<bool,12> deferred{};
 std::array<bool,12> settled{}; // Exact constant lane; each new gesture invalidates it.
 std::array<float,4> steps{};
 ArpValues latched{};
 double sampleRate=48000,phase=0;
 std::array<float,4> oscillatorLanes{};
 unsigned waveSize=4096;
 float rateScale=0;
 const float* sine=nullptr; // assigned only by off-audio prepare; static lifetime
 std::uint64_t frame=0;
 bool prepared=false,first=true,frequencyAutomated=false,oscillatorFresh=true;
};
}
