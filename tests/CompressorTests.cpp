// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Compressor.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
namespace {thread_local bool watching=false;std::atomic<unsigned> operations{0};
void check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}}
void* operator new(std::size_t size){if(watching)++operations;if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {if(watching)++operations;std::free(p);}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
int main(){
    using namespace aion;
    Compressor a,b;std::array<float,4099> l{},r{},al{},ar{},bl{},br{};
    for(unsigned i=0;i<l.size();++i){l[i]=static_cast<float>(.6*std::sin(i*.11));r[i]=static_cast<float>(.3*std::cos(i*.073));}
    auto values=compressorDefaults();check(validCompressorValues(values),"Original compressor defaults valid");
    watching=true;
    check(!a.setTargets(values),"Unprepared controls rejected");
    for(double rate:{8000.,16000.,22050.,32000.,44100.,48000.,96000.,192000.,384000.}){
        check(a.prepare(rate)&&b.prepare(rate),"Supported rates prepare");
        check(a.latencyFrames()==std::min(1023u,static_cast<unsigned>(.006f*static_cast<float>(rate))),"Original integer/capped lookahead");
        a.process(l.data(),r.data(),2,al.data(),ar.data(),l.size());
        unsigned pos=0;
        for(unsigned block:{1u,17u,64u,128u,257u,3632u}){
            const auto n=std::min<std::size_t>(block,l.size()-pos);
            b.process(l.data()+pos,r.data()+pos,2,bl.data()+pos,br.data()+pos,n);pos+=static_cast<unsigned>(n);
        }
        check(pos==l.size()&&al==bl&&ar==br,"Variable blocks retain exact continuous compressor clock");
        for(float v:al)check(std::isfinite(v),"Finite stereo rendering");
    }
    check(a.prepare(48000)&&b.prepare(48000),"Prepare paired controls");
    for(unsigned p=0;p<values.size();++p){
        for(float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),compressorParameters[p].minimum-1,compressorParameters[p].maximum+1}){
            auto bad=values;bad[p]=invalid;
            check(!a.prepare(96000,bad)&&!a.setTargets(bad),"Invalid batch is atomic");
            a.process(l.data(),r.data(),2,al.data(),ar.data(),128);b.process(l.data(),r.data(),2,bl.data(),br.data(),128);
            check(std::equal(al.begin(),al.begin()+128,bl.begin())&&std::equal(ar.begin(),ar.begin()+128,br.begin()),"Rejected update preserves delay/envelope");
        }
    }
    for(double rate:{0.,7999.,384001.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()})check(!a.prepare(rate),"Invalid rate rejected");
    check(!a.setTarget(7,1),"Unknown parameter rejected");
    // Automation must also be independent of callback division and channel-linked.
    check(a.prepare(48000)&&b.prepare(48000),"Prepare automated pair");
    for(unsigned change=0;change<20;++change){
        const float thr=-static_cast<float>(change*5);
        check(a.setTarget(0,thr)&&b.setTarget(0,thr),"Rapid threshold accepted");
        a.process(l.data(),r.data(),2,al.data(),ar.data(),4099);
        for(unsigned i=0;i<4099;i+=64)b.process(l.data()+i,r.data()+i,2,bl.data()+i,br.data()+i,std::min(64u,4099-i));
        check(al==bl&&ar==br,"Rapid controls retain exact variable-block stereo output");
    }
    check(a.prepare(48000)&&b.prepare(48000),"Fresh mono and stereo");
    a.process(l.data(),nullptr,1,al.data(),ar.data(),4099);b.process(l.data(),l.data(),2,bl.data(),br.data(),4099);
    check(al==ar&&al==bl&&ar==br,"Original mono duplication and stereo link");
    // NaN/Inf are explicitly quarantined before the shared detector. Subsequent
    // finite signal matches a clean instance receiving zero at those positions.
    for(float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){
        check(a.prepare(48000)&&b.prepare(48000),"Recovery prepare");
        auto bad=l,clean=l;bad[0]=invalid;clean[0]=0;
        a.process(bad.data(),r.data(),2,al.data(),ar.data(),4099);b.process(clean.data(),r.data(),2,bl.data(),br.data(),4099);
        check(al==bl&&ar==br,"Nonfinite PCM cannot poison linked detector/delay");
    }
    // Finite full-range corruption must not strand the gain at zero or poison
    // the reduction meter. Normal audio must recover without another prepare.
    check(a.prepare(8000),"Extreme finite recovery prepare");
    auto huge=l;huge.fill(std::numeric_limits<float>::max());
    for(unsigned i=0;i<64;++i)a.process(huge.data(),huge.data(),2,al.data(),ar.data(),4099);
    for(unsigned i=0;i<64;++i)a.process(l.data(),r.data(),2,al.data(),ar.data(),4099);
    bool nonzero=false;for(float v:al){check(std::isfinite(v),"Recovered output finite");nonzero|=v!=0;}
    check(nonzero&&std::isfinite(a.reductionDb()),"Extreme finite input cannot permanently poison compressor or meter");
    values[6]=0;check(a.prepare(48000,values),"Pure dry");
    a.process(l.data(),r.data(),2,al.data(),ar.data(),4099);check(al==l&&ar==r,"Dry is immediate/uncompensated as original graph");
    a.reset();a.process(nullptr,nullptr,0,al.data(),ar.data(),4099);
    for(unsigned i=0;i<4099;++i)check(al[i]==0&&ar[i]==0,"Reset removes lookahead audio");
    for(unsigned p=1;p<=11;++p){check(compressorPreset(p,values)&&validCompressorValues(values),"Computed original presets valid");}
    auto unchanged=values;check(!compressorPreset(0,values)&&!compressorPreset(12,values)&&values==unchanged,"Invalid preset preserves values");
    watching=false;check(operations==0,"No new/delete in prepare/controls/reset/render");
    std::cout<<"PASS compressor fixed storage, atomic validation, clock subdivision, stereo/mono, dry routing, automation, reset and numeric recovery; zero new/delete\n";
}
