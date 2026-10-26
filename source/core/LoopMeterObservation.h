#pragma once
#include "core/Engine.h"
#include "core/LoopMeterGraph.h"
#include "core/CallbackAudibleTime.h"

namespace aion {
// Message-owned receipt, issued by the explicit graph owner. Tickets must not
// be reused for another graph during one Engine generation. Native source and
// clip revisions are independent: healthy retriggers keep analyser history.
struct LoopMeterBinding {
    std::uint32_t generation=0,track=0,ticket=0;
    std::uint64_t analyser=0,data=0;
};
// Message owner only. Calculate into fixed-size local state and commit only
// while evidence is still current. No allocation/wait and no audio-thread copy.
template<class Current,class Observe>
bool commitLoopMeterObservation(LoopMeterGraph& graph,Current&& current,Observe&& observe) noexcept {
    if(!current())return false;
    auto candidate=graph;
    if(!observe(candidate)||!current())return false;
    graph=candidate;return true;
}
// Compose existing graph, PCM/source and exact callback guards. This only
// observes an active, attached graph. Inactive/silence observations and original
// timer delivery remain explicit owner operations. A request ticket proves
// provenance. The window ticket additionally requires an explicit audio-owner reset;
// neither ticket certifies graph timer delivery or DAC continuity.
inline bool observeBoundLoopMeterWindow(LoopMeterGraph& graph,const LoopMeterBinding& binding,
                                       const MeterWindowEvent& packet,const EngineView& view,
                                       const CallbackClockStamp& clock,CallbackAudibleTime time) noexcept {
    const auto identity=graph.inspect();
    if(!binding.generation||!binding.ticket||!binding.analyser||!binding.data
        ||binding.generation!=view.generation||binding.track!=packet.track
        ||binding.ticket!=packet.windowGraphTicket||packet.graphStartFrame>packet.endFrame
        ||binding.ticket!=packet.graphTicket||binding.analyser!=identity.analyser||binding.data!=identity.data
        ||!identity.buffer||!identity.present||identity.destroyed||!LoopMeterGraph::activeState(identity.state)
        ||!sameLoopMeterSource(packet,view)||!sameLoopMeterCallback(packet,view)
        ||clock!=packet.callbackClock||clock.nonRealtime||!clock.hasOutputLatency||clock.beginFrame==clock.endFrame
        ||!view.tracks[packet.track].length||!packet.waveformId
        ||!std::isfinite(time.renderedTime)||!std::isfinite(time.audibleTime)
        ||clock.endFrame>(std::uint64_t{1}<<53)||time.audibleTime<0||time.audibleTime>time.renderedTime
        ||time.renderedTime!=static_cast<double>(clock.endFrame)/clock.sampleRate)return false;
    return graph.observe(packet.samples,binding.data,time.renderedTime,time.audibleTime,view.generation,true);
}
}
