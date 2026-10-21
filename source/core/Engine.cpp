#include "core/Engine.h"
#include <algorithm>
#include <cmath>
namespace aion {
void Engine::prepare(double sampleRate, std::uint32_t poolFrames) {
    auto midiContext=midiDispatch();midi->prepare(midiContext);
    double rate = sampleRate;
    if (!std::isfinite(rate) || rate < 8000.0 || rate > 384000.0) rate = 48000.0;
    epoch.fetch_add(1, std::memory_order_acq_rel);
    audioReset = preparedRate != 0 && preparedRate != rate;
    if (preparedRate != rate || loops.capacity() != std::clamp(poolFrames / LoopBank::blockFrames, 1u, LoopBank::maxBlocks) * LoopBank::blockFrames)
        loops.prepare(poolFrames, static_cast<std::uint32_t>(rate * 0.005), rate);
    else (void) loops.apply(LoopAction::stopAll, 0);
    loops.resetTimeline();liveInput->cancel();sentWaveforms.fill(0);waveformCursor=0;metronome.prepare(rate);countIn.prepare(rate);preparedRate = rate;
    LoopCommand oldCommand{}; while (loopCommands.pop(oldCommand)) {};
    hasPendingLoop = false; lastRequestId = 0; rejected = false;
    frame = 0; gain = target = increment = renderedMasterPeak = 0.0f; remaining = 0;
    rampLength = static_cast<std::uint32_t>(std::clamp(rate * 0.02, 1.0, 38400.0));
    GainCommand discard{}; while (commands.pop(discard)) {};
    hasPending = false;
}
void Engine::setTarget(float value) noexcept {
    value = std::isfinite(value) ? std::clamp(value, 0.0f, 2.0f) : 0.0f;
    if (value == target) return;
    target = value; remaining = rampLength; increment = (target - gain) / static_cast<float>(remaining);
}
bool Engine::enqueue(GainCommand command) noexcept {
    return std::isfinite(command.gain) && command.gain >= 0.0f && command.gain <= 2.0f && commands.push(command);
}
bool Engine::enqueueLoop(LoopCommand command) noexcept {
    if (command.generation == 0 || command.generation != generation() || command.track >= LoopBank::trackCount
        || !std::isfinite(command.value)) return false;
    if(command.action==LoopAction::stutterStart)command.stutterRevision=stutterRevisions[command.track].load(std::memory_order_acquire);
    return loopCommands.push(command);
}
bool Engine::applyMidi(LoopAction action,std::uint32_t track,double value) noexcept {
    if(action==LoopAction::record&&!midiInputAvailable)return false;
    if(action==LoopAction::stopAll){
        if(hasPendingLoop){lastRequestId=std::max(lastRequestId,pendingLoop.requestId);hasPendingLoop=false;rejected=true;}
        LoopCommand discarded{};for(unsigned n=0;n<256&&loopCommands.pop(discarded);++n){lastRequestId=std::max(lastRequestId,discarded.requestId);rejected=true;}
    }
    return applyLoop(action,track,value);
}
bool Engine::applyLoop(LoopAction action,std::uint32_t track,double value) noexcept {
    const bool ok=action==LoopAction::startDelay?loops.setStartDelay(track,value):action==LoopAction::clearMergedSource?loops.clearMergedSource(track,value):loops.apply(action,track,static_cast<float>(value));
    if(!ok)return false;
    auto context=midiDispatch();
    if(action==LoopAction::stopAll||action==LoopAction::clearAll)midi->cancel(context);
    else if(action==LoopAction::clear)midi->cancel(context,static_cast<int>(track));
    if(action==LoopAction::record){
        const auto view=loops.trackView(track);
        if(view.state==LoopState::recording&&view.dynamicRecorded)liveInput->start(track,generation(),view.captureStartFrame);
        const double start=loops.armedStartSeconds(track);
        if(start>=0){const auto settings=loops.syncView().settings;countInTrack=track;
            countIn.schedule(static_cast<double>(frame)/preparedRate,start,settings.bpm,settings.numerator,settings.countInAudio,settings.countIn);}
    }else if(action==LoopAction::stopAll||action==LoopAction::clearAll||
        (track==countInTrack&&(action==LoopAction::finish||action==LoopAction::stop||action==LoopAction::stopImmediate||action==LoopAction::clear)))countIn.cancel();
    return true;
}
void Engine::process(float* const* channels, int count, int frames,std::span<const MidiEvent> midiEvents) noexcept {
    if (count < 0 || frames < 0 || (count > 0 && channels == nullptr)) return;
    float peak = 0.0f, inputPeak = 0.0f, masterPeak=0.0f;
    loops.beginBlock();
    auto midiContext=midiDispatch();midi->begin(midiContext);
    const auto midiRevision=midiCancellation.load(std::memory_order_acquire);
    if(midiRevision!=seenMidiCancellation){midi->cancel(midiContext);seenMidiCancellation=midiRevision;}
    const auto learnRevision=midiLearnCancellation.load(std::memory_order_acquire);
    if(learnRevision!=seenMidiLearnCancellation){midi->cancelLearn();seenMidiLearnCancellation=learnRevision;}
    const auto overflowRevision=midiOverflowRevision.load(std::memory_order_acquire);
    if(overflowRevision!=seenMidiOverflow){midi->overflow(midiContext);seenMidiOverflow=overflowRevision;}
    if(midiEvents.size()>256){midi->overflow(midiContext);midiEvents={};}
    for(std::size_t n=0;n<midiEvents.size();++n)if(midiEvents[n].offset>=static_cast<unsigned>(frames)||(n&&midiEvents[n].offset<midiEvents[n-1].offset)){midi->overflow(midiContext);midiEvents={};break;}
    std::size_t midiRead=0;
    ExportRequest exportRequest{};
    if(exportRequests.pop(exportRequest))
        (void)loops.requestExport(exportRequest.generation==generation()?exportRequest.track:LoopBank::trackCount,exportRequest.waveformId);
    for(unsigned id=0;id<LoopBank::trackCount;++id){const auto revision=stutterRevisions[id].load(std::memory_order_acquire);
        if(revision!=seenStutterRevisions[id]){loops.apply(LoopAction::stutterStop,id);seenStutterRevisions[id]=revision;}
    }
    const auto metroClock=loops.syncView();
    metronome.configure(static_cast<float>(metroClock.settings.bpm),static_cast<float>(metroClock.origin/preparedRate),metroClock.settings.numerator);
    int loopApplied = 0;
    int applied = 0; // Bound work even when many past-due commands arrive together.
    for (int i = 0; i < frames; ++i, ++frame) {
        if (!hasPending) hasPending = commands.pop(pending);
        while (hasPending && pending.frame <= frame && applied < 128) {
            setTarget(pending.gain); ++applied; hasPending = commands.pop(pending);
        }
        if (!hasPendingLoop) hasPendingLoop = loopCommands.pop(pendingLoop);
        while (hasPendingLoop && pendingLoop.frame <= frame && loopApplied < 128) {
            lastRequestId = pendingLoop.requestId;
            const bool cancelledStutter=pendingLoop.action==LoopAction::stutterStart&&pendingLoop.stutterRevision!=stutterRevisions[pendingLoop.track].load(std::memory_order_acquire);
            rejected = pendingLoop.generation != generation() ||
                (!cancelledStutter&&!applyLoop(pendingLoop.action,pendingLoop.track,pendingLoop.value));
            ++loopApplied; hasPendingLoop = loopCommands.pop(pendingLoop);
        }
        midiContext.frame=frame;
        while(midiRead<midiEvents.size()&&midiEvents[midiRead].offset<=static_cast<unsigned>(i))midi->event(midiEvents[midiRead++],midiContext);
        midi->tick(midiContext);
        const float left = count > 0 && channels[0] && std::isfinite(channels[0][i]) ? channels[0][i] : 0.0f;
        const float right = count > 1 && channels[1] && std::isfinite(channels[1][i]) ? channels[1][i] : left;
        inputPeak = std::max(inputPeak, std::max(std::abs(left), std::abs(right)));
        liveInput->push(left,right);
        const auto loopOutput = loops.tick({left, right});
        // Monitor-only, like the original direct-to-destination worklet: never
        // feed clicks to capture, and keep its volume independent of MASTER.
        const float click=metronome.tick(frame)+countIn.tick(frame);
        if (remaining > 0) { gain += increment; if (--remaining == 0) gain = target; }
        for (int c = 0; c < count; ++c) {
            if (channels[c] == nullptr) continue;
            const float value = std::isfinite(channels[c][i]) ? channels[c][i] : 0.0f;
            const float loopValue = c == 0 ? loopOutput.left : (c == 1 ? loopOutput.right : 0.0f);
            const double mainScaled=(static_cast<double>(monitoring ? value : 0.0f)+loopValue)*gain;
            masterPeak=std::max(masterPeak,std::abs(static_cast<float>(std::clamp(mainScaled,-1.0,1.0))));
            const double scaled=mainScaled+(c<2?click:0.0f);
            channels[c][i] = static_cast<float>(std::clamp(scaled, -1.0, 1.0));
            peak = std::max(peak, std::abs(channels[c][i]));
        }
    }
    // Telemetry is lossy by design; never wait when the editor is closed/slow.
    renderedMasterPeak=masterPeak;
    (void) meters.push({frame, peak});
    const auto tracks=loops.view();
    if(liveInput->running()){
        const auto& track=tracks[liveInput->track()];
        if(track.state==LoopState::recording&&track.dynamicRecorded)(void)liveInput->publish(frame);
        else liveInput->cancel();
    }
    unsigned emitted=0;
    for(unsigned n=0;n<LoopBank::trackCount&&emitted<2;++n){
        const auto id=waveformCursor;waveformCursor=(waveformCursor+1)%LoopBank::trackCount;
        const auto revision=tracks[id].waveformId;
        if(!revision){sentWaveforms[id]=0;continue;}
        if(revision==sentWaveforms[id])continue;
        WaveformEvent event{};event.generation=generation();event.track=id;event.id=revision;
        if(loops.copyWaveform(id,revision,event.peaks)&&waveforms->push(event)){sentWaveforms[id]=revision;++emitted;}
        else break; // Closed/slow editor: retry later, never wait.
    }
    (void) views->push({tracks, frame, preparedRate, generation(), lastRequestId, loops.available(), inputPeak, peak, rejected, loops.exhausted(), audioReset, loops.nextUndo(), loops.nextRedo(), loops.syncView(), metronome.flash(frame), masterPeak, countIn.flash(frame),midi->view()});
}
bool Engine::enqueueExport(ExportRequest request) noexcept {
    if(exportOutstanding||request.generation!=generation()||!request.generation||request.track>=LoopBank::trackCount||!request.waveformId)return false;
    if(!exportRequests.push(request))return false;
    exportOutstanding=true;return true;
}
bool Engine::popExport(WavExport& output) noexcept {
    if(!exportOutstanding||!loops.popExport(output))return false;
    exportOutstanding=false;return true;
}
}
