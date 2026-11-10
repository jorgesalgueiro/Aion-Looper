// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace aion {
// Original worklet parameter order (host repetitions map to feedback / 11.1).
struct ArpParameter {std::string_view name;float initial,minimum,maximum;};
inline constexpr std::array<ArpParameter,10> arpParameters{{
 {"time",.4f,.01f,4},{"feedback",.18f,0,.98f},{"mix",.5f,0,1},
 {"stay",0,0,1},{"scale",2,0,19},{"sync",0,0,1},{"bpm",120,30,300},
 {"amplitude",1,0,2},{"range",1,.25f,3},{"humanize",.35f,0,1}
}};
using ArpValues=std::array<float,arpParameters.size()>;
constexpr ArpValues arpDefaults() noexcept {
 ArpValues result{};for(std::size_t p=0;p<result.size();++p)result[p]=arpParameters[p].initial;return result;
}
bool validArpValues(const ArpValues&) noexcept;
struct ArpState {
 std::uint32_t write=0,clock=0,sequenceIndex=0;int step=0,direction=1;
 double phase=0,semitone=0,grainRate=0,grainTarget=0,envelope=0,tail=1;
 std::array<float,16> sequence{};std::uint64_t randomDraws=0;
 bool operator==(const ArpState&) const noexcept = default;
};
// Isolated original worklet kernel. Not yet a product effect/streaming adapter.
// One owner. Prepare allocates off audio; controls/process/reset never allocate.
// Each call is ONE original worklet invocation, 0..128 frames; its idle gate and
// oscillator normalization intentionally retain original block dependence.
// Inputs: 0 channels (tails), 1 (duplicate left), 2; output is always stereo.
// Parameters are already rendered Float32 AudioParam first values, not targets.
// Post-worklet StereoPanner/automation/GUI/CLI integration are separate work.
class ArpDelay {
public:
 bool prepare(double rate,const ArpValues& =arpDefaults(),std::uint32_t seed=123456789) noexcept;
 bool setValues(const ArpValues&) noexcept;
 bool reset(std::uint32_t seed=123456789) noexcept;
 bool process(const float* left,const float* right,unsigned channels,
              float* outLeft,float* outRight,std::size_t frames) noexcept;
 const ArpValues& values() const noexcept{return controls;}
 ArpState state() const noexcept{return status;}
 std::size_t capacity() const noexcept{return ring.size();}
private:
 struct Frame {float left=0,right=0;std::uint32_t generation=0;};
 struct Rotation {double c=1,s=0;void rotate(double cosine,double sine,bool normalize) noexcept;};
 double random() noexcept;
 double read(std::size_t channel,std::size_t position) const noexcept;
 std::vector<Frame> ring;
 std::array<float,1024> hann{};
 std::array<double,2> lowpass{},highpassInput{},highpassOutput{};
 std::array<Rotation,4> rotations{};
 ArpValues controls=arpDefaults();ArpState status{};
 double sampleRate=48000,wander=0,wanderTarget=0,envelopeTarget=0;
 unsigned envelopeAttack=0;std::uint32_t generation=0,randomState=0;
};
}
