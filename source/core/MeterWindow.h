#pragma once
#include <array>
#include <cmath>
#include <span>
namespace aion {
// Raw 2048-frame time-domain tap, matching the original loop analyser size.
// One audio owner only. No queue, locks, allocation, FFT or display ballistics.
class MeterWindow {
public:
    static constexpr unsigned size=2048;
    void reset() noexcept {cursor=valid=0;} // O(1): hide stale storage with a zero prefix.
    void pushMono(float value) noexcept {samples[cursor]=std::isfinite(value)?value:0;cursor=(cursor+1)%size;if(valid<size)++valid;}
    void pushStereo(float left,float right) noexcept {pushMono(left*0.5f+right*0.5f);}
    void copy(std::span<float,size> destination) const noexcept {
        for(unsigned i=0;i<size;++i)destination[i]=i<size-valid?0:samples[(cursor+i)%size];
    }
private:
    std::array<float,size> samples{};
    unsigned cursor=0,valid=0;
};
}
