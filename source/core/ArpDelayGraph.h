// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ArpModulation.h"
namespace aion {
// Combined original worklet/50ms automation/post-pan, with owner-approved
// zero-latency causal idle wake-up. One owner; prepare/destruction OFF audio.
// No product controls, storage bank/admission or persistence are implied here.
class ArpDelayGraph {
public:
 static constexpr std::size_t maxProcessFrames=4096;
 bool prepare(double rate,const ArpModValues& =arpModDefaults(),std::uint32_t seed=123456789,std::uint64_t firstFrame=0) noexcept;
 bool reset(std::uint32_t seed=123456789,std::uint64_t firstFrame=0) noexcept;
 bool setTarget(std::size_t parameter,float value) noexcept{return prepared&&modulation.setTarget(parameter,value);}
 bool setTargets(const ArpModValues& values) noexcept{return prepared&&modulation.setTargets(values);}
 bool process(const float* left,const float* right,unsigned channels,float* outLeft,float* outRight,std::size_t frames) noexcept;
 const ArpModValues& targets() const noexcept{return modulation.targets();}
 ArpState state() const noexcept{return delay.state();}
 std::uint64_t position() const noexcept{return modulation.position();}
 unsigned latencyFrames() const noexcept{return 0;}
private:
 ArpDelay delay;
 ArpModulation modulation;
 std::array<float,128> pan{};
 bool prepared=false,freshWorklet=true;
};
}
