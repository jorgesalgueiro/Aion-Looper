#pragma once
#include "core/MeterWindow.h"
#include "core/SpscQueue.h"
#include "core/CallbackClock.h"
#include <atomic>
#include <cstdint>

namespace aion {
// Raw PCM telemetry, not a rendered meter or an audible-time observation.
// endFrame is exclusive in the local audio clock, never a DAW seek position.
struct MeterWindowEvent {
    std::array<float,MeterWindow::size> samples{};
    std::uint64_t endFrame=0,clearEpoch=0,waveformId=0,sourceRevision=0;
    double sampleRate=0;
    std::uint32_t generation=0,track=0,graphTicket=0; // Request provenance, never sourceRevision/analyser identity.
    std::uint32_t windowGraphTicket=0; // Actual reset, separate from request provenance.
    std::uint64_t graphStartFrame=0;
    CallbackClockStamp callbackClock{};
};

// Construct on the heap off audio. One audio publisher and one message consumer.
// Requests coalesce per track; reset cancels them. Queue storage is never reset
// from the producer: the consumer discards old generations on its bounded pop.
class MeterTransfer {
public:
    static constexpr unsigned trackCount=20,queueSize=8,publishLimit=2;
    bool request(unsigned track) noexcept {
        return requestWord(track,1); // Generation zero denotes the existing unbound diagnostic API.
    }
    bool request(unsigned track,std::uint32_t generation,std::uint32_t ticket) noexcept {
        if(!generation||!ticket)return false;
        return requestWord(track,(static_cast<std::uint64_t>(generation)<<32)|ticket);
    }
    // Audio owner only, at the prepare barrier. Already queued events stay intact.
    void reset() noexcept {
        requested.exchange(0,std::memory_order_acq_rel);pending=0;cursor=0;
        pendingRequests.fill(0);
        for(auto& word:requestWords)word.exchange(0,std::memory_order_acq_rel);
    }
    // Fill is audio-owned and supplies the current window + clear/clip identity.
    // At most two window copies per callback, no wait/retry loop on queue-full.
    template<class Fill> void publish(std::uint32_t generation,std::uint64_t frame,double rate,Fill&& fill) noexcept {
        pending|=requested.exchange(0,std::memory_order_acq_rel);
        if(!pending)return;
        unsigned emitted=0;
        for(unsigned n=0;n<trackCount&&emitted<publishLimit;++n){
            const auto id=cursor;cursor=(cursor+1)%trackCount;
            const auto bit=1u<<id;if(!(pending&bit))continue;
            if(const auto word=requestWords[id].exchange(0,std::memory_order_acq_rel))pendingRequests[id]=word;
            const auto word=pendingRequests[id];
            const auto requestedGeneration=static_cast<std::uint32_t>(word>>32);
            if(!word||(requestedGeneration&&requestedGeneration!=generation)){
                pending&=~bit;pendingRequests[id]=0;continue;
            }
            producerEvent.generation=generation;producerEvent.endFrame=frame;
            producerEvent.sampleRate=rate;producerEvent.track=id;
            producerEvent.graphTicket=requestedGeneration?static_cast<std::uint32_t>(word):0;
            fill(id,producerEvent);
            if(!events.push(producerEvent))break; // Keep request, recapture current PCM later.
            pending&=~bit;pendingRequests[id]=0;++emitted;
        }
    }
    // Message owner only. Generation zero is never a prepared engine. Failure
    // leaves output untouched, including when all seven queued events are stale.
    // Clip/clear metadata must also be compared with a coherent current view;
    // these identities do not certify playback/retrigger/analyser lifecycle.
    bool pop(MeterWindowEvent& output,std::uint32_t generation) noexcept {
        for(unsigned n=0;n<queueSize-1&&events.pop(consumerEvent);++n){
            if(generation&&consumerEvent.generation==generation){output=consumerEvent;return true;}
        }
        return false;
    }
private:
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,"Meter request tickets must never use a locking atomic");
    bool requestWord(unsigned track,std::uint64_t word) noexcept {
        if(track>=trackCount)return false;
        requestWords[track].store(word,std::memory_order_release);
        requested.fetch_or(1u<<track,std::memory_order_release);return true;
    }
    std::array<std::atomic<std::uint64_t>,trackCount> requestWords{};
    std::array<std::uint64_t,trackCount> pendingRequests{}; // Audio owner only; recapture after queue-full.
    std::atomic<std::uint32_t> requested{0};
    std::uint32_t pending=0;
    unsigned cursor=0;
    MeterWindowEvent producerEvent{},consumerEvent{};
    SpscQueue<MeterWindowEvent,queueSize> events;
};
}
