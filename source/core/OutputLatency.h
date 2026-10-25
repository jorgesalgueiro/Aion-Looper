#pragma once
#include <cmath>
namespace aion {
// Device report captured at the prepare barrier, not a measured acoustic delay.
// Missing is distinct from a device explicitly reporting zero samples.
struct DeviceOutputLatency {
    bool available=false;
    double sampleRate=0;
    int samples=-1;
};
inline DeviceOutputLatency validatedOutputLatency(DeviceOutputLatency value,double preparedRate) noexcept {
    if(!value.available||!std::isfinite(value.sampleRate)||value.sampleRate<=0
        ||value.sampleRate!=preparedRate||value.samples<0)return {};
    return value;
}
}
