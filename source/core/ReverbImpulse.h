// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ReverbSettings.h"
#include <atomic>
#include <span>
#include <vector>
namespace aion {
// Worker-only replacement builder. Never prepare, mutate or destroy a published
// impulse while audio borrows it. No product convolution owner is connected yet.
class ReverbImpulse {
public:
 ReverbImpulse()=default;
 ReverbImpulse(const ReverbImpulse&)=delete;ReverbImpulse& operator=(const ReverbImpulse&)=delete;
 static std::size_t requiredFrames(double sampleRate,const ReverbSettings&) noexcept;
 static std::size_t requiredBytes(double sampleRate,const ReverbSettings& s) noexcept {return requiredFrames(sampleRate,s)*2*sizeof(float);}
 bool prepare(double sampleRate,const ReverbSettings&,std::uint32_t seed,const std::atomic<bool>* cancelled=nullptr) noexcept;
 std::span<const float> channel(unsigned c) const noexcept {return c<2?std::span<const float>(samples[c]):std::span<const float>{};}
 double sampleRate() const noexcept{return rate;}
 std::uint32_t seed() const noexcept{return noiseSeed;}
private:
 std::array<std::vector<float>,2> samples;
 double rate=0,duration=0,decay=0;std::uint32_t noiseSeed=0;
};
}
