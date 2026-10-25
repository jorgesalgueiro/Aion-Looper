#pragma once
#include <cmath>
#include <cstdint>
#include <type_traits>
namespace aion {
// Native processor-entry observation in JUCE's high-resolution counter seconds.
// This edge follows device/host work: it is NOT output delivery or hostTimeNs.
struct ProcessorClockObservation {
    double seconds=0;
    bool available=false;
    bool operator==(const ProcessorClockObservation&) const noexcept = default;
};
inline ProcessorClockObservation validatedProcessorClock(ProcessorClockObservation value) noexcept {
    return value.available&&std::isfinite(value.seconds)&&value.seconds>=0?value:ProcessorClockObservation{};
}
// Raw callback-start timestamp, not an audible-output or performance.now stamp.
// Preserve the host's unknown clock origin and full integer nanosecond precision.
// Local [beginFrame,endFrame) is independent of DAW seeks and includes zero-size
// callbacks. Serial distinguishes successive callbacks at the same local frame.
struct CallbackClockStamp {
    std::uint64_t serial=0,beginFrame=0,endFrame=0,hostTimeNs=0;
    double sampleRate=0;
    std::uint32_t generation=0;
    bool hasHostTimeNs=false,nonRealtime=false;
    std::uint32_t outputLatencySamples=0;
    bool hasOutputLatency=false; // Prepared device report, not a clock correlation.
    ProcessorClockObservation processorEntry{};
    bool operator==(const CallbackClockStamp&) const noexcept = default;
};
static_assert(std::is_trivially_copyable_v<CallbackClockStamp>);
inline bool validCallbackClock(const CallbackClockStamp& stamp) noexcept {
    return stamp.generation&&stamp.serial&&stamp.beginFrame<=stamp.endFrame
        &&std::isfinite(stamp.sampleRate)&&stamp.sampleRate>0
        &&(stamp.hasHostTimeNs||stamp.hostTimeNs==0)
        &&(stamp.hasOutputLatency||stamp.outputLatencySamples==0)
        &&stamp.processorEntry==validatedProcessorClock(stamp.processorEntry);
}
}
