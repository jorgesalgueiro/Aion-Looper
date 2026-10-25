#pragma once
#include <algorithm>
#include <cmath>

namespace aion {
// An immutable, coherent observation supplied by the device/host adapter.
// Seconds use the local audio origin; performance fields use the same monotonic
// millisecond origin. No clock query, driver call, latency guess or DAW position.
struct PlaybackClockInput {
    bool hasContext=false,hasOutputTimestamp=false;
    double currentTime=0,contextTime=0,performanceTime=0,wallTime=0;
    double baseLatency=0,outputLatency=0;
};
// Exact AudioEngine.playbackTime policy. For visual selection only: never use
// this delayed estimate for audio scheduling or graph teardown timer delivery.
// hasOutputTimestamp=false covers missing/throwing APIs, null stamps or absent
// performance.now(). A successful call may still carry invalid/startup fields.
inline double playbackTime(const PlaybackClockInput& input) noexcept {
    if(!input.hasContext)return 0;
    const double now=std::isfinite(input.currentTime)?std::max(0.0,input.currentTime):0;
    if(input.hasOutputTimestamp&&std::isfinite(input.contextTime)&&input.contextTime>0
        &&std::isfinite(input.performanceTime)&&input.performanceTime>0&&std::isfinite(input.wallTime)){
        const double audible=input.contextTime+(input.wallTime-input.performanceTime)/1000;
        if(std::isfinite(audible))return std::clamp(audible,0.0,now);
    }
    const auto latency=[](double value) noexcept {return std::isfinite(value)?std::max(0.0,value):0;};
    return std::max(0.0,now-latency(input.baseLatency)-latency(input.outputLatency));
}
}
