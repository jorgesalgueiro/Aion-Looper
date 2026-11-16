// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace aion {
struct FaustControl {
    std::string path;
    float initial=0,minimum=0,maximum=1,step=0;
};
// Optional native Faust bytecode VM. Compile/clear/destruction are OFF audio and
// must not overlap processing of this instance. Source is trusted local DSP,
// not a sandbox for arbitrary/unbounded programs. No loop routing/UI is implied.
// Metadata is immutable. One audio owner writes controls/processes an instance.
class FaustInterpreter {
public:
    static constexpr unsigned maxFrames=4096,maxControls=64,maxSourceBytes=65536;
    static std::unique_ptr<FaustInterpreter> compile(std::string_view source,int sampleRate,
                                                   const std::string& libraryDirectory,std::string& error);
    ~FaustInterpreter();
    FaustInterpreter(const FaustInterpreter&)=delete;
    FaustInterpreter& operator=(const FaustInterpreter&)=delete;
    unsigned inputs() const noexcept;
    unsigned outputs() const noexcept;
    const std::vector<FaustControl>& controls() const noexcept;
    bool setControl(unsigned index,float value) noexcept;
    // All input is copied to prepared scratch before compute, allowing matching
    // in-place buffers. Actual DSP I/O count is 0..2 inputs and 1..2 outputs.
    // Invalid arguments leave output/state untouched; a DSP fault latches silence.
    bool process(const float* const* input,float* const* output,unsigned frames) noexcept;
    bool faulted() const noexcept;
    bool clearOffAudio() noexcept;
private:
    struct Impl;
    explicit FaustInterpreter(std::unique_ptr<Impl>) noexcept;
    std::unique_ptr<Impl> impl;
};
}
