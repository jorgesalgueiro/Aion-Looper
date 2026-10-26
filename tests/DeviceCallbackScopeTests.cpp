#include "core/DeviceCallbackScope.h"
#include "core/Engine.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <thread>
extern thread_local bool watch;
extern std::atomic<int> allocations;
namespace {void check(bool ok,const char* text){if(!ok){std::cerr<<text<<'\n';std::exit(1);}}}
void deviceCallbackScopeTests(){
    using namespace aion;
    DeviceCallbackBinding owner,other;owner.advance();other.advance();
    watch=true;
    check(!DeviceCallbackScope::take(&owner,64,owner.epoch()).available,"No IO scope means no delivery observation");
    {
        DeviceCallbackScope outer(&owner,64,owner.epoch(),0);
        check(!DeviceCallbackScope::take(&other,64,other.epoch()).available,"Wrong owner cannot steal IO scope");
        {DeviceCallbackScope inner(&other,128,other.epoch(),5);check(DeviceCallbackScope::take(&other,128,other.epoch()).seconds==5,"Nested IO scope has its own owner and time");}
        const auto entry=DeviceCallbackScope::take(&owner,64,owner.epoch());
        check(entry.available&&entry.seconds==0&&entry.serial&&entry.frames==64,"Nested return restores unconsumed outer scope, including known zero");
        check(!DeviceCallbackScope::take(&owner,64,owner.epoch()).available,"One IO observation cannot feed two processor blocks");
    }
    {DeviceCallbackScope scope(&owner,128,owner.epoch(),1);check(!DeviceCallbackScope::take(&owner,64,owner.epoch()).available&&!DeviceCallbackScope::take(&owner,128,owner.epoch()).available,"Split/mismatched blocks consume and reject the owner observation");}
    {
        const auto old=owner.epoch();DeviceCallbackScope scope(&owner,64,old,1);owner.advance();
        check(!DeviceCallbackScope::take(&owner,64,owner.epoch()).available&&!DeviceCallbackScope::take(&owner,64,old).available,"Prepare invalidates both old and newly cached claims on a waiting scope");
    }
    {
        const auto old=owner.epoch();owner.advance();DeviceCallbackScope scope(&owner,64,old,2);
        check(!DeviceCallbackScope::take(&owner,64,owner.epoch()).available,"Prepare between epoch snapshot and clock read cannot relabel the old scope");
    }
    for(double bad:{-1.0,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){
        DeviceCallbackScope scope(&owner,64,owner.epoch(),bad);check(DeviceCallbackScope::take(&owner,64,owner.epoch())==DeviceCallbackObservation{},"Invalid IO clocks clear availability and all fields");
    }
    for(int frames:{0,-1}){DeviceCallbackScope scope(&owner,frames,owner.epoch(),1);check(!DeviceCallbackScope::take(&owner,frames,owner.epoch()).available,"Empty/invalid IO block cannot anchor delivered PCM");}
    watch=false;
    {
        DeviceCallbackScope scope(&owner,64,owner.epoch(),10);
        std::thread worker([&]{check(!DeviceCallbackScope::take(&owner,64,owner.epoch()).available,"IO scopes never cross audio threads");});worker.join();
        watch=true;check(DeviceCallbackScope::take(&owner,64,owner.epoch()).available,"Other thread cannot consume current audio owner's scope");watch=false;
    }
    for(double rate:{8000.0,48000.0})for(unsigned block:{64u,128u}){
        auto a=std::make_unique<Engine>(),b=std::make_unique<Engine>();a->prepare(rate,8192,{true,rate,256});b->prepare(rate,8192,{true,rate,256});
        a->setMonitoring(true);b->setMonitoring(true);a->setTarget(1);b->setTarget(1);
        std::array<float,128> al{},ar{},bl{},br{};float* ac[]={al.data(),ar.data()};float* bc[]={bl.data(),br.data()};
        auto packet=std::make_unique<MeterWindowEvent>();EngineView view;
        std::uint64_t previous=0;
        watch=true;
        for(unsigned n=0;n<32;++n){
            const unsigned frames=n%7?block:0;al.fill(.2f);ar.fill(-.1f);bl=al;br=ar;
            DeviceCallbackScope scope(&owner,static_cast<int>(frames),owner.epoch(),100+static_cast<double>(n)*.01);
            const auto entry=DeviceCallbackScope::take(&owner,static_cast<int>(frames),owner.epoch());
            check(a->requestLoopMeterWindow(n%20),"Request IO-correlated meter PCM");
            a->process(ac,2,static_cast<int>(frames),{},n%5==0,{200+static_cast<double>(n),true},entry);
            b->process(bc,2,static_cast<int>(frames),{},n%5==0);
            check(al==bl&&ar==br,"Device-entry metadata leaves actual monitored PCM bit-identical");
            check(a->popView(view)&&a->popLoopMeterWindow(*packet)&&sameLoopMeterCallback(*packet,view),"IO entry transferred with the exact callback and PCM");
            const auto stamp=view.callbackClock;
            check(stamp.deviceEntry==entry&&validCallbackClock(stamp)&&stamp.deviceEntry.available==(frames>0),"IO interval, optional clock and full tuple remain coherent");
            if(frames){check(entry.serial!=previous&&entry.seconds<stamp.processorEntry.seconds,"Device clock remains distinct from later processor entry");previous=entry.serial;}
            auto bad=view;bad.callbackClock.deviceEntry.serial++;check(!sameLoopMeterCallback(*packet,bad),"Different IO callback cannot match a PCM window");
        }
        a->process(ac,2,static_cast<int>(block),{},false,{},DeviceCallbackObservation{1,1,block+1,true});
        check(a->inspectCallbackClockAudioThread().deviceEntry==DeviceCallbackObservation{},"Engine also rejects mismatched device block lengths");
        watch=false;a->prepare(rate,8192);
        check(!a->inspectCallbackClockAudioThread().deviceEntry.available,"Prepare clears the old IO callback observation");
    }
    check(allocations.load()==0,"Device scope, matching and publication have zero tracked new/delete");
    std::cout<<"PASS: scoped device entry, single-use owner/frame/thread/nesting guards, 64/128/zero/offline PCM transfer/reset; zero tracked new/delete\n";
}
