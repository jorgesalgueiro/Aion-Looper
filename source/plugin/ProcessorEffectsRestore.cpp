// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
bool AionProcessor::serviceEffectsRestore(){
    // Called only by the message owner, with midiSettingsMutex held. Host state
    // setters publish FX intent/cancellation under that mutex, never a second FX
    // request producer. This currently restores existing Q/C/chains with inactive A;
    // original Arp serialization/UI/MIDI is deliberately not enabled here.
    if(effectsRestore.active||eqRestorePending||compressorRestorePending||chainRestorePending)
        for(unsigned n=0;n<8&&engine.popView(latest);++n){} // Also progresses with a closed editor, via the existing Timer.
    if(effectsRestore.active){
        if(!effectsRestore.completed){
            aion::ArpBankResult result;
            if(!engine.popArpBank(result))return false;
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
                }
                eqRestorePending=compressorRestorePending=chainRestorePending=false;
            }
            effectsRestore={};
        }
    }
    if(!(eqRestorePending||compressorRestorePending||chainRestorePending))return true;
    if(!meterPrepared()||latest.generation!=engine.generation()||nextRequest==UINT32_MAX)return false;
    // This is an ordering fence, never evidence of successful settings. All
    // these IDs belong to the ordinary FIFO. An earlier bank ID is already
    // covered by its exact terminal receipt and must not strand the next load.
    if(nextRequest!=lastEffectsRequest&&latest.lastRequestId<nextRequest)return false;
    aion::ArpBankRequest command;command.requestId=nextRequest+1;command.generation=engine.generation();command.restoreEffects=true;
    command.effects={eqSettings,compressorSettings,chainSettings};
    for(unsigned i=0;i<eqSettings.size();++i){const auto& track=latest.tracks[i];
        if(track.arp.enabled)return false; // Product A remains gated; never reconstruct original Arp controls.
        command.settings[i]=track.arp;command.clearEpochs[i]=track.clearEpoch;command.settingsRevisions[i]=track.arpSettingsRevision;
    }
    if(!engine.enqueueArpBank(command))return false;
    lastEffectsRequest=nextRequest=command.requestId;
    effectsRestore={};effectsRestore.active=true;effectsRestore.request=command.requestId;effectsRestore.generation=command.generation;effectsRestore.intent=effectsIntent;
    return false;
}
