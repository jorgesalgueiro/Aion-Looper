#include "plugin/Processor.h"
#include "core/DeviceCallbackScope.h"
#include "core/CallbackAudibleTime.h"
#include <aion_device_splitter_test.h>
#include <array>
#include <atomic>
#include <cstdlib>
#include <iostream>

namespace {
void check(bool ok,const char* text){if(!ok){std::cerr<<text<<'\n';std::exit(1);}}
// Controlled IO only: no physical device is opened or driver clock invented.
class SplitDevice final : public juce::AudioIODevice {
public:
    int block;double rate;
    SplitDevice(int b,double r):AudioIODevice("controlled split","test"),block(b),rate(r){}
    juce::StringArray getOutputChannelNames() override{return {"L","R"};}
    juce::StringArray getInputChannelNames() override{return {"L","R"};}
    juce::Array<double> getAvailableSampleRates() override{return {rate};}
    juce::Array<int> getAvailableBufferSizes() override{return {block};}
    int getDefaultBufferSize() override{return block;}
    juce::String open(const juce::BigInteger&,const juce::BigInteger&,double,int) override{return {};}
    void close() override{} bool isOpen() override{return true;}
    void start(juce::AudioIODeviceCallback*) override{} void stop() override{}
    bool isPlaying() override{return false;}
    juce::String getLastError() override{return {};}
    int getCurrentBufferSizeSamples() override{return block;}
    double getCurrentSampleRate() override{return rate;}
    int getCurrentBitDepth() override{return 32;}
    juce::BigInteger getActiveOutputChannels() const override{return juce::BigInteger(3);}
    juce::BigInteger getActiveInputChannels() const override{return juce::BigInteger(3);}
    int getOutputLatencyInSamples() override{return 256;}
    int getInputLatencyInSamples() override{return 0;}
};
class SplitSink final : public juce::AudioIODeviceCallback {
public:
    aion::DeviceCallbackBinding binding;
    std::unique_ptr<AionProcessor> processor,reference;
    juce::MidiBuffer midi,referenceMidi;
    std::array<float,128> left{},right{};
    unsigned calls=0,starts=0,stops=0,offset=0,total=0;
    std::uint64_t begin=0,previousSerial=0;
    double rate;
    const std::uint64_t* expectedHost=nullptr;
    const float* expectedInput[2]{};
    float* expectedOutput[2]{};
    bool heard=false;
    explicit SplitSink(double r):rate(r){
        const auto latency=[r]{return aion::DeviceOutputLatency{true,r,256};};
        processor=std::make_unique<AionProcessor>(AionProcessor::ExportDestination{},latency,nullptr,&binding);
        reference=std::make_unique<AionProcessor>(AionProcessor::ExportDestination{},latency);
        midi.ensureSize(256);referenceMidi.ensureSize(256);
        for(auto* p:{processor.get(),reference.get()})
            for(const auto* json:{R"({"operation":"setParameter","id":"masterGain","value":1})",R"({"operation":"setParameter","id":"inputMonitor","value":1})"})
                check(p->request({juce::JSON::parse(json)}).getProperty("ok",false),"Configure split monitored PCM");
    }
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override{
        ++starts;begin=0;previousSerial=0;
        for(auto* p:{processor.get(),reference.get()}){
            p->setPlayConfigDetails(2,2,rate,std::max(1,device->getCurrentBufferSizeSamples()));
            p->prepareToPlay(rate,std::max(1,device->getCurrentBufferSizeSamples()));
        }
    }
    void audioDeviceStopped() override{
        ++stops;processor->releaseResources();reference->releaseResources();
    }
    void audioDeviceIOCallbackWithContext(const float* const* input,int numInput,float* const* output,int numOutput,int frames,const juce::AudioIODeviceCallbackContext& context) override{
        check(numInput==2&&numOutput==2&&frames>0&&frames<=128,"Actual JUCE splitter bounds channels/length");
        check(context.hostTimeNs==expectedHost,"Actual splitter retains original optional raw context for all parts");
        for(unsigned c=0;c<2;++c)
            check(input[c]==expectedInput[c]+offset&&output[c]==expectedOutput[c]+offset,"Actual splitter advances every input/output channel by the exact enclosing-buffer offset");
        for(int i=0;i<frames;++i){output[0][i]=left[static_cast<unsigned>(i)]=input[0][i];output[1][i]=right[static_cast<unsigned>(i)]=input[1][i];}
        float* referenceChannels[]={left.data(),right.data()};
        juce::AudioBuffer<float> actual(output,2,frames),control(referenceChannels,2,frames);
        // Reproduce the generated holder's exact scope contract after splitting.
        aion::DeviceCallbackScope scope{&binding,frames,binding.epoch(),juce::Time::getMillisecondCounterHiRes()*.001};
        processor->processBlock(actual,midi);reference->processBlock(control,referenceMidi);
        const auto stamp=processor->inspectCallbackClockAudioThread();
        check(stamp.deviceEntry.available&&stamp.deviceEntry.frames==static_cast<unsigned>(frames)&&stamp.deviceEntry.serial!=previousSerial,"Each split has its own single-use holder dispatch stamp");
        check(stamp.beginFrame==begin+offset&&stamp.endFrame==begin+offset+static_cast<unsigned>(frames),"Actual processor split ranges advance by actual partial length");
        check(stamp.processorEntry.seconds>=stamp.deviceEntry.seconds&&!reference->inspectCallbackClockAudioThread().deviceEntry.available,"Holder and processor edges distinct; no scope leaks into another owner");
        previousSerial=stamp.deviceEntry.serial;
        for(int c=0;c<2;++c)for(int i=0;i<frames;++i){
            check(actual.getSample(c,i)==control.getSample(c,i),"Split clock capture leaves production monitored PCM bit-identical");
            heard=heard||std::abs(actual.getSample(c,i))>.01f;
        }
        // Independent synthetic enclosing-delivery timeline, NOT scope.seconds.
        const aion::CallbackClockCorrelation anchor{stamp,{11,100+static_cast<double>(begin)/rate},total,offset};
        for(unsigned lag:{0u,1u,128u,4096u}){
            aion::CallbackAudibleTime time;
            const auto physical=begin+lag;
            const double emitted=physical>256?static_cast<double>(physical-256):0;
            const auto expected=std::min(emitted,static_cast<double>(stamp.endFrame))/rate;
            check(aion::callbackAudibleTime(stamp,anchor,{11,100+static_cast<double>(physical)/rate},stamp.generation,time)&&std::abs(time.audibleTime-expected)<1e-12,"Actual split ranges obey synthetic enclosing-buffer FIFO timeline");
        }
        offset+=static_cast<unsigned>(frames);++calls;
    }
};
}
void deviceSplitTests(bool& watch,std::atomic<int>& allocations){
    unsigned callbacks=0;
    for(double rate:{8000.0,48000.0})for(int block:{64,128}){
        SplitDevice device(block,rate);SplitSink sink(rate);
        aion_split_test::CallbackMaxSizeEnforcer splitter(sink);
        std::array<float,402> inL{},inR{},outL{},outR{};
        for(unsigned i=0;i<inL.size();++i){inL[i]=static_cast<float>(i%31)*.01f;inR[i]=-static_cast<float>(i%23)*.01f;}
        const float* inputs[]={inL.data(),inR.data()};float* outputs[]={outL.data(),outR.data()};
        for(unsigned c=0;c<2;++c){sink.expectedInput[c]=inputs[c];sink.expectedOutput[c]=outputs[c];}
        const std::uint64_t host=~std::uint64_t{0};
        juce::MemoryBlock saved,after;sink.processor->getStateInformation(saved);
        for(unsigned restart=0;restart<2;++restart){
            splitter.audioDeviceAboutToStart(&device);
            for(int frames:{0,1,block-1,block,block+1,3*block+17}){
                sink.total=static_cast<unsigned>(frames);sink.offset=0;
                outL.fill(-99);outR.fill(-99);
                sink.expectedHost=frames%2?&host:nullptr;
                const auto before=sink.calls;
                watch=true;
                splitter.audioDeviceIOCallbackWithContext(inputs,2,outputs,2,frames,{sink.expectedHost});
                watch=false;
                check(sink.offset==static_cast<unsigned>(frames)&&sink.calls-before==static_cast<unsigned>((frames+block-1)/block),"Actual splitter delivers each frame exactly once including partial/empty buffers");
                check(outL[static_cast<unsigned>(frames)]==-99&&outR[static_cast<unsigned>(frames)]==-99,"Splitter never writes beyond device buffer");
                sink.begin+=static_cast<unsigned>(frames);
            }
            splitter.audioDeviceStopped();
        }
        callbacks+=sink.calls;
        check(sink.starts==2&&sink.stops==2&&sink.heard,"Splitter forwards restart lifecycle and tests nonzero PCM");
        sink.processor->getStateInformation(after);check(saved==after,"Splitting/clock acquisition leaves saved parameters and MIDI unchanged");
        // JUCE's JACK reconfiguration guard: zero maximum clears output and never
        // calls the processor. No invented progress/time observation is allowed.
        device.block=0;splitter.audioDeviceAboutToStart(&device);
        const auto before=sink.calls;outL.fill(-99);outR.fill(-99);
        watch=true;splitter.audioDeviceIOCallbackWithContext(inputs,2,outputs,2,129,{});watch=false;
        check(sink.calls==before,"Zero device maximum suppresses processor callbacks");
        for(unsigned i=0;i<129;++i)check(outL[i]==0&&outR[i]==0,"Zero device maximum fills requested outputs with silence");
        check(outL[129]==-99&&outR[129]==-99,"Zero-maximum silence stays inside buffer");
        splitter.audioDeviceStopped();
    }
    check(allocations.load()==0,"Actual JUCE splitter, production processors and split model have zero tracked callback new/delete");
    std::cout<<"PASS: "<<callbacks<<" actual JUCE split callbacks, 64/128, partial/empty/zero-max/restart, raw context, exact PCM/state, synthetic FIFO clock; zero tracked new/delete\n";
}
