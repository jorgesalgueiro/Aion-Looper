#pragma once
#include "core/OutputLatency.h"
namespace aion {
// Lifecycle/prepare owner only. The same adapter serves JUCE AudioIODevice and
// a controlled test device; neither getter is ever invoked by processBlock.
template<class Device> DeviceOutputLatency readDeviceOutputLatency(Device* device) noexcept {
    if(!device)return {};
    try{return {true,device->getCurrentSampleRate(),device->getOutputLatencyInSamples()};}
    catch(...){return {};}
}
}
