// SPDX-License-Identifier: AGPL-3.0-only
#include "FaustInterpreter.h"
#include <faust/dsp/interpreter-dsp.h>
#include <faust/gui/PathBuilder.h>
#include <faust/gui/UI.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <mutex>

namespace aion {
namespace {
// All libfaust factory/cache calls are serialized OFF audio. compute never calls
// this mutex or the factory API; each live instance retains its factory reference.
std::mutex& compilerMutex(){static std::mutex mutex;return mutex;}
struct ControlCollector final:UI,PathBuilder {
    std::vector<FaustControl> controls;
    std::array<float*,FaustInterpreter::maxControls> zones{};
    unsigned depth=0;
    bool invalid=false;
    ControlCollector(){controls.reserve(FaustInterpreter::maxControls);}
    void open(const char* label){if(++depth<=32)pushLabel(label?label:"");else invalid=true;}
    void openTabBox(const char* s) override{open(s);}
    void openHorizontalBox(const char* s) override{open(s);}
    void openVerticalBox(const char* s) override{open(s);}
    void closeBox() override{if(!depth){invalid=true;return;}if(depth<=32)popLabel();--depth;}
    void add(const char* label,float* zone,float initial,float low,float high,float step){
        if(invalid)return;
        const auto path=buildPath(label?label:"");
        if(!zone||path.size()>255||controls.size()>=FaustInterpreter::maxControls
            ||!std::isfinite(initial)||!std::isfinite(low)||!std::isfinite(high)||!std::isfinite(step)
            ||low>high||initial<low||initial>high||step<0
            ||std::any_of(controls.begin(),controls.end(),[&](const auto& c){return c.path==path;})) {invalid=true;return;}
        zones[controls.size()]=zone;controls.push_back({path,initial,low,high,step});
    }
    void addButton(const char* s,float* z) override{add(s,z,0,0,1,1);}
    void addCheckButton(const char* s,float* z) override{add(s,z,0,0,1,1);}
    void addVerticalSlider(const char* s,float* z,float v,float l,float h,float step) override{add(s,z,v,l,h,step);}
    void addHorizontalSlider(const char* s,float* z,float v,float l,float h,float step) override{add(s,z,v,l,h,step);}
    void addNumEntry(const char* s,float* z,float v,float l,float h,float step) override{add(s,z,v,l,h,step);}
    void addHorizontalBargraph(const char*,float*,float,float) override{}
    void addVerticalBargraph(const char*,float*,float,float) override{}
    void addSoundfile(const char*,const char*,Soundfile**) override{invalid=true;}
};
}
struct FaustInterpreter::Impl {
    interpreter_dsp_factory* factory=nullptr;
    interpreter_dsp* dsp=nullptr;
    ControlCollector ui;
    unsigned inCount=0,outCount=0;
    std::array<std::array<float,maxFrames>,2> in{},out{};
    std::atomic<bool> failed{false};
    ~Impl(){std::lock_guard lock(compilerMutex());delete dsp;if(factory)deleteInterpreterDSPFactory(factory);}
};
static_assert(std::atomic<bool>::is_always_lock_free);
FaustInterpreter::FaustInterpreter(std::unique_ptr<Impl> p) noexcept:impl(std::move(p)){}
FaustInterpreter::~FaustInterpreter()=default;
std::unique_ptr<FaustInterpreter> FaustInterpreter::compile(std::string_view source,int rate,const std::string& libraries,std::string& error){
    error.clear();
    if(source.empty()||source.size()>maxSourceBytes||source.find('\0')!=std::string_view::npos||rate<8000||rate>192000){error="Invalid Faust source size or sample rate";return {};}
    // Instance is declared before the guard so rollback releases the guard before
    // its destructor reacquires it. Never destroy cached factories on audio.
    std::unique_ptr<Impl> candidate;
    try{
        candidate=std::make_unique<Impl>();
        std::lock_guard lock(compilerMutex());
        const char* args[]{"-single","-scal","-I",libraries.c_str()};
        candidate->factory=createInterpreterDSPFactoryFromString("AionEffect",std::string(source),libraries.empty()?2:4,args,error);
        if(!candidate->factory){if(error.size()>2048)error.resize(2048);return {};}
        candidate->dsp=candidate->factory->createDSPInstance();
        if(!candidate->dsp){error="Faust instance creation failed";return {};}
        const int ni=candidate->dsp->getNumInputs(),no=candidate->dsp->getNumOutputs();
        if(ni<0||ni>2||no<1||no>2){error="Faust requires 0..2 inputs and 1..2 outputs";return {};}
        candidate->inCount=static_cast<unsigned>(ni);candidate->outCount=static_cast<unsigned>(no);
        candidate->dsp->buildUserInterface(&candidate->ui);
        if(candidate->ui.invalid||candidate->ui.depth){error="Unsupported Faust controls or soundfile widget";return {};}
        candidate->dsp->init(rate);
        return std::unique_ptr<FaustInterpreter>(new FaustInterpreter(std::move(candidate)));
    }catch(...){error="Faust preparation failed";return {};}
}
unsigned FaustInterpreter::inputs() const noexcept{return impl->inCount;}
unsigned FaustInterpreter::outputs() const noexcept{return impl->outCount;}
const std::vector<FaustControl>& FaustInterpreter::controls() const noexcept{return impl->ui.controls;}
bool FaustInterpreter::faulted() const noexcept{return impl->failed.load(std::memory_order_relaxed);}
bool FaustInterpreter::setControl(unsigned index,float value) noexcept {
    if(index>=impl->ui.controls.size()||!std::isfinite(value))return false;
    const auto& c=impl->ui.controls[index];if(value<c.minimum||value>c.maximum)return false;
    *impl->ui.zones[index]=value;return true;
}
bool FaustInterpreter::clearOffAudio() noexcept {
    try{impl->dsp->instanceClear();impl->failed.store(false,std::memory_order_relaxed);return true;}
    catch(...){impl->failed.store(true,std::memory_order_relaxed);return false;}
}
bool FaustInterpreter::process(const float* const* input,float* const* output,unsigned frames) noexcept {
    if(frames>maxFrames)return false;
    if(!frames)return !faulted();
    if(!output)return false;
    for(unsigned c=0;c<impl->outCount;++c)if(!output[c])return false;
    if(impl->outCount==2&&output[0]==output[1])return false;
    if(impl->inCount&&!input)return false;
    for(unsigned c=0;c<impl->inCount;++c)if(!input[c])return false;
    if(!faulted()){
        for(unsigned c=0;c<impl->inCount;++c)for(unsigned i=0;i<frames;++i)impl->in[c][i]=std::isfinite(input[c][i])?input[c][i]:0;
        std::array<float*,2> in{impl->in[0].data(),impl->in[1].data()},out{impl->out[0].data(),impl->out[1].data()};
        try{impl->dsp->compute(static_cast<int>(frames),in.data(),out.data());}
        catch(...){impl->failed.store(true,std::memory_order_relaxed);}
        for(unsigned c=0;c<impl->outCount;++c)for(unsigned i=0;i<frames;++i)
            if(!std::isfinite(impl->out[c][i]))impl->failed.store(true,std::memory_order_relaxed);
    }
    const bool failed=faulted();
    for(unsigned c=0;c<impl->outCount;++c){if(failed)std::fill_n(output[c],frames,0.f);else std::copy_n(impl->out[c].data(),frames,output[c]);}
    return !failed;
}
}
