#pragma once
#include "core/CallbackAudibleTime.h"
#include "core/MeterGraphEvents.h"
#include <limits>
namespace aion {
struct MeterStopCallbackTime {
    std::uint32_t generation=0,track=0,ticket=0;
    std::uint64_t bufferId=0,queueId=0,eventSerial=0,callbackSerial=0,clockOrigin=0,cleanupSerial=0;
    double commandSeconds=0,delaySeconds=0,deadlineSeconds=0,remainingSeconds=0;
    bool operator==(const MeterStopCallbackTime&) const noexcept = default;
};
// Logical command-counter deadline, not DAC time or timer activation. IO owns
// the common origin of processor-entry capture and the supplied message counter.
inline bool meterStopCallbackTime(const MeterGraphEvent& event,std::uint64_t origin,
                                 MonotonicClockPoint wall,std::uint32_t generation,
                                 MeterStopCallbackTime& output) noexcept {
    const auto& c=event.callbackClock;
    const bool immediate=event.kind==MeterGraphEventKind::immediateStop,queued=event.kind==MeterGraphEventKind::queuedStop;
    const bool scheduled=event.kind==MeterGraphEventKind::scheduledStop,panic=event.kind==MeterGraphEventKind::stopAll;
    if((!immediate&&!queued&&!scheduled&&!panic)||!origin||wall.origin!=origin||!event.serial||!event.bufferId
        ||event.track>=MeterGraphEvents::trackCount||event.retiredTicket||event.queueDelaySeconds!=0
        ||(queued?!event.queueId:(!event.ticket||event.queueId!=0))
        ||!std::isfinite(event.stopLeadSeconds)||(scheduled?event.stopLeadSeconds<=0:panic?event.stopLeadSeconds!=.015:event.stopLeadSeconds!=0)
        ||!validCallbackClock(c)||c.nonRealtime||!c.processorEntry.available||event.generation!=generation||c.generation!=generation
        ||!std::isfinite(wall.seconds)||wall.seconds<c.processorEntry.seconds||event.frame<c.beginFrame
        ||(c.beginFrame==c.endFrame?event.frame!=c.beginFrame:event.frame>=c.endFrame)
        ||c.endFrame-c.beginFrame>static_cast<std::uint64_t>(std::numeric_limits<int>::max()))return false;
    const double offset=static_cast<double>(event.frame-c.beginFrame)/c.sampleRate;
    const double command=c.processorEntry.seconds+offset;
    const double delay=immediate||queued?.025:event.stopLeadSeconds+.05;
    const double deadline=command+delay,remaining=std::max(0.0,deadline-wall.seconds);
    if(!std::isfinite(offset)||!std::isfinite(command)||!std::isfinite(delay)||!std::isfinite(deadline)||!std::isfinite(remaining)
        ||(offset>0&&command<=c.processorEntry.seconds)||deadline<=command)return false;
    output={generation,event.track,event.ticket,event.bufferId,event.queueId,event.serial,c.serial,origin,0,command,delay,deadline,remaining};
    return true;
}
}
