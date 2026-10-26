#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
namespace aion {
struct MeterGraphStart {
    std::uint64_t frame=0,clearEpoch=0,order=0;
    std::uint32_t track=0,ticket=0;
};
// Audio-owner-only fixed min-heap. Allocate its storage at Engine construction.
// Future telemetry must never sit at the head of the performance command FIFO.
class MeterGraphStarts {
public:
    static constexpr unsigned capacity=256;
    bool push(std::uint64_t frame,std::uint64_t clearEpoch,std::uint32_t track,std::uint32_t ticket) noexcept {
        if(count==capacity||!ticket||sequence==std::numeric_limits<std::uint64_t>::max())return false;
        heap[count++]={frame,clearEpoch,sequence++,track,ticket};
        std::push_heap(heap.begin(),heap.begin()+count,Later{});return true;
    }
    bool popDue(std::uint64_t frame,MeterGraphStart& output) noexcept {
        if(!count||heap[0].frame>frame)return false;
        output=heap[0];std::pop_heap(heap.begin(),heap.begin()+count,Later{});--count;return true;
    }
    void clear() noexcept {count=0;sequence=0;}
    template<class Stale> void discardIf(Stale&& stale) noexcept {
        unsigned kept=0;for(unsigned n=0;n<count;++n)if(!stale(heap[n]))heap[kept++]=heap[n];
        if(kept!=count){count=kept;std::make_heap(heap.begin(),heap.begin()+count,Later{});}
    }
    unsigned size() const noexcept {return count;}
private:
    struct Later {
        bool operator()(const MeterGraphStart& a,const MeterGraphStart& b) const noexcept {
            return a.frame>b.frame||(a.frame==b.frame&&a.order>b.order);
        }
    };
    std::array<MeterGraphStart,capacity> heap{};
    unsigned count=0;
    std::uint64_t sequence=0;
};
}
