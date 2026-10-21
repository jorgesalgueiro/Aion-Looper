#pragma once
#include "core/LoopBank.h"
#include <limits>
namespace aion {
// Packed binding: kind (1 note, 2 CC, 3 program), number, optional channel.
constexpr std::uint16_t midiBinding(unsigned kind,unsigned number,unsigned channel=0) noexcept {return static_cast<std::uint16_t>(kind|(number<<2)|(channel<<9));}
struct MidiSettings {
    static constexpr unsigned bindingCount=73,volumeBase=33,panBase=53;
    bool enabled=false;
    std::array<std::uint16_t,MidiSettings::bindingCount> bindings=[] {std::array<std::uint16_t,MidiSettings::bindingCount> b{};for(unsigned i=0;i<10;++i)b[i]=midiBinding(1,36+i);b[30]=midiBinding(1,46);b[31]=midiBinding(1,47);b[32]=midiBinding(1,48);return b;}();
    std::uint32_t revision=0,learnToken=0,learnGeneration=0;
    int learnTarget=-1,modeOverride=-1;
};
struct MidiEvent {std::uint32_t offset=0;std::uint8_t status=0,data1=0,data2=0,size=0;};
struct MidiLearnResult {int target=-1;std::uint16_t binding=0;std::uint32_t token=0,generation=0;};
struct MidiObservation {std::uint64_t frame=0;MidiEvent event{};};
struct MidiView {
    std::array<std::uint16_t,MidiSettings::bindingCount> bindings{};
    std::uint32_t revision=0,modeRevision=0,messages=0,overflows=0,actions=0,rejected=0;
    std::array<MidiObservation,8> recent{};
    std::uint32_t observed=0;
    int learnTarget=-1,mode=0;
    bool enabled=false,sustain=false;
};
class MidiController {
public:
    struct Dispatch {LoopBank& bank;std::uint64_t frame;double rate;std::uint32_t generation;void* context;bool(*apply)(void*,LoopAction,std::uint32_t,double);};
    static bool valid(const MidiSettings&) noexcept;
    static int targetFor(const MidiSettings&,const MidiEvent&) noexcept;
    bool configure(const MidiSettings& value) noexcept {return valid(value)&&configs.push(value);}
    bool popLearn(MidiLearnResult& value) noexcept {return learned.pop(value);}
    void begin(Dispatch&) noexcept;
    void event(const MidiEvent&,Dispatch&) noexcept;
    void tick(Dispatch&) noexcept;
    void cancel(Dispatch&,int track=-1) noexcept;
    void prepare(Dispatch&) noexcept;
    void overflow(Dispatch&) noexcept;
    MidiView view() const noexcept;
private:
    enum class TaskKind {main,cut,erase};
    struct Task {TaskKind kind=TaskKind::main;unsigned track=0;int mode=0;std::uint64_t clearEpoch=0,epoch=0;};
    struct Press {
        bool active=false,suppressed=false,cut=false,erase=false,canDelete=false;
        unsigned key=0,track=0;
        std::uint64_t start=0,clearEpoch=0;
        LoopState initial=LoopState::empty;
        int sustainAction=0;float oldMute=0;
    };
    SpscQueue<MidiSettings,32> configs;
    SpscQueue<MidiLearnResult,8> learned;
    MidiSettings settings;
    std::array<MidiObservation,8> recent{};
    std::uint32_t observed=0;
    std::array<Press,64> presses{};
    struct ProgramPulse {std::uint64_t frame=0;unsigned key=0;bool valid=false;};
    std::array<ProgramPulse,33> programPulses{};
    std::array<Task,128> tasks{};
    unsigned read=0,write=0;
    std::uint64_t epoch=0,nextDeadline=std::numeric_limits<std::uint64_t>::max();
    std::uint32_t modeRevision=0,messages=0,overflows=0,actions=0,rejections=0;
    int mode=0;bool sustain=false,learnCaptured=false;
    void push(Task,Dispatch&) noexcept;
    void main(unsigned,int,Dispatch&) noexcept;
    void release(Press&,Dispatch&) noexcept;
    bool apply(LoopAction,unsigned,double,Dispatch&) noexcept;
};
}
