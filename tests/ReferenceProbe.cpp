#include "core/LoopBank.h"
#include <iomanip>
#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>
int main(int argc,char**) {
    const bool audioOnly=argc>1;
    aion::LoopBank bank; bank.prepare(20*4096,0);
    std::string operation; unsigned id; float value;
    const auto wait=[&]{for(int n=0;bank.finalizationPending()&&n<10000;++n){bank.beginBlock();bank.tick({});std::this_thread::sleep_for(std::chrono::milliseconds(1));}return !bank.finalizationPending();};
    std::string line;
    while (std::getline(std::cin,line)) {
        std::istringstream input(line);if(!(input>>operation>>id>>value))return 2;
        float right=value;(void)(input>>right);
        bool accepted=true; aion::Stereo output{};
        using A=aion::LoopAction;
        if(operation=="seed") {accepted=bank.apply(A::record,id);if(accepted){for(int i=0;i<8;++i)bank.tick({0.1f,0.2f});accepted=bank.apply(A::finish,id)&&wait();}}
        else if(operation=="prepare")bank.prepare(id,0,value);
        else if(operation=="advance") {for(unsigned i=0;i<id;++i)output=bank.tick({});}
        else if(operation=="autoplay")bank.setAutoPlay(value!=0);
        else if(operation=="sync") {unsigned bars=2,countIn=0,den=4,audio=0;(void)(input>>bars>>countIn>>den>>audio);bank.configureSync({static_cast<aion::SyncMode>(id),value,bars,static_cast<unsigned>(right),den,countIn!=0,audio!=0});}
        else if(operation=="transport") {const auto t=bank.view()[id];const auto clock=bank.syncView();std::cout<<1<<' '<<static_cast<unsigned>(t.state)<<' '<<t.length<<' '<<t.position<<' '<<t.scheduledFrame<<' '<<t.stopPending<<' '<<bank.available()<<' '<<clock.frame<<' '<<std::setprecision(15)<<clock.origin<<' '<<clock.loopSeconds<<' '<<t.effectiveRate<<'\n';continue;}
        else if(operation=="stopImmediate")accepted=bank.apply(A::stopImmediate,id);
        else if(operation=="wait")accepted=wait();
        else if(operation=="waitStill") {for(int n=0;bank.finalizationPending()&&n<10000;++n){bank.beginBlock();std::this_thread::sleep_for(std::chrono::milliseconds(1));}accepted=!bank.finalizationPending();}
        else if(operation=="substitute")accepted=bank.apply(A::substitute,id);
        else if(operation=="record")accepted=bank.apply(A::record,id);
        else if(operation=="finish")accepted=bank.apply(A::finish,id);
        else if(operation=="overdub")accepted=bank.apply(A::overdub,id);
        else if(operation=="multiply")accepted=bank.apply(A::multiply,id,value);
        else if(operation=="undo")accepted=bank.apply(A::undo,id);
        else if(operation=="redo")accepted=bank.apply(A::redo,id);
        else if(operation=="undoGlobal")accepted=bank.apply(A::undoGlobal,0);
        else if(operation=="redoGlobal")accepted=bank.apply(A::redoGlobal,0);
        else if(operation=="stopAll")accepted=bank.apply(A::stopAll,0);
        else if(operation=="clearAll")accepted=bank.apply(A::clearAll,0);
        else if(operation=="normalize")accepted=bank.apply(A::normalize,id);
        else if(operation=="stop")accepted=bank.apply(A::stop,id);
        else if(operation=="play")accepted=bank.apply(A::play,id);
        else if(operation=="reverse")accepted=bank.apply(A::reverse,id,value);
        else if(operation=="retrigger")accepted=bank.apply(A::sourceRestart,id);
        else if(operation=="length") {const auto t=bank.view()[id];std::cout<<1<<' '<<t.length<<' '<<static_cast<unsigned>(t.state)<<'\n';continue;}
        else if(operation=="history") {std::cout<<1;for(const auto& t:bank.view())std::cout<<' '<<t.undoCount<<' '<<t.redoCount;std::cout<<' '<<bank.nextUndo()<<' '<<bank.nextRedo()<<'\n';continue;}
        else if(operation=="tick")output=bank.tick({value,right});
        else if(operation=="volume")accepted=bank.apply(A::volume,id,value);
        else if(operation=="feedback")accepted=bank.apply(A::feedback,id,value);
        else if(operation=="pan")accepted=bank.apply(A::pan,id,value);
        else if(operation=="speed")accepted=bank.apply(A::speed,id,value);
        else if(operation=="mute")accepted=bank.apply(A::mute,id,value);
        else if(operation=="solo")accepted=bank.apply(A::solo,id);
        else if(operation=="clear")accepted=bank.apply(A::clear,id);
        else return 2;
        const auto views=bank.view();
        std::cout << accepted << std::setprecision(9);
        if(!audioOnly)for(const auto& t:views)std::cout << ' ' << t.volume << ' ' << t.feedback << ' ' << t.pan << ' ' << t.playbackRate << ' ' << t.muted << ' ' << t.solo << ' ' << t.reverse;
        std::cout << ' ' << output.left << ' ' << output.right << '\n';
    }
}
