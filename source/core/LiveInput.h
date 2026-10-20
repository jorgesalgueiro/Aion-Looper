#pragma once
#include "core/SpscQueue.h"
#include <array>
#include <cmath>
#include <cstdint>
namespace aion {
struct LiveInputEvent {
    std::array<float,512> samples{};
    std::uint64_t frame=0,startFrame=0;
    std::uint32_t generation=0,track=0;
};
// Constructed off callback. One audio producer, one message-thread consumer.
class LiveInput {
public:
    void start(std::uint32_t track,std::uint32_t generation,std::uint64_t frame) noexcept {
        history.fill(0);cursor=0;active=true;owner=track;epoch=generation;startFrame=frame;
    }
    void cancel() noexcept {active=false;}
    bool running() const noexcept {return active;}
    unsigned track() const noexcept {return owner;}
    void push(float left,float right) noexcept {
        if(!active)return;
        left=std::isfinite(left)?left:0;right=std::isfinite(right)?right:0;
        history[cursor]=left*0.5f+right*0.5f;cursor=(cursor+1)%history.size();
    }
    bool publish(std::uint64_t frame) noexcept {
        if(!active)return false;
        LiveInputEvent event{};event.frame=frame;event.startFrame=startFrame;event.generation=epoch;event.track=owner;
        for(unsigned i=0;i<history.size();++i)event.samples[i]=history[(cursor+i)%history.size()];
        return events.push(event); // Drop telemetry when closed/slow; never wait.
    }
    bool pop(LiveInputEvent& event) noexcept {return events.pop(event);}
private:
    std::array<float,512> history{};
    unsigned cursor=0,owner=0;
    bool active=false;
    std::uint32_t epoch=0;
    std::uint64_t startFrame=0;
    SpscQueue<LiveInputEvent,4> events;
};
}
