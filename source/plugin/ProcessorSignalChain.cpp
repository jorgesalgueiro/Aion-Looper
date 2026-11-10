// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include <charconv>
#include <cmath>
namespace {
juce::String chainText(const aion::SignalChain& c){return juce::String::fromUTF8(c.codes.data(),c.size);}
}
void AionProcessor::saveChains(juce::ValueTree& state){
    std::lock_guard guard(midiSettingsMutex);
    juce::ValueTree bank("LoopSignalChains");bank.setProperty("version",1,nullptr);
    for(unsigned i=0;i<chainSettings.size();++i){juce::ValueTree t("Chain");t.setProperty("track",static_cast<int>(i),nullptr);t.setProperty("order",chainText(chainSettings[i]),nullptr);bank.appendChild(t,nullptr);}
    state.removeChild(state.getChildWithName("LoopSignalChains"),nullptr);state.appendChild(bank,nullptr);
}
bool AionProcessor::readChains(const juce::ValueTree& state,ChainBank& bank){
    bank={};const auto stored=state.getChildWithName("LoopSignalChains");if(!stored.isValid())return true;
    unsigned count=0;for(const auto& child:state)if(child.hasType("LoopSignalChains"))++count;
    if(count!=1||stored.getProperty("version").toString()!="1"||stored.getNumChildren()!=static_cast<int>(bank.size()))return false;
    std::array<bool,aion::LoopBank::trackCount> seen{};
    for(const auto& child:stored){
        if(!child.hasType("Chain"))return false;
        const auto str=child.getProperty("track").toString().toStdString();unsigned id=0;
        const auto [end,error]=std::from_chars(str.data(),str.data()+str.size(),id);
        if(error!=std::errc{}||end!=str.data()+str.size()||id>=bank.size()||seen[id])return false;
        seen[id]=true;
        const auto order=child.getProperty("order");if(!order.isString())return false;
        const auto text=order.toString().toStdString();if(text.empty()||text.size()>13)return false;
        const auto next=aion::normalizedSignalChain(text);
        if(next.text()!=text)return false; // Persisted state is canonical, never silently repaired.
        bank[id]=next;
    }
    return true;
}
void AionProcessor::serviceChains(){
    std::lock_guard guard(midiSettingsMutex);
    if(latest.generation!=engine.generation())return;
    for(unsigned i=0;i<chainSettings.size();++i){
        if(chainPending[i]){
            if(latest.lastRequestId>=chainPending[i]){chainSettings[i]=latest.tracks[i].signalChain;chainPending[i]=0;chainClearing[i]=false;}
        }else chainSettings[i]=latest.tracks[i].signalChain;
    }
}
void AionProcessor::chainCleared(unsigned track,bool all,std::uint32_t request){
    std::lock_guard guard(midiSettingsMutex);
    for(unsigned i=0;i<chainSettings.size();++i)if(all||i==track){chainSettings[i]={};chainPending[i]=request;chainClearing[i]=true;}
}
void AionProcessor::chainSnapshot(juce::DynamicObject& object,unsigned track){
    std::lock_guard guard(midiSettingsMutex);object.setProperty("signalChain",chainText(chainSettings[track]));
}
bool AionProcessor::chainRequest(const juce::var& request,juce::DynamicObject& result){
    const auto fail=[&](const char* why){result.setProperty("error",why);return false;};
    std::lock_guard guard(midiSettingsMutex);
    const auto id=request.getProperty("track",{});if(!id.isInt()||static_cast<int>(id)<0||static_cast<int>(id)>=static_cast<int>(chainSettings.size()))return fail("Invalid chain track");
    const auto track=static_cast<unsigned>(static_cast<int>(id));auto next=chainSettings[track];
    const auto action=request.getProperty("action","get").toString();
    if(action=="get"){result.setProperty("signalChain",chainText(next));return true;}
    const auto expires=request.getProperty("expiresAt",{});const auto now=static_cast<double>(juce::Time::currentTimeMillis());
    if(!(expires.isDouble()||expires.isInt64()||expires.isInt())||!std::isfinite(static_cast<double>(expires))||static_cast<double>(expires)<now||static_cast<double>(expires)>now+5000)return fail("Chain command expired");
    const auto generation=request.getProperty("generation",{});
    if(!generation.isInt()||static_cast<int>(generation)!=static_cast<int>(engine.generation())||latest.generation!=engine.generation())return fail("Audio generation changed");
    if(eqRestorePending||compressorRestorePending||chainRestorePending||chainClearing[track])return fail("Wait for chain restore/clear acknowledgement");
    if(action=="reset")next={};
    else if(action=="set"){
        const auto order=request.getProperty("order",{});if(!order.isString()||order.toString().getNumBytesAsUTF8()>256)return fail("Expected bounded chain text");
        // Unicode case conversion stays off audio; built-in codes are ASCII.
        next=aion::normalizedSignalChain(order.toString().toUpperCase().toStdString());
    }else return fail("Unknown chain action");
    if(nextRequest==UINT32_MAX)return fail("Request IDs exhausted");
    aion::LoopCommand command{aion::LoopAction::signalChain,track,nextRequest+1,engine.generation()};
    command.signalChain=next;command.chainClearEpoch=latest.tracks[track].clearEpoch;
    if(!engine.enqueueLoop(command))return fail("Chain command queue is full");
    chainSettings[track]=next;chainPending[track]=++nextRequest;
    result.setProperty("queued",true);result.setProperty("requestId",static_cast<juce::int64>(nextRequest));result.setProperty("signalChain",chainText(next));return true;
}
