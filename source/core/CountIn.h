#pragma once
#include "core/SyncClock.h"
#include <array>
#include <memory>
namespace aion {
// A bounded, audio-owned port of LoopManager.scheduleCountIn. No browser timers.
// The reference delegates the triangle to WebAudio; native uses a Nyquist-limited
// odd-harmonic wavetable with reference pitch-band blending and normalization,
// built only in prepare(). See docs/VALIDATION.md for source and acceptance.
class CountIn {
public:
    void prepare(double sampleRate) {
        if(!tables)tables=std::make_unique<Tables>();
        rate=std::isfinite(sampleRate)&&sampleRate>=8000&&sampleRate<=384000?sampleRate:48000;
        cancel();
        // Match the reference browser's three pitch bands per octave. Preserve
        // the common full-wave normalization when high partials are removed;
        // normalizing each truncated triangle would incorrectly boost its gain.
        const unsigned referenceSize=rate<=24000?2048u:rate<=88200?4096u:16384u;
        double normalization=0;
        for(unsigned h=1;h<referenceSize/2;h+=2)normalization+=1.0/(h*h);
        for(unsigned tone=0;tone<2;++tone){
            const double frequency=tone?880:440;
            const float pitch=1+3*std::log2(static_cast<float>(frequency/(rate/referenceSize)));
            const auto band=static_cast<unsigned>(pitch);
            const double blend=pitch-band;
            const auto high=static_cast<unsigned>((referenceSize/2)*std::pow(2.0,-static_cast<double>(band)/3));
            const auto low=static_cast<unsigned>((referenceSize/2)*std::pow(2.0,-static_cast<double>(band+1)/3));
            for(unsigned i=0;i<=tableSize;++i){
                double value=0;
                for(unsigned h=1;h<=high;h+=2){
                    const double weight=h<=low?1:1-blend;
                    value+=weight*((h%4==1)?1.0:-1.0)*std::sin(2*pi*i*h/tableSize)/(h*h);
                }
                (*tables)[tone][i]=value/normalization;
            }
        }
    }
    void cancel() noexcept {count=0;}
    void schedule(double now,double target,double bpm,unsigned numerator,bool sound,bool visual) noexcept {
        cancel();
        if(!std::isfinite(now)||!std::isfinite(target)||!std::isfinite(bpm)||bpm<10||bpm>999||target<=now)return;
        const double spb=60/bpm;
        const double beats=std::ceil((target-now)/spb);
        for(unsigned i=1;i<=std::clamp(numerator,1u,32u)&&i<=beats;++i){
            const double start=target-i*spb;
            if(start>now+0.05)cues[count++]={start,i==1,sound,visual};
        }
    }
    float tick(std::uint64_t frame) const noexcept {
        if(!tables)return 0;
        const double now=static_cast<double>(frame)/rate;
        double sum=0;
        for(unsigned i=0;i<count;++i){
            const auto& cue=cues[i];
            if(!cue.sound||now<cue.start||now>=cue.start+0.1)continue;
            const double cycles=(now-cue.start)*(cue.last?880:440);
            const double phase=(cycles-std::floor(cycles))*tableSize;
            const auto index=static_cast<unsigned>(phase);
            const double fraction=phase-index;
            const auto& table=(*tables)[cue.last?1:0];
            sum+=0.5*(table[index]+fraction*(table[index+1]-table[index]));
        }
        return static_cast<float>(sum);
    }
    unsigned flash(std::uint64_t frame) const noexcept {
        const double now=static_cast<double>(frame)/rate;
        unsigned color=0;double latest=-1;
        for(unsigned i=0;i<count;++i){const auto& cue=cues[i];
            if(cue.visual&&now>=cue.start&&now<cue.start+0.15&&cue.start>latest){latest=cue.start;color=cue.last?2:1;}
        }
        return color;
    }
private:
    static constexpr unsigned tableSize=4096;
    static constexpr double pi=3.1415926535897932384626433832795;
    struct Cue {double start=0;bool last=false,sound=false,visual=false;};
    std::array<Cue,32> cues{};
    using Tables=std::array<std::array<double,tableSize+1>,2>;
    std::unique_ptr<Tables> tables; // Allocated in prepare; no large callback stack.
    double rate=48000;
    unsigned count=0;
};
}
