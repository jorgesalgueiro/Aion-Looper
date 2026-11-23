// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include <cmath>
namespace {
juce::String name(std::string_view s){return juce::String::fromUTF8(s.data(),static_cast<int>(s.size()));}
juce::var value(const aion::DelaySettings& s){
 auto* object=new juce::DynamicObject;object->setProperty("enabled",s.enabled);
 object->setProperty("preset",s.preset?name(aion::delayPresetNames[s.preset-1]):juce::String());
 object->setProperty("repeatMode",s.repeatMode==aion::DelayRepeatMode::exact?"exact":"feedback");object->setProperty("repeatCount",static_cast<int>(s.repeatCount));
 auto* values=new juce::DynamicObject;for(unsigned p=0;p<aion::delayControls.size();++p)values->setProperty(name(aion::delayControls[p].name),s.values[p]);
 object->setProperty("values",juce::var(values));return juce::var(object);
}
bool numeric(const juce::var& v,double& out){if(!(v.isInt()||v.isInt64()||v.isDouble()))return false;out=static_cast<double>(v);return std::isfinite(out);}
}
void AionProcessor::delaySnapshot(juce::DynamicObject& object,unsigned track){
 std::lock_guard guard(midiSettingsMutex);object.setProperty("delay",value(delaySettings[track]));const auto& record=*delayOwner.track(track);
 const bool restore=eqRestorePending||compressorRestorePending||chainRestorePending||arpRestorePending||driveRestorePending||delayRestorePending||effectsRestore.active;
 const bool settling=latest.tracks[track].delayTransitioning||latest.tracks[track].delaySettingsRevision<record.revision;
 object.setProperty("delayBusy",restore||record.pending||settling||pendingDelayClears[track].request!=0);
 object.setProperty("delayPending",restore||record.pending);
 object.setProperty("delayRequestId",static_cast<juce::int64>(record.requestId));
 object.setProperty("delayReady",delayOwner.bound()&&!restore&&!record.pending&&!pendingDelayClears[track].request&&record.status==aion::DelayEditStatus::ready);
}
bool AionProcessor::delayRequest(const juce::var& request,juce::DynamicObject& result){
 const auto fail=[&](const char* why){result.setProperty("error",why);return false;};std::lock_guard guard(midiSettingsMutex);
 const auto id=request.getProperty("track",{});if(!id.isInt()||static_cast<int>(id)<0||static_cast<int>(id)>=static_cast<int>(delaySettings.size()))return fail("Invalid Delay track");
 const auto track=static_cast<unsigned>(static_cast<int>(id));const auto action=request.getProperty("action","get").toString();
 if(action=="get"){delaySnapshot(result,track);return true;}
 double expires=0;const auto now=static_cast<double>(juce::Time::currentTimeMillis());
 if(!numeric(request.getProperty("expiresAt",{}),expires)||expires<now||expires>now+5000)return fail("Delay command expired");
 const auto generation=request.getProperty("generation",{});
 if(!generation.isInt()||static_cast<int>(generation)!=static_cast<int>(engine.generation())||latest.generation!=engine.generation()||!meterPrepared()||!delayOwner.bound())return fail("Audio generation changed");
 if(eqRestorePending||compressorRestorePending||chainRestorePending||arpRestorePending||driveRestorePending||delayRestorePending||effectsRestore.active||pendingDelayClears[track].request||delayOwner.track(track)->pending||latest.tracks[track].delayTransitioning||latest.tracks[track].delaySettingsRevision<delayOwner.track(track)->revision)return fail("Wait for Delay restore/edit/clear/transition acknowledgement");
 if(nextRequest==UINT32_MAX)return fail("Delay request IDs exhausted");
 const auto requestId=nextRequest+1;bool accepted=false;
 if(action=="toggle"||action=="enable"){
  const auto enabled=request.getProperty("enabled",{});if(action=="enable"&&!enabled.isBool())return fail("Expected Delay boolean");
  accepted=delayOwner.enable(track,action=="toggle"?!delaySettings[track].enabled:static_cast<bool>(enabled),requestId);
 }else if(action=="reset")accepted=delayOwner.reset(track,requestId);
 else if(action=="preset"){
  const auto preset=request.getProperty("preset",{});unsigned p=0;if(!preset.isString())return fail("Expected Delay preset name");
  for(;p<aion::delayPresetNames.size()&&preset.toString()!=name(aion::delayPresetNames[p]);++p){}
  if(p==aion::delayPresetNames.size())return fail("Unknown Delay preset");accepted=delayOwner.preset(track,p+1,requestId);
 }else if(action=="set"){
  const auto parameter=request.getProperty("parameter",{});unsigned p=0;double number=0;
  if(!parameter.isString()||!numeric(request.getProperty("value",{}),number))return fail("Expected Delay parameter/value");
  for(;p<aion::delayControls.size()&&parameter.toString()!=name(aion::delayControls[p].name);++p){}
  accepted=delayOwner.set(track,p,number,requestId);
 }else if(action=="repeats"){
  const auto mode=request.getProperty("repeatMode",{}),count=request.getProperty("repeatCount",{});
  if(!mode.isString()||(mode.toString()!="exact"&&mode.toString()!="feedback")||!count.isInt()||static_cast<int>(count)<1||static_cast<int>(count)>static_cast<int>(aion::delayMaximumRepeats))return fail("Expected Feedback/exact mode and count 1–10");
  accepted=delayOwner.repeats(track,mode.toString()=="exact"?aion::DelayRepeatMode::exact:aion::DelayRepeatMode::feedback,static_cast<unsigned>(static_cast<int>(count)),requestId);
 }else return fail("Unknown Delay action");
 if(!accepted)return fail("Invalid Delay settings or command queue unavailable");
 delaySettings[track]=*delayOwner.intent(track);nextRequest=requestId;
 result.setProperty("queued",true);result.setProperty("requestId",static_cast<juce::int64>(requestId));delaySnapshot(result,track);return true;
}
