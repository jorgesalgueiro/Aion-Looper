#pragma once
#include <cmath>
#include <cstdint>
#include <type_traits>
namespace aion {
// One host observation at the START of a callback. Missing fields stay explicit.
// This is transport input/telemetry, not arbitration or a sync scheduler.
struct Transport {
    double bpm=120.0,ppq=0.0;
    bool playing=false,hostValid=false;
    std::uint64_t callbackFrame=0;
    std::int64_t samplePosition=0;
    double seconds=0,barStartPpq=0,loopStartPpq=0,loopEndPpq=0;
    int numerator=0,denominator=0;
    bool recording=false,looping=false;
    bool hasBpm=false,hasPpq=false,hasSamples=false,hasSeconds=false;
    bool hasTimeSignature=false,hasBarStart=false,hasLoopPoints=false;
};
static_assert(std::is_trivially_copyable_v<Transport>);
inline Transport validatedTransport(Transport t) noexcept {
    if(!t.hostValid){const auto frame=t.callbackFrame;t={};t.callbackFrame=frame;return t;}
    t.hasBpm=t.hasBpm&&std::isfinite(t.bpm)&&t.bpm>0;if(!t.hasBpm)t.bpm=120;
    t.hasPpq=t.hasPpq&&std::isfinite(t.ppq);if(!t.hasPpq)t.ppq=0;
    t.hasSeconds=t.hasSeconds&&std::isfinite(t.seconds);if(!t.hasSeconds)t.seconds=0;
    t.hasBarStart=t.hasBarStart&&std::isfinite(t.barStartPpq);if(!t.hasBarStart)t.barStartPpq=0;
    t.hasTimeSignature=t.hasTimeSignature&&t.numerator>0&&t.denominator>0;
    if(!t.hasTimeSignature)t.numerator=t.denominator=0;
    t.hasLoopPoints=t.hasLoopPoints&&std::isfinite(t.loopStartPpq)&&std::isfinite(t.loopEndPpq)&&t.loopEndPpq>t.loopStartPpq;
    if(!t.hasLoopPoints)t.loopStartPpq=t.loopEndPpq=0;
    if(!t.hasSamples)t.samplePosition=0;
    return t;
}
}
