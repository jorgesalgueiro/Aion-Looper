#pragma once
#include "core/MeterFollower.h"
#include "core/MeterWindow.h"
#include <cstdint>

namespace aion {
enum class MeterLoopState : unsigned {empty,stopped,armed,queued,recording,restarting,finalizing,playing,stopping,overdubbing,substituting,multiplying};
struct LoopMeterGraphView {
    MeterLoopState state=MeterLoopState::empty;
    bool buffer=false,present=false,destroyed=false;
    std::uint64_t analyser=0,data=0;
    double teardownDelay=-1; // Seconds requested from the event owner; -1 means none.
};
// Explicit graph-lifetime notifications, one owner, no allocation or clock query.
// Not inferred from native sourceRevision: healthy retriggers retain this graph.
// Timer delivery is external, so a stalled audio clock never invents teardown.
class LoopMeterGraph {
public:
    void setBuffer(bool available) noexcept {view.buffer=available;}
    void setState(MeterLoopState state) noexcept {view.state=state;}
    bool startGraph() noexcept {
        if(!view.buffer)return false;
        if(view.state==MeterLoopState::stopping)teardown();
        if(view.state==MeterLoopState::playing)return false;
        if(++serial==0)++serial;
        view.present=true;view.destroyed=false;view.analyser=view.data=serial;
        // Only the stopping branch cancels an existing cleanup request. A
        // caller changing stopping to queued first retains the original timer.
        view.state=MeterLoopState::playing;return true;
    }
    bool stop(double now,double scheduledTime=0,bool hasVolume=true) noexcept {
        // Native event contract: reject invalid clocks without changing lifetime.
        if(!std::isfinite(now)||!std::isfinite(scheduledTime))return false;
        const auto s=view.state;
        if(!activeState(s)&&s!=MeterLoopState::queued&&s!=MeterLoopState::restarting)return false;
        if(scheduledTime>now){view.state=MeterLoopState::stopping;view.teardownDelay=scheduledTime-now+0.05;}
        else if(view.present&&hasVolume){view.state=MeterLoopState::stopping;view.teardownDelay=0.025;}
        else teardown();
        return true;
    }
    void destroyGraph() noexcept {if(view.present)view.destroyed=true;}
    void teardown() noexcept {
        view.present=view.destroyed=false;view.analyser=0;view.teardownDelay=-1;
        view.state=view.buffer?MeterLoopState::stopped:MeterLoopState::empty;
        // Original _teardown retains analyserData and the meter's holds/history.
    }
    void clear() noexcept {
        view.present=view.destroyed=view.buffer=false;view.state=MeterLoopState::empty;view.teardownDelay=-1;
        // Original Loop.clear resets the meter only while analyser is attached.
        if(view.analyser){view.analyser=view.data=0;follower=MeterFollower{};reading={};}
    }
    static bool activeState(MeterLoopState state) noexcept {
        return state==MeterLoopState::playing||state==MeterLoopState::stopping||state==MeterLoopState::overdubbing||state==MeterLoopState::substituting||state==MeterLoopState::multiplying;
    }
    bool observe(std::span<const float> samples,std::uint64_t windowIdentity,
        double now,double playbackTime,std::uint64_t context,bool running) noexcept {
        const bool eligible=activeState(view.state)&&view.present&&!view.destroyed;
        const bool needsWindow=eligible&&view.analyser&&view.data&&running&&context;
        // A native caller must supply a window owned by this graph, never an
        // older graph's queued PCM. Reject atomically; no false silence/history.
        if(needsWindow&&(windowIdentity!=view.data||samples.size()!=MeterWindow::size))return false;
        reading=follower.update(needsWindow?samples:std::span<const float>{},now,playbackTime,context,
            eligible?view.analyser:0,eligible?view.data:0,running);
        return true;
    }
    const MeterReading& value() const noexcept {return reading;}
    LoopMeterGraphView inspect() const noexcept {return view;}
private:
    std::uint64_t serial=0;
    LoopMeterGraphView view{};
    MeterFollower follower;
    MeterReading reading{};
};
}
