// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace aion {
// Stable names/defaults/ranges from the original eq-processor AudioParams.
// This is the DSP kernel, not an enabled product effect or a second state owner.
struct EqParameter {
    std::string_view name;
    float initial, minimum, maximum;
};
inline constexpr std::array<EqParameter,24> eqParameters{{
    {"lcFreq",20,10,22000}, {"lsFreq",100,10,22000}, {"lsGain",0,-24,24},
    {"p1Freq",1000,10,22000}, {"p1Gain",0,-24,24}, {"p1Q",.707f,.1f,10},
    {"p2Freq",500,10,22000}, {"p2Gain",0,-24,24}, {"p2Q",.707f,.1f,10},
    {"p3Freq",1000,10,22000}, {"p3Gain",0,-24,24}, {"p3Q",.707f,.1f,10},
    {"p4Freq",2000,10,22000}, {"p4Gain",0,-24,24}, {"p4Q",.707f,.1f,10},
    {"p5Freq",4000,10,22000}, {"p5Gain",0,-24,24}, {"p5Q",.707f,.1f,10},
    {"p6Freq",8000,10,22000}, {"p6Gain",0,-24,24}, {"p6Q",.707f,.1f,10},
    {"hsFreq",5000,10,22000}, {"hsGain",0,-24,24}, {"hcFreq",20000,10,22000}
}};
using EqValues = std::array<float,eqParameters.size()>;
constexpr EqValues eqDefaults() noexcept {
    EqValues values{};
    for(std::size_t i=0;i<values.size();++i)values[i]=eqParameters[i].initial;
    return values;
}
bool validEqValues(const EqValues&) noexcept;
// Owner-approved low-rate repair. Stored values remain intact; only the active
// coefficient frequency is limited. Normal-rate settings remain unchanged.
double effectiveEqFrequency(float requested,double sampleRate) noexcept;
// Peak half-gain bandwidth in the same bilinear frequency mapping as the DSP.
// For graph handles: display both edges; dragging either edge sets the same Q.
bool eqPeakRange(float frequency,float q,double sampleRate,double& low,double& high) noexcept;
bool eqPeakQ(float frequency,double edge,double sampleRate,float& q) noexcept;

// Coefficient updates are explicit: the caller samples AudioParams at the start
// of each original 128-frame render quantum, independent of device block size.
// Storage is fixed; construction, configuration, reset and processing allocate
// nothing. Absent input channels receive zero while their filter tails continue.
class Equalizer {
public:
    // A failed prepare/configure is atomic: it preserves the complete old filter.
    bool prepare(double sampleRate, const EqValues& = eqDefaults()) noexcept;
    bool configure(const EqValues&) noexcept;
    void reset() noexcept;
    void process(const float* left, const float* right, float* outLeft,
                 float* outRight, std::size_t frames) noexcept;
    bool ready() const noexcept { return prepared; }
    // Message/UI-side curve evaluation; no PCM scan, FFT or audio-thread traffic.
    bool magnitude(double frequency,double& amplitude) const noexcept;
private:
    struct Coefficients { double a1=0,a2=0,a3=0,m0=0,m1=0,m2=0; };
    struct State { double one=0,two=0; };
    struct Configuration {
        std::array<Coefficients,10> coefficients{};
        std::array<bool,10> bypass{};
    } configuration{};
    static bool calculate(double,const EqValues&,Configuration&) noexcept;
    void commit(const Configuration&) noexcept;
    double tick(unsigned channel,double input) noexcept;
    std::array<std::array<State,10>,2> states{};
    double rate=48000;
    bool prepared=false;
};

// Original smoothSetParam: a 50 ms exponential target, Float32 AudioParam
// samples, coefficients held for each 128-frame worklet quantum. Controls may
// arrive at any sample; device block subdivision does not change that clock.
// This owns only filter/automation state, not product presets or transport.
class SmoothedEqualizer {
public:
    bool prepare(double sampleRate,const EqValues& = eqDefaults()) noexcept;
    bool setTarget(std::size_t parameter,float value) noexcept;
    bool setTargets(const EqValues&) noexcept;
    void reset() noexcept; // Reconstruct worklet/AudioParams at stored targets.
    void process(const float*,const float*,float*,float*,std::size_t frames) noexcept;
    EqValues currentValues() const noexcept;
    const EqValues& targets() const noexcept { return target; }
private:
    double valueAt(std::size_t) const noexcept;
    Equalizer filter{};
    EqValues target=eqDefaults(),last=eqDefaults();
    std::array<double,24> start{};
    std::array<std::uint64_t,24> changedAt{};
    std::uint64_t frame=0;
    double rate=48000;
    bool initialized=false;
};
}
