// SPDX-License-Identifier: AGPL-3.0-only
#include "core/DistortionWaveShaper.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <string_view>
#include <vector>
namespace {thread_local bool watching=false;std::atomic<unsigned> operations{0};
void check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}}
void* operator new(std::size_t n){if(watching)++operations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{if(watching)++operations;std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p) noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept{::operator delete(p);}
using namespace aion;
static void probe(bool none){
    unsigned frames,count,block;DistortionSettings settings;
    if(!(std::cin>>frames>>count>>block>>settings.values[0]>>settings.values[1])||!frames||frames>131072||count>2||!block||block>4096)std::exit(2);
    DistortionCurve curve{};if(!prepareDistortionCurve(settings,curve))std::exit(2);
    std::vector<float> l(frames),r(frames),ol(frames),orr(frames);
    for(unsigned i=0;i<frames;++i){if(count&&!(std::cin>>l[i]))std::exit(2);if(count==2&&!(std::cin>>r[i]))std::exit(2);}
    auto shaper=std::make_unique<DistortionWaveShaper>();shaper->prepare();
    for(unsigned first=0;first<frames;first+=block){const auto n=std::min(block,frames-first);
        if(none)for(unsigned i=first;i<first+n;++i){ol[i]=DistortionWaveShaper::lookup(curve,l[i]);orr[i]=DistortionWaveShaper::lookup(curve,count==2?r[i]:l[i]);}
        else check(shaper->process(curve,l.data()+first,r.data()+first,count,ol.data()+first,orr.data()+first,n),"Probe process");
    }
    std::cout<<std::setprecision(17);
    for(unsigned i=0;i<frames;++i){if(i)std::cout<<' ';std::cout<<ol[i]<<' '<<orr[i];}std::cout<<'\n';
}
int main(int argc,char** argv){
    if(argc==3&&std::string_view(argv[1])=="probe"){probe(std::string_view(argv[2])=="none");return 0;}
    if(argc!=1)return 2;
    constexpr unsigned frames=8192;
    std::vector<float> l(frames),r(frames),ol(frames),orr(frames),bl(frames),br(frames),zero(frames);
    for(unsigned i=0;i<frames;++i){l[i]=static_cast<float>(.7*std::sin(i*.071));r[i]=static_cast<float>(.2*std::cos(i*.113));}
    DistortionSettings settings;DistortionCurve curve{};check(prepareDistortionCurve(settings,curve),"Prepared original curve");
    auto a=std::make_unique<DistortionWaveShaper>(),b=std::make_unique<DistortionWaveShaper>();
    check(!a->process(curve,l.data(),r.data(),2,ol.data(),orr.data(),64),"Unprepared processing rejected");
    a->prepare();b->prepare();
    watching=true;
    for(unsigned first=0;first<frames;first+=128)check(a->process(curve,l.data()+first,r.data()+first,2,ol.data()+first,orr.data()+first,128),"128 processing");
    for(unsigned first=0;first<frames;first+=64)check(b->process(curve,l.data()+first,r.data()+first,2,bl.data()+first,br.data()+first,64),"64 processing");
    check(ol==bl&&orr==br,"64/128 subdivision bit identity");
    b->reset();unsigned first=0,k=0;constexpr unsigned chunks[]{1,7,63,128,129,4096};
    while(first<frames){const auto n=std::min(chunks[k++%6],frames-first);check(b->process(curve,l.data()+first,r.data()+first,2,bl.data()+first,br.data()+first,n),"Irregular processing");first+=n;}
    check(ol==bl&&orr==br,"Irregular subdivision bit identity");watching=false;
    auto inL=l,inR=r;b->reset();watching=true;
    for(unsigned pos=0;pos<frames;pos+=64)check(b->process(curve,inL.data()+pos,inR.data()+pos,2,inL.data()+pos,inR.data()+pos,64),"In-place processing");
    check(ol==inL&&orr==inR,"Matching in-place stereo identity");
    a->reset();b->reset();check(a->process(curve,l.data(),r.data(),2,ol.data(),orr.data(),128),"Prime tail");
    check(b->process(curve,l.data(),r.data(),2,bl.data(),br.data(),128),"Prime control tail");
    check(a->process(curve,nullptr,nullptr,0,ol.data(),orr.data(),1024)&&b->process(curve,zero.data(),zero.data(),2,bl.data(),br.data(),1024),"No input drains FIR history");
    check(std::equal(ol.begin(),ol.begin()+1024,bl.begin())&&std::equal(orr.begin(),orr.begin()+1024,br.begin()),"Tail drainage equals actual zero input");
    a->reset();check(a->process(curve,nullptr,nullptr,0,ol.data(),orr.data(),1024),"Explicit reset clears tail");
    check(std::all_of(ol.begin(),ol.begin()+1024,[](float v){return v==0;}),"Reset exact silence");
    check(a->process(curve,l.data(),nullptr,1,ol.data(),orr.data(),128)&&std::equal(ol.begin(),ol.begin()+128,orr.begin()),"Mono duplicates stereo");
    a->reset();b->reset();
    check(!a->process(curve,l.data(),r.data(),3,ol.data(),orr.data(),64)&&!a->process(curve,l.data(),r.data(),2,ol.data(),orr.data(),4097)
        &&!a->process(curve,nullptr,r.data(),2,ol.data(),orr.data(),64)&&!a->process(curve,l.data(),r.data(),2,ol.data(),ol.data(),64),"Invalid arguments reject");
    check(a->process(curve,l.data(),r.data(),2,ol.data(),orr.data(),64)&&b->process(curve,l.data(),r.data(),2,bl.data(),br.data(),64)
        &&std::equal(ol.begin(),ol.begin()+64,bl.begin()),"Rejected calls preserve history");
    check(a->process(curve,nullptr,nullptr,0,nullptr,nullptr,0),"Zero-size block");watching=false;
    l.assign(frames,0);r=l;l[0]=.0001f;settings.values[1]=0;check(prepareDistortionCurve(settings,curve),"Symmetric impulse curve");a->reset();
    check(a->process(curve,l.data(),r.data(),2,ol.data(),orr.data(),1024),"Impulse");
    const auto peak=std::max_element(ol.begin(),ol.begin()+1024,[](float x,float y){return std::abs(x)<std::abs(y);});
    check(static_cast<unsigned>(peak-ol.begin())==DistortionWaveShaper::wetLatencyFrames,"Original wet-path group delay is 192 base-rate frames");
    l[0]=std::numeric_limits<float>::quiet_NaN();l[1]=std::numeric_limits<float>::infinity();l[2]=std::numeric_limits<float>::max();l[3]=-l[2];a->reset();
    watching=true;check(a->process(curve,l.data(),r.data(),2,ol.data(),orr.data(),1024),"Non-finite/huge input safety");watching=false;
    check(std::all_of(ol.begin(),ol.begin()+1024,[](float v){return std::isfinite(v);}),"No poisoned FIR output");
    check(operations.load()==0,"No callback new/delete");
    std::cout<<"PASS: 4x WaveShaper fixed-state stereo/mono/silence, 64/128/irregular/in-place identity, reset/tails, 192-frame wet delay, rejection/non-finite safety and zero callback new/delete\n";
}
