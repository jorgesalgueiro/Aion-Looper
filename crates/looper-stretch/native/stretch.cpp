// C++ exceptions and native resource ownership never cross the Rust boundary.
#include "rubberband/RubberBandStretcher.h"
#include <algorithm>
#include <cstddef>
#include <vector>

extern "C" int looper_stretch(const float *input, unsigned int frames,
    unsigned int rate, unsigned int channels, double ratio, double pitch,
    float *output, unsigned int expected) noexcept {
    try {
        using RB=RubberBand::RubberBandStretcher;
        RB engine(rate,channels,RB::OptionProcessOffline | RB::OptionEngineFiner |
            RB::OptionChannelsTogether | RB::OptionThreadingNever | RB::OptionPitchHighQuality,ratio,pitch);
        if(engine.getEngineVersion()!=3) return -2;
        constexpr unsigned int block=4096;
        engine.setExpectedInputDuration(frames);
        engine.setMaxProcessSize(block);
        std::vector<float> left(frames),right(frames);
        for(unsigned int i=0;i<frames;i++) {left[i]=input[i*2];right[i]=input[i*2+1];}
        for(unsigned int pos=0;pos<frames;pos+=block) {
            const auto count=std::min(block,frames-pos);
            const float *planar[]={left.data()+pos,right.data()+pos};
            engine.study(planar,count,pos+count==frames);
        }
        std::vector<float> out_left(block),out_right(block);
        float *out[]={out_left.data(),out_right.data()};
        std::size_t produced=0;
        for(unsigned int pos=0;pos<frames;pos+=block) {
            const auto count=std::min(block,frames-pos);
            const float *planar[]={left.data()+pos,right.data()+pos};
            engine.process(planar,count,pos+count==frames);
            while(engine.available()>0) {
                const auto count_out=engine.retrieve(out,std::min(block,static_cast<unsigned int>(engine.available())));
                if(!count_out || produced+count_out>static_cast<std::size_t>(expected)+65536) return -3;
                for(unsigned int i=0;i<count_out;i++) {
                    if(produced+i<expected) {
                        output[(produced+i)*2]=out_left[i];
                        output[(produced+i)*2+1]=channels==1?out_left[i]:out_right[i];
                    }
                }
                produced+=count_out;
            }
        }
        // Offline mode compensates its own startup latency and provides the
        // requested duration. Never silently return a truncated unflushed tail.
        return produced>=expected ? 0 : -4;
    } catch(...) { return -1; }
}
