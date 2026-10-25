#include "core/LoopMeterGraph.h"
#include <array>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
int main(){
    aion::LoopMeterGraph graph;graph.setBuffer(true);graph.startGraph();
    std::string line;std::cout<<std::setprecision(17);
    while(std::getline(std::cin,line)){
        std::istringstream input(line);unsigned context,running,mode;std::array<std::string,7> tokens;
        if(!(input>>context>>running>>tokens[0]>>tokens[1]>>tokens[2]>>mode>>tokens[3]>>tokens[4]>>tokens[5]>>tokens[6]))return 2;
        aion::PlaybackClockInput clock;clock.hasContext=context!=0;clock.hasOutputTimestamp=mode==1;
        // strtod preserves valid subnormal doubles on libstdc++, where stod
        // throws out_of_range on the oracle's smallest finite timestamp.
        std::array<double,7> values{};
        for(unsigned i=0;i<tokens.size();++i){char* end=nullptr;values[i]=std::strtod(tokens[i].c_str(),&end);if(end==tokens[i].c_str()||*end)return 2;}
        clock.currentTime=values[0];clock.baseLatency=values[1];clock.outputLatency=values[2];
        clock.contextTime=values[3];clock.performanceTime=values[4];clock.wallTime=values[5];const float level=static_cast<float>(values[6]);
        std::array<float,2048> samples{};const float pattern[]={1,-1,0.5f,-0.5f};
        for(unsigned i=0;i<samples.size();++i)samples[i]=level*pattern[i%4];
        if(!graph.observe(samples,graph.inspect().data,clock,context,running!=0))return 3;
        const auto& v=graph.value();std::cout<<aion::playbackTime(clock)<<' '<<v.rms<<' '<<v.peak<<' '<<v.linearPeak<<' '
            <<v.peakDb<<' '<<v.rmsPercent<<' '<<v.peakPercent<<' '<<v.clipped<<'\n';
    }
}
