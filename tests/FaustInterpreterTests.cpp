// SPDX-License-Identifier: AGPL-3.0-only
#include "faust/FaustInterpreter.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <thread>
#include <vector>
namespace {
thread_local bool watching=false;
std::atomic<unsigned> allocations{0},deallocations{0};
std::atomic<unsigned> heapCalls{0};
void require(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
}
#if AION_FAUST_WRAP_ALLOCATOR
extern "C" {
void* __real_malloc(std::size_t);
void* __real_calloc(std::size_t,std::size_t);
void* __real_realloc(void*,std::size_t);
void __real_free(void*);
void* __wrap_malloc(std::size_t n){if(watching)++heapCalls;return __real_malloc(n);}
void* __wrap_calloc(std::size_t n,std::size_t size){if(watching)++heapCalls;return __real_calloc(n,size);}
void* __wrap_realloc(void* p,std::size_t n){if(watching)++heapCalls;return __real_realloc(p,n);}
void __wrap_free(void* p){if(watching)++heapCalls;__real_free(p);}
}
#endif
void* operator new(std::size_t n){if(watching)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{if(watching)++deallocations;std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p) noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept{::operator delete(p);}
using aion::FaustInterpreter;
int main(int argc,char** argv){
    require(argc==2,"Expected Faust library directory");
    std::string error;
    const std::string gain="g=hslider(\"gain\",0.5,0,2,0.01); process=(_*g),(_*g);";
    auto compile=[&](const std::string& source,int rate=48000){
        auto p=FaustInterpreter::compile(source,rate,argv[1],error);
        if(!p){std::cerr<<error<<'\n';require(false,"Faust preparation");}return p;
    };
    auto a=compile(gain),b=compile(gain);
    require(a->inputs()==2&&a->outputs()==2&&a->controls().size()==1,"Stereo/control metadata");
    require(a->controls()[0].initial==.5f&&a->controls()[0].minimum==0&&a->controls()[0].maximum==2,"Control bounds/defaults");
    require(a->controls()[0].path.find("gain")!=std::string::npos,"Stable Faust path");
    std::array<float,4096> l{},r{},ol{},orr{},bl{},br{};
    for(unsigned i=0;i<4096;++i){l[i]=static_cast<float>(std::sin(i*.07));r[i]=static_cast<float>(.25*std::cos(i*.11));}
    const float* in[]{l.data(),r.data()};float* out[]{ol.data(),orr.data()},*other[]{bl.data(),br.data()};
    watching=true;
    require(a->process(in,out,64),"First un-warmed callback");
    for(unsigned i=0;i<64;++i)require(ol[i]==l[i]*.5f&&orr[i]==r[i]*.5f,"Exact stereo gain");
    require(a->setControl(0,1.25f)&&a->process(in,out,128),"Live control and 128 callback");
    require(b->process(in,other,128),"Independent cached-factory instance");
    for(unsigned i=0;i<128;++i)require(ol[i]==l[i]*1.25f&&br[i]==r[i]*.5f,"Independent control zones");
    require(!a->setControl(1,1)&&!a->setControl(0,3)&&!a->setControl(0,std::numeric_limits<float>::quiet_NaN()),"Reject invalid controls");
    require(!a->process(in,out,4097)&&!a->process(nullptr,out,64)&&!a->process(in,nullptr,64),"Reject buffers/bounds");
    require(a->process(nullptr,nullptr,0),"Zero-frame no-op");
    float* alias[]{l.data(),r.data()};require(a->process(in,alias,4096),"Maximum block, matching in-place");
    l[0]=std::numeric_limits<float>::infinity();r[0]=std::numeric_limits<float>::quiet_NaN();
    require(a->process(in,out,1)&&ol[0]==0&&orr[0]==0,"Sanitize nonfinite input");
    watching=false;
    a.reset(); // Deleting one factory reference must not invalidate another.
    watching=true;require(b->process(in,other,64),"Factory reference retained");watching=false;
    for(const auto& bad:std::vector<std::string>{"invalid syntax !", "process=_,_,_;", "process=();", "process=soundfile(\"file\",1);"})
        require(!FaustInterpreter::compile(bad,48000,argv[1],error)&&!error.empty(),"Malformed/unsupported DSP rejected");
    require(!FaustInterpreter::compile(gain,0,argv[1],error),"Invalid sample rate");
    require(!FaustInterpreter::compile(std::string(65537,' '),48000,argv[1],error),"Source bound");
    std::string tooMany="process=";for(unsigned i=0;i<65;++i){if(i)tooMany+='+';tooMany+="hslider(\"c"+std::to_string(i)+"\",0,0,1,0.01)";}tooMany+=';';
    require(!FaustInterpreter::compile(tooMany,48000,argv[1],error),"Control count bound");
    require(b->process(in,other,128),"Rejected compilation preserves running instance");
    const std::string delay="import(\"stdfaust.lib\"); process=de.delay(1024,32);";
    auto d64=compile(delay),d128=compile(delay),odd=compile(delay);
    require(d64->inputs()==1&&d64->outputs()==1,"Standard library import");
    l.fill(0);l[0]=1;
    watching=true;
    for(unsigned pos=0;pos<4096;pos+=64){const float* ii[]{l.data()+pos};float* oo[]{ol.data()+pos};require(d64->process(ii,oo,64),"64 delay");}
    for(unsigned pos=0;pos<4096;pos+=128){const float* ii[]{l.data()+pos};float* oo[]{bl.data()+pos};require(d128->process(ii,oo,128),"128 delay");}
    unsigned pos=0,k=0;constexpr unsigned chunks[]{1,7,63,129,512};
    while(pos<4096){auto n=std::min(chunks[k++%5],4096-pos);const float* ii[]{l.data()+pos};float* oo[]{br.data()+pos};require(odd->process(ii,oo,n),"Variable delay");pos+=n;}
    require(ol==bl&&ol==br&&ol[32]==1,"Delay subdivision bit identity");
    for(unsigned i=0;i<4096;++i)require(ol[i]==(i==32?1.f:0.f),"Exact delay/zero tail");
    watching=false;require(d64->clearOffAudio(),"Explicit off-audio clear");
    auto feedback=compile("process=+~*(0.5);");
    watching=true;require(feedback->process(in,out,64),"Persistent recursive DSP");
    for(unsigned i=0;i<64;++i)require(ol[i]==std::ldexp(1.f,-static_cast<int>(i)),"Feedback impulse");watching=false;
    auto fault=compile("g=hslider(\"divisor\",1,0,1,1); process=_/g;");
    require(fault->setControl(0,0),"Arm numerical fault");l.fill(1);
    watching=true;
    require(!fault->process(in,out,64)&&fault->faulted(),"Nonfinite output latches fault");
    for(unsigned i=0;i<64;++i)require(ol[i]==0,"Fault silences whole callback");
    require(fault->setControl(0,1)&&!fault->process(in,out,128),"Fault remains latched");
    watching=false;require(fault->clearOffAudio(),"Explicit fault recovery");
    watching=true;require(fault->process(in,out,128)&&ol[0]==1,"Recovered DSP");watching=false;
    // Compilation/cache calls on a worker may run while another instance computes.
    std::atomic<bool> done{false},success{false};
    std::thread worker([&]{std::string e;auto p=FaustInterpreter::compile(gain,44100,argv[1],e);success=!!p;done=true;});
    watching=true;do{require(b->process(in,other,64),"Compute during worker compile");}while(!done.load());watching=false;
    worker.join();require(success,"Worker compilation");
    auto synth=compile("process=0.25,-0.5;",96000);
    watching=true;require(synth->process(nullptr,out,128)&&ol[0]==.25f&&orr[0]==-.5f,"Zero-input generator");watching=false;
    for(const auto& tableSource:{"process=int(_) : rdtable(waveform{0.25,-0.5,1.0,0.0});", "process=int(_) : rdtable(waveform{1,2,-3,0});"}){
        auto table=compile(tableSource);
        // Determine the explicitly written table rather than infer storage type.
        const bool fractional=std::string_view(tableSource).find("0.25")!=std::string_view::npos;
        const auto expected=fractional?std::array<float,4>{.25f,-.5f,1,0}:std::array<float,4>{1,2,-3,0};
        for(unsigned i=0;i<128;++i)l[i]=static_cast<float>(i%4);
        watching=true;require(table->process(in,out,128),"Prepared constant waveform");
        for(unsigned i=0;i<128;++i)require(ol[i]==expected[i%4],"Exact constant waveform samples");
        watching=false;
    }
    // Exercise a real standard-library nonlinear implementation, not only
    // arithmetic kernels. This is an embedding probe, not sound-quality approval.
    const std::string nonlinear="import(\"stdfaust.lib\"); process=aa.tanh1,aa.tanh1;";
    auto nl=compile(nonlinear),nlOther=compile(nonlinear);
    for(unsigned i=0;i<4096;++i){l[i]=static_cast<float>(1.5*std::sin(i*.17));r[i]=static_cast<float>(.25+std::cos(i*.27));}
    watching=true;
    for(unsigned p=0;p<4096;p+=64){const float* ii[]{l.data()+p,r.data()+p};float* oo[]{ol.data()+p,orr.data()+p};require(nl->process(ii,oo,64),"Imported ADAA 64 processing");}
    for(unsigned p=0;p<4096;p+=128){const float* ii[]{l.data()+p,r.data()+p};float* oo[]{bl.data()+p,br.data()+p};require(nlOther->process(ii,oo,128),"Imported ADAA 128 processing");}
    require(ol==bl&&orr==br,"Imported nonlinear subdivision identity");watching=false;
    for(int rate:{8000,22050,32000,44100,48000,96000,192000}){
        auto rateDSP=compile(gain,rate);
        watching=true;require(rateDSP->process(in,out,64)&&rateDSP->process(in,out,128),"Prepared sample-rate range");watching=false;
    }
    for(auto block:{64u,128u}){
      for(auto* processor:{b.get(),nl.get()}){
        constexpr unsigned iterations=3000;double worst=0,total=0;
        for(unsigned i=0;i<iterations;++i){watching=true;const auto start=std::chrono::steady_clock::now();
            const bool ok=processor->process(in,other,block);const auto end=std::chrono::steady_clock::now();watching=false;
            require(ok,"Benchmark callback");const double us=std::chrono::duration<double,std::micro>(end-start).count();total+=us;worst=std::max(worst,us);}
        std::cout<<(processor==b.get()?"Stereo gain ":"Stereo ADAA probe ")<<block<<" frames: mean "<<total/iterations<<" us, worst "<<worst<<" us; 48k deadline "<<block/48.0<<" ms\n";
      }
    }
    require(allocations==0&&deallocations==0,"Callback new/delete detected");
    require(heapCalls==0,"Callback wrapped heap call detected");
    std::cout<<"Faust interpreter contract PASS; callback new="<<allocations<<" delete="<<deallocations;
#if AION_FAUST_WRAP_ALLOCATOR
    std::cout<<" wrapped malloc/calloc/realloc/free="<<heapCalls;
#endif
    std::cout<<" (not lock or shared-library internal-allocation instrumentation)\n";
}
