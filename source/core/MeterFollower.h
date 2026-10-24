#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
namespace aion {
struct MeterReading {
    double rms=0,peak=0,linearPeak=0,peakDb=-60,rmsPercent=0,peakPercent=0;
    bool clipped=false;
};
// Port of AudioEngine.readMeter. One owner; fixed history, no allocation/locks.
// Caller supplies the original mono analyser window and stable source identities.
// This component does not collect PCM, query devices, or infer audible latency.
class MeterFollower {
public:
    static constexpr std::size_t maximumWindow=32768; // WebAudio analyser maximum.
    const MeterReading& update(std::span<const float> data,double now,double playbackTime,
        std::uint64_t context,std::uint64_t analyser,std::uint64_t dataIdentity,bool running) noexcept {
        // Reject invalid caller windows atomically; never scan arbitrary clips.
        if(data.size()>maximumWindow)return value;
        running=running&&context!=0;
        now=std::isfinite(now)?std::max(0.0,now):0;
        playbackTime=std::isfinite(playbackTime)?std::clamp(playbackTime,0.0,now):now;
        if(!initialized||context!=oldContext||running!=oldRunning||now<sampleTime
            ||(analyser&&(analyser!=oldAnalyser||(dataIdentity&&dataIdentity!=oldData)))){
            value={};count=write=0;time=peakUntil=playbackTime;clipUntil=-INFINITY;
        }
        initialized=true;oldContext=context;oldRunning=running;oldAnalyser=analyser;
        oldData=dataIdentity;sampleTime=now;
        double rms=0,peak=0;
        if(running&&analyser&&dataIdentity&&!data.empty()){
            double sum=0;
            for(float sample:data){const double v=std::isfinite(sample)?sample:0;sum+=v*v;peak=std::max(peak,std::abs(v));}
            rms=std::sqrt(sum/static_cast<double>(data.size()));
        }
        if(!count||history[(write+history.size()-1)%history.size()].time!=now){
            history[write]={now,rms,peak};write=(write+1)%history.size();count=std::min(count+1,history.size());
        }
        rms=peak=0;
        for(std::size_t i=0;i<count;++i){const auto& entry=history[(write+history.size()-1-i)%history.size()];
            if(entry.time<=playbackTime){rms=entry.rms;peak=entry.peak;break;}
        }
        const double nextTime=std::max(time,playbackTime),dt=nextTime-time;
        value.rms=rms>=value.rms?rms:rms+(value.rms-rms)*std::exp(-dt/0.18);
        value.peak*=std::pow(10.0,-std::max(0.0,nextTime-std::max(time,peakUntil)));
        if(peak>0&&peak>=value.peak){value.peak=peak;peakUntil=nextTime+0.8;}
        if(peak>=1)clipUntil=nextTime+1.2;
        value.clipped=nextTime<clipUntil;time=nextTime;
        value.linearPeak=peak<0.001?0:peak;
        if(value.rms<0.001)value.rms=0;
        if(value.peak<0.001)value.peak=0;
        const auto db=[](double v){return v>0?std::max(-60.0,20*std::log10(v)):-60.0;};
        const auto percent=[](double v){return std::clamp((v+60)/60*100,0.0,100.0);};
        value.peakDb=db(value.peak);value.rmsPercent=percent(db(value.rms));value.peakPercent=percent(value.peakDb);
        return value;
    }
private:
    struct Entry {double time=0,rms=0,peak=0;};
    std::array<Entry,128> history{};
    std::size_t count=0,write=0;
    MeterReading value{};
    double sampleTime=0,time=0,peakUntil=0,clipUntil=-INFINITY;
    std::uint64_t oldContext=0,oldAnalyser=0,oldData=0;
    bool initialized=false,oldRunning=false;
};
}
