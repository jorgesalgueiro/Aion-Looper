// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace aion {
struct CompressorParameter { std::string_view name; float initial,minimum,maximum; };
// Append-only original effect parameter order; gain is linear, not dB.
inline constexpr std::array<CompressorParameter,7> compressorParameters{{
    {"threshold",-16,-100,0},{"ratio",4,1,20},{"knee",10,0,40},
    {"attack",.01f,0,1},{"release",.15f,.01f,1},{"gain",2,0,20},{"mix",1,0,1}
}};
using CompressorValues=std::array<float,compressorParameters.size()>;
constexpr CompressorValues compressorDefaults() noexcept {
    CompressorValues result{};
    for(std::size_t i=0;i<result.size();++i)result[i]=compressorParameters[i].initial;
    return result;
}
bool validCompressorValues(const CompressorValues&) noexcept;
inline constexpr std::array<std::string_view,11> compressorPresetNames{{
    "default","modern","drums","guitar","vocals","soft","hard","sustain",
    "parallel-crush","gentle-glide","snap"
}};
// Original applyCompressorPreset computes makeup; it does NOT use preset.gain.
// IDs 1..11; zero is custom. Invalid IDs leave output untouched.
bool compressorPreset(unsigned id,CompressorValues&) noexcept;

// Fixed-storage DSP/automation kernel, used by each loop's native C effect. Adapted
// Web Audio detector/curve/envelope: see Compressor.cpp and the BSD notice.
// One owner only. Fixed storage; prepare/reset/targets/process never allocate.
// Call with 0 (silence), 1 (mono duplicated to stereo), or 2 input channels.
// Input and matching output may alias. Arbitrary subdivisions retain the
// original 128-frame parameter and 32-frame envelope clocks on the supplied
// global render timeline. Source restarts do not reset this persistent owner.
// Controls emulate original currentTime=frame/rate timestamps, including the
// one-sample ceil boundary of a-rate events after a binary time round-trip.
class Compressor {
public:
    bool prepare(double sampleRate,const CompressorValues& = compressorDefaults(),std::uint64_t firstFrame=0) noexcept;
    // Snapshot update: retarget changed fields. Explicit setTarget is a gesture
    // and restarts automation even when the requested destination repeats.
    bool setTargets(const CompressorValues&) noexcept;
    bool setTarget(std::size_t parameter,float value) noexcept;
    void reset(std::uint64_t firstFrame=0) noexcept; // Fresh graph at stored targets; clears delay/envelope.
    void process(const float* left,const float* right,unsigned channels,
                 float* outLeft,float* outRight,std::size_t frames) noexcept;
    unsigned latencyFrames() const noexcept { return delayFrames; }
    float reductionDb() const noexcept { return meteringGain; }
    const CompressorValues& targets() const noexcept { return target; }
private:
    void retarget(std::size_t,float) noexcept;
    float automationTarget(std::size_t p) const noexcept { return p<7?target[p]:1-target[6]; }
    float audioRate(std::size_t) noexcept;
    void configure(const CompressorValues&) noexcept;
    void envelope() noexcept;
    float kneeCurve(float x,float k) const noexcept;
    float saturate(float x,float k) const noexcept;
    float kAtSlope(float desired) const noexcept;
    std::array<std::array<float,1024>,2> delay{};
    CompressorValues target=compressorDefaults();
    std::array<float,8> last{},start{}; // independent dry GainNode at index 7
    std::array<std::uint64_t,8> changedAt{};
    struct AudioRateState { std::array<float,4> lane{}; float base=0,next=0; unsigned phase=0; };
    std::array<AudioRateState,3> audioRates{};
    std::array<float,3> oldBoundary{};
    std::array<bool,3> deferredBoundary{};
    std::array<float,4> smoothingSteps{};
    float controlStep=0;
    std::uint64_t frame=0;
    float rate=48000,detectorAverage=0,compressorGain=1,meteringGain=1,meteringRelease=0;
    float maxAttackDiff=-1,scaledDesired=0,envelopeRate=0;
    float threshold=-1,linearThreshold=-1,kneeDb=-1,ratio=-1,slope=-1;
    float kneeThreshold=-1,dbKneeThreshold=-1,dbYKneeThreshold=-1,knee=-1;
    float postGain=1,attackFrames=1,satReleaseFrames=1;
    std::array<float,5> releaseCurve{};
    unsigned readIndex=0,writeIndex=256,delayFrames=256;
    bool prepared=false,firstProcess=true;
};
}
