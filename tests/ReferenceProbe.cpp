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
        else if(operation=="wait")accepted=wait();
        else if(operation=="waitStill") {for(int n=0;bank.finalizationPending()&&n<10000;++n){bank.beginBlock();std::this_thread::sleep_for(std::chrono::milliseconds(1));}accepted=!bank.finalizationPending();}
        else if(operation=="substitute")accepted=bank.apply(A::substitute,id);
        else if(operation=="record")accepted=bank.apply(A::record,id);
        else if(operation=="finish")accepted=bank.apply(A::finish,id);
        else if(operation=="overdub")accepted=bank.apply(A::overdub,id);
        else if(operation=="normalize")accepted=bank.apply(A::normalize,id);
        else if(operation=="stop")accepted=bank.apply(A::stop,id);
        else if(operation=="play")accepted=bank.apply(A::play,id);
        else if(operation=="reverse")accepted=bank.apply(A::reverse,id,value);
        else if(operation=="retrigger")accepted=bank.apply(A::retrigger,id);
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
