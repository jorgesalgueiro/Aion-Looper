// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "DistortionSettings.h"
#include <cstddef>

namespace aion {
// Wet WaveShaper only: original 4x curve processing with causal, fixed-storage
// resampling. No tone/mix/automation, product routing or state ownership yet.
// prepare OFF audio; curve must be immutable and alive for the entire call.
// Persistent across process calls; explicit reset clears both channels' tails.
class DistortionWaveShaper {
public:
    static constexpr std::size_t maxProcessFrames=4096;
    static constexpr unsigned wetLatencyFrames=192;
    void prepare() noexcept;
    void reset() noexcept;
    static float lookup(const DistortionCurve&,float input) noexcept;
    // 0 inputs drains tails, 1 duplicates mono, 2 is stereo. Matching input/
    // output channels may alias. Separate output channels are required.
    // Invalid arguments reject without processing or changing output/history.
    bool process(const DistortionCurve&,const float* left,const float* right,unsigned channels,
                 float* outLeft,float* outRight,std::size_t frames) noexcept;
private:
    struct Up {
        std::array<float,128> history{};unsigned index=0;
        std::array<float,2> process(float,const std::array<float,128>&) noexcept;
    };
    struct Down {
        std::array<float,128> even{},odd{};unsigned index=0;
        float process(const std::array<float,2>&,const std::array<float,128>&) noexcept;
    };
    struct Channel {Up up1,up2;Down down2,down1;};
    std::array<Channel,2> channels{};
    std::array<float,128> upKernel{},downKernel{};
    bool prepared=false;
};
}
