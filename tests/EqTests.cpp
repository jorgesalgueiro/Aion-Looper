// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Equalizer.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <numbers>

namespace {thread_local bool watching=false;std::atomic<unsigned> operations{0};
void check(bool value,const char* message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}}
void* operator new(std::size_t size){if(watching)++operations;if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {if(watching)++operations;std::free(p);}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}

int main(){
    using namespace aion;
    Equalizer a,b;auto values=eqDefaults();std::array<float,128> left{},right{},al{},ar{},bl{},br{};
    check(validEqValues(values),"Original EQ defaults valid");
    // Configuration and construction-time prepare/reset are also checked, not
    // just steady-state rendering. Invalid batches must preserve both channels.
    watching=true;
    check(!a.configure(values),"Unprepared EQ rejects configure");
    a.process(left.data(),right.data(),al.data(),ar.data(),0);
    check(a.prepare(48000)&&b.prepare(48000),"Prepare stereo EQ");
    values[4]=24;values[5]=10;check(a.configure(values)&&b.configure(values),"Highest gain/Q accepted");
    left[0]=1;right[1]=-.5f;a.process(left.data(),right.data(),al.data(),ar.data(),128);b.process(left.data(),right.data(),bl.data(),br.data(),128);
    for(unsigned index=0;index<values.size();++index){
        for(float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),eqParameters[index].minimum-1,eqParameters[index].maximum+1}){
            auto bad=values;bad[index]=invalid;check(!a.configure(bad),"Invalid EQ batch rejected");
            check(!a.prepare(96000,bad),"Invalid prepare preserves old rate and state");
            a.process(nullptr,nullptr,al.data(),ar.data(),128);b.process(nullptr,nullptr,bl.data(),br.data(),128);
            check(al==bl&&ar==br,"Invalid update preserves complete filter and tail");
        }
    }
    for(double rate:{0.0,7999.0,384001.0,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()})check(!a.prepare(rate),"Invalid sample rate rejected atomically");
    a.process(nullptr,nullptr,al.data(),ar.data(),128);b.process(nullptr,nullptr,bl.data(),br.data(),128);check(al==bl&&ar==br,"Invalid rate retains old filter");
    for(double rate:{8000.0,16000.0,22050.0,32000.0}){
        auto stored=eqDefaults();const auto original=stored;
        check(a.prepare(rate,stored),"Approved frequency limit permits low-rate defaults");
        left.fill(0);left[0]=.1f;
        for(unsigned i=0;i<64;++i){a.process(i?nullptr:left.data(),nullptr,al.data(),ar.data(),128);for(float sample:al)check(std::isfinite(sample),"Low-rate default impulse/tail remains finite");}
        check(stored==original,"Frequency limiting preserves stored values");
        check(effectiveEqFrequency(20000,rate)<rate*.5,"Effective frequency remains strictly below Nyquist");
    }
    // No fabricated stereo duplication: missing right input continues its old
    // right tail; a fresh right channel receiving no input stays exactly silent.
    check(a.prepare(48000),"Fresh channel state");a.process(left.data(),nullptr,al.data(),ar.data(),128);
    for(float value:ar)check(value==0,"Absent fresh right channel stays silent");
    a.reset();left.fill(0);right.fill(0);a.process(left.data(),right.data(),al.data(),ar.data(),128);
    for(unsigned i=0;i<128;++i)check(al[i]==0&&ar[i]==0,"Reset fully removes both tails");
    // NaN/Inf and numeric overflow cannot poison subsequent callbacks. The
    // unaffected stereo channel must remain bit-identical to a clean instance.
    for(float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){
        check(a.prepare(48000)&&b.prepare(48000),"Recovery prepare");left.fill(0);right.fill(0);left[0]=invalid;right[0]=.25f;
        a.process(left.data(),right.data(),al.data(),ar.data(),128);left[0]=0;b.process(left.data(),right.data(),bl.data(),br.data(),128);
        check(ar==br,"Bad input cannot contaminate other channel");
        for(unsigned i=0;i<128;++i)check(std::isfinite(al[i])&&al[i]==0,"Malformed input suppressed and state recovered");
    }
    {
        auto hot=eqDefaults();hot[4]=24;
        check(a.prepare(48000,hot)&&b.prepare(48000,hot),"Finite overflow containment setup");
        left.fill(0);right.fill(0);left[0]=std::numeric_limits<float>::max();right[0]=.25f;
        a.process(left.data(),right.data(),al.data(),ar.data(),128);left[0]=0;b.process(left.data(),right.data(),bl.data(),br.data(),128);
        check(ar==br,"Finite overflow cannot affect the other stereo channel");
        for(float sample:al)check(std::isfinite(sample),"Finite full-scale float overflow never escapes as Inf/NaN");
        for(unsigned i=0;i<64;++i){a.process(nullptr,nullptr,al.data(),ar.data(),128);for(float sample:al)check(std::isfinite(sample),"Overflow tail remains finite");}
    }
    // Block subdivision must not reset filter memories or alter coefficients.
    for(double rate:{8000.0,16000.0,22050.0,32000.0,44100.0,48000.0,96000.0,192000.0,384000.0}){
        auto safe=eqDefaults();safe[23]=static_cast<float>(std::min(20000.0,rate*.4));safe[4]=-24;safe[5]=.1f;
        check(a.prepare(rate,safe)&&b.prepare(rate,safe),"Valid settings across native rates");
        for(unsigned iteration=0;iteration<16;++iteration){
            for(unsigned i=0;i<128;++i){left[i]=static_cast<float>(std::sin((iteration*128+i)*.12)*.2);right[i]=static_cast<float>(std::cos((iteration*128+i)*.21)*.3);}
            a.process(left.data(),right.data(),al.data(),ar.data(),128);
            b.process(left.data(),right.data(),bl.data(),br.data(),64);b.process(left.data()+64,right.data()+64,bl.data()+64,br.data()+64,64);
            check(al==bl&&ar==br,"Exact 64/128 kernel invariance");
            // In-place stereo must read both inputs before either output write.
            auto c=a,d=a;c.process(left.data(),right.data(),al.data(),ar.data(),128);d.process(left.data(),right.data(),left.data(),right.data(),128);check(left==al&&right==ar,"In-place output matches separate buffers");
        }
    }
    for(double rate:{8000.0,22050.0,48000.0,96000.0}){
        SmoothedEqualizer c,d;
        check(!c.setTarget(4,1),"Unprepared smoothing target rejected");
        check(c.prepare(rate)&&d.prepare(rate),"Prepare independent automation instances");
        auto hot=eqDefaults();hot[4]=24;hot[5]=10;hot[22]=-12;
        check(c.setTargets(hot)&&d.setTargets(hot),"Atomic multi-parameter target accepted");
        auto bad=hot;bad.back()=std::numeric_limits<float>::quiet_NaN();
        check(!c.setTargets(bad)&&c.targets()==hot,"Invalid final field cannot partially change prior targets");
        check(!c.setTarget(24,1)&&!c.setTarget(4,25)&&!c.setTarget(4,std::numeric_limits<float>::infinity()),"Invalid individual targets rejected");
        for(unsigned iteration=0;iteration<64;++iteration){
            for(unsigned i=0;i<128;++i){left[i]=static_cast<float>(std::sin((iteration*128+i)*.12)*.2);right[i]=static_cast<float>(std::cos((iteration*128+i)*.21)*.3);}
            c.process(left.data(),right.data(),al.data(),ar.data(),128);
            d.process(left.data(),right.data(),bl.data(),br.data(),37);d.process(left.data()+37,right.data()+37,bl.data()+37,br.data()+37,27);d.process(left.data()+64,right.data()+64,bl.data()+64,br.data()+64,64);
            check(al==bl&&ar==br&&c.currentValues()==d.currentValues(),"Automation independent of 37/27/64 vs 128 device subdivisions");
            if(iteration%8==0){const float gain=iteration%16?-24.f:24.f;check(c.setTarget(4,gain)&&d.setTarget(4,gain),"Rapid signed automation targets");}
        }
        const auto stored=c.targets();c.reset();check(c.targets()==stored&&c.currentValues()==stored,"Reconstruction resets tails/ramps at stored targets");
        check(d.prepare(rate,stored),"Fresh comparison at stored targets");
        c.process(left.data(),right.data(),al.data(),ar.data(),128);d.process(left.data(),right.data(),bl.data(),br.data(),128);check(al==bl&&ar==br,"Reset equals new worklet construction at targets");
    }
    // The future graph must plot this filter, including Nyquist safety, not an
    // unrelated cookbook approximation. Check the state-space response against
    // a DFT of actual Float32 steady-tone output at exact frequency bins. Tiny
    // state flushing is nonlinear: a unit-impulse DFT is not the exact linear
    // transfer function (the 8kHz/bin131 discrepancy is retained in EqParity).
    double responseError=0;
    for(double rate:{8000.0,22050.0,48000.0,96000.0})for(bool peaks:{false,true}){
        auto p=eqDefaults();
        if(peaks){p[3]=static_cast<float>(rate*.04);p[4]=12;p[5]=1.5f;p[12]=static_cast<float>(rate*.13);p[13]=-9;p[14]=.7f;p[21]=static_cast<float>(std::min(22000.0,rate*.3));p[22]=4;}
        check(a.prepare(rate,p),"Response setup");
        constexpr unsigned size=65536;const std::array<unsigned,6> bins{131,431,997,8191,15991,29491};std::array<double,6> real{},imaginary{};
        // Simultaneous low-level tones also check that plotted independent bands
        // describe their combined sound without adding an FFT to product audio.
        constexpr unsigned warm=65536;
        for(unsigned offset=0;offset<size+warm;offset+=128){
            left.fill(0);for(unsigned i=0;i<128;++i)for(unsigned j=0;j<bins.size();++j)left[i]+=static_cast<float>(.03*std::sin(2*std::numbers::pi*bins[j]*(offset+i)/size));
            a.process(left.data(),nullptr,al.data(),nullptr,128);
            if(offset<warm)continue;
            for(unsigned i=0;i<128;++i)for(unsigned j=0;j<bins.size();++j){const double angle=2*std::numbers::pi*bins[j]*(offset+i)/size;real[j]+=al[i]*std::cos(angle);imaginary[j]-=al[i]*std::sin(angle);}
        }
        for(unsigned j=0;j<bins.size();++j){double magnitude=0;check(a.magnitude(rate*bins[j]/size,magnitude),"Finite graph response");const double measured=std::hypot(real[j],imaginary[j])*2/(size*.03),error=std::abs(magnitude-measured);responseError=std::max(responseError,error);if(error>1e-6)std::cerr<<"rate="<<rate<<" peaks="<<peaks<<" bin="<<bins[j]<<" analytic="<<magnitude<<" DFT="<<measured<<" error="<<error<<'\n';check(error<=1e-6,"Curve magnitude matches actual Float32 steady tones within 1e-6");}
        double unchanged=7;check(!a.magnitude(-1,unchanged)&&!a.magnitude(rate,unchanged)&&!a.magnitude(std::numeric_limits<double>::quiet_NaN(),unchanged)&&unchanged==7,"Invalid graph query preserves caller output");
    }
    double halfGainError=0;
    for(double rate:{8000.0,22050.0,44100.0,48000.0,96000.0})for(float frequency:{20.f,1000.f,22000.f})for(float q:{.1f,.707f,1.f,10.f}){
        double low=0,high=0;check(eqPeakRange(frequency,q,rate,low,high)&&low>0&&high>low&&high<rate*.5,"Peak range uses valid warped frequency edges");
        float inverse=0;check(eqPeakQ(frequency,low,rate,inverse)&&std::abs(inverse-q)<=1e-5f,"Dragging low edge recovers original Q");check(eqPeakQ(frequency,high,rate,inverse)&&std::abs(inverse-q)<=1e-5f,"Dragging high edge recovers original Q");
        // A self-consistent forward/inverse mapping alone is insufficient: the
        // displayed width must coincide with the filter's actual half-gain.
        for(float gain:{-12.f,12.f}){
            auto p=eqDefaults();p[3]=frequency;p[4]=gain;p[5]=q;check(a.prepare(rate,p),"Half-gain peak setup");p[4]=0;check(b.prepare(rate,p),"Half-gain baseline cuts");
            for(double edge:{low,high}){double active=0,base=0;check(a.magnitude(edge,active)&&b.magnitude(edge,base)&&base>0,"Finite half-gain response");const double error=std::abs(active/base-std::pow(10.0,gain/40.0));halfGainError=std::max(halfGainError,error);check(error<=1e-6,"Graph bandwidth edges must match actual half-gain response");}
        }
    }
    {
        double low=7,high=8;float q=3;
        check(!eqPeakRange(0,1,48000,low,high)&&low==7&&high==8,"Invalid bandwidth query atomic");
        check(!eqPeakQ(1000,24000,48000,q)&&q==3,"Invalid Nyquist edge rejected");
    }
    for(unsigned origin:{1u,37u,63u,127u,129u}){
        SmoothedEqualizer continuous,attached;check(continuous.prepare(48000)&&attached.prepare(48000,eqDefaults(),origin),"Attach EQ to nonzero global render quantum");
        al.fill(0);ar.fill(0);continuous.process(nullptr,nullptr,al.data(),ar.data(),origin>128?128:origin);
        if(origin>128)continuous.process(nullptr,nullptr,al.data(),ar.data(),origin-128);
        check(continuous.setTarget(4,12)&&attached.setTarget(4,12),"Same global target offset");left.fill(.1f);right.fill(-.2f);
        for(unsigned n=0;n<16;++n){continuous.process(left.data(),right.data(),al.data(),ar.data(),128);attached.process(left.data(),right.data(),bl.data(),br.data(),128);check(al==bl&&ar==br,"Nonzero attachment retains global 128-frame coefficient clock");}
    }
    watching=false;check(operations.load()==0,"EQ prepare/configure/reset/process must never allocate or free");
    std::cout<<"PASS: EQ atomic validation, finite recovery/overflow, stereo/tails/reset, exact 64/128 and in-place, curve/steady-tone DFT max "<<responseError<<", bandwidth half-gain max "<<halfGainError<<", handle inverse, zero new/delete\n";
}
