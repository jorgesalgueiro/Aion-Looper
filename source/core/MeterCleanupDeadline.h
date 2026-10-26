#pragma once
#include "core/LoopMeterLifecycle.h"
#include <algorithm>
#include <cmath>

namespace aion {
// Message-owner wall-clock policy. Never infer cleanup from audio sample time.
class MeterCleanupDeadline {
public:
    static bool canArm(double wall,double delay) noexcept {
        return std::isfinite(wall)&&wall>=0&&std::isfinite(delay)&&delay>=0
            &&std::isfinite(wall+delay)&&(delay==0||wall+delay>wall);
    }
    bool arm(MeterCleanupRequest next,double wall) noexcept {
        if(!next.serial||!next.generation||!canArm(wall,next.delay))return false;
        if(next==request)return true; // Polling must never postpone the original stop.
        request=next;begin=wall;deadline=wall+next.delay;return true;
    }
    void cancel() noexcept {request={};begin=deadline=0;}
    MeterCleanupRequest pending() const noexcept {return request;}
    bool remaining(double wall,double& seconds) const noexcept {
        if(!request.serial||!std::isfinite(wall)||wall<begin)return false;
        seconds=std::max(0.0,deadline-wall);return true;
    }
    bool takeDue(MeterCleanupRequest current,double wall,MeterCleanupRequest& out) noexcept {
        if(current!=request){cancel();return false;}
        double left=0;
        if(!remaining(wall,left)||left>0)return false;
        out=request;cancel();return true;
    }
private:
    MeterCleanupRequest request{};
    double begin=0,deadline=0;
};
}
