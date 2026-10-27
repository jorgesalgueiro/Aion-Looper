#include "core/LoopMeterGraph.h"
#include "core/LoopMeterLifecycle.h"
#include <memory>
#include <array>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
int main(int argc,char** argv){
    if(argc==2&&std::string(argv[1])=="--queued-timing"){
        unsigned block,id,mode;double bpm,lead;
        std::cout<<std::setprecision(17);
        while(std::cin>>block>>id>>mode>>bpm>>lead){
            if((block!=64&&block!=128)||id>=20||(mode!=1&&mode!=2)||!std::isfinite(bpm)||bpm<10||bpm>999
                ||!std::isfinite(lead)||lead<0||lead>1)return 2;
            auto engine=std::make_unique<aion::Engine>(aion::MeterEventScope::acceptedPlayback);
            engine->prepare(48000,8192);engine->configureSync({aion::SyncMode::off,121,1,4,4,false});
            aion::LoopMeterLifecycle owner(*engine,id);
            std::array<float,128> left{},right{};float* channels[]={left.data(),right.data()};
            aion::EngineView view;
            const auto cycle=[&](unsigned count,bool offline=false){
                left.fill(.25f);right.fill(-.125f);engine->process(channels,2,static_cast<int>(count),{},offline);
                while(engine->popView(view)){}
            };
            const auto action=[&](aion::LoopAction next){return engine->enqueueLoop({next,id,1,engine->generation(),0,0});};
            if(!action(aion::LoopAction::record))return 3;
            for(unsigned n=0;n<512/block;++n)cycle(block);
            if(!action(aion::LoopAction::finish))return 3;
            cycle(block,true);
            if(!action(aion::LoopAction::stopImmediate))return 3;
            cycle(block);
            aion::MeterGraphEvent event;while(engine->popMeterGraphEvent(event))if(!owner.consumeBuffer(event))return 4;
            engine->configureSync({static_cast<aion::SyncMode>(mode),bpm,1,4,4,false});cycle(block);
            const double origin=view.sync.origin/48000,period=view.sync.loopSeconds;
            const double now=static_cast<double>(engine->clock())/48000;
            double grid=origin+std::max(0.0,std::ceil((now+.03-origin)/period))*period;
            auto target=std::floor((grid-lead)*48000);
            while(target<static_cast<double>(engine->clock())){grid+=period;target=std::floor((grid-lead)*48000);}
            const auto targetFrame=static_cast<std::uint64_t>(target);
            while(engine->clock()<targetFrame)cycle(static_cast<unsigned>(std::min<std::uint64_t>(block,targetFrame-engine->clock())));
            const auto frame=engine->clock();if(!action(aion::LoopAction::play))return 3;cycle(block);
            if(!engine->popMeterGraphEvent(event)||event.kind!=aion::MeterGraphEventKind::queuedPlay||event.frame!=frame)return 4;
            if(!owner.consumeQueuedPlay(event)||owner.queuedCallbackDelay()!=event.queueDelaySeconds
                ||owner.inspect().present||owner.cleanupRequest().serial)return 4;
            std::cout<<frame<<' '<<origin<<' '<<period<<' '<<event.queueDelaySeconds<<' '<<view.tracks[id].scheduledFrame<<'\n';
        }
        return std::cin.eof()?0:2;
    }
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
