#include "core/MeterFollower.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <type_traits>
extern thread_local bool watch;
extern std::atomic<int> allocations;
namespace {void check(bool ok,const char* label){if(!ok){std::cerr<<label<<'\n';std::exit(1);}}}
void meterTests(){
    static_assert(std::is_trivially_destructible_v<aion::MeterFollower>);
    const int before=allocations.load();
    watch=true;
    {
        aion::MeterFollower meter;
        const float hot[]={1,-1,1,-1},silent[]={0,0,0,0};
        auto v=meter.update(hot,0,0,1,1,1,true);
        check(v.rms==1&&v.peak==1&&v.clipped,"Meter unity RMS/peak/clip");
        static constexpr std::array<float,aion::MeterFollower::maximumWindow+1> oversized{};
        const auto rejected=meter.update(oversized,999,999,99,99,99,false);
        check(rejected.rms==1&&rejected.peak==1&&rejected.clipped,"Oversized window rejection preserves complete meter ownership/state");
        v=meter.update(silent,0.8,0.8,1,1,1,true);
        check(v.peak==1&&v.clipped,"Meter 0.8-second hold boundary");
        v=meter.update(silent,1.2,1.2,1,1,1,true);
        check(std::abs(v.peak-std::pow(10.0,-0.4))<1e-12&&!v.clipped,"Meter clip ends at 1.2s and peak falls 20 dB/s");
        for(unsigned i=0;i<1024;++i){const double t=1.3+i/100.0;meter.update(silent,t,t,1,1,1,true);}
        v=meter.update(silent,20,20,1,1,1,true);
        check(v.rms==0&&v.peak==0&&v.peakDb==-60,"Meter silence floor after bounded-history wraps");
        const float invalid[]={std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),0.5f,-0.5f};
        v=meter.update(invalid,0,0,2,2,2,true);
        check(std::abs(v.rms-std::sqrt(0.125))<1e-12&&v.peak==0.5,"Meter sanitizes nonfinite samples and resets clock/source");
        v=meter.update(hot,1,1,2,2,2,false);check(v.rms==0&&v.peak==0&&!v.clipped,"Suspended context resets to silence");
        // Identical 256-frame analyser observations assembled from 64/128 blocks.
        aion::MeterFollower a,b;std::array<float,256> x{},y{};
        for(unsigned frame=0;frame<8192;frame+=256){
            for(unsigned block=0;block<256;block+=64)for(unsigned i=0;i<64;++i)x[block+i]=static_cast<float>(std::sin((frame+block+i)*0.07)*0.75);
            for(unsigned block=0;block<256;block+=128)for(unsigned i=0;i<128;++i)y[block+i]=static_cast<float>(std::sin((frame+block+i)*0.07)*0.75);
            const double t=static_cast<double>(frame)/48000;
            const auto av=a.update(x,t,t,1,1,1,true),bv=b.update(y,t,t,1,1,1,true);
            check(av.rms==bv.rms&&av.peak==bv.peak&&av.clipped==bv.clipped,"Meter supplied-window 64/128 partition invariance");
        }
    }
    watch=false;
    check(allocations.load()==before,"Meter construction/update/destruction must not allocate/free");
    std::cout<<"PASS: native meter RMS, peak/clip boundaries, invalid samples, ring wraps, source reset and supplied-window 64/128; zero tracked new/delete\n";
}
