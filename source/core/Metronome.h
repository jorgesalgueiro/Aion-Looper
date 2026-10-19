#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace aion {
// Native port of the preserved MetronomeProcessor. Audio-owned, fixed storage.
class Metronome {
public:
    void prepare(double sampleRate) noexcept {
        rate=std::isfinite(sampleRate)&&sampleRate>=8000&&sampleRate<=384000?sampleRate:48000;
        decay=std::exp(-1/(rate*0.0113));phase=env=0;frequency=440;hasBeat=hasClick=false;
    }
    void configure(float tempo,float originSeconds,unsigned meter) noexcept {
        bpm=std::isfinite(tempo)?std::clamp(tempo,1.0f,999.0f):120;
        origin=std::isfinite(originSeconds)?originSeconds:0;
        beatsPerBar=std::clamp(meter,1u,32u);
    }
    void setEnabled(bool value) noexcept {enabled=value;}
    void setVolume(float value) noexcept {volume=std::isfinite(value)?std::clamp(value,0.0f,2.0f):0;}
    float tick(std::uint64_t frame) noexcept {
        if(!enabled){hasBeat=hasClick=false;env=0;return 0;}
        const double current=static_cast<double>(frame);
        const double beat=std::floor(((current/rate-origin)*bpm)/60.0);
        if(!hasBeat){lastBeat=std::floor((((current-1)/rate-origin)*bpm)/60.0);hasBeat=true;}
        if(beat!=lastBeat){
            lastBeat=beat;
            if(beat>=0){env=1;frequency=std::fmod(beat,beatsPerBar)==0?2000:1000;phase=0;lastClick=frame;hasClick=true;}
        }
        double sample=0;
        if(env>0.001){
            sample=std::sin(phase)*env*volume;phase+=(2*pi*frequency)/rate;
            if(phase>100*pi)phase-=100*pi;
            env*=decay;
        }else env=0;
        return static_cast<float>(sample);
    }
    bool flash(std::uint64_t frame) const noexcept {return enabled&&hasClick&&frame>=lastClick&&static_cast<double>(frame-lastClick)<rate*0.1;}
private:
    static constexpr double pi=3.1415926535897932384626433832795;
    double rate=48000,decay=std::exp(-1/(48000*0.0113)),phase=0,env=0,frequency=440,lastBeat=0;
    float bpm=120,origin=0,volume=0.5f;
    unsigned beatsPerBar=4;
    std::uint64_t lastClick=0;
    bool enabled=false,hasBeat=false,hasClick=false;
};
}
