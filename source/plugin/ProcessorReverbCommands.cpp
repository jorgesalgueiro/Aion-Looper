// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include <cmath>
namespace {
juce::String name(std::string_view s){return juce::String::fromUTF8(s.data(),static_cast<int>(s.size()));}
juce::var value(const aion::ReverbSettings& s){
 auto* object=new juce::DynamicObject;object->setProperty("enabled",s.enabled);
 object->setProperty("room",name(aion::reverbRooms[static_cast<unsigned>(s.room)].name));
 const bool machine=s.engine==aion::ReverbEngine::machine;
 object->setProperty("engine",machine?"machine":"convolution");
 object->setProperty("preset",!machine&&s.preset?name(aion::reverbPresetNames[s.preset-1]):juce::String());
 object->setProperty("platePreset",machine&&s.preset?name(aion::platePresetNames[s.preset-1]):juce::String());
 auto* values=new juce::DynamicObject;for(unsigned p=0;p<aion::reverbControls.size();++p)values->setProperty(name(aion::reverbControls[p].name),s.values[p]);
 object->setProperty("values",juce::var(values));
 auto* plate=new juce::DynamicObject;for(unsigned p=0;p<aion::plateControls.size();++p)plate->setProperty(name(aion::plateControls[p].name),s.plate[p]);
 object->setProperty("plate",juce::var(plate));return juce::var(object);
}
bool numeric(const juce::var& v,double& out){if(!(v.isInt()||v.isInt64()||v.isDouble()))return false;out=static_cast<double>(v);return std::isfinite(out);}
}
void AionProcessor::reverbSnapshot(juce::DynamicObject& object,unsigned track){
 std::lock_guard guard(midiSettingsMutex);object.setProperty("reverb",value(reverbSettings[track]));const auto& record=*reverbOwner.track(track);
 const bool restore=reverbRestorePending||reverbRestoreFailed||reverbOwner.restoring();
 const bool settling=latest.tracks[track].reverbTransitioning||latest.tracks[track].reverbSettingsRevision<record.revision;
 object.setProperty("reverbBusy",restore||record.pending||settling||pendingReverbClears[track].request!=0);
 object.setProperty("reverbPending",restore||record.pending);
 object.setProperty("reverbRequestId",static_cast<juce::int64>(record.requestId));
 object.setProperty("reverbReady",reverbOwner.bound()&&!restore&&!record.pending&&!pendingReverbClears[track].request&&record.status==aion::ReverbEditStatus::ready);
}
bool AionProcessor::reverbRequest(const juce::var& request,juce::DynamicObject& result){
 const auto fail=[&](const char* why){result.setProperty("error",why);return false;};std::lock_guard guard(midiSettingsMutex);
 const auto id=request.getProperty("track",{});if(!id.isInt()||static_cast<int>(id)<0||static_cast<int>(id)>=static_cast<int>(reverbSettings.size()))return fail("Invalid Reverb track");
 const auto track=static_cast<unsigned>(static_cast<int>(id));const auto action=request.getProperty("action","get").toString();
 if(action=="get"){reverbSnapshot(result,track);return true;}
 double expires=0;const auto now=static_cast<double>(juce::Time::currentTimeMillis());
 if(!numeric(request.getProperty("expiresAt",{}),expires)||expires<now||expires>now+5000)return fail("Reverb command expired");
 const auto generation=request.getProperty("generation",{});
 if(!generation.isInt()||static_cast<int>(generation)!=static_cast<int>(engine.generation())||latest.generation!=engine.generation()||!meterPrepared()||!reverbOwner.bound())return fail("Audio generation changed");
 if(reverbRestorePending||reverbRestoreFailed||reverbOwner.restoring()||pendingReverbClears[track].request||reverbOwner.track(track)->pending
  ||latest.tracks[track].reverbTransitioning||latest.tracks[track].reverbSettingsRevision<reverbOwner.track(track)->revision)return fail("Wait for Reverb restore/edit/clear/transition acknowledgement");
 if(nextRequest==UINT32_MAX)return fail("Reverb request IDs exhausted");
 const auto requestId=nextRequest+1;bool accepted=false;
 if(action=="toggle"||action=="enable"){
  const auto enabled=request.getProperty("enabled",{});if(action=="enable"&&!enabled.isBool())return fail("Expected Reverb boolean");
  accepted=reverbOwner.enable(track,action=="toggle"?!reverbSettings[track].enabled:static_cast<bool>(enabled),requestId);
 }else if(action=="reset")accepted=reverbOwner.reset(track,requestId);
 else if(action=="preset"){
  const auto preset=request.getProperty("preset",{});if(!preset.isString())return fail("Expected Reverb preset name");
  unsigned p=0;for(;p<aion::reverbPresetNames.size()&&preset.toString()!=name(aion::reverbPresetNames[p]);++p){}
  if(p<aion::reverbPresetNames.size())accepted=reverbOwner.preset(track,p+1,requestId);
  else{unsigned q=0;for(;q<aion::platePresetNames.size()&&preset.toString()!=name(aion::platePresetNames[q]);++q){}
   if(q==aion::platePresetNames.size())return fail("Unknown Reverb preset");
   accepted=reverbOwner.preset(track,static_cast<unsigned>(aion::reverbPresetNames.size())+q+1,requestId);}
 }else if(action=="engine"){
  const auto engineName=request.getProperty("engine",{}).toString();
  if(engineName!="machine"&&engineName!="convolution")return fail("Expected Reverb engine");
  auto candidate=reverbSettings[track];candidate.engine=engineName=="machine"?aion::ReverbEngine::machine:aion::ReverbEngine::convolution;candidate.preset=0;
  accepted=reverbOwner.assign(track,candidate,requestId);
 }else if(action=="room"){
  const auto room=request.getProperty("room",{});unsigned r=0;if(!room.isString())return fail("Expected Reverb room name");
  for(;r<aion::reverbRooms.size()&&room.toString()!=name(aion::reverbRooms[r].name);++r){}
  if(r==aion::reverbRooms.size())return fail("Unknown Reverb room");accepted=reverbOwner.room(track,static_cast<aion::ReverbRoom>(r),requestId);
 }else if(action=="set"){
  const auto parameter=request.getProperty("parameter",{});unsigned p=0;double number=0;
  if(!parameter.isString()||!numeric(request.getProperty("value",{}),number))return fail("Expected Reverb parameter/value");
  const bool machine=reverbSettings[track].engine==aion::ReverbEngine::machine;
  const auto& defs=machine?aion::plateControls:aion::reverbControls;
  for(;p<defs.size()&&parameter.toString()!=name(defs[p].name);++p){}
  if(p==defs.size())return fail("Unknown Reverb parameter");accepted=reverbOwner.set(track,p,number,requestId);
 }else return fail("Unknown Reverb action");
 if(!accepted)return fail("Invalid Reverb settings or command queue unavailable");
 reverbSettings[track]=*reverbOwner.intent(track);nextRequest=requestId;
 result.setProperty("queued",true);result.setProperty("requestId",static_cast<juce::int64>(requestId));reverbSnapshot(result,track);return true;
}
