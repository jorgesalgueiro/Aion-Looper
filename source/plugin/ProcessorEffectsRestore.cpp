// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
bool AionProcessor::serviceEffectsRestore(){
    // Called only by the message owner, with midiSettingsMutex held. Host state
    // setters publish FX intent/cancellation under that mutex, never a second FX
    // request producer. The original settings owner is the sole Arp receipt consumer.
    for(unsigned n=0;n<8&&engine.popView(latest);++n){} // Bounded Timer service also owns closed-editor Arp edits/clears.
    arpOwner.service(latest);
    if(!arpOwner.bound()){
        // prepare resets native Arp graphs. Bind only the known reset originals;
        // the accepted session intent is restored separately through the worker.
        if(latest.generation!=engine.generation()||!arpOwner.bind(latest))return false;
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
            if(effectsRestore.intent==effectsIntent){
                for(unsigned i=0;i<eqSettings.size();++i){
                    eqSettings[i]=latest.tracks[i].equalizer;compressorSettings[i]=latest.tracks[i].compressor;chainSettings[i]=latest.tracks[i].signalChain;
                    arpSettings[i]=arpOwner.track(i)->committed;
                }
                eqRestorePending=compressorRestorePending=chainRestorePending=arpRestorePending=false;
            }
            effectsRestore={};
        }
    }
    if(!(eqRestorePending||compressorRestorePending||chainRestorePending||arpRestorePending)){
        for(unsigned i=0;i<arpSettings.size();++i)if(!arpOwner.track(i)->pending)arpSettings[i]=arpOwner.track(i)->committed;
        return true;
    }
    if(!meterPrepared()||latest.generation!=engine.generation()||nextRequest==UINT32_MAX)return false;
    // This is an ordering fence, never evidence of successful settings. All
    // these IDs belong to the ordinary FIFO. An earlier bank ID is already
    // covered by its exact terminal receipt and must not strand the next load.
    if(latest.lastRequestId<lastOrdinaryRequest)return false;
    const aion::LoopBank::EffectsBank effects{eqSettings,compressorSettings,chainSettings};
    if(!arpOwner.restore(arpSettings,nextRequest+1,&effects))return false;
    lastEffectsRequest=++nextRequest;
    effectsRestore={};effectsRestore.active=true;effectsRestore.request=nextRequest;effectsRestore.generation=engine.generation();effectsRestore.intent=effectsIntent;
    return false;
}
