#include "plugin/Processor.h"
#include "core/DeviceCallbackScope.h"
#include "core/CallbackAudibleTime.h"
#include <aion_device_splitter_test.h>
#include <aion_device_delivery_splitter_test.h>
#include <array>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <utility>

namespace {
void check(bool ok,const char* text){if(!ok){std::cerr<<text<<'\n';std::exit(1);}}
// Controlled IO only: no physical device is opened or driver clock invented.
class SplitDevice final : public juce::AudioIODevice {
public:
    int block;double rate;
    juce::AudioIODeviceCallback* callback=nullptr;
    bool opened=false;
    SplitDevice(int b,double r):AudioIODevice("controlled split","test"),block(b),rate(r){}
    juce::StringArray getOutputChannelNames() override{return {"L","R"};}
    juce::StringArray getInputChannelNames() override{return {"L","R"};}
    juce::Array<double> getAvailableSampleRates() override{return {rate};}
    juce::Array<int> getAvailableBufferSizes() override{return {block};}
    int getDefaultBufferSize() override{return block;}
    juce::String open(const juce::BigInteger&,const juce::BigInteger&,double r,int b) override{if(r!=rate||b!=block)return "Unexpected controlled format";opened=true;return {};}
    void close() override{stop();opened=false;} bool isOpen() override{return opened;}
    void start(juce::AudioIODeviceCallback* next) override{stop();callback=next;if(callback)callback->audioDeviceAboutToStart(this);}
    void stop() override{if(auto* previous=std::exchange(callback,nullptr))previous->audioDeviceStopped();}
    bool isPlaying() override{return callback!=nullptr;}
    juce::String getLastError() override{return {};}
    int getCurrentBufferSizeSamples() override{return block;}
    double getCurrentSampleRate() override{return rate;}
    int getCurrentBitDepth() override{return 32;}
    juce::BigInteger getActiveOutputChannels() const override{return juce::BigInteger(3);}
    juce::BigInteger getActiveInputChannels() const override{return juce::BigInteger(3);}
    int getOutputLatencyInSamples() override{return 256;}
    int getInputLatencyInSamples() override{return 0;}
};
class SplitDeviceType final : public juce::AudioIODeviceType {
    int block;
public:
    explicit SplitDeviceType(int b):AudioIODeviceType("test"),block(b){}
    void scanForDevices() override{}
    juce::StringArray getDeviceNames(bool) const override{return {"controlled split"};}
    int getDefaultDeviceIndex(bool) const override{return 0;}
    int getIndexOfDevice(juce::AudioIODevice*,bool) const override{return 0;}
    bool hasSeparateInputsAndOutputs() const override{return false;}
    juce::AudioIODevice* createDevice(const juce::String&,const juce::String&) override{return new SplitDevice(block,48000);}
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
    bool requireDelivery;
    const void* deliveryOwner=this;
    std::uint64_t deliverySerial=0;
    double deliverySeconds=0,entryLower=0;
    explicit SplitSink(double r,bool delivery):rate(r),requireDelivery(delivery){
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
        aion::DeviceCallbackScope scope{&binding,frames,binding.epoch(),juce::Time::getMillisecondCounterHiRes()*.001,deliveryOwner};
        processor->processBlock(actual,midi);reference->processBlock(control,referenceMidi);
        const auto stamp=processor->inspectCallbackClockAudioThread();
        check(stamp.deviceEntry.available&&stamp.deviceEntry.frames==static_cast<unsigned>(frames)&&stamp.deviceEntry.serial!=previousSerial,"Each split has its own single-use holder dispatch stamp");
        check(stamp.beginFrame==begin+offset&&stamp.endFrame==begin+offset+static_cast<unsigned>(frames),"Actual processor split ranges advance by actual partial length");
        check(stamp.processorEntry.seconds>=stamp.deviceEntry.seconds&&!reference->inspectCallbackClockAudioThread().deviceEntry.available,"Holder and processor edges distinct; no scope leaks into another owner");
        previousSerial=stamp.deviceEntry.serial;
        const auto delivery=stamp.deviceEntry.delivery;
        check(delivery.available==requireDelivery,"Only instrumented device callbacks can supply enclosing delivery");
        if(requireDelivery){
            check(delivery.frames==total&&delivery.frameOffset==offset&&delivery.seconds>=entryLower&&delivery.seconds<=stamp.deviceEntry.seconds,"Real enclosing entry precedes holder/processor and carries the exact split range");
            if(!offset){deliverySerial=delivery.serial;deliverySeconds=delivery.seconds;}
            check(delivery.serial==deliverySerial&&delivery.seconds==deliverySeconds,"Every part retains one enclosing callback identity and entry");
        }
        for(int c=0;c<2;++c)for(int i=0;i<frames;++i){
            check(actual.getSample(c,i)==control.getSample(c,i),"Split clock capture leaves production monitored PCM bit-identical");
            heard=heard||std::abs(actual.getSample(c,i))>.01f;
        }
        // Independent synthetic enclosing-delivery timeline, NOT scope.seconds.
        const aion::CallbackClockCorrelation anchor{stamp,{11,100+static_cast<double>(begin)/rate},requireDelivery?delivery.frames:total,requireDelivery?delivery.frameOffset:offset};
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
void deviceManagerClockTests(bool& watch,std::atomic<int>& allocations){
   #if JUCE_LINUX
    // The stock manager constructs a MIDI-device-list observer even for this
    // audio-only fixture. Do not suppress its assertions or mock ALSA to claim
    // an end-to-end pass when WSL lacks the sequencer kernel device.
    if(!juce::File("/dev/snd/seq").exists()){
        std::cout<<"SKIP: linked production AudioDeviceManager check requires /dev/snd/seq; real manager acceptance unavailable on this Linux host. Original/instrumented splitter and production processor checks remain enabled.\n";
        return;
    }
   #endif
    unsigned calls=0;
    for(int block:{64,128}){
        // Register exactly one controlled device type before initialise, so no
        // platform device is enumerated/opened. Exercise the linked production
        // AudioDeviceManager module rather than an extracted test class.
        juce::AudioDeviceManager manager;
        manager.addAudioDeviceType(std::make_unique<SplitDeviceType>(block));
        SplitSink sink(48000,true);sink.deliveryOwner=&manager;
        manager.addAudioCallback(&sink);
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        setup.sampleRate=48000;setup.bufferSize=block;
        setup.inputDeviceName=setup.outputDeviceName="controlled split";
        setup.useDefaultInputChannels=setup.useDefaultOutputChannels=false;
        setup.inputChannels=setup.outputChannels=juce::BigInteger(3);
        check(manager.initialise(2,2,nullptr,false,"controlled split",&setup).isEmpty(),"Initialise actual manager with controlled IO only");
        auto* device=dynamic_cast<SplitDevice*>(manager.getCurrentAudioDevice());
        check(device&&device->callback&&sink.starts==1,"Actual manager opens only the fake device and prepares the processor");
        std::array<float,402> inL{},inR{},outL{},outR{};
        inL.fill(.2f);inR.fill(-.1f);
        const float* inputs[]={inL.data(),inR.data()};float* outputs[]={outL.data(),outR.data()};
        for(unsigned c=0;c<2;++c){sink.expectedInput[c]=inputs[c];sink.expectedOutput[c]=outputs[c];}
        juce::MemoryBlock saved,after;sink.processor->getStateInformation(saved);
        const auto drive=[&](int frames,bool tracked){
            sink.total=static_cast<unsigned>(frames);sink.offset=0;sink.deliverySerial=0;sink.deliverySeconds=0;
            sink.expectedHost=nullptr;sink.entryLower=juce::Time::getMillisecondCounterHiRes()*.001;
            outL.fill(-99);outR.fill(-99);
            watch=tracked;
            device->callback->audioDeviceIOCallbackWithContext(inputs,2,outputs,2,frames,{});
            watch=false;
            check(sink.offset==static_cast<unsigned>(frames)&&outL[static_cast<unsigned>(frames)]==-99&&outR[static_cast<unsigned>(frames)]==-99,"Real manager preserves exact enclosing coverage and bounds");
            sink.begin+=static_cast<unsigned>(frames);
        };
        // Preserve JUCE's existing lazy scratch-buffer allocation. This warm-up
        // is explicitly outside steady callback allocation acceptance; no claim
        // that the complete existing manager/player is allocation/lock-free.
        drive(block,false);
        for(int frames:{1,block,block+1,3*block+17})drive(frames,true);
        check(allocations.load()==0&&sink.heard,"Actual manager steady callback path retains nonzero identical PCM and zero tracked new/delete");
        sink.processor->getStateInformation(after);check(saved==after,"Actual manager clock handoff leaves processor parameter/MIDI state unchanged");
        calls+=sink.calls;
        manager.removeAudioCallback(&sink);manager.closeAudioDevice();
        check(sink.stops==1&&!manager.getCurrentAudioDevice(),"Actual manager stop removes callback before closing controlled IO");
    }
    std::cout<<"PASS: "<<calls<<" linked production AudioDeviceManager blocks, exact manager-owner handoff, enclosing entry/ranges, nonzero PCM/state, steady zero tracked new/delete after existing scratch warm-up; no physical IO\n";
}
}
void deviceSplitTests(bool& watch,std::atomic<int>& allocations){
    unsigned callbacks=0;
    for(bool delivery:{false,true})for(double rate:{8000.0,48000.0})for(int block:{64,128}){
        SplitDevice device(block,rate);SplitSink sink(rate,delivery);
        std::unique_ptr<juce::AudioIODeviceCallback> splitter;
        if(delivery)splitter=std::make_unique<aion_delivery_split_test::CallbackMaxSizeEnforcer>(sink,&sink);
        else splitter=std::make_unique<aion_split_test::CallbackMaxSizeEnforcer>(sink);
        std::array<float,402> inL{},inR{},outL{},outR{};
        for(unsigned i=0;i<inL.size();++i){inL[i]=static_cast<float>(i%31)*.01f;inR[i]=-static_cast<float>(i%23)*.01f;}
        const float* inputs[]={inL.data(),inR.data()};float* outputs[]={outL.data(),outR.data()};
        for(unsigned c=0;c<2;++c){sink.expectedInput[c]=inputs[c];sink.expectedOutput[c]=outputs[c];}
        const std::uint64_t host=~std::uint64_t{0};
        juce::MemoryBlock saved,after;sink.processor->getStateInformation(saved);
        for(unsigned restart=0;restart<2;++restart){
            splitter->audioDeviceAboutToStart(&device);
            for(int frames:{0,1,block-1,block,block+1,3*block+17}){
                sink.total=static_cast<unsigned>(frames);sink.offset=0;
                outL.fill(-99);outR.fill(-99);
                sink.expectedHost=frames%2?&host:nullptr;
                const auto before=sink.calls;
                sink.deliverySerial=0;sink.deliverySeconds=0;sink.entryLower=juce::Time::getMillisecondCounterHiRes()*.001;
                watch=true;
                splitter->audioDeviceIOCallbackWithContext(inputs,2,outputs,2,frames,{sink.expectedHost});
                watch=false;
                check(sink.offset==static_cast<unsigned>(frames)&&sink.calls-before==static_cast<unsigned>((frames+block-1)/block),"Actual splitter delivers each frame exactly once including partial/empty buffers");
                check(outL[static_cast<unsigned>(frames)]==-99&&outR[static_cast<unsigned>(frames)]==-99,"Splitter never writes beyond device buffer");
                sink.begin+=static_cast<unsigned>(frames);
            }
            splitter->audioDeviceStopped();
        }
        callbacks+=sink.calls;
        check(sink.starts==2&&sink.stops==2&&sink.heard,"Splitter forwards restart lifecycle and tests nonzero PCM");
        sink.processor->getStateInformation(after);check(saved==after,"Splitting/clock acquisition leaves saved parameters and MIDI unchanged");
        // JUCE's JACK reconfiguration guard: zero maximum clears output and never
        // calls the processor. No invented progress/time observation is allowed.
        device.block=0;splitter->audioDeviceAboutToStart(&device);
        const auto before=sink.calls;outL.fill(-99);outR.fill(-99);
        watch=true;splitter->audioDeviceIOCallbackWithContext(inputs,2,outputs,2,129,{});watch=false;
        check(sink.calls==before,"Zero device maximum suppresses processor callbacks");
        for(unsigned i=0;i<129;++i)check(outL[i]==0&&outR[i]==0,"Zero device maximum fills requested outputs with silence");
        check(outL[129]==-99&&outR[129]==-99,"Zero-maximum silence stays inside buffer");
        splitter->audioDeviceStopped();
    }
    check(allocations.load()==0,"Actual JUCE splitter, production processors and split model have zero tracked callback new/delete");
    std::cout<<"PASS: "<<callbacks<<" original/instrumented JUCE split callbacks, exact enclosing entry/range, 64/128, partial/empty/zero-max/restart, raw context, exact PCM/state, synthetic FIFO clock; zero tracked new/delete\n";
    deviceManagerClockTests(watch,allocations);
}
