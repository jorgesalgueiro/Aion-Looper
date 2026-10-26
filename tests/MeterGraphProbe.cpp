#include "core/LoopMeterGraph.h"
#include "core/LoopMeterLifecycle.h"
#include <memory>
#include <array>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
int main(int argc,char** argv){
    if(argc==2&&std::string(argv[1])=="--lifecycle"){
        auto engine=std::make_unique<aion::Engine>();engine->prepare(48000,8192);
        aion::LoopMeterLifecycle owner(*engine,19);std::string line;std::cout<<std::setprecision(17);
        while(std::getline(std::cin,line)){
            std::istringstream input(line);std::string op;if(!(input>>op))return 2;
            if(op=="buffer"){int yes;if(!(input>>yes))return 2;owner.setBuffer(yes!=0);}
            else if(op=="state"){unsigned state;if(!(input>>state)||state>11)return 2;owner.setState(static_cast<aion::MeterLoopState>(state));}
            else if(op=="play")(void)owner.start(0);
            else if(op=="stop"){double now,scheduled;int volume;if(!(input>>now>>scheduled>>volume))return 2;(void)owner.stop(now,scheduled,volume!=0);}
            else if(op=="destroy")owner.destroyGraph();
            else if(op=="teardown"){const auto timer=owner.cleanupRequest();if(timer.serial)(void)owner.deliverCleanup(timer);else owner.teardown();}
            else if(op=="clear")owner.clear();
            else if(op!="retain"&&op!="read")return 2;
            const auto view=owner.inspect();
            std::cout<<static_cast<unsigned>(view.state)<<' '<<view.buffer<<' '<<view.present<<' '<<view.destroyed<<' '<<view.analyser<<' '<<view.data<<' '<<view.teardownDelay<<'\n';
        }
        return 0;
    }
    if(argc!=1)return 2;
    aion::LoopMeterGraph graph;std::string line;std::cout<<std::setprecision(17);
    while(std::getline(std::cin,line)){
        std::istringstream input(line);std::string op;if(!(input>>op))return 2;
        if(op=="buffer"){int yes;if(!(input>>yes))return 2;graph.setBuffer(yes!=0);}
        else if(op=="state"){unsigned state;if(!(input>>state)||state>11)return 2;graph.setState(static_cast<aion::MeterLoopState>(state));}
        else if(op=="play")(void)graph.startGraph();
        else if(op=="stop"){double now,scheduled;int volume;if(!(input>>now>>scheduled>>volume))return 2;(void)graph.stop(now,scheduled,volume!=0);}
        else if(op=="destroy")graph.destroyGraph();
        else if(op=="teardown")graph.teardown();
        else if(op=="clear")graph.clear();
        else if(op=="retain"){} // Actual source replacement, no graph notification.
        else if(op=="read"){
            double now,playback;float level;std::uint64_t context;int running;
            if(!(input>>context>>running>>now>>playback>>level))return 2;
            std::array<float,2048> window{};const float pattern[]={1,-1,0.5f,-0.5f};
            for(unsigned i=0;i<window.size();++i)window[i]=pattern[i%4]*level;
            if(!graph.observe(window,graph.inspect().data,now,playback,context,running!=0))return 3;
        }else return 2;
        const auto view=graph.inspect();const auto& v=graph.value();
        std::cout<<static_cast<unsigned>(view.state)<<' '<<view.buffer<<' '<<view.present<<' '<<view.destroyed<<' '<<view.analyser<<' '<<view.data<<' '<<view.teardownDelay<<' '
            <<v.rms<<' '<<v.peak<<' '<<v.linearPeak<<' '<<v.peakDb<<' '<<v.rmsPercent<<' '<<v.peakPercent<<' '<<v.clipped<<'\n';
    }
}
