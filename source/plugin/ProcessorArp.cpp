// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include <cmath>
namespace {
juce::String name(std::string_view s){return juce::String::fromUTF8(s.data(),static_cast<int>(s.size()));}
juce::var value(const aion::ArpOwnedSettings& original){
 auto* object=new juce::DynamicObject;object->setProperty("enabled",original.controls.enabled);
 object->setProperty("preset",original.controls.preset?name(aion::arpPresetNames[original.controls.preset-1]):juce::String());
 auto* values=new juce::DynamicObject;for(unsigned p=0;p<aion::arpControls.size();++p)values->setProperty(name(aion::arpControls[p].name),original.controls.values[p]);
 object->setProperty("values",juce::var(values));object->setProperty("bpm",original.bpm);return juce::var(object);
}
bool numeric(const juce::var& v,double& out){if(!(v.isInt()||v.isInt64()||v.isDouble()))return false;out=static_cast<double>(v);return std::isfinite(out);}
}
void AionProcessor::arpSnapshot(juce::DynamicObject& object,unsigned track){
 std::lock_guard guard(midiSettingsMutex);object.setProperty("arp",value(arpSettings[track]));
 const auto* record=arpOwner.track(track);const bool pending=record->pending&&!arpOwner.tempoPending();
 object.setProperty("arpBusy",arpRestorePending||record->pending);
 object.setProperty("arpPending",arpRestorePending||pending);
 object.setProperty("arpRequestId",static_cast<juce::int64>(record->requestId));
 object.setProperty("arpReady",arpOwner.bound()&&!arpRestorePending&&!pending&&record->status==aion::ArpPreparationStatus::ready);
}
bool AionProcessor::arpRequest(const juce::var& request,juce::DynamicObject& result){
 const auto fail=[&](const char* why){result.setProperty("error",why);return false;};std::lock_guard guard(midiSettingsMutex);
 const auto id=request.getProperty("track",{});if(!id.isInt()||static_cast<int>(id)<0||static_cast<int>(id)>=static_cast<int>(arpSettings.size()))return fail("Invalid Arp track");
 const auto track=static_cast<unsigned>(static_cast<int>(id));const auto action=request.getProperty("action","get").toString();
 if(action=="get"){arpSnapshot(result,track);return true;}
 double expires=0;const auto now=static_cast<double>(juce::Time::currentTimeMillis());
 if(!numeric(request.getProperty("expiresAt",{}),expires)||expires<now||expires>now+5000)return fail("Arp command expired");
 const auto generation=request.getProperty("generation",{});
 if(!generation.isInt()||static_cast<int>(generation)!=static_cast<int>(engine.generation())||latest.generation!=engine.generation()||!meterPrepared()||!arpOwner.bound())return fail("Audio generation changed");
 if(eqRestorePending||compressorRestorePending||chainRestorePending||arpRestorePending||effectsRestore.active||eqClearing[track]||compressorClearing[track]||chainClearing[track]||arpOwner.track(track)->pending)return fail("Wait for Arp restore/edit/clear acknowledgement");
 if(nextRequest==UINT32_MAX)return fail("Arp request IDs exhausted");
 const auto requestId=nextRequest+1;auto candidate=arpSettings[track];const double bpm=syncBpm->load(std::memory_order_relaxed);bool accepted=false;
 if(action=="toggle"||action=="enable"){
  const auto enabled=request.getProperty("enabled",{});if(action=="enable"&&!enabled.isBool())return fail("Expected Arp boolean");
  candidate.controls.enabled=action=="toggle"?!candidate.controls.enabled:static_cast<bool>(enabled);
  if(candidate.controls.enabled&&!arpSettings[track].controls.enabled)candidate.bpm=bpm;
  accepted=arpOwner.enable(track,candidate.controls.enabled,bpm,requestId);
 }else if(action=="preset"){
  const auto preset=request.getProperty("preset",{});unsigned p=0;
  if(!preset.isString())return fail("Expected Arp preset name");
  for(;p<aion::arpPresetNames.size()&&preset.toString()!=name(aion::arpPresetNames[p]);++p){}
  if(p==aion::arpPresetNames.size())return fail("Unknown Arp preset");
  (void)aion::applyArpPreset(candidate.controls,p+1);candidate.bpm=bpm;accepted=arpOwner.preset(track,p+1,bpm,requestId);
 }else if(action=="set"){
  const auto parameter=request.getProperty("parameter",{});unsigned p=0;double number=0;
  if(!parameter.isString()||!numeric(request.getProperty("value",{}),number))return fail("Expected Arp parameter/value");
  for(;p<aion::arpControls.size()&&parameter.toString()!=name(aion::arpControls[p].name);++p){}
  if(!aion::setArpControl(candidate.controls,p,number))return fail("Unknown or out-of-range Arp parameter");
  candidate.bpm=bpm;accepted=arpOwner.set(track,p,number,bpm,requestId);
 }else return fail("Unknown Arp action");
 if(!accepted)return fail("Arp command queue is full or unavailable");
 arpSettings[track]=candidate;nextRequest=requestId;
 result.setProperty("queued",true);result.setProperty("requestId",static_cast<juce::int64>(requestId));arpSnapshot(result,track);return true;
}
