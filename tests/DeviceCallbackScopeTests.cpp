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
namespace {
void deliveryScopeTests(){
    using namespace aion;
    DeviceCallbackBinding device,other,processor;device.advance();other.advance();processor.advance();
    int key=0,otherKey=0;
    watch=true;
    check(!DeviceDeliveryScope::read(&key,64).available,"No enclosing callback cannot invent delivery metadata");
    {
        DeviceDeliveryScope outer{&key,&device,129,device.epoch(),DeviceCallbackBinding::configurationEpoch(),0};
        check(!DeviceDeliveryScope::read(&key,64).available,"Outer callback alone has no split range");
        DeviceDeliveryScope::Block block{64,64};
        const auto entry=DeviceDeliveryScope::read(&key,64);
        check(entry.available&&entry.seconds==0&&entry.serial&&entry.frames==129&&entry.frameOffset==64,"Known-zero enclosing clock carries exact partial range");
        check(!DeviceDeliveryScope::read(&otherKey,64).available&&!DeviceDeliveryScope::read(&key,63).available,"Enclosing owner and actual block length must match");
        {DeviceDeliveryScope::Block nested{128,1};check(DeviceDeliveryScope::read(&key,1).frameOffset==128,"Partial tail has exact offset");}
        check(DeviceDeliveryScope::read(&key,64)==entry,"Nested ranges restore previous observation");
        {DeviceDeliveryScope nested{&otherKey,&other,64,other.epoch(),DeviceCallbackBinding::configurationEpoch(),1};DeviceDeliveryScope::Block inner{0,64};check(!DeviceDeliveryScope::read(&key,64).available&&DeviceDeliveryScope::read(&otherKey,64).available,"Nested devices cannot borrow enclosing owner");}
        check(DeviceDeliveryScope::read(&key,64)==entry,"Nested devices restore original delivery tuple");
        {DeviceCallbackScope holder{&processor,64,processor.epoch(),1,&key};check(DeviceCallbackScope::take(&processor,64,processor.epoch()).delivery==entry,"Holder captures current enclosing delivery and exact sub-block");}
        {DeviceDeliveryScope::Block bad{128,2};check(!DeviceDeliveryScope::read(&key,2).available,"Out-of-range tail rejected");}
        {DeviceDeliveryScope::Block bad{-1,64};check(!DeviceDeliveryScope::read(&key,64).available,"Negative split offset rejected");}
        {DeviceCallbackScope holder{&processor,64,processor.epoch(),1,&key};other.advance();const auto observed=DeviceCallbackScope::take(&processor,64,processor.epoch());check(observed.available&&!observed.delivery.available,"Preparation during holder wait invalidates enclosing metadata without inventing a new edge");}
        check(!DeviceDeliveryScope::read(&key,64).available,"Configuration change invalidates all pending enclosing observations");
    }
    {
        const auto configuration=DeviceCallbackBinding::configurationEpoch();processor.advance();
        DeviceDeliveryScope stale{&key,&device,64,device.epoch(),configuration,2};DeviceDeliveryScope::Block block{0,64};
        check(!DeviceDeliveryScope::read(&key,64).available,"Prepare between epoch snapshot and clock read rejects enclosing capture");
    }
    {
        DeviceDeliveryScope outer{&key,&device,64,device.epoch(),DeviceCallbackBinding::configurationEpoch(),3};DeviceDeliveryScope::Block block{0,64};
        device.advance();check(!DeviceDeliveryScope::read(&key,64).available,"Device stop invalidates enclosing lifecycle before callback exits");
    }
    for(double bad:{-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
        DeviceDeliveryScope outer{&key,&device,64,device.epoch(),DeviceCallbackBinding::configurationEpoch(),bad};DeviceDeliveryScope::Block block{0,64};check(!DeviceDeliveryScope::read(&key,64).available,"Invalid enclosing clocks never escape");
    }
    for(int frames:{0,-1}){DeviceDeliveryScope outer{&key,&device,frames,device.epoch(),DeviceCallbackBinding::configurationEpoch(),1};DeviceDeliveryScope::Block block{0,64};check(!DeviceDeliveryScope::read(&key,64).available,"Empty/negative device buffers have no delivery observation");}
    watch=false;
    {DeviceDeliveryScope outer{&key,&device,64,device.epoch(),DeviceCallbackBinding::configurationEpoch(),4};DeviceDeliveryScope::Block block{0,64};std::thread worker([&]{check(!DeviceDeliveryScope::read(&key,64).available,"Enclosing delivery cannot cross audio threads");});worker.join();check(DeviceDeliveryScope::read(&key,64).available,"Other thread cannot alter enclosing observation");}
    std::cout<<"PASS: enclosing delivery owner/range/thread/nesting/prepare/stop guards; zero tracked new/delete\n";
}
}
void deviceCallbackScopeTests(){
    using namespace aion;
    deliveryScopeTests();
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
            DeviceDeliveryScope outer{&owner,&owner,static_cast<int>(frames*2),owner.epoch(),DeviceCallbackBinding::configurationEpoch(),50+static_cast<double>(n)*.01};
            DeviceDeliveryScope::Block part{static_cast<int>(frames),static_cast<int>(frames)};
            DeviceCallbackScope scope(&owner,static_cast<int>(frames),owner.epoch(),100+static_cast<double>(n)*.01,&owner);
            const auto entry=DeviceCallbackScope::take(&owner,static_cast<int>(frames),owner.epoch());
            check(a->requestLoopMeterWindow(n%20),"Request IO-correlated meter PCM");
            a->process(ac,2,static_cast<int>(frames),{},n%5==0,{200+static_cast<double>(n),true},entry);
            b->process(bc,2,static_cast<int>(frames),{},n%5==0);
            check(al==bl&&ar==br,"Device-entry metadata leaves actual monitored PCM bit-identical");
            check(a->popView(view)&&a->popLoopMeterWindow(*packet)&&sameLoopMeterCallback(*packet,view),"IO entry transferred with the exact callback and PCM");
            const auto stamp=view.callbackClock;
            check(stamp.deviceEntry==entry&&validCallbackClock(stamp)&&stamp.deviceEntry.available==(frames>0),"IO interval, optional clock and full tuple remain coherent");
            check(entry.delivery.available==(frames>0)&&(!frames||(entry.delivery.frames==frames*2&&entry.delivery.frameOffset==frames)),"Enclosing range survives actual Engine view/requested-PCM publication");
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
