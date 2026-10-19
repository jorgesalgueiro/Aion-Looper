#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
namespace aion {
enum class SyncMode : unsigned { off, fixed, dynamic };
struct SyncSettings {
    SyncMode mode=SyncMode::off;
    double bpm=120;
    unsigned bars=2,numerator=4,denominator=4;
    bool countIn=true;
    bool countInAudio=false;
};
struct SyncView {
    SyncSettings settings{};
    double origin=0,loopSeconds=4,barSeconds=2,position=0;
    std::uint64_t frame=0;
};
// Audio-owned grid. Denominator is display metadata, as in the unchanged JS.
class SyncClock {
public:
    void reset(double sampleRate) noexcept {rate=sampleRate;origin=0;settings=SyncSettings{};}
    void configure(SyncSettings next,std::uint64_t frame) noexcept {
        next.bpm=std::isfinite(next.bpm)?std::clamp(next.bpm,10.0,999.0):120;
        next.bars=std::clamp(next.bars,1u,64u);next.numerator=std::clamp(next.numerator,1u,32u);next.denominator=std::clamp(next.denominator,1u,32u);
        if(static_cast<unsigned>(next.mode)>2)next.mode=SyncMode::off;
        if(next.bpm!=settings.bpm)origin=static_cast<double>(frame)-(static_cast<double>(frame)-origin)*settings.bpm/next.bpm;
        settings=next;
    }
    double barFrames() const noexcept {return 60/settings.bpm*settings.numerator*rate;}
    double loopFrames() const noexcept {return (60/settings.bpm*settings.numerator*settings.bars)*rate;}
    double nextGridSeconds(std::uint64_t now) const noexcept {
        const double length=60/settings.bpm*settings.numerator*settings.bars;
        const double start=origin/rate;
        return start+std::max(0.0,std::ceil((static_cast<double>(now)/rate+0.03-start)/length))*length;
    }
    double nextGrid(std::uint64_t now) const noexcept {return nextGridSeconds(now)*rate;}
    double nextBar(std::uint64_t now) const noexcept {return origin+(std::floor((static_cast<double>(now)-origin)/barFrames())+1)*barFrames();}
    static std::uint64_t deadline(double target) noexcept {
        // Arithmetic such as 0.3 * sampleRate can land one ULP above an
        // exact sample. Remove only that representation noise before ceiling.
        const double nearest=std::round(target);
        if(std::abs(target-nearest)<=4*std::numeric_limits<double>::epsilon()*std::max(1.0,std::abs(target)))target=nearest;
        return static_cast<std::uint64_t>(std::max(0.0,std::ceil(target)));
    }
    SyncView view(std::uint64_t frame) const noexcept {
        double phase=std::fmod(static_cast<double>(frame)-origin,loopFrames());if(phase<0)phase+=loopFrames();
        return {settings,origin,loopFrames()/rate,barFrames()/rate,phase/loopFrames(),frame};
    }
    SyncSettings settings{};
    double origin=0;
private:
    double rate=48000;
};
}
