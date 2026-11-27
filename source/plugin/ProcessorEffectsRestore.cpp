// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
bool AionProcessor::serviceEffectsRestore(){
    // Called only by the message owner, with midiSettingsMutex held. Host state
    // setters publish FX intent/cancellation under that mutex, never a second FX
    // request producer. The original settings owner is the sole Arp receipt consumer.
    for(unsigned n=0;n<8&&engine.popView(latest);++n){} // Bounded Timer service also owns closed-editor Arp edits/clears.
    bool retainedDelayFailure=false;
    if(delayRestoreFailed){bool pending=false;
        for(unsigned id=0;id<delaySettings.size();++id){retainedDelayFailure|=delaySettings[id]!=delayOwner.track(id)->committed;pending|=delayOwner.track(id)->pending;}
        if(!retainedDelayFailure&&!pending)delayRestoreFailed=false;
    }
    delayOwner.suspendMidi(retainedDelayFailure||effectsRestore.active||eqRestorePending||compressorRestorePending||chainRestorePending||arpRestorePending||driveRestorePending||delayRestorePending);
    const auto retainedReverbFailure=[&]{
        if(!reverbRestoreFailed)return false;
        bool differs=false,pending=false;
        for(unsigned id=0;id<reverbSettings.size();++id){differs|=reverbSettings[id]!=reverbOwner.track(id)->committed;pending|=reverbOwner.track(id)->pending;}
        if(!differs&&!pending)reverbRestoreFailed=false;
        return differs;
    };
    reverbOwner.suspendMidi(retainedReverbFailure()||effectsRestore.active||eqRestorePending||compressorRestorePending||chainRestorePending||arpRestorePending||driveRestorePending||delayRestorePending||reverbRestorePending);
    serviceDelay();
    serviceReverb();
    arpOwner.service(latest);
    serviceArpTempo(false); // Retire maintenance before a new FX restore can own the bank receipt.
    if(!arpOwner.bound()){
        // A changed-rate prepare resets graphs; a same-rate prepare can retain
        // them. Choose only confirmed originals or known reset defaults, never
        // reconstruct original precision/preset metadata from render floats.
        if(latest.generation!=engine.generation())return false;
        aion::ArpSettingsOwner::Bank known{};
        for(unsigned id=0;id<known.size();++id){const auto& old=arpOwner.track(id)->committed;aion::ArpRenderSettings render;
            if(aion::compileArpSettings(old.controls,old.bpm,old.seed,render)&&render==latest.tracks[id].arp)known[id]=old;
        }
        if(!arpOwner.bind(latest,known))return false;
    }
    if(effectsRestore.active){
        if(!effectsRestore.completed){
            if(arpOwner.restoring())return false;
            const auto result=arpOwner.restoreReceipt();
            if(result.requestId!=effectsRestore.request||result.generation!=effectsRestore.generation)return false;
            effectsRestore.terminal=result;effectsRestore.completed=true;
        }
        const auto& result=effectsRestore.terminal;
        if(result.generation!=engine.generation()){effectsRestore={};}
        else {
            // A closed consumer may still have old snapshots at the same sample
            // frame. Fence with the producing callback serial, not global IDs.
            if(latest.generation!=result.generation||latest.callbackClock.serial<result.callbackSerial)return false;
            if(result.status==aion::ArpPreparationStatus::ready){
                if(!delayOwner.acceptEffectsRestore(latest,effectsRestore.delays,result,effectsRestore.delayEpochs,effectsRestore.delayRevisions))return false;
                if(effectsRestore.includesReverbs&&!reverbOwner.acceptEffectsRestore(latest,effectsRestore.reverbs,result,effectsRestore.reverbEpochs,effectsRestore.reverbRevisions))return false;
            }
            if(effectsRestore.intent==effectsIntent){
                // A clear admitted after this load is ordered before the joint bank
                // receipt. Keep the newer intent so the next service submits the
                // cleared candidate, rather than latching a terminal B failure.
                bool newerReverbClear=false;
                if(effectsRestore.includesReverbs)for(unsigned id=0;id<reverbSettings.size();++id)
                    if(reverbOwner.track(id)->clearEpoch>effectsRestore.reverbEpochs[id]){newerReverbClear=true;break;}
                const bool clearRace=result.status==aion::ArpPreparationStatus::cancelled&&newerReverbClear;
                const bool retainReverb=effectsRestore.retainReverbOnFailure&&(result.status==aion::ArpPreparationStatus::failed||result.status==aion::ArpPreparationStatus::busy||clearRace);
                for(unsigned i=0;i<eqSettings.size();++i){
                    eqSettings[i]=latest.tracks[i].equalizer;compressorSettings[i]=latest.tracks[i].compressor;chainSettings[i]=latest.tracks[i].signalChain;
                    driveSettings[i]=latest.tracks[i].drives;
                    arpSettings[i]=arpOwner.track(i)->committed;
                    const bool retain=effectsRestore.retainDelayOnFailure&&(result.status==aion::ArpPreparationStatus::failed||result.status==aion::ArpPreparationStatus::busy);
                    if(!retain&&!pendingDelayClears[i].request)delaySettings[i]=delayOwner.track(i)->committed;
                    if(effectsRestore.includesReverbs&&!retainReverb&&!pendingReverbClears[i].request)reverbSettings[i]=reverbOwner.track(i)->committed;
                }
                delayRestorePending=false;delayRestoreFailed=result.status!=aion::ArpPreparationStatus::ready;
                if(effectsRestore.includesReverbs&&!clearRace){reverbRestorePending=false;reverbRestoreFailed=result.status!=aion::ArpPreparationStatus::ready;}
                eqRestorePending=compressorRestorePending=chainRestorePending=arpRestorePending=driveRestorePending=false;
            }
            effectsRestore={};
        }
    }
    if(!(eqRestorePending||compressorRestorePending||chainRestorePending||arpRestorePending||driveRestorePending||delayRestorePending||reverbRestorePending)){
        delayOwner.suspendMidi(retainedDelayFailure);
        // Include a failure delivered in this very service: there must be no
        // callback window where MIDI owns the old graph against retained intent.
        reverbOwner.suspendMidi(retainedReverbFailure());
        for(unsigned i=0;i<arpSettings.size();++i)if(!arpOwner.track(i)->pending)arpSettings[i]=arpOwner.track(i)->committed;
        return true;
    }
    if(!meterPrepared()||latest.generation!=engine.generation()||nextRequest==UINT32_MAX)return false;
    if(!delayOwner.bound())return false;
    if(reverbRestorePending&&!reverbOwner.bound())return false;
    // This is an ordering fence, never evidence of successful settings. All
    // these IDs belong to the ordinary FIFO. An earlier bank ID is already
    // covered by its exact terminal receipt and must not strand the next load.
    if(latest.lastRequestId<lastOrdinaryRequest)return false;
    aion::LoopBank::EffectsBank effects{eqSettings,compressorSettings,chainSettings,driveSettings};
    effects.restoreDelays=true;effects.delays=delaySettings;
    for(unsigned id=0;id<delaySettings.size();++id)effects.delayRevisions[id]=delayOwner.track(id)->revision;
    if(reverbRestorePending){
        for(unsigned id=0;id<reverbSettings.size();++id)if(reverbOwner.track(id)->pending)return false;
        effects.restoreReverbs=true;effects.reverbs=reverbSettings;
        for(unsigned id=0;id<reverbSettings.size();++id)effects.reverbRevisions[id]=reverbOwner.track(id)->revision;
    }
    if(!arpOwner.restore(arpSettings,nextRequest+1,&effects))return false;
    lastEffectsRequest=++nextRequest;
    effectsRestore={};effectsRestore.active=true;effectsRestore.request=nextRequest;effectsRestore.generation=engine.generation();effectsRestore.intent=effectsIntent;
    effectsRestore.delays=delaySettings;effectsRestore.delayRevisions=effects.delayRevisions;effectsRestore.retainDelayOnFailure=delayRestorePending;
    for(unsigned id=0;id<delaySettings.size();++id)effectsRestore.delayEpochs[id]=delayOwner.track(id)->clearEpoch;
    effectsRestore.includesReverbs=reverbRestorePending;
    effectsRestore.reverbs=reverbSettings;effectsRestore.reverbRevisions=effects.reverbRevisions;effectsRestore.retainReverbOnFailure=reverbRestorePending;
    for(unsigned id=0;id<reverbSettings.size();++id)effectsRestore.reverbEpochs[id]=reverbOwner.track(id)->clearEpoch;
    return false;
}
