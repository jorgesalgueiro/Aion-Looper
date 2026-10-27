#pragma once
#include "core/MeterCleanupDeadline.h"
#include <juce_events/juce_events.h>
#include <atomic>
#include <limits>

namespace aion {
// One per Engine/track, on the message thread, outlived by its lifecycle owner.
// Explicit stop calls capture the deadline; synchronise only validates/cancels it.
// Processor service owns these routes; playback stop notification callers are pending.
class LoopMeterCleanupTimer final : private juce::Timer {
public:
    explicit LoopMeterCleanupTimer(LoopMeterLifecycle& lifecycle,const std::atomic<std::uint32_t>* prepared=nullptr) noexcept
        :owner(lifecycle),preparedGeneration(prepared){assertMessageThread();}
    LoopMeterCleanupTimer(const LoopMeterCleanupTimer&)=delete;
    LoopMeterCleanupTimer& operator=(const LoopMeterCleanupTimer&)=delete;
    ~LoopMeterCleanupTimer() override {
        // A host may destroy the processor on its own thread while retaining
        // MessageManager ownership. No callback may run against this destructor.
        JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED
        stopTimer();
    }
    bool stop(double audioNow,double scheduled=0,bool hasVolume=true) noexcept {
        assertMessageThread();
        if(preparedGeneration&&!preparedGeneration->load(std::memory_order_acquire))return false;
        const double wall=wallTime();
        const double delay=scheduled>audioNow?scheduled-audioNow+.05:.025;
        // Validate before mutating the owner, including overflow/clock precision.
        if(!std::isfinite(audioNow)||!std::isfinite(scheduled)||!MeterCleanupDeadline::canArm(wall,delay))return false;
        if(!owner.stop(audioNow,scheduled,hasVolume))return false;
        const auto request=owner.cleanupRequest();
        stopTimer();deadline.cancel();
        if(!request.serial)return true;
        const bool armed=deadline.arm(request,wall);jassert(armed);
        if(armed)schedule(wall);
        return armed;
    }
    void synchronise() noexcept {
        assertMessageThread();
        if(!preparedFor(deadline.pending())||!owner.cleanupCurrent(deadline.pending())){stopTimer();deadline.cancel();}
        else if(!isTimerRunning())schedule(wallTime());
    }
    bool consumeClear(const MeterGraphEvent& event) noexcept {
        assertMessageThread();
        if(!owner.consumeClear(event))return false;
        synchronise();return true;
    }
    bool pending() const noexcept {assertMessageThread();return deadline.pending().serial!=0;}
    bool routesTo(const Engine& source,unsigned track) const noexcept {assertMessageThread();return owner.routesTo(source,track);}
private:
    static void assertMessageThread() noexcept {jassert(juce::MessageManager::existsAndIsCurrentThread());}
    static double wallTime() noexcept {return juce::Time::getMillisecondCounterHiRes()*.001;}
    bool preparedFor(MeterCleanupRequest request) const noexcept {
        return !preparedGeneration||(request.generation&&preparedGeneration->load(std::memory_order_acquire)==request.generation);
    }
    void schedule(double wall) noexcept {
        double remaining=0;if(!deadline.remaining(wall,remaining))return;
        // Clamp before converting: very distant scheduled stops remain safe.
        const double milliseconds=std::clamp(std::ceil(remaining*1000.0),1.0,
            static_cast<double>(std::numeric_limits<int>::max()));
        startTimer(static_cast<int>(milliseconds));
    }
    void timerCallback() override {
        assertMessageThread();stopTimer();
        const auto request=deadline.pending();
        if(!preparedFor(request)||!owner.cleanupCurrent(request)){deadline.cancel();return;}
        const double wall=wallTime();MeterCleanupRequest due;
        if(deadline.takeDue(request,wall,due))owner.deliverCleanup(due);
        else schedule(wall); // Early callbacks cannot deliver before the deadline.
    }
    LoopMeterLifecycle& owner;
    const std::atomic<std::uint32_t>* preparedGeneration=nullptr; // Optional processor lifetime, outlives this adapter.
    MeterCleanupDeadline deadline;
};
}
