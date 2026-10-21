#include "core/MidiController.h"
#include <algorithm>
#include <cmath>
namespace aion {
bool MidiController::valid(const MidiSettings& s) noexcept {
    if(s.learnTarget < -1||s.learnTarget>=static_cast<int>(MidiSettings::bindingCount)||(s.learnTarget>=20&&s.learnTarget<30)||s.modeOverride < -1||s.modeOverride>2)return false;
    for(unsigned i=0;i<MidiSettings::bindingCount;++i){const auto b=s.bindings[i];if(!b)continue;
        const unsigned kind=b&3,number=(b>>2)&127,channel=b>>9;
        if(!kind||channel>16||(i>=20&&i<30)||(i>=MidiSettings::volumeBase&&kind!=2)||(kind==2&&(number==120||number==123)))return false;
        for(unsigned j=0;j<i;++j){const auto a=s.bindings[j];if(a&&(a&511)==(b&511)&&(!(a>>9)||!channel||(a>>9)==channel))return false;}
    }
    return true;
}
int MidiController::targetFor(const MidiSettings& s,const MidiEvent& e) noexcept {
    const unsigned type=e.status&240,channel=(e.status&15)+1;
    const unsigned kind=(type==128||type==144)&&e.size==3?1:type==176&&e.size==3?2:type==192&&e.size==2?3:0;
    if(!kind||e.data1>127||e.data2>127)return -1;
    for(unsigned i=0;i<MidiSettings::bindingCount;++i){const auto b=s.bindings[i];if((b&3)==kind&&((b>>2)&127)==e.data1&&(!(b>>9)||(b>>9)==channel))return static_cast<int>(i);}
    return -1;
}
bool MidiController::apply(LoopAction action,unsigned id,double value,Dispatch& d) noexcept {
    const bool ok=d.apply(d.context,action,id,value);if(ok)++actions;else ++rejections;return ok;
}
void MidiController::release(Press& p,Dispatch& d) noexcept {
    if(p.sustainAction&&!p.suppressed&&d.bank.trackView(p.track).clearEpoch==p.clearEpoch){
        if(p.sustainAction==1)apply(LoopAction::finish,p.track,0,d);
        else if(p.sustainAction==2)apply(LoopAction::stopImmediate,p.track,0,d);
        else if(p.sustainAction==3)apply(LoopAction::mute,p.track,p.oldMute,d);
    }
    p.active=false;
}
void MidiController::cancel(Dispatch& d,int track) noexcept {
    if(track<0){++epoch;read=write=0;cancelLearn();programPulses={};}
    for(auto& p:presses)if(p.active&&(track<0||p.track==static_cast<unsigned>(track))){
        const auto copy=p;release(p,d);p=copy;p.suppressed=true;p.sustainAction=0;
    }
    nextDeadline=std::numeric_limits<std::uint64_t>::max();
    for(const auto& p:presses)if(p.active&&!p.suppressed)nextDeadline=std::min(nextDeadline,p.start+static_cast<std::uint64_t>(std::ceil(d.rate*(p.cut?4.0:0.6))));
}
void MidiController::prepare(Dispatch& d) noexcept {cancel(d);presses={};settings.learnTarget=-1;learnCaptured=false;mode=0;sustain=false;++modeRevision;}
void MidiController::overflow(Dispatch& d) noexcept {++overflows;cancel(d);}
void MidiController::begin(Dispatch& d) noexcept {
    MidiSettings next;
    for(unsigned n=0;n<31&&configs.pop(next);++n){
        // Slider learning owns no transport gesture. Keep held notes, deadlines,
        // deferred actions and PC burst suppression when only learn/mixer changes.
        const bool transportChanged=!std::equal(next.bindings.begin(),next.bindings.begin()+MidiSettings::volumeBase,settings.bindings.begin());
        if(next.enabled!=settings.enabled||transportChanged)cancel(d);
        else if(next.bindings!=settings.bindings||next.learnToken!=settings.learnToken||next.learnTarget!=settings.learnTarget||next.learnGeneration!=settings.learnGeneration)cancelLearn();
        if(next.modeOverride>=0){mode=next.modeOverride;++modeRevision;}
        settings=next;settings.modeOverride=-1;
        if(settings.learnGeneration!=d.generation)settings.learnTarget=-1;
    }
}
void MidiController::push(Task task,Dispatch& d) noexcept {
    const auto next=(write+1)%tasks.size();if(next==read){overflow(d);return;}tasks[write]=task;write=static_cast<unsigned>(next);
}
void MidiController::main(unsigned id,int requestedMode,Dispatch& d) noexcept {
    const auto t=d.bank.trackView(id);using S=LoopState;using A=LoopAction;
    if(t.state==S::empty){
        for(unsigned other=0;other<LoopBank::trackCount;++other){const auto s=d.bank.trackView(other).state;
            if(other!=id&&(s==S::armed||s==S::recording||s==S::overdubbing||s==S::substituting||s==S::multiplying)){apply(A::finish,other,0,d);return;}}
        apply(A::record,id,0,d);
    }else if(t.state==S::stopped)apply(A::play,id,0,d);
    else if(t.state==S::playing)apply(requestedMode==1?A::overdub:requestedMode==2?A::substitute:A::stop,id,0,d);
    else if(t.state==S::queued||t.state==S::stopping)apply(A::stop,id,0,d);
    else apply(A::finish,id,0,d);
}
void MidiController::continuous(int target,std::uint8_t cc,Dispatch& d) noexcept {
    const bool pan=target>=static_cast<int>(MidiSettings::panBase);
    const auto track=static_cast<unsigned>(target)-(pan?MidiSettings::panBase:MidiSettings::volumeBase);
    // Original setLoopPan uses parseInt after mapping 0..127 onto 0..10.
    const double value=pan?std::floor((cc/127.0)*10):(cc/127.0)*2;
    apply(pan?LoopAction::pan:LoopAction::volume,track,value,d);
}
void MidiController::event(const MidiEvent& e,Dispatch& d) noexcept {
    if(!settings.enabled)return;
    recent[observed%recent.size()]={d.frame,e};++observed;
    const unsigned type=e.status&240,channel=(e.status&15)+1;
    if(e.status==252&&e.size==1){apply(LoopAction::stopAll,0,0,d);return;}
    if(type==176&&e.size==3&&(e.data1==120||e.data1==123)){cancel(d);return;}
    unsigned kind=0;bool down=false,up=false,pulse=false;
    if((type==144||type==128)&&e.size==3){kind=1;down=type==144&&e.data2>0;up=!down;}
    else if(type==176&&e.size==3){kind=2;down=e.data2>0;up=!down;}
    else if(type==192&&e.size==2){kind=3;down=pulse=true;}
    else return;
    if(e.data1>127||e.data2>127)return;
    ++messages;
    // A learn packet may be consumed/rejected, but must still release an action
    // owned by this exact kind/channel/key. Otherwise CC zero strands SUS holds.
    const unsigned key=(kind<<12)|(channel<<7)|e.data1;
    const bool learnRelease=up&&kind==2&&settings.learnTarget>=static_cast<int>(MidiSettings::volumeBase);
    if(learnRelease){tick(d);for(auto& p:presses)if(p.active&&p.key==key)release(p,d);}
    // Continuous CC learn accepts zero and consumes the event; Note/PC keep
    // their transport mappings while a slider waits for CC, as in the original.
    if(settings.learnTarget>=static_cast<int>(MidiSettings::volumeBase)&&kind==2){
        if(!learnCaptured){
            if(learned.push({settings.learnTarget,midiBinding(2,e.data1),settings.learnToken,d.generation,learnCancellation.load(std::memory_order_relaxed)})){
                learnCaptured=true;
                // Learning is omni: reject any overlapping channel assignment,
                // without mutating the persisted configuration on audio.
                bool overlap=false;
                for(unsigned i=0;i<MidiSettings::bindingCount;++i)if(i!=static_cast<unsigned>(settings.learnTarget)){
                    const auto b=settings.bindings[i];if((b&3)==2&&((b>>2)&127)==e.data1){overlap=true;break;}
                }
                if(!overlap){provisionalTarget=settings.learnTarget;provisionalBinding=midiBinding(2,e.data1);}
            }
            return; // Only the learn packet is consumed for a valid assignment.
        }
        if(provisionalTarget<0)return; // Await rejection; never trigger the conflicting button.
        if(((provisionalBinding>>2)&127)==e.data1){continuous(provisionalTarget,e.data2,d);return;}
    }
    if(settings.learnTarget>=0&&settings.learnTarget<static_cast<int>(MidiSettings::volumeBase)&&down){
        if(!learnCaptured&&learned.push({settings.learnTarget,midiBinding(kind,e.data1),settings.learnToken,d.generation,learnCancellation.load(std::memory_order_relaxed)}))learnCaptured=true;
        return;
    }
    const int continuousTarget=kind==2?targetFor(settings,e):-1;
    if(continuousTarget>=static_cast<int>(MidiSettings::volumeBase)){
        continuous(continuousTarget,e.data2,d);return;
    }
    if(up){if(!learnRelease){tick(d);for(auto& p:presses)if(p.active&&p.key==key)release(p,d);}return;}
    if(!down)return;
    const int target=targetFor(settings,e);
    if(target<0)return;
    // Resolve current ownership before consulting a former loop press. A held
    // key can have been reassigned to a global mode since its last NoteOn.
    if(!(kind==1&&target>=30))for(auto& held:presses)if(held.active&&held.key==key){
        // Original handleInputPress discards a lost-release entry only when a
        // fresh loop NoteOn arrives strictly after ten seconds; never a timer.
        if(kind!=1||held.track>=20||d.frame-held.start<=static_cast<std::uint64_t>(std::ceil(d.rate*10.0)))return;
        held.active=false;break;
    }
    if(pulse){
        // Some foot controllers emit a burst of identical PCs for one pedal tap.
        // Anchor the 20ms window to the first message so a stream cannot extend it.
        auto& previous=programPulses[static_cast<unsigned>(target)];
        if(previous.valid&&previous.key==key&&d.frame-previous.frame<static_cast<std::uint64_t>(std::ceil(d.rate*0.02)))return;
        previous={d.frame,key,true};
    }
    Press* p=nullptr;
    // Original global mode NoteOns bypass handleInputPress: every positive
    // packet toggles, with no held entry or dependency on a matching NoteOff.
    // Learned CC mode buttons still require zero to rearm; PC keeps burst filtering.
    if(!pulse&&!(kind==1&&target>=30)){for(auto& slot:presses)if(!slot.active){p=&slot;break;}if(!p){overflow(d);return;}
        *p={};p->active=true;p->key=key;p->track=static_cast<unsigned>(target);p->start=d.frame;
    }
    if(target>=30){if(target==32)sustain=!sustain;else mode=mode==target-29?0:target-29;++modeRevision;return;}
    const auto t=d.bank.trackView(static_cast<unsigned>(target));
    if(p){p->initial=t.state;p->clearEpoch=t.clearEpoch;p->canDelete=(t.state==LoopState::playing&&mode==0)||t.state==LoopState::stopping;}
    if(sustain&&!pulse){
        using S=LoopState;using A=LoopAction;
        if(t.state==S::empty){if(apply(A::record,target,0,d))p->sustainAction=1;}
        else if(t.state==S::stopped){if(apply(A::play,target,0,d))p->sustainAction=2;}
        else if(t.state==S::playing){if(mode){if(apply(mode==1?A::overdub:A::substitute,target,0,d))p->sustainAction=1;}else{p->oldMute=t.muted?1.0f:0.0f;if(apply(A::mute,target,t.muted?0:1,d))p->sustainAction=3;}}
    }else{
        push({TaskKind::main,static_cast<unsigned>(target),mode,t.clearEpoch,epoch},d);
        if(p&&kind==1)nextDeadline=std::min(nextDeadline,d.frame+static_cast<std::uint64_t>(std::ceil(d.rate*0.6)));
        else if(p){p->cut=p->erase=true;p->canDelete=false;}
    }
}
void MidiController::tick(Dispatch& d) noexcept {
    if(d.frame>=nextDeadline){nextDeadline=std::numeric_limits<std::uint64_t>::max();
        for(auto& p:presses)if(p.active&&!p.suppressed&&p.track<20&&!p.sustainAction){
            const auto t=d.bank.trackView(p.track);if(t.clearEpoch!=p.clearEpoch){p.suppressed=true;continue;}
            const auto cut=p.start+static_cast<std::uint64_t>(std::ceil(d.rate*0.6)),erase=p.start+static_cast<std::uint64_t>(std::ceil(d.rate*4));
            if(!p.cut&&d.frame>=cut){p.cut=true;if((p.initial==LoopState::playing||p.initial==LoopState::stopping||p.initial==LoopState::stopped))push({TaskKind::cut,p.track,0,p.clearEpoch,epoch},d);}
            if(!p.erase&&d.frame>=erase){p.erase=true;if(p.canDelete)push({TaskKind::erase,p.track,0,p.clearEpoch,epoch},d);}
            if(!p.cut)nextDeadline=std::min(nextDeadline,cut);else if(!p.erase)nextDeadline=std::min(nextDeadline,erase);
        }
    }
    for(unsigned n=0;read!=write&&n<128;++n){const auto task=tasks[read];const auto t=d.bank.trackView(task.track);
        if(task.epoch!=epoch||task.clearEpoch!=t.clearEpoch){read=(read+1)%tasks.size();continue;}
        if(t.state==LoopState::finalizing)break;
        if(task.kind==TaskKind::main&&t.state==LoopState::empty&&d.bank.finalizationPending())break;
        read=(read+1)%tasks.size();
        if(task.kind==TaskKind::main)main(task.track,task.mode,d);
        else if(task.kind==TaskKind::cut&&(t.state==LoopState::playing||t.state==LoopState::stopping))apply(LoopAction::stopImmediate,task.track,0,d);
        else if(task.kind==TaskKind::erase&&(t.state==LoopState::playing||t.state==LoopState::stopping||t.state==LoopState::stopped))apply(LoopAction::clear,task.track,0,d);
    }
}
MidiView MidiController::view() const noexcept {return {settings.bindings,settings.revision,modeRevision,messages,overflows,actions,rejections,recent,observed,settings.learnTarget,mode,settings.enabled,sustain};}
}
