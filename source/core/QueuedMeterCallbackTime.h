#pragma once
#include "core/CallbackAudibleTime.h"
#include "core/MeterGraphEvents.h"
#include <limits>

namespace aion {
struct QueuedMeterCallbackTime {
    std::uint32_t generation=0,track=0;
    std::uint64_t queueId=0,bufferId=0,eventSerial=0,callbackSerial=0,clockOrigin=0;
    double commandSeconds=0,deadlineSeconds=0,remainingSeconds=0;
    bool operator==(const QueuedMeterCallbackTime&) const noexcept = default;
};
// Counter-aligned command deadline, not audible-output time. The IO owner must
// establish one origin for processor-entry capture and the supplied message wall.
// Never infer it from hostTimeNs, delivery latency, a later view or clock magnitude.
// Delayed receipt reduces remaining time; it cannot postpone the original due time.
inline bool queuedMeterCallbackTime(const MeterGraphEvent& event,std::uint64_t nativeOrigin,
                                   MonotonicClockPoint wall,std::uint32_t generation,
                                   QueuedMeterCallbackTime& output) noexcept {
    const auto& callback=event.callbackClock;
    if(!nativeOrigin||wall.origin!=nativeOrigin||!event.serial||event.kind!=MeterGraphEventKind::queuedPlay
        ||!event.queueId||!event.bufferId||event.track>=MeterGraphEvents::trackCount||event.retiredTicket||event.stopLeadSeconds!=0
        ||!std::isfinite(event.queueDelaySeconds)||event.queueDelaySeconds<0
        ||!validCallbackClock(callback)||event.generation!=generation||callback.generation!=generation
        ||callback.nonRealtime||!callback.processorEntry.available||!std::isfinite(wall.seconds)
        ||wall.seconds<callback.processorEntry.seconds||event.frame<callback.beginFrame
        ||(callback.beginFrame==callback.endFrame?event.frame!=callback.beginFrame:event.frame>=callback.endFrame)
        ||callback.endFrame-callback.beginFrame>static_cast<std::uint64_t>(std::numeric_limits<int>::max()))return false;
    // Subtract integers first: full-width frame identities remain exact even
    // above 2^53, while the callback-local offset is bounded by an int block size.
    const double offset=static_cast<double>(event.frame-callback.beginFrame)/callback.sampleRate;
    const double command=callback.processorEntry.seconds+offset;
    const double deadline=command+event.queueDelaySeconds;
    const double remaining=std::max(0.0,deadline-wall.seconds);
    if(!std::isfinite(offset)||!std::isfinite(command)||!std::isfinite(deadline)||!std::isfinite(remaining)
        ||(offset>0&&command<=callback.processorEntry.seconds)||(event.queueDelaySeconds>0&&deadline<=command))return false;
    output={generation,event.track,event.queueId,event.bufferId,event.serial,callback.serial,nativeOrigin,command,deadline,remaining};
    return true;
}
}
