#include "core/Engine.h"
#include "plugin/DeviceOutputLatency.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
extern thread_local bool watch;
extern std::atomic<int> allocations;
namespace {
void check(bool ok,const char* label){if(!ok){std::cerr<<label<<'\n';std::exit(1);}}
struct Device {
    unsigned rateCalls=0,latencyCalls=0;bool fail=false;
    double getCurrentSampleRate(){check(!watch,"Device sample-rate query reached processing");++rateCalls;return 48000;}
    int getOutputLatencyInSamples(){check(!watch,"Device latency query reached processing");++latencyCalls;if(fail)throw std::bad_alloc();return 256;}
};
}
void outputLatencyTests(){
    using namespace aion;
    Device device;auto read=readDeviceOutputLatency(&device);
    check(read.available&&read.sampleRate==48000&&read.samples==256&&device.rateCalls==1&&device.latencyCalls==1,"Device adapter reads exact reported output sample latency");
    check(!readDeviceOutputLatency(static_cast<Device*>(nullptr)).available,"Missing device report remains unavailable");
    device.fail=true;check(!readDeviceOutputLatency(&device).available,"Throwing device getter remains unavailable");
    for(double rate:{8000.0,48000.0})for(unsigned block:{64u,128u}){
        auto a=std::make_unique<Engine>(),b=std::make_unique<Engine>();
        const DeviceOutputLatency reports[]={{true,rate,256},{true,rate,0},{true,rate,-1},{true,rate+1,128},
            {true,std::numeric_limits<double>::quiet_NaN(),128},{false,rate,128},{true,rate,std::numeric_limits<int>::max()}};
        const bool known[]={true,true,false,false,false,false,true};
        std::array<float,128> al{},ar{},bl{},br{};float* ac[]={al.data(),ar.data()};float* bc[]={bl.data(),br.data()};
        auto packet=std::make_unique<MeterWindowEvent>();EngineView view;MeterWindowEvent old;std::uint32_t previous=0;
        for(unsigned index=0;index<std::size(reports);++index){
            a->prepare(rate,8192,reports[index]);b->prepare(rate,8192);a->setTarget(1);b->setTarget(1);
            check(!a->inspectCallbackClockAudioThread().serial,"Prepare does not publish an imaginary latency-bearing callback");
            for(unsigned n=0;n<16;++n){
                al.fill(.2f);ar.fill(-.1f);bl=al;br=ar;watch=true;
                a->requestLoopMeterWindow(19);a->process(ac,2,n%7?block:0,{},n%5==0);b->process(bc,2,n%7?block:0,{},n%5==0);
                check(a->popView(view)&&a->popLoopMeterWindow(*packet)&&sameLoopMeterCallback(*packet,view),"Reported output latency shares exact callback/PCM snapshot");
                const auto& clock=view.callbackClock;
                check(clock.hasOutputLatency==known[index]&&clock.outputLatencySamples==(known[index]?static_cast<std::uint32_t>(reports[index].samples):0),"Known zero, negative/missing/nonfinite/rate-mismatched output latency remain distinct");
                check(al==bl&&ar==br,"Reported output latency never delays/changes rendered PCM");
                if(!n&&previous)check(!sameLoopMeterCallback(old,view),"Reprepare cannot reuse old latency/clock generation");
                auto bad=view;bad.callbackClock.outputLatencySamples^=1;
                check(!sameLoopMeterCallback(*packet,bad),"Different latency report cannot pair with the same callback PCM");
                watch=false;
            }
            previous=a->generation();old=*packet;
        }
    }
    check(allocations.load()==0,"Output latency validation/publication/pairing has zero tracked new/delete");
    std::cout<<"PASS: prepared device output latency, known-zero/missing/error/rate guards, coherent 64/128/zero/offline transfer, reset and bit-identical PCM; zero tracked new/delete\n";
}
