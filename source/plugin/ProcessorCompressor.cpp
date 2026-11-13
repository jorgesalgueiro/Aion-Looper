// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include <charconv>
#include <cmath>
#include <limits>
namespace {
juce::String name(std::string_view s){return juce::String::fromUTF8(s.data(),static_cast<int>(s.size()));}
juce::var settingsValue(const aion::CompressorSettings& settings){
    auto* object=new juce::DynamicObject;object->setProperty("enabled",settings.enabled);
    object->setProperty("preset",settings.preset?name(aion::compressorPresetNames[settings.preset-1]):juce::String());
    auto* values=new juce::DynamicObject;for(unsigned i=0;i<7;++i)values->setProperty(name(aion::compressorParameters[i].name),settings.values[i]);
    object->setProperty("values",juce::var(values));return juce::var(object);
}
bool numeric(const juce::var& v,double& number){
    if(!(v.isInt()||v.isInt64()||v.isDouble()))return false;
    number=static_cast<double>(v);return std::isfinite(number);
}
bool storedNumber(const juce::var& v,float& out){
    const auto s=v.toString().toStdString();double number=0;
    const auto [end,error]=std::from_chars(s.data(),s.data()+s.size(),number);
    if(error!=std::errc{}||end!=s.data()+s.size()||!std::isfinite(number)||std::abs(number)>std::numeric_limits<float>::max())return false;
    out=static_cast<float>(number);return true;
}
}
void AionProcessor::saveCompressors(juce::ValueTree& state){
    std::lock_guard guard(midiSettingsMutex);
    juce::ValueTree bank("LoopCompressors");bank.setProperty("version",1,nullptr);
    for(unsigned i=0;i<compressorSettings.size();++i){
        juce::ValueTree t("C");t.setProperty("track",static_cast<int>(i),nullptr);t.setProperty("enabled",compressorSettings[i].enabled,nullptr);t.setProperty("preset",static_cast<int>(compressorSettings[i].preset),nullptr);
        for(unsigned p=0;p<7;++p)t.setProperty(name(aion::compressorParameters[p].name),compressorSettings[i].values[p],nullptr);
        bank.appendChild(t,nullptr);
    }
    state.removeChild(state.getChildWithName("LoopCompressors"),nullptr);state.appendChild(bank,nullptr);
}
bool AionProcessor::readCompressors(const juce::ValueTree& state,CompressorBank& bank){
    bank={};const auto stored=state.getChildWithName("LoopCompressors");if(!stored.isValid())return true; // Legacy state = original defaults/off.
    unsigned count=0;for(const auto& child:state)if(child.hasType("LoopCompressors"))++count;
    if(count!=1||stored.getProperty("version").toString()!="1"||stored.getNumChildren()!=static_cast<int>(bank.size()))return false;
    std::array<bool,aion::LoopBank::trackCount> seen{};
    for(const auto& child:stored){
        if(!child.hasType("C"))return false;
        const auto track=child.getProperty("track").toString();unsigned id=0;const auto str=track.toStdString();
        const auto [end,error]=std::from_chars(str.data(),str.data()+str.size(),id);
        if(error!=std::errc{}||end!=str.data()+str.size()||id>=bank.size()||seen[id])return false;
        seen[id]=true;
        const auto enabled=child.getProperty("enabled").toString(),preset=child.getProperty("preset").toString();
        if(enabled!="0"&&enabled!="1")return false;
        unsigned which=0;const auto ps=preset.toStdString();const auto parsed=std::from_chars(ps.data(),ps.data()+ps.size(),which);
        if(parsed.ec!=std::errc{}||parsed.ptr!=ps.data()+ps.size()||which>11)return false;
        auto& s=bank[id];s.enabled=enabled=="1";s.preset=static_cast<std::uint8_t>(which);
        for(unsigned p=0;p<7;++p)if(!storedNumber(child.getProperty(name(aion::compressorParameters[p].name)),s.values[p]))return false;
        if(!aion::validCompressorSettings(s))return false;
    }
    return true;
}
void AionProcessor::serviceCompressor(){
    std::lock_guard guard(midiSettingsMutex);
    if(latest.generation!=engine.generation())return;
    for(unsigned i=0;i<compressorSettings.size();++i){
        if(compressorPending[i]){
            if(latest.lastRequestId>=compressorPending[i]){compressorSettings[i]=latest.tracks[i].compressor;compressorPending[i]=0;compressorClearing[i]=false;}
        }else compressorSettings[i]=latest.tracks[i].compressor;
        // Accepted pending intent is protected above. Otherwise audio owns the
        // truth, including internal capture cleanup without a DEL epoch change.
    }
}
void AionProcessor::compressorCleared(unsigned track,bool all,std::uint32_t requestId){
    std::lock_guard guard(midiSettingsMutex);
    for(unsigned i=0;i<compressorSettings.size();++i)if(all||i==track){compressorSettings[i]={};compressorPending[i]=requestId;compressorClearing[i]=true;}
}
void AionProcessor::compressorSnapshot(juce::DynamicObject& object,unsigned track){
    std::lock_guard guard(midiSettingsMutex);object.setProperty("compressor",settingsValue(compressorSettings[track]));
}
bool AionProcessor::compressorRequest(const juce::var& request,juce::DynamicObject& result){
    const auto fail=[&](const char* why){result.setProperty("error",why);return false;};
    std::lock_guard guard(midiSettingsMutex);
    const auto id=request.getProperty("track",{});if(!id.isInt()||static_cast<int>(id)<0||static_cast<int>(id)>=static_cast<int>(compressorSettings.size()))return fail("Invalid Compressor track");
    const auto track=static_cast<unsigned>(static_cast<int>(id));auto next=compressorSettings[track];
    const auto action=request.getProperty("action","get").toString();
    if(action=="get"){result.setProperty("compressor",settingsValue(next));return true;}
    if(action=="curve"){
        juce::Array<juce::var> points;points.ensureStorageAllocated(321);
        for(unsigned x=0;x<=320;++x)points.add(aion::compressorDisplayCurveDb(-60+(x/320.0)*60,next.values));
        result.setProperty("points",points);result.setProperty("compressor",settingsValue(next));return true;
    }
    double expires=0;const auto now=static_cast<double>(juce::Time::currentTimeMillis());
    if(!numeric(request.getProperty("expiresAt",{}),expires)||expires<now||expires>now+5000)return fail("Compressor command expired");
    const auto generation=request.getProperty("generation",{});
    if(!generation.isInt()||static_cast<int>(generation)!=static_cast<int>(engine.generation())||latest.generation!=engine.generation())return fail("Audio generation changed");
    if(eqRestorePending||compressorRestorePending||chainRestorePending||arpRestorePending||compressorClearing[track])return fail("Wait for Compressor restore/clear acknowledgement");
    std::uint32_t mask=0;
    if(action=="toggle")next.enabled=!next.enabled;
    else if(action=="enable"){const auto enabled=request.getProperty("enabled",{});if(!enabled.isBool())return fail("Expected Compressor boolean");next.enabled=static_cast<bool>(enabled);}
    else if(action=="reset"){next.values=aion::compressorDefaults();next.preset=0;mask=aion::compressorAllParameters;}
    else if(action=="preset"){
        const auto preset=request.getProperty("preset",{});if(!preset.isString())return fail("Expected Compressor preset name");unsigned p=0;
        for(;p<aion::compressorPresetNames.size()&&preset.toString()!=name(aion::compressorPresetNames[p]);++p){}
        if(p==aion::compressorPresetNames.size())return fail("Unknown Compressor preset");
        next.preset=static_cast<std::uint8_t>(p+1);next.values=aion::compressorPresetValues(p+1);mask=aion::compressorAllParameters;
    }else if(action=="set"){
        const auto params=request.getProperty("values",{});auto* values=params.getDynamicObject();if(!values||values->getProperties().size()<1||values->getProperties().size()>7)return fail("Expected bounded Compressor values");
        for(const auto& item:values->getProperties()){
            unsigned p=0;for(;p<7&&item.name.toString()!=name(aion::compressorParameters[p].name);++p){}
            double number=0;
            if(p==7||!numeric(item.value,number))return fail("Unknown or out-of-range Compressor parameter");
            const auto& descriptor=aion::compressorParameters[p];
            // Validate decimal widget endpoints before Float32 storage.
            const double minimum=aion::compressorMidiRanges[p][0];
            if(number<minimum||number>descriptor.maximum)return fail("Unknown or out-of-range Compressor parameter");
            next.values[p]=static_cast<float>(number);mask|=1u<<p;
        }next.preset=0;
    }else return fail("Unknown Compressor action");
    if(!aion::validCompressorSettings(next)||nextRequest==UINT32_MAX)return fail("Invalid Compressor settings/request exhausted");
    aion::LoopCommand command{aion::LoopAction::compressor,track,nextRequest+1,engine.generation()};
    command.compressor=next;command.compressorMask=mask;command.compressorClearEpoch=latest.tracks[track].clearEpoch;
    if(!engine.enqueueLoop(command))return fail("Compressor command queue is full");
    compressorSettings[track]=next;compressorPending[track]=lastOrdinaryRequest=++nextRequest;
    result.setProperty("queued",true);result.setProperty("requestId",static_cast<juce::int64>(nextRequest));result.setProperty("compressor",settingsValue(next));return true;
}
