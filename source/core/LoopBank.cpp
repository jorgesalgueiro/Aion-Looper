#include "core/LoopBank.h"
#include "core/ReferenceDsp.h"
#include <cmath>
#include <chrono>
namespace aion {
namespace {
float safe(float value) noexcept { return std::isfinite(value) ? std::clamp(value,-8.0f,8.0f) : 0.0f; }
bool capturing(LoopState state) noexcept {return state==LoopState::recording||state==LoopState::overdubbing||state==LoopState::substituting;}
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
    freeCount=totalBlocks;residentBlocks=0;
    for(std::uint32_t i=0;i<totalBlocks;++i)freeBlocks[i]=static_cast<std::uint16_t>(i);
    fadeLength=std::min(fadeFrames,1920u);poolFull=false;soloTrack=-1;
    rate=std::isfinite(sampleRate)&&sampleRate>0?sampleRate:48000;
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
                output->length=job.edit?job.base->length:job.length;
                if(job.normalize){
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
                        (void)reference::crossfade(take,rate,0.015);
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
            if(job.normalize&&resultChanged&&track.activeReverse&&!track.reverseHold)
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
        if(result){if(!job.normalize){track.phase=0;track.info.position=0;restartSource(track);}
            if(resultChanged&&!(job.normalize&&track.activeReverse))blend(track);}
    }else retire(result);
    result=nullptr;jobState.store(0,std::memory_order_release);
}
bool LoopBank::finish(Track& track) noexcept {
    if(!capturing(track.info.state))return false;
    if(!track.captureLength){
        if(track.info.state==LoopState::recording)clear(track);
        else track.info.state=LoopState::playing;
        return true;
    }
    if(finalizationPending())return false;
    job.blocks=track.blocks;job.blockCount=track.blockCount;job.length=track.captureLength;
    job.track=static_cast<std::uint32_t>(&track-tracks.data());job.revision=track.revision;
    job.normalize=false;job.edit=track.info.state!=LoopState::recording;job.base=track.clip;
    if(job.edit)job.base->leases.fetch_add(1,std::memory_order_relaxed);
    job.feedback=track.info.state==LoopState::substituting?0.0:track.info.feedback;job.offset=track.captureOffset;
    track.blockCount=0;track.captureLength=0;track.finalPlay=true;track.playWhileFinalizing=job.edit;
    track.info.state=LoopState::finalizing;
    jobState.store(1,std::memory_order_release);return true;
}
void LoopBank::blend(Track& track) noexcept {track.blendFrom=track.last;track.blendRemaining=fadeLength;}
void LoopBank::restartSource(Track& track) noexcept {
    retire(track.reverseHold);track.reverseHold=nullptr;
    track.activeReverse=track.info.state==LoopState::playing&&track.info.reverse;
}
void LoopBank::stop(Track& track) noexcept {
    if(capturing(track.info.state))(void)finish(track);
    if(track.info.state==LoopState::finalizing){track.finalPlay=false;track.playWhileFinalizing=false;}
    else if(track.info.state!=LoopState::empty)track.info.state=LoopState::stopped;
    retire(track.reverseHold);track.reverseHold=nullptr;track.activeReverse=false;
    track.tail=track.last;track.tailRemaining=fadeLength;track.phase=0;track.info.position=0;
}
void LoopBank::clear(Track& track) noexcept {
    ++track.revision;
    retire(track.reverseHold);track.reverseHold=nullptr;track.activeReverse=false;
    track.tail=track.last;track.tailRemaining=fadeLength;
    for(std::uint32_t i=0;i<track.blockCount;++i)freeBlocks[freeCount++]=track.blocks[i];
    track.blockCount=track.captureLength=0;
    if(track.clip){residentBlocks-=(track.clip->length+blockFrames-1)/blockFrames;retire(track.clip);track.clip=nullptr;}
    const auto id=static_cast<std::uint32_t>(&track-tracks.data());
    if(soloTrack==static_cast<int>(id))soloTrack=-1;
    (void)apply(LoopAction::pan,id,5);
    const bool muted=track.info.muted;track.info=LoopView{};track.info.muted=muted;
    track.phase=0;track.rate=1;track.rateRemaining=0;track.playWhileFinalizing=false;
    if(freeCount)poolFull=false;
}
bool LoopBank::apply(LoopAction action,std::uint32_t id,float value) noexcept {
    if(action==LoopAction::stopAll){for(auto& track:tracks)stop(track);return true;}
    if(action==LoopAction::clearAll){for(auto& track:tracks)clear(track);return true;}
    if(id>=trackCount)return false;
    auto& t=tracks[id];auto& s=t.info.state;
    switch(action){
        case LoopAction::record:
            if(s!=LoopState::empty||!available()||hasCapture())return false;
            t.info.playbackRate=t.rate=1;t.rateRemaining=0;t.captureLength=t.captureOffset=0;
            t.info.length=t.info.position=0;t.phase=0;t.info.finalizationFailed=false;s=LoopState::recording;poolFull=false;return true;
        case LoopAction::finish:return finish(t);
        case LoopAction::stop:stop(t);return true;
        case LoopAction::clear:clear(t);return true;
        case LoopAction::play:
            if(s!=LoopState::stopped||!t.clip)return false;
            s=LoopState::playing;t.phase=0;t.info.position=0;t.tailRemaining=0;restartSource(t);blend(t);return true;
        case LoopAction::overdub:
        case LoopAction::substitute:
            if(s!=LoopState::playing||!t.clip||hasCapture()||t.info.playbackRate!=1||t.rateRemaining)return false;
            t.captureLength=0;t.captureOffset=t.info.position;t.info.finalizationFailed=false;
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
            job.base=t.clip;job.edit=true;job.normalize=true;
            job.base->leases.fetch_add(1,std::memory_order_relaxed);
            t.finalPlay=t.playWhileFinalizing=s==LoopState::playing;t.info.finalizationFailed=false;
            s=LoopState::finalizing;jobState.store(1,std::memory_order_release);return true;
        case LoopAction::retrigger:
            if(!t.clip||capturing(s)||s==LoopState::finalizing)return false;
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
        case LoopAction::speed:
            if (!std::isfinite(value) || value < 0.25f || value > 2 || s == LoopState::recording
                || s == LoopState::overdubbing || s == LoopState::substituting || s == LoopState::finalizing) return false;
            if (value == t.info.playbackRate) return true;
            t.info.playbackRate = value; t.rateRemaining = fadeLength * 4;
            if (t.rateRemaining) t.rateIncrement = (value-t.rate)/t.rateRemaining;
            else t.rate = value;
            return true;
        case LoopAction::clearAll:
        case LoopAction::stopAll:break;
    }
    return false;
}
std::uint32_t LoopBank::available() const noexcept {
    auto used=residentBlocks;
    for(const auto& track:tracks)if(track.info.state==LoopState::recording)used+=track.blockCount;
    if(finalizationPending()&&!job.edit)used+=job.blockCount;
    return std::min(freeCount,totalBlocks-std::min(used,totalBlocks))*blockFrames;
}
bool LoopBank::hasCapture() const noexcept {
    if(finalizationPending())return true;
    for(const auto& track:tracks)if(capturing(track.info.state))return true;
    return false;
}
void LoopBank::beginBlock() noexcept {acceptResult();for(auto& track:tracks)track.info.peak=0;}
Stereo LoopBank::tick(Stereo input) noexcept {
    input={safe(input.left),safe(input.right)};Stereo mix{};
    for(auto& t:tracks){
        auto& v=t.info;
        if(capturing(v.state)){
            if(t.captureLength%blockFrames==0){
                const bool room=v.state!=LoopState::recording||residentBlocks+t.blockCount<totalBlocks;
                if(freeCount&&room)t.blocks[t.blockCount++]=freeBlocks[--freeCount];
                else {poolFull=true;(void)finish(t);}
            }
            if(capturing(v.state)){
                sample(t,t.captureLength,0)=input.left;sample(t,t.captureLength,1)=input.right;++t.captureLength;
                if(v.state==LoopState::recording)v.length=t.captureLength;
            }
        }
        const bool audible=t.clip&&(v.state==LoopState::playing||v.state==LoopState::overdubbing||v.state==LoopState::substituting||(v.state==LoopState::finalizing&&t.playWhileFinalizing));
        if(t.rateRemaining){t.rate+=t.rateIncrement;if(--t.rateRemaining==0)t.rate=v.playbackRate;}
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
    return mix;
}
std::array<LoopView,LoopBank::trackCount> LoopBank::view() const noexcept {
    std::array<LoopView,trackCount> views{};
    for(std::uint32_t i=0;i<trackCount;++i){views[i]=tracks[i].info;views[i].solo=soloTrack==static_cast<int>(i);}
    return views;
}
}
