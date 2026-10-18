#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace aion {
enum class LoopState : std::uint8_t { empty, recording, playing, stopped, overdubbing, substituting };
enum class LoopAction : std::uint8_t { record, finish, play, stop, overdub, substitute, clear, stopAll, volume, feedback, mute };
struct Stereo { float left = 0, right = 0; };
struct LoopView {
    LoopState state = LoopState::empty;
    std::uint32_t length = 0, position = 0;
    float volume = 1, feedback = 0.8f, peak = 0;
    bool muted = false;
};
// Audio-owned fixed-block pool. Only prepare() allocates, while audio is stopped.
class LoopBank {
public:
    static constexpr std::uint32_t trackCount = 20, blockFrames = 4096, maxBlocks = 2048;
    static constexpr std::uint32_t defaultFrames = blockFrames * maxBlocks; // 64 MiB stereo
    void prepare(std::uint32_t frames = defaultFrames, std::uint32_t fadeFrames = 240);
    bool apply(LoopAction, std::uint32_t track, float value = 0) noexcept;
    Stereo tick(Stereo input) noexcept;
    void beginBlock() noexcept;
    std::array<LoopView, trackCount> view() const noexcept;
    std::uint32_t capacity() const noexcept { return totalBlocks * blockFrames; }
    std::uint32_t available() const noexcept { return freeCount * blockFrames; }
    bool exhausted() const noexcept { return poolFull; }
private:
    struct Track {
        LoopView info{};
        std::array<std::uint16_t, maxBlocks> blocks{};
        std::uint32_t blockCount = 0, blendRemaining = 0, tailRemaining = 0;
        Stereo last{}, blendFrom{}, tail{};
        float gain = 1;
    };
    float& sample(Track&, std::uint32_t frame, std::uint32_t channel) noexcept;
    void stop(Track&) noexcept;
    void clear(Track&) noexcept;
    void blend(Track&) noexcept;
    std::array<Track, trackCount> tracks{};
    std::array<std::uint16_t, maxBlocks> freeBlocks{};
    std::vector<float> audio;
    std::uint32_t totalBlocks = 0, freeCount = 0, fadeLength = 240;
    bool poolFull = false;
};
}
