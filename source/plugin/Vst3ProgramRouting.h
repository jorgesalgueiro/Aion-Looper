#pragma once
#include <cmath>
#include <cstdint>
#include <optional>
namespace aion::vst3 {
// Wrapper-only IDs; these are not APVTS parameters or application presets.
inline constexpr std::uint32_t programBase=0x6a500000;
inline constexpr std::int32_t unitBase=0x6a510000,listBase=0x6a520000;
constexpr bool programId(std::uint32_t id) noexcept {return id>=programBase&&id<programBase+16;}
struct ProgramEvent {int channel,number,offset;};
inline std::optional<ProgramEvent> decode(std::uint32_t id,int offset,double value) noexcept {
    if(!programId(id)||offset<0||!std::isfinite(value)||value<0||value>1)return {};
    return ProgramEvent{static_cast<int>(id-programBase)+1,static_cast<int>(std::floor(value*127+0.5)),offset};
}
// A narrow wrapper-to-processor signal, never a fabricated MIDI packet.
struct OverflowSink {virtual ~OverflowSink()=default;virtual void programInputOverflow() noexcept=0;};
// Cap all program queues together; native processing rejects the whole block on
// excess through the existing overflow/cancellation policy.
struct PointBudget {
    unsigned used=0;bool overflow=false;
    bool take() noexcept {if(used==256){overflow=true;return false;}++used;return true;}
};
}
