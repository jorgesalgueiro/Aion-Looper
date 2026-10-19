#include "core/LoopBank.h"
#include "core/ReferenceDsp.h"
#include <cmath>
#include <chrono>
namespace aion {
namespace {
float safe(float value) noexcept { return std::isfinite(value) ? std::clamp(value,-8.0f,8.0f) : 0.0f; }
void append(std::array<std::uint8_t,50>& order,std::uint32_t& size,std::uint32_t id) noexcept {
    if(size==order.size()){std::move(order.begin()+1,order.end(),order.begin());--size;}
    order[size++]=static_cast<std::uint8_t>(id);
}
bool remove(std::array<std::uint8_t,50>& order,std::uint32_t& size,std::uint32_t id,bool last) noexcept {
    int found=-1;
    for(std::uint32_t i=0;i<size;++i)if(order[i]==id){found=static_cast<int>(i);if(!last)break;}
    if(found<0)return false;
    std::move(order.begin()+found+1,order.begin()+size,order.begin()+found);--size;return true;
}
void forget(std::array<std::uint8_t,50>& order,std::uint32_t& size,std::uint32_t id) noexcept {
    std::uint32_t out=0;for(std::uint32_t i=0;i<size;++i)if(order[i]!=id)order[out++]=order[i];size=out;
}
bool capturing(LoopState state) noexcept {return state==LoopState::recording||state==LoopState::overdubbing||state==LoopState::substituting||state==LoopState::multiplying;}
}
LoopBank::~LoopBank(){shutdown();}
void LoopBank::shutdown(){
    stopping.store(true,std::memory_order_release);
    if(worker.joinable())worker.join();
    owned.clear();
    Clip* discarded=nullptr;while(retired.pop(discarded)){}
    result=nullptr;jobState.store(0,std::memory_order_relaxed);
}
void LoopBank::prepare(std::uint32_t frames,std::uint32_t fadeFrames,double sampleRate){
    shutdown();
    totalBlocks=std::clamp(frames/blockFrames,1u,maxBlocks);
    audio.assign(static_cast<std::size_t>(totalBlocks)*blockFrames*2,0.0f);
    for(auto& track:tracks)track=Track{};
    freeCount=totalBlocks;residentBlocks=0;undoSize=redoSize=0;
    for(std::uint32_t i=0;i<totalBlocks;++i)freeBlocks[i]=static_cast<std::uint16_t>(i);
    fadeLength=std::min(fadeFrames,1920u);poolFull=false;soloTrack=-1;
    rate=std::isfinite(sampleRate)&&sampleRate>0?sampleRate:48000;
    resetTimeline();previousInput=olderInput={};
    owned.reserve(128);stopping.store(false,std::memory_order_release);
    worker=std::thread([this]{workerLoop();});
}
float& LoopBank::sample(Track& track,std::uint32_t frame,std::uint32_t channel) noexcept {
    return audio[(static_cast<std::size_t>(track.blocks[frame/blockFrames])*blockFrames+frame%blockFrames)*2+channel];
}
void LoopBank::retire(Clip* clip) noexcept {
    if(clip)(void)retired.push(clip); // On overflow it stays worker-owned until stopped prepare; never delete on audio.
}
void LoopBank::releaseOnWorker(Clip* clip) noexcept {
    if(!clip||clip->leases.fetch_sub(1,std::memory_order_acq_rel)!=1)return;
    const auto found=std::find_if(owned.begin(),owned.end(),[clip](const auto& item){return item.get()==clip;});
    if(found!=owned.end())owned.erase(found);
}
void LoopBank::workerLoop() noexcept {
    while(!stopping.load(std::memory_order_acquire)){
        unsigned expected=1;
        if(jobState.compare_exchange_strong(expected,2,std::memory_order_acquire)){
            result=nullptr;resultChanged=true;
            try {
                auto output=std::make_unique<Clip>();
                output->length=job.multiply?job.base->length+(job.doubling?job.base->length:job.length):(job.edit?job.base->length:job.outputFrames);
                if(job.multiply){
                    for(unsigned c=0;c<2;++c){
                        auto& data=output->channels[c];data=job.base->channels[c];
                        if(job.doubling)data.insert(data.end(),job.base->channels[c].begin(),job.base->channels[c].end());
                        else {
                            std::vector<float> take(job.length);
                            for(std::uint32_t i=0;i<job.length;++i)take[i]=audio[(static_cast<std::size_t>(job.blocks[i/blockFrames])*blockFrames+i%blockFrames)*2+c];
                            reference::removeDc(take);(void)reference::crossfade(take,rate,0.008);
                            data.insert(data.end(),take.begin(),take.end());
                        }
                    }
                } else if(job.normalize){
                    output->channels=job.base->channels;
                    resultChanged=reference::normalize(output->channels[0],output->channels[1]);
                } else for(unsigned c=0;c<2;++c){
                    std::vector<float> take(job.length);
                    for(std::uint32_t i=0;i<job.length;++i)
                        take[i]=audio[(static_cast<std::size_t>(job.blocks[i/blockFrames])*blockFrames+i%blockFrames)*2+c];
                    if(job.edit){
                        reference::removeDc(take);
                        (void)reference::crossfade(take,rate,0.008);
                        output->channels[c]=job.base->channels[c];
                        (void)reference::mixCircular(output->channels[c],take,rate,static_cast<double>(job.offset)/rate,job.feedback);
                    } else {
                        if(job.recordMode==SyncMode::fixed){
                            std::vector<float> extracted(job.outputFrames,0);
                            const auto count=std::min(job.outputFrames,job.length);
                            std::copy_n(take.begin(),count,extracted.begin());
                            const auto tail=std::min(static_cast<std::uint32_t>(rate*0.02),job.length-count);
                            if(tail&&count==job.outputFrames){
                                for(std::uint32_t i=0;i<tail;++i)extracted[i]=static_cast<float>(std::clamp(static_cast<double>(extracted[i])+take[count+i]*(1-static_cast<double>(i)/tail),-1.0,1.0));
                            }else{
                                const auto fade=std::min(256u,count);
                                for(std::uint32_t i=0;i<fade;++i)extracted[count-1-i]=static_cast<float>(extracted[count-1-i]*std::sin(static_cast<double>(i)/fade*1.5707963267948966));
                                (void)reference::crossfade(extracted,rate,0.010);
                            }
                            take=std::move(extracted);
                        }else{
                            take.resize(job.outputFrames);
                            (void)reference::crossfade(take,rate,0.015);
                        }
                        reference::removeDc(take);
                        output->channels[c]=std::move(take);
                    }
                }
                result=output.get();owned.push_back(std::move(output));
            }catch(...){result=nullptr;}
            if(job.edit)releaseOnWorker(job.base);
            jobState.store(3,std::memory_order_release);
        }
        Clip* garbage=nullptr;
        while(retired.pop(garbage)){
            releaseOnWorker(garbage);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
void LoopBank::acceptResult() noexcept {
    if(jobState.load(std::memory_order_acquire)!=3)return;
    auto& track=tracks[job.track];
    for(std::uint32_t i=0;i<job.blockCount;++i)freeBlocks[freeCount++]=job.blocks[i];
    if(track.revision==job.revision&&track.info.state==LoopState::finalizing){
        if(result){
            if(track.clip)residentBlocks-=(track.clip->length+blockFrames-1)/blockFrames;
            // Web Audio reverses into an independent source buffer. NORM edits
            // the stored buffer, leaving that playing reversed copy untouched.
            if(job.edit&&!job.normalize)saveUndo(track,job.track,!job.multiply);
            else if(job.normalize&&resultChanged&&track.activeReverse&&!track.reverseHold)
                track.reverseHold=track.clip; // Transfer the old audio lease.
            else retire(track.clip);
            track.clip=result;
            residentBlocks+=(result->length+blockFrames-1)/blockFrames;
            track.info.length=result->length;
            track.info.finalizationFailed=false;
        }else track.info.finalizationFailed=true;
        const bool play=track.finalPlay&&(job.edit||autoPlay);
        track.info.state=track.clip?(play?LoopState::playing:LoopState::stopped):LoopState::empty;
        if(!track.clip)track.info.length=0;
        if(result){updateRate(track);if(!job.normalize){track.phase=0;track.info.position=0;restartSource(track);}
            if(resultChanged&&!(job.normalize&&track.activeReverse))blend(track);}
    }else retire(result);
    result=nullptr;jobState.store(0,std::memory_order_release);
}
bool LoopBank::finish(Track& track) noexcept {
    if(track.info.state==LoopState::armed){clear(track);return true;}
    if(!capturing(track.info.state))return false;
    track.endAt=0;track.info.stopPending=false;
    if(!track.captureLength||(track.info.state==LoopState::multiplying&&track.captureLength<128)){
        for(std::uint32_t i=0;i<track.blockCount;++i)freeBlocks[freeCount++]=track.blocks[i];
        track.blockCount=track.captureLength=0;
        if(track.info.state==LoopState::recording)clear(track);
        else track.info.state=LoopState::playing;
        return true;
    }
    if(finalizationPending())return false;
    job.blocks=track.blocks;job.blockCount=track.blockCount;job.length=track.captureLength;
    job.track=static_cast<std::uint32_t>(&track-tracks.data());job.revision=track.revision;
    job.normalize=false;job.multiply=track.info.state==LoopState::multiplying;job.doubling=false;
    job.edit=track.info.state!=LoopState::recording;job.base=track.clip;job.baseLength=track.info.length;
    job.recordMode=track.recordMode;job.outputFrames=job.length;
    if(!job.edit){
        if(job.recordMode==SyncMode::fixed)job.outputFrames=track.plannedFrames;
        else if(job.recordMode==SyncMode::dynamic){
            const double bars=std::max(1.0,std::floor(job.length/track.recordBarFrames+0.5));
            job.outputFrames=static_cast<std::uint32_t>(std::min(static_cast<double>(job.length),std::floor(bars*track.recordBarFrames+0.5)));
        }
    }
    if(job.edit)job.base->leases.fetch_add(1,std::memory_order_relaxed);
    job.feedback=track.info.state==LoopState::substituting?0.0:track.info.feedback;job.offset=track.captureOffset;
    track.blockCount=0;track.captureLength=0;track.finalPlay=true;track.playWhileFinalizing=job.edit;
    track.info.state=LoopState::finalizing;
    jobState.store(1,std::memory_order_release);return true;
}
void LoopBank::blend(Track& track) noexcept {track.blendFrom=track.last;track.blendRemaining=fadeLength;}
bool LoopBank::setStartDelay(std::uint32_t id,double value) noexcept {
    if(id>=trackCount||!std::isfinite(value)||value<0||value>1)return false;
    tracks[id].info.startDelay=value;return true;
}
void LoopBank::restartSource(Track& track) noexcept {
    ++track.phaseEpoch;track.info.phaseApplyFrame=0;
    retire(track.reverseHold);track.reverseHold=nullptr;
    track.activeReverse=track.info.state==LoopState::playing&&track.info.reverse;
    if(track.clip&&track.info.state==LoopState::playing){
        const double length=track.clip->length,shift=track.info.startDelay*length;
        const double offset=sync.settings.mode!=SyncMode::off?(static_cast<double>(frame)-sync.origin)*track.phaseRate-shift:shift;
        track.phase=std::fmod(offset,length);if(track.phase<0)track.phase+=length;
        track.info.position=static_cast<std::uint32_t>(track.phase);
    }
}
void LoopBank::saveUndo(Track& track,std::uint32_t id,bool global) noexcept {
    if(track.info.undoCount==track.undo.size()){
        retire(track.undo[0]);std::move(track.undo.begin()+1,track.undo.end(),track.undo.begin());
        --track.info.undoCount;(void)remove(undoOrder,undoSize,id,false);
    }
    track.undo[track.info.undoCount++]=track.clip; // Transfer current audio lease into history.
    for(std::uint32_t i=0;i<track.info.redoCount;++i)retire(track.redo[i]);
    track.redo.fill(nullptr);track.info.redoCount=0;
    if(global){append(undoOrder,undoSize,id);redoSize=0;}
}
bool LoopBank::restore(Track& track,std::uint32_t id,bool redo) noexcept {
    auto& from=redo?track.redo:track.undo;auto& to=redo?track.undo:track.redo;
    auto& fromCount=redo?track.info.redoCount:track.info.undoCount;
    auto& toCount=redo?track.info.undoCount:track.info.redoCount;
    if(!track.clip||!fromCount||toCount>=to.size()||
       (track.info.state!=LoopState::playing&&track.info.state!=LoopState::stopped))return false;
    const auto restored=from[fromCount-1];
    if(restored->length>track.clip->length&&restored->length-track.clip->length>growthRoom(track))return false;
    residentBlocks-=(track.clip->length+blockFrames-1)/blockFrames;
    to[toCount++]=track.clip;track.clip=from[--fromCount];from[fromCount]=nullptr;
    residentBlocks+=(track.clip->length+blockFrames-1)/blockFrames;
    track.info.length=track.clip->length;track.info.finalizationFailed=false;updateRate(track);
    if(redo){if(remove(redoOrder,redoSize,id,true))append(undoOrder,undoSize,id);}
    else if(remove(undoOrder,undoSize,id,true))append(redoOrder,redoSize,id);
    if(track.info.state==LoopState::playing){track.phase=0;track.info.position=0;restartSource(track);blend(track);}
    return true;
}
void LoopBank::stop(Track& track) noexcept {
    if(track.info.state==LoopState::armed){clear(track);return;}
    track.startAt=track.endAt=0;++track.phaseEpoch;track.info.phaseApplyFrame=0;track.info.stopPending=false;
    if(capturing(track.info.state))(void)finish(track);
    if(track.info.state==LoopState::finalizing){track.finalPlay=false;track.playWhileFinalizing=false;}
    else if(track.info.state!=LoopState::empty)track.info.state=LoopState::stopped;
    retire(track.reverseHold);track.reverseHold=nullptr;track.activeReverse=false;
    track.tail=track.last;track.tailRemaining=fadeLength;track.phase=0;track.info.position=0;
}
void LoopBank::clear(Track& track) noexcept {
    ++track.phaseEpoch;++track.revision;track.startAt=track.endAt=0;track.plannedFrames=0;
    for(std::uint32_t i=0;i<track.info.undoCount;++i)retire(track.undo[i]);
    for(std::uint32_t i=0;i<track.info.redoCount;++i)retire(track.redo[i]);
    track.undo.fill(nullptr);track.redo.fill(nullptr);
    const auto historyId=static_cast<std::uint32_t>(&track-tracks.data());
    forget(undoOrder,undoSize,historyId);forget(redoOrder,redoSize,historyId);
    retire(track.reverseHold);track.reverseHold=nullptr;track.activeReverse=false;
    track.tail=track.last;track.tailRemaining=fadeLength;
    for(std::uint32_t i=0;i<track.blockCount;++i)freeBlocks[freeCount++]=track.blocks[i];
    track.blockCount=track.captureLength=0;
    if(track.clip){residentBlocks-=(track.clip->length+blockFrames-1)/blockFrames;retire(track.clip);track.clip=nullptr;}
    const auto id=static_cast<std::uint32_t>(&track-tracks.data());
    if(soloTrack==static_cast<int>(id))soloTrack=-1;
    (void)apply(LoopAction::pan,id,5);
    const bool muted=track.info.muted;track.info=LoopView{};track.info.muted=muted;
    track.phase=0;track.phaseRate=1;track.rate=track.targetRate=1;track.rateRemaining=0;track.playWhileFinalizing=false;
    if(freeCount)poolFull=false;
}
bool LoopBank::apply(LoopAction action,std::uint32_t id,float value) noexcept {
    if(action==LoopAction::stopAll){for(auto& track:tracks)stop(track);return true;}
    if(action==LoopAction::clearAll){undoSize=redoSize=0;for(auto& track:tracks){++track.clearEpoch;clear(track);}return true;}
    if(action==LoopAction::undoGlobal){const int next=nextUndo();return next>=0&&restore(tracks[next],static_cast<std::uint32_t>(next),false);}
    if(action==LoopAction::redoGlobal){const int next=nextRedo();return next>=0&&restore(tracks[next],static_cast<std::uint32_t>(next),true);}
    if(id>=trackCount)return false;
    auto& t=tracks[id];auto& s=t.info.state;
    switch(action){
        case LoopAction::multiply: {
            if(value!=0&&value!=1)return false;
            if(s==LoopState::multiplying)return finish(t);
            if(!t.clip||(s!=LoopState::playing&&s!=LoopState::stopped)||hasCapture())return false;
            if(s==LoopState::playing&&(std::abs(t.info.playbackRate-1)>0.01f||t.rateRemaining))return false;
            if(s==LoopState::playing&&value==1){
                if(!freeCount||growthRoom(t)<128)return false;
                ++t.phaseEpoch;t.info.phaseApplyFrame=0;t.captureLength=t.captureOffset=0;t.info.finalizationFailed=false;s=LoopState::multiplying;poolFull=false;return true;
            }
            if(t.clip->length>growthRoom(t))return false;
            job.blockCount=0;job.length=0;job.offset=0;job.track=id;job.revision=t.revision;
            job.base=t.clip;job.baseLength=t.info.length;job.edit=true;job.normalize=false;job.multiply=job.doubling=true;
            job.base->leases.fetch_add(1,std::memory_order_relaxed);
            ++t.phaseEpoch;t.info.phaseApplyFrame=0;t.finalPlay=t.playWhileFinalizing=s==LoopState::playing;t.info.finalizationFailed=false;
            s=LoopState::finalizing;jobState.store(1,std::memory_order_release);return true;
        }
        case LoopAction::undo:return restore(t,id,false);
        case LoopAction::redo:return restore(t,id,true);
        case LoopAction::record: {
            if(s!=LoopState::empty||!available()||hasCapture())return false;
            t.recordMode=sync.settings.mode;t.recordBarFrames=sync.barFrames();t.recordOrigin=sync.origin;t.plannedFrames=t.recordPrefix=0;
            if(t.recordMode==SyncMode::fixed){
                const double length=std::floor(sync.loopFrames()+0.5);
                if(length>available()||length<1)return false;
                t.plannedFrames=static_cast<std::uint32_t>(length);
            }
            t.phaseRate=1;t.info.playbackRate=t.rate=t.targetRate=1;t.rateRemaining=0;t.captureLength=t.captureOffset=0;
            t.info.length=t.info.position=0;t.phase=0;t.info.finalizationFailed=false;
            t.info.originalBpm=sync.settings.bpm;t.info.dynamicRecorded=t.recordMode==SyncMode::dynamic;
            s=LoopState::recording;poolFull=false;
            if(t.recordMode==SyncMode::fixed){
                double start=sync.nextGridSeconds(frame);
                if((sync.settings.countIn||sync.settings.countInAudio)&&start-static_cast<double>(frame)/rate<sync.barFrames()/rate*0.75)start+=sync.barFrames()/rate;
                t.recordStartSeconds=start;t.startAt=SyncClock::deadline(start*rate);
                // The unchanged extractor floors (targetTime - firstWorkletTime)*rate.
                // Preserve the preceding sample(s) when that falls before the
                // first deadline sample, including floating-point integer edges.
                const double first=static_cast<double>(frame)+std::floor((start-static_cast<double>(frame)/rate)*rate);
                t.recordPrefix=static_cast<std::uint32_t>(std::clamp(static_cast<double>(t.startAt)-first,0.0,2.0));
                t.endAt=t.startAt+t.plannedFrames+static_cast<std::uint64_t>(std::floor(rate*0.02));s=LoopState::armed;
            }
            return true;
        }
        case LoopAction::finish:
            if(s==LoopState::recording&&t.recordMode==SyncMode::dynamic&&!t.info.stopPending){
                const double end=t.recordOrigin+(std::floor((static_cast<double>(frame)-t.recordOrigin)/t.recordBarFrames)+1)*t.recordBarFrames;
                if(end-frame>rate*0.05){t.endAt=SyncClock::deadline(end);t.info.stopPending=true;return true;}
            }
            return finish(t);
        case LoopAction::stopImmediate:stop(t);return true;
        case LoopAction::stop:
            if(s==LoopState::playing&&sync.settings.mode!=SyncMode::off){++t.phaseEpoch;t.info.phaseApplyFrame=0;s=LoopState::stopping;t.endAt=SyncClock::deadline(sync.nextGrid(frame));return true;}
            stop(t);return true;
        case LoopAction::clear:++t.clearEpoch;clear(t);return true;
        case LoopAction::play:
            if(s!=LoopState::stopped||!t.clip)return false;
            if(sync.settings.mode!=SyncMode::off){s=LoopState::queued;t.startAt=SyncClock::deadline(sync.nextGrid(frame));return true;}
            s=LoopState::playing;t.phase=0;t.info.position=0;t.tailRemaining=0;restartSource(t);blend(t);return true;
        case LoopAction::overdub:
        case LoopAction::substitute:
            if(s!=LoopState::playing||!t.clip||hasCapture()||t.info.playbackRate!=1||t.rateRemaining)return false;
            ++t.phaseEpoch;t.info.phaseApplyFrame=0;t.captureLength=0;t.captureOffset=t.info.position;t.info.finalizationFailed=false;
            s=action==LoopAction::overdub?LoopState::overdubbing:LoopState::substituting;return true;
        case LoopAction::volume:
            if(!std::isfinite(value)||value<0||value>2)return false;
            t.info.volume=value;return true;
        case LoopAction::feedback:
            if(!std::isfinite(value)||value<0||value>1)return false;
            t.info.feedback=value;return true;
        case LoopAction::mute:
            if(value!=0&&value!=1)return false;
            t.info.muted=value!=0;return true;
        case LoopAction::reverse:
            if(value!=0&&value!=1)return false;
            t.info.reverse=value!=0;return true; // Apply on the next play/retrigger.
        case LoopAction::solo:soloTrack=soloTrack==static_cast<int>(id)?-1:static_cast<int>(id);return true;
        case LoopAction::normalize:
            if(!t.clip||(s!=LoopState::playing&&s!=LoopState::stopped)||hasCapture())return false;
            job.blockCount=0;job.length=t.clip->length;job.offset=0;job.track=id;job.revision=t.revision;
            job.base=t.clip;job.baseLength=t.info.length;job.edit=true;job.normalize=true;job.multiply=job.doubling=false;
            job.base->leases.fetch_add(1,std::memory_order_relaxed);
            ++t.phaseEpoch;t.info.phaseApplyFrame=0;t.finalPlay=t.playWhileFinalizing=s==LoopState::playing;t.info.finalizationFailed=false;
            s=LoopState::finalizing;jobState.store(1,std::memory_order_release);return true;
        case LoopAction::retrigger:
            if(!t.clip||capturing(s)||s==LoopState::finalizing||s==LoopState::armed||s==LoopState::queued||s==LoopState::stopping)return false;
            s=LoopState::playing;t.phase=0;t.info.position=0;t.tailRemaining=0;restartSource(t);blend(t);return true;
        case LoopAction::pan: {
            if (!std::isfinite(value) || value < 0 || value > 10) return false;
            value = std::trunc(value); // UIManager.setLoopPan uses parseInt.
            if (value == t.info.pan) return true;
            t.info.pan = value;
            // Reference GainNodes feed mono ChannelMerger inputs, at angle pan*pi/20.
            const float angle = value * 0.15707963267948966f;
            t.panTarget = {value == 10 ? 0.0f : std::cos(angle), value == 0 ? 0.0f : std::sin(angle)};
            t.panRemaining = fadeLength * 4;
            if (t.panRemaining) t.panIncrement = {(t.panTarget.left-t.panGain.left)/t.panRemaining,(t.panTarget.right-t.panGain.right)/t.panRemaining};
            else t.panGain = t.panTarget;
            return true;
        }
        case LoopAction::startDelay:return setStartDelay(id,value);
        case LoopAction::nudgeStart: {
            if(value!=-1&&value!=1)return false;
            if(!t.clip||!t.info.length)return true;
            const double previous=t.info.startDelay;
            double phase=previous+value*((60.0/sync.settings.bpm/16.0)/(t.info.length/rate));
            // Preserve the reference's single-wrap arithmetic and endpoints.
            if(phase<0)phase+=1;
            if(phase>1)phase-=1;
            // A nudge can span many cycles on a tiny clip. Keep it playable.
            if(phase<0||phase>1){phase=std::fmod(phase,1.0);if(phase<0)phase+=1;}
            t.info.startDelay=phase;
            if(apply(LoopAction::applyStart,id))return true;
            t.info.startDelay=previous;return false;
        }
        case LoopAction::quantizeStart: {
            if(!t.clip||!t.info.length)return true;
            // Original Q uses recorded duration, not varispeed-adjusted duration.
            // floor(x+0.5) rounds the representable value just below a half
            // upward on some inputs; JS Math.round must leave it below.
            const auto round=[](double x) noexcept {const double base=std::floor(x);return base+(x-base>=0.5?1.0:0.0);};
            const double divisions=round((t.info.length/rate)/(60.0/sync.settings.bpm/4.0));
            if(divisions<=0)return true;
            const double previous=t.info.startDelay;
            t.info.startDelay=std::clamp(round(previous*divisions)/divisions,0.0,1.0);
            if(apply(LoopAction::applyStart,id))return true;
            t.info.startDelay=previous;return false; // FIFO rejection preserves accepted state.
        }
        case LoopAction::applyStart: {
            if(s!=LoopState::playing||!t.clip)return true; // Original restart guard.
            const double target=static_cast<double>(frame)/rate+0.05;
            const auto deadline=SyncClock::deadline(target*rate);
            const double length=t.clip->length,shift=t.info.startDelay*length;
            const bool excluded=soloTrack>=0&&soloTrack!=static_cast<int>(id);
            const double offset=sync.settings.mode!=SyncMode::off&&!excluded?(target*rate-sync.origin)*t.phaseRate-shift:shift;
            // The reference creates the future source immediately. Latch its
            // offset and reverse choice, not a later slider/toggle value.
            double phase=std::fmod(offset+(static_cast<double>(deadline)-target*rate)*t.phaseRate,length);
            if(phase<0)phase+=length;
            // Reclaim cancelled events at the head, bounded by fixed capacity.
            while(phaseRead!=phaseWrite&&phaseEvents[phaseRead].epoch!=tracks[phaseEvents[phaseRead].track].phaseEpoch)
                phaseRead=(phaseRead+1)%phaseCapacity;
            const unsigned next=(phaseWrite+1)%phaseCapacity;
            if(next==phaseRead)return false; // Keep every older accepted source.
            phaseEvents[phaseWrite]={deadline,t.phaseEpoch,phase,id,t.info.reverse};phaseWrite=next;
            t.info.phaseApplyFrame=deadline;return true;
        }
        case LoopAction::speed:
            if (!std::isfinite(value) || value < 0.25f || value > 2 || s == LoopState::recording
                || s == LoopState::overdubbing || s == LoopState::substituting || s == LoopState::multiplying || s == LoopState::finalizing) return false;
            if (value == t.info.playbackRate) return true;
            t.info.playbackRate = value;updateRate(t);
            return true;
        case LoopAction::undoGlobal:
        case LoopAction::redoGlobal:
        case LoopAction::clearAll:
        case LoopAction::stopAll:break;
    }
    return false;
}
std::uint32_t LoopBank::available() const noexcept {
    auto used=residentBlocks;
    for(const auto& track:tracks){
        if(track.info.state==LoopState::armed||(track.info.state==LoopState::recording&&track.recordMode==SyncMode::fixed))used+=(track.plannedFrames+blockFrames-1)/blockFrames;
        else if(track.info.state==LoopState::recording)used+=track.blockCount;
        if(track.info.state==LoopState::multiplying)
            used+=(track.info.length+track.captureLength+blockFrames-1)/blockFrames-(track.info.length+blockFrames-1)/blockFrames;
    }
    if(finalizationPending()){
        if(!job.edit)used+=(job.outputFrames+blockFrames-1)/blockFrames;
        else if(job.multiply)used+=(job.baseLength+(job.doubling?job.baseLength:job.length)+blockFrames-1)/blockFrames-(job.baseLength+blockFrames-1)/blockFrames;
    }
    return std::min(freeCount,totalBlocks-std::min(used,totalBlocks))*blockFrames;
}
std::uint32_t LoopBank::growthRoom(const Track& track) const noexcept {
    auto used=residentBlocks;
    for(const auto& t:tracks){
        if(t.info.state==LoopState::armed||(t.info.state==LoopState::recording&&t.recordMode==SyncMode::fixed))used+=(t.plannedFrames+blockFrames-1)/blockFrames;
        else if(t.info.state==LoopState::recording)used+=t.blockCount;
        if(t.info.state==LoopState::multiplying&& &t!=&track)
            used+=(t.info.length+t.captureLength+blockFrames-1)/blockFrames-(t.info.length+blockFrames-1)/blockFrames;
    }
    if(finalizationPending()){
        if(!job.edit)used+=(job.outputFrames+blockFrames-1)/blockFrames;
        else if(job.multiply&& &tracks[job.track]!=&track){
            const auto length=job.baseLength;
            used+=(length+(job.doubling?length:job.length)+blockFrames-1)/blockFrames-(length+blockFrames-1)/blockFrames;
        }
    }
    return (totalBlocks-std::min(used,totalBlocks)+(track.info.length+blockFrames-1)/blockFrames)*blockFrames-track.info.length;
}
bool LoopBank::hasCapture() const noexcept {
    if(finalizationPending())return true;
    for(const auto& track:tracks)if(capturing(track.info.state)||track.info.state==LoopState::armed)return true;
    return false;
}
void LoopBank::beginBlock() noexcept {acceptResult();for(auto& track:tracks)track.info.peak=0;}
void LoopBank::applyPhaseEvents() noexcept {
    for(unsigned n=0;phaseRead!=phaseWrite&&n<phaseCapacity;++n){
        const auto event=phaseEvents[phaseRead];auto& track=tracks[event.track];
        const bool valid=event.epoch==track.phaseEpoch;
        if(valid&&event.frame>frame)break;
        phaseRead=(phaseRead+1)%phaseCapacity;
        if(!valid||track.info.state!=LoopState::playing||!track.clip)continue;
        retire(track.reverseHold);track.reverseHold=nullptr;
        track.phase=event.phase;track.activeReverse=event.reverse;
        track.info.position=static_cast<std::uint32_t>(track.phase);
        if(track.info.phaseApplyFrame==event.frame)track.info.phaseApplyFrame=0;
        blend(track);
    }
}
Stereo LoopBank::tick(Stereo input) noexcept {
    applyPhaseEvents();
    input={safe(input.left),safe(input.right)};Stereo mix{};
    for(auto& t:tracks){
        auto& v=t.info;schedules(t);
        if(capturing(v.state)){
            if(v.state==LoopState::multiplying&&t.captureLength>=growthRoom(t)){poolFull=true;(void)finish(t);}
            if(capturing(v.state)&&t.captureLength%blockFrames==0){
                const bool room=v.state!=LoopState::recording||t.recordMode==SyncMode::fixed||residentBlocks+t.blockCount<totalBlocks;
                if(freeCount&&room)t.blocks[t.blockCount++]=freeBlocks[--freeCount];
                else {poolFull=true;(void)finish(t);}
            }
            if(capturing(v.state)){
                sample(t,t.captureLength,0)=input.left;sample(t,t.captureLength,1)=input.right;++t.captureLength;
                if(v.state==LoopState::recording)v.length=t.captureLength;
            }
        }
        const bool audible=t.clip&&(v.state==LoopState::playing||v.state==LoopState::stopping||v.state==LoopState::overdubbing||v.state==LoopState::substituting||v.state==LoopState::multiplying||(v.state==LoopState::finalizing&&t.playWhileFinalizing));
        if(t.rateRemaining){t.rate+=t.rateIncrement;if(--t.rateRemaining==0)t.rate=t.targetRate;}
        if(t.panRemaining){t.panGain.left+=t.panIncrement.left;t.panGain.right+=t.panIncrement.right;if(--t.panRemaining==0)t.panGain=t.panTarget;}
        Stereo out{};
        if(audible){
            v.position=static_cast<std::uint32_t>(t.phase);
            const auto next=v.position+1==t.clip->length?0:v.position+1;
            const float fraction=static_cast<float>(t.phase-v.position);
            const auto* source=t.reverseHold?t.reverseHold:t.clip;
            const auto index=t.activeReverse?source->length-1-v.position:v.position;
            const auto nextIndex=t.activeReverse?source->length-1-next:next;
            const auto& left=source->channels[0];const auto& right=source->channels[1];
            out={left[index]+(left[nextIndex]-left[index])*fraction,right[index]+(right[nextIndex]-right[index])*fraction};
            if(t.blendRemaining){const float progress=1-static_cast<float>(--t.blendRemaining)/fadeLength;out={t.blendFrom.left+(out.left-t.blendFrom.left)*progress,t.blendFrom.right+(out.right-t.blendFrom.right)*progress};}
            t.last=out;t.phase+=t.rate;
            if(t.phase>=t.clip->length){t.phase=std::fmod(t.phase,static_cast<double>(t.clip->length));blend(t);}
            v.position=static_cast<std::uint32_t>(t.phase);
        }else if(t.tailRemaining){const float progress=static_cast<float>(--t.tailRemaining)/fadeLength;out={t.tail.left*progress,t.tail.right*progress};t.last=out;}
        else t.last={};
        const bool excluded=soloTrack>=0&&soloTrack!=static_cast<int>(&t-tracks.data());
        const float desired=v.muted||excluded?0:v.volume;
        const float step=fadeLength?1.0f/static_cast<float>(fadeLength*4):2;
        t.gain+=std::clamp(desired-t.gain,-step,step);
        const float mono=(out.left+out.right)*0.5f;out={mono*t.panGain.left*t.gain,mono*t.panGain.right*t.gain};
        v.peak=std::max(v.peak,std::max(std::abs(out.left),std::abs(out.right)));mix.left+=out.left;mix.right+=out.right;
    }
    olderInput=previousInput;previousInput=input;++frame;return mix;
}
void LoopBank::configureSync(SyncSettings settings) noexcept {
    sync.configure(settings,frame);for(auto& track:tracks)updateRate(track);
}
void LoopBank::updateRate(Track& track) noexcept {
    if(!track.clip)return;
    double target=track.info.playbackRate;
    if(sync.settings.mode!=SyncMode::off&&!track.info.dynamicRecorded&&track.info.originalBpm>0&&track.info.length){
        const double duration=static_cast<double>(track.info.length)/rate;
        const double expected=duration/(sync.settings.bpm/track.info.originalBpm);
        const double ratio=expected/(sync.loopFrames()/rate);
        const double natural=ratio>=1?std::floor(ratio+0.5):1/std::max(1.0,std::floor(1/ratio+0.5));
        target=duration/(sync.loopFrames()/rate*natural)*track.info.playbackRate;
    }
    track.phaseRate=std::max(0.001,target); // JS source offset uses Number before AudioParam float conversion.
    const auto next=static_cast<float>(track.phaseRate);
    if(next==track.targetRate)return;
    track.targetRate=next;track.rateRemaining=fadeLength*4;
    if(track.rateRemaining)track.rateIncrement=(next-track.rate)/track.rateRemaining;else track.rate=next;
}
void LoopBank::schedules(Track& track) noexcept {
    auto& state=track.info.state;
    if(track.startAt&&frame>=track.startAt){
        track.startAt=0;
        if(state==LoopState::armed){
            state=LoopState::recording;
            if(track.recordPrefix&&freeCount){
                track.blocks[track.blockCount++]=freeBlocks[--freeCount];
                for(std::uint32_t i=0;i<track.recordPrefix;++i){
                    const auto value=track.recordPrefix==2&&i==0?olderInput:previousInput;
                    sample(track,track.captureLength,0)=value.left;sample(track,track.captureLength,1)=value.right;++track.captureLength;
                }
            }
            track.recordPrefix=0;
        }
        else if(state==LoopState::queued){state=LoopState::playing;track.tailRemaining=0;restartSource(track);blend(track);}
    }
    if(track.endAt&&frame>=track.endAt){
        track.endAt=0;
        if(state==LoopState::stopping)stop(track);
        else if(state==LoopState::recording)(void)finish(track);
    }
}
std::array<LoopView,LoopBank::trackCount> LoopBank::view() const noexcept {
    std::array<LoopView,trackCount> views{};
    for(std::uint32_t i=0;i<trackCount;++i){views[i]=tracks[i].info;views[i].solo=soloTrack==static_cast<int>(i);views[i].effectiveRate=tracks[i].targetRate;
        views[i].clearEpoch=tracks[i].clearEpoch;views[i].scheduledFrame=tracks[i].startAt?tracks[i].startAt:tracks[i].endAt;}
    return views;
}
}
