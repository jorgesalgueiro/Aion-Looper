#include "core/MeterFollower.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
int main(){
    aion::MeterFollower meter;std::string line;
    std::cout<<std::setprecision(17);
    while(std::getline(std::cin,line)){
        std::istringstream input(line);std::uint64_t context,analyser,dataId;int running;unsigned size;std::string n,p;
        if(!(input>>context>>analyser>>dataId>>running>>n>>p>>size)||size>aion::MeterFollower::maximumWindow)return 2;
        std::vector<float> samples(size);
        for(auto& sample:samples){std::string token;if(!(input>>token))return 3;sample=static_cast<float>(std::stod(token));}
        const auto& v=meter.update(samples,std::stod(n),std::stod(p),context,analyser,dataId,running!=0);
        std::cout<<v.rms<<' '<<v.peak<<' '<<v.linearPeak<<' '<<v.peakDb<<' '<<v.rmsPercent<<' '<<v.peakPercent<<' '<<v.clipped<<'\n';
    }
}
