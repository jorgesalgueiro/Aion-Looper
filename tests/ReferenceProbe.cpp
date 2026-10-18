#include "core/LoopBank.h"
#include <iomanip>
#include <iostream>
#include <string>
int main() {
    aion::LoopBank bank; bank.prepare(20*4096,0);
    std::string operation; unsigned id; float value;
    while (std::cin >> operation >> id >> value) {
        bool accepted=true; aion::Stereo output{};
        using A=aion::LoopAction;
        if(operation=="seed") {accepted=bank.apply(A::record,id);if(accepted){for(int i=0;i<8;++i)bank.tick({0.1f,0.2f});accepted=bank.apply(A::finish,id);}}
        else if(operation=="record")accepted=bank.apply(A::record,id);
        else if(operation=="finish")accepted=bank.apply(A::finish,id);
        else if(operation=="overdub")accepted=bank.apply(A::overdub,id);
        else if(operation=="retrigger")accepted=bank.apply(A::retrigger,id);
        else if(operation=="tick")output=bank.tick({value,value});
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
        for(const auto& t:views)std::cout << ' ' << t.volume << ' ' << t.feedback << ' ' << t.pan << ' ' << t.playbackRate << ' ' << t.muted << ' ' << t.solo;
        std::cout << ' ' << output.left << ' ' << output.right << '\n';
    }
}
