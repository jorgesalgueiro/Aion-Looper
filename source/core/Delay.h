// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "DelaySettings.h"
#include <vector>
namespace aion {
// Single-owner DSP component, not yet a connected product D control. Only
// prepare/destruction allocate/free. Caller must transfer ownership outside the
// callback. reset is O(1); stale ring samples are hidden by valid-history count.
class Delay {
public:
 Delay()=default;
 Delay(const Delay&)=delete;
 Delay& operator=(const Delay&)=delete;
 Delay(Delay&&)=delete;
 Delay& operator=(Delay&&)=delete;
 bool prepare(double sampleRate,const DelaySettings& settings={},double bpm=120) noexcept;
 bool set(const DelaySettings&,double bpm) noexcept;
 void reset() noexcept;
 void tick(float left,float right,float& outLeft,float& outRight) noexcept;
 const DelaySettings& settings() const noexcept{return target;}
 std::size_t storageFrames() const noexcept{return ring.size();}
private:
 double read(unsigned channel,double frames) const noexcept;
 void derive(double bpm) noexcept;
 std::vector<std::array<float,2>> ring;
 DelaySettings target{};
 std::size_t write=0,history=0;
 double rate=48000,delayA=18000,delayB=18000,wanted=18000;
 unsigned fade=0,fadeLength=960;
 double smoothing=0,mix=0,feedback=0,depth=0,enable=0;
 double g=0,gTarget=0,a1=0;
 double cachedDamp=-1,cachedPanSpeed=-1;
 std::array<double,2> ic1{},ic2{};
 double sine=0,cosine=1,rotationSin=0,rotationCos=1;
 unsigned phaseAge=0;
 bool prepared=false,asleep=true;
};
}
