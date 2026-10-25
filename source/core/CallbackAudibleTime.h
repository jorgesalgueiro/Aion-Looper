#pragma once
#include "core/CallbackClock.h"
#include <algorithm>

namespace aion {
// Explicit correlation supplied by an IO owner, not inferred from hostTimeNs.
// Both points must use one nonzero owner-assigned monotonic clock origin.
struct MonotonicClockPoint {
    std::uint64_t origin=0;
    double seconds=0;
};
struct CallbackClockCorrelation {
    CallbackClockStamp callback;
    MonotonicClockPoint entry;
};
struct CallbackAudibleTime {
    double renderedTime=0,audibleTime=0;
    bool operator==(const CallbackAudibleTime&) const noexcept = default;
};
// Reported-latency model for visual selection, never a measured DAC timestamp.
// The correlation must describe delivery of this nonempty output callback.
// No IO, clock query, allocation or extrapolation beyond rendered PCM. A failed
// observation leaves the caller's previous result untouched. Production IO must
// establish the callback-delivery edge before supplying this correlation.
inline bool callbackAudibleTime(const CallbackClockStamp& callback,
                               const CallbackClockCorrelation& correlation,
                               MonotonicClockPoint wall,std::uint32_t generation,
                               CallbackAudibleTime& result) noexcept {
    constexpr std::uint64_t maxExactFrame=std::uint64_t{1}<<53;
    if(!validCallbackClock(callback)||callback.generation!=generation
        ||callback.nonRealtime||!callback.hasOutputLatency
        ||callback.beginFrame==callback.endFrame||callback.endFrame>maxExactFrame
        ||callback!=correlation.callback||!wall.origin||wall.origin!=correlation.entry.origin
        ||!std::isfinite(wall.seconds)||!std::isfinite(correlation.entry.seconds)
        ||correlation.entry.seconds<0||wall.seconds<correlation.entry.seconds)return false;
    const double elapsed=wall.seconds-correlation.entry.seconds;
    const double end=static_cast<double>(callback.endFrame)/callback.sampleRate;
    // Keep the startup offset signed until after adding elapsed time. Clamping
    // begin-latency early would start the visual clock before audio can emerge.
    const double atEntry=(static_cast<double>(callback.beginFrame)
                          -static_cast<double>(callback.outputLatencySamples))/callback.sampleRate;
    const double audible=atEntry+elapsed;
    if(!std::isfinite(elapsed)||!std::isfinite(end)||!std::isfinite(audible))return false;
    result={end,std::clamp(audible,0.0,end)};
    return true;
}
}
