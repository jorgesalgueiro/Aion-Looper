// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include <charconv>
#include <cmath>
#include <limits>
namespace {
juce::String name(std::string_view s){return juce::String::fromUTF8(s.data(),static_cast<int>(s.size()));}
juce::var settingsValue(const aion::EqSettings& settings){
    auto* object=new juce::DynamicObject;object->setProperty("enabled",settings.enabled);
    object->setProperty("preset",settings.preset?name(aion::eqPresetNames[settings.preset-1]):juce::String());
    auto* values=new juce::DynamicObject;for(unsigned i=0;i<24;++i)values->setProperty(name(aion::eqParameters[i].name),settings.values[i]);
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
void AionProcessor::saveEqualizers(juce::ValueTree& state){
    std::lock_guard guard(midiSettingsMutex);
    juce::ValueTree bank("LoopEqualizers");bank.setProperty("version",1,nullptr);
    for(unsigned i=0;i<eqSettings.size();++i){
        juce::ValueTree t("EQ");t.setProperty("track",static_cast<int>(i),nullptr);t.setProperty("enabled",eqSettings[i].enabled,nullptr);t.setProperty("preset",static_cast<int>(eqSettings[i].preset),nullptr);
        for(unsigned p=0;p<24;++p)t.setProperty(name(aion::eqParameters[p].name),eqSettings[i].values[p],nullptr);
        bank.appendChild(t,nullptr);
    }
    state.removeChild(state.getChildWithName("LoopEqualizers"),nullptr);state.appendChild(bank,nullptr);
}
bool AionProcessor::readEqualizers(const juce::ValueTree& state,EqBank& bank){
    bank={};const auto stored=state.getChildWithName("LoopEqualizers");if(!stored.isValid())return true; // Legacy state = original defaults/off.
    unsigned count=0;for(const auto& child:state)if(child.hasType("LoopEqualizers"))++count;
    if(count!=1||stored.getProperty("version").toString()!="1"||stored.getNumChildren()!=static_cast<int>(bank.size()))return false;
    std::array<bool,aion::LoopBank::trackCount> seen{};
    for(const auto& child:stored){
        if(!child.hasType("EQ"))return false;
        const auto track=child.getProperty("track").toString();unsigned id=0;const auto str=track.toStdString();
        const auto [end,error]=std::from_chars(str.data(),str.data()+str.size(),id);
        if(error!=std::errc{}||end!=str.data()+str.size()||id>=bank.size()||seen[id])return false;
        seen[id]=true;
        const auto enabled=child.getProperty("enabled").toString(),preset=child.getProperty("preset").toString();
        if(enabled!="0"&&enabled!="1")return false;
        unsigned which=0;const auto ps=preset.toStdString();const auto parsed=std::from_chars(ps.data(),ps.data()+ps.size(),which);
        if(parsed.ec!=std::errc{}||parsed.ptr!=ps.data()+ps.size()||which>10)return false;
        auto& s=bank[id];s.enabled=enabled=="1";s.preset=static_cast<std::uint8_t>(which);
        for(unsigned p=0;p<24;++p)if(!storedNumber(child.getProperty(name(aion::eqParameters[p].name)),s.values[p]))return false;
        if(!aion::validEqSettings(s))return false;
    }
    return true;
}
void AionProcessor::serviceEqualizer(){
    std::lock_guard guard(midiSettingsMutex);
    if(eqRestorePending){
        if(!meterPrepared()||nextRequest>UINT32_MAX-aion::LoopBank::trackCount-1)return;
        const auto first=nextRequest+1;
        if(!engine.enqueueEqBank(eqSettings,first))return; // No partial publication; retry next message pass.
        for(unsigned i=0;i<eqSettings.size();++i){eqPending[i]=first+i;eqClearing[i]=false;}
        nextRequest+=aion::LoopBank::trackCount;eqRestorePending=false;
    }
    if(latest.generation!=engine.generation())return;
    for(unsigned i=0;i<eqSettings.size();++i){
        if(eqPending[i]){
            if(latest.lastRequestId>=eqPending[i]){eqSettings[i]=latest.tracks[i].equalizer;eqPending[i]=0;eqClearing[i]=false;}
        }else eqSettings[i]=latest.tracks[i].equalizer;
        // Accepted pending intent is protected above. Otherwise audio owns the
        // truth, including internal capture cleanup without a DEL epoch change.
    }
}
void AionProcessor::equalizerCleared(unsigned track,bool all,std::uint32_t requestId){
    std::lock_guard guard(midiSettingsMutex);
    for(unsigned i=0;i<eqSettings.size();++i)if(all||i==track){eqSettings[i]={};eqPending[i]=requestId;eqClearing[i]=true;}
}
void AionProcessor::equalizerSnapshot(juce::DynamicObject& object,unsigned track){
    std::lock_guard guard(midiSettingsMutex);object.setProperty("equalizer",settingsValue(eqSettings[track]));
}
bool AionProcessor::equalizerRequest(const juce::var& request,juce::DynamicObject& result){
    const auto fail=[&](const char* why){result.setProperty("error",why);return false;};
    std::lock_guard guard(midiSettingsMutex);
    const auto id=request.getProperty("track",{});if(!id.isInt()||static_cast<int>(id)<0||static_cast<int>(id)>=static_cast<int>(eqSettings.size()))return fail("Invalid EQ track");
    const auto track=static_cast<unsigned>(static_cast<int>(id));auto next=eqSettings[track];
    const auto action=request.getProperty("action","get").toString();
    if(action=="get"){result.setProperty("equalizer",settingsValue(next));return true;}
    if(action=="curve"){
        aion::Equalizer curve;if(!curve.prepare(latest.sampleRate,next.values))return fail("EQ curve unavailable");
        constexpr unsigned count=256;const double maximum=std::min(22000.0,latest.sampleRate*.5*(1-1e-6));
        juce::Array<juce::var> points,ranges;points.ensureStorageAllocated(count);
        for(unsigned n=0;n<count;++n){const double frequency=10*std::pow(maximum/10,static_cast<double>(n)/(count-1));double magnitude=0;
            if(!curve.magnitude(frequency,magnitude))return fail("EQ curve unavailable");
            points.add(20*std::log10(std::max(magnitude,1e-12)));}
        for(unsigned p=0;p<6;++p){double low=0,high=0;(void)aion::eqPeakRange(next.values[3+p*3],next.values[5+p*3],latest.sampleRate,low,high);juce::Array<juce::var> range;range.add(low);range.add(high);ranges.add(range);}
        result.setProperty("points",points);result.setProperty("ranges",ranges);result.setProperty("maximum",maximum);result.setProperty("sampleRate",latest.sampleRate);result.setProperty("equalizer",settingsValue(next));return true;
    }
    double expires=0;const auto now=static_cast<double>(juce::Time::currentTimeMillis());
    if(!numeric(request.getProperty("expiresAt",{}),expires)||expires<now||expires>now+5000)return fail("EQ command expired");
    const auto generation=request.getProperty("generation",{});
    if(!generation.isInt()||static_cast<int>(generation)!=static_cast<int>(engine.generation())||latest.generation!=engine.generation())return fail("Audio generation changed");
    if(eqRestorePending||eqClearing[track])return fail("Wait for EQ restore/clear acknowledgement");
    std::uint32_t mask=0;
    if(action=="toggle")next.enabled=!next.enabled;
    else if(action=="enable"){const auto enabled=request.getProperty("enabled",{});if(!enabled.isBool())return fail("Expected EQ boolean");next.enabled=static_cast<bool>(enabled);}
    else if(action=="reset"){next.values=aion::eqDefaults();next.preset=0;mask=aion::eqAllParameters;}
    else if(action=="preset"){
        const auto preset=request.getProperty("preset",{});if(!preset.isString())return fail("Expected EQ preset name");unsigned p=0;
        for(;p<aion::eqPresetNames.size()&&preset.toString()!=name(aion::eqPresetNames[p]);++p){}
        if(p==aion::eqPresetNames.size())return fail("Unknown EQ preset");
        next.preset=static_cast<std::uint8_t>(p+1);next.values=aion::eqPresetValues(p+1);mask=aion::eqAllParameters;
    }else if(action=="set"){
        const auto params=request.getProperty("values",{});auto* values=params.getDynamicObject();if(!values||values->getProperties().size()<1||values->getProperties().size()>24)return fail("Expected bounded EQ values");
        for(const auto& item:values->getProperties()){
            unsigned p=0;for(;p<24&&item.name.toString()!=name(aion::eqParameters[p].name);++p){}
            double number=0;
            if(p==24||!numeric(item.value,number))return fail("Unknown or out-of-range EQ parameter");
            const auto& descriptor=aion::eqParameters[p];
            // JSON's decimal 0.1 is slightly below Float32(0.1). Validate the
            // original decimal Q endpoint before storing its Float32 value.
            const double minimum=descriptor.name.ends_with('Q')?.1:static_cast<double>(descriptor.minimum);
            if(number<minimum||number>descriptor.maximum)return fail("Unknown or out-of-range EQ parameter");
            next.values[p]=static_cast<float>(number);mask|=1u<<p;
        }next.preset=0;
    }else if(action=="width"){
        const auto band=request.getProperty("band",{});double edge=0;
        if(!band.isInt()||static_cast<int>(band)<1||static_cast<int>(band)>6||!numeric(request.getProperty("edge",{}),edge))return fail("Invalid EQ bandwidth handle");
        const auto p=3+3*static_cast<unsigned>(static_cast<int>(band)-1);float q=0;
        if(!aion::eqPeakQ(next.values[p],edge,latest.sampleRate,q))return fail("Invalid EQ bandwidth edge");
        next.values[p+2]=q;next.preset=0;mask=1u<<(p+2);
    }else return fail("Unknown EQ action");
    if(!aion::validEqSettings(next)||nextRequest==UINT32_MAX)return fail("Invalid EQ settings/request exhausted");
    aion::LoopCommand command{aion::LoopAction::equalizer,track,nextRequest+1,engine.generation()};
    command.equalizer=next;command.eqMask=mask;command.eqClearEpoch=latest.tracks[track].clearEpoch;
    if(!engine.enqueueLoop(command))return fail("EQ command queue is full");
    eqSettings[track]=next;eqPending[track]=++nextRequest;
    result.setProperty("queued",true);result.setProperty("requestId",static_cast<juce::int64>(nextRequest));result.setProperty("equalizer",settingsValue(next));return true;
}
