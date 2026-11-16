// SPDX-License-Identifier: AGPL-3.0-only
#include "core/DistortionSettings.h"
#include <bit>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string_view>
using namespace aion;
static void check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
static void values(const DistortionValues& v){std::cout<<'[';for(unsigned p=0;p<v.size();++p){if(p)std::cout<<',';std::cout<<v[p];}std::cout<<']';}
static void catalog(){
    std::cout<<"{\"curveSize\":"<<DistortionCurve{}.size()<<",\"controls\":[";
    for(unsigned p=0;p<distortionControls.size();++p){const auto& c=distortionControls[p];if(p)std::cout<<',';
        std::cout<<"{\"name\":\""<<c.name<<"\",\"initial\":"<<c.initial<<",\"min\":"<<c.minimum<<",\"max\":"<<c.maximum<<",\"step\":"<<c.step<<'}';}
    std::cout<<"],\"presets\":[";
    for(unsigned id=1;id<=distortionPresetNames.size();++id){DistortionSettings s;check(applyDistortionPreset(s,id),"Catalog preset");if(id>1)std::cout<<',';
        std::cout<<"{\"name\":\""<<distortionPresetNames[id-1]<<"\",\"values\":";values(s.values);std::cout<<'}';}
    std::cout<<"]}\n";
}
static void curves(){
    DistortionSettings s;
    while(std::cin>>s.values[0]){
        for(unsigned p=1;p<s.values.size();++p)if(!(std::cin>>s.values[p]))std::exit(2);
        DistortionCurve curve{};
        if(!prepareDistortionCurve(s,curve)){std::cout<<"null\n";continue;}
        constexpr char digits[]="0123456789abcdef";
        for(float value:curve){const auto bits=std::bit_cast<std::uint32_t>(value);
            for(unsigned byte=0;byte<4;++byte){const auto b=(bits>>(byte*8))&255;std::cout<<digits[b>>4]<<digits[b&15];}}
        std::cout<<'\n';
    }
    if(!std::cin.eof())std::exit(2);
}
static void unit(){
    DistortionSettings s;
    check(validDistortionSettings(s)&&!s.enabled&&!s.preset&&s.values[0]==40&&s.values[2]==4000,"Untouched disabled defaults");
    for(unsigned id=1;id<=distortionPresetNames.size();++id){
        for(bool enabled:{false,true}){s={};s.enabled=enabled;check(applyDistortionPreset(s,id)&&s.enabled==enabled&&s.preset==id,"Preset keeps enable");
            const auto old=s;check(!applyDistortionPreset(s,0)&&!applyDistortionPreset(s,7)&&s==old,"Unknown presets atomic");
            check(setDistortionControl(s,0,s.values[0])&&!s.preset&&s.enabled==enabled,"Equal edit clears only preset");}
    }
    s={};check(applyDistortionPreset(s,1)&&s.values[0]==35&&s.values[2]==5000,"Named default differs from untouched defaults");
    for(unsigned p=0;p<distortionControls.size();++p){const auto& c=distortionControls[p];
        for(double bad:{c.minimum-1,c.maximum+1,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}){
            const auto old=s;check(!setDistortionControl(s,p,bad)&&s==old,"Rejected control preserves all settings");
            auto invalid=s;invalid.preset=0;invalid.values[p]=bad;DistortionCurve curve{};curve.fill(.125f);const auto previous=curve;
            check(!validDistortionSettings(invalid)&&!prepareDistortionCurve(invalid,curve)&&curve==previous,"Malformed compile preserves previous complete curve");
        }
        check(setDistortionControl(s,p,c.minimum)&&setDistortionControl(s,p,c.maximum),"Inclusive control bounds");
    }
    const auto old=s;check(!setDistortionControl(s,5,0)&&s==old,"Unknown control atomic");
    s={};s.preset=1;DistortionCurve curve{};curve.fill(.25f);const auto previous=curve;
    check(!validDistortionSettings(s)&&!prepareDistortionCurve(s,curve)&&curve==previous,"Preset mismatch rejected");
    s={};check(setDistortionControl(s,1,.123456789)&&s.values[1]==.123456789,"Continuous controls keep JS doubles, not widget-step quantization");
    for(double amount:{0.,.001,40.,100.})for(double shape:{0.,.5,1.}){
        s={};s.values[0]=amount;s.values[1]=shape;check(prepareDistortionCurve(s,curve),"Compile valid curve");
        check(curve[2048]==0&&!std::signbit(curve[2048]),"Exact positive-zero anchor");
        for(unsigned i=0;i<curve.size();++i){check(std::isfinite(curve[i])&&curve[i]>=-1&&curve[i]<=1,"Finite bounded samples");if(i)check(curve[i]>=curve[i-1],"Monotonic transfer curve");}
        if(amount==0&&shape==0)check(curve.back()<.8f&&curve.back()>.7f,"Amount zero is still tanh, not bypass");
        if(shape==0)for(unsigned i=0;i<curve.size();++i)check(curve[i]==-curve[curve.size()-1-i],"Symmetric shape exact mirror");
    }
    std::cout<<"PASS: distortion defaults/presets, atomic validation, exact-double controls and fixed-storage curve preparation; T audio path remains unported\n";
}
int main(int argc,char** argv){std::cout<<std::setprecision(17);if(argc==1)unit();else if(argc==2&&std::string_view(argv[1])=="catalog")catalog();else if(argc==2&&std::string_view(argv[1])=="curves")curves();else return 2;}
