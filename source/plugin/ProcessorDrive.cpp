// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include <charconv>
#include <cmath>
namespace {
juce::String text(std::string_view s){return juce::String::fromUTF8(s.data(),static_cast<int>(s.size()));}
juce::var value(aion::DriveKind k,const aion::DriveSettings& s){
 auto* object=new juce::DynamicObject;object->setProperty("enabled",s.enabled);
 object->setProperty("preset",s.preset?text(aion::drivePresets(k)[s.preset-1].name):juce::String());
 auto* values=new juce::DynamicObject;for(unsigned p=0;p<5;++p)if(!aion::driveControls(k)[p].name.empty())values->setProperty(text(aion::driveControls(k)[p].name),s.values[p]);
 object->setProperty("values",juce::var(values));return juce::var(object);
}
bool number(const juce::var& v,double& n){if(!(v.isInt()||v.isInt64()||v.isDouble()))return false;n=static_cast<double>(v);return std::isfinite(n);}
bool stored(const juce::var& v,double& n){const auto str=v.toString().toStdString();const auto r=std::from_chars(str.data(),str.data()+str.size(),n);return r.ec==std::errc{}&&r.ptr==str.data()+str.size()&&std::isfinite(n);}
bool integer(const juce::var& v,unsigned& n){const auto str=v.toString().toStdString();const auto r=std::from_chars(str.data(),str.data()+str.size(),n);return r.ec==std::errc{}&&r.ptr==str.data()+str.size();}
}
void AionProcessor::saveDrives(juce::ValueTree& state){
 std::lock_guard guard(midiSettingsMutex);juce::ValueTree bank("LoopDrives");bank.setProperty("version",1,nullptr);
 for(unsigned i=0;i<driveSettings.size();++i){juce::ValueTree track("Track");track.setProperty("id",static_cast<int>(i),nullptr);
  for(unsigned k=0;k<3;++k){const auto kind=static_cast<aion::DriveKind>(k);const auto& s=driveSettings[i][k];juce::ValueTree effect(text(aion::driveNames[k]));
   effect.setProperty("enabled",s.enabled,nullptr);effect.setProperty("preset",static_cast<int>(s.preset),nullptr);
   for(unsigned p=0;p<5;++p)if(!aion::driveControls(kind)[p].name.empty())effect.setProperty(text(aion::driveControls(kind)[p].name),s.values[p],nullptr);
   track.appendChild(effect,nullptr);
  }bank.appendChild(track,nullptr);
 }state.removeChild(state.getChildWithName("LoopDrives"),nullptr);state.appendChild(bank,nullptr);
}
bool AionProcessor::readDrives(const juce::ValueTree& state,DriveBanks& bank){
 bank=aion::LoopBank::defaultDriveBanks();const auto storedBank=state.getChildWithName("LoopDrives");if(!storedBank.isValid())return true;
 unsigned count=0;for(const auto& child:state)if(child.hasType("LoopDrives"))++count;
 if(count!=1||storedBank.getProperty("version").toString()!="1"||storedBank.getNumChildren()!=static_cast<int>(bank.size()))return false;
 std::array<bool,aion::LoopBank::trackCount> seen{};
 for(const auto& child:storedBank){unsigned id=0;if(!child.hasType("Track")||!integer(child.getProperty("id"),id)||id>=bank.size()||seen[id]||child.getNumChildren()!=3)return false;seen[id]=true;
  std::array<bool,3> effects{};
  for(const auto& effect:child){unsigned k=0;for(;k<3&&effect.getType().toString()!=text(aion::driveNames[k]);++k){}if(k==3||effects[k])return false;effects[k]=true;
   auto& s=bank[id][k];const auto enabled=effect.getProperty("enabled").toString();unsigned preset=0;if((enabled!="0"&&enabled!="1")||!integer(effect.getProperty("preset"),preset)||preset>aion::drivePresets(static_cast<aion::DriveKind>(k)).size())return false;
   s.enabled=enabled=="1";s.preset=static_cast<std::uint8_t>(preset);
   for(unsigned p=0;p<5;++p)if(!aion::driveControls(static_cast<aion::DriveKind>(k))[p].name.empty()&&!stored(effect.getProperty(text(aion::driveControls(static_cast<aion::DriveKind>(k))[p].name)),s.values[p]))return false;
   if(!aion::validDriveSettings(static_cast<aion::DriveKind>(k),s))return false;
  }
 }return true;
}
void AionProcessor::serviceDrives(){
 std::lock_guard guard(midiSettingsMutex);if(latest.generation!=engine.generation())return;
 for(unsigned i=0;i<driveSettings.size();++i){
  if(drivePending[i]){if(latest.lastRequestId>=drivePending[i]){driveSettings[i]=latest.tracks[i].drives;drivePending[i]=0;driveClearing[i]=false;}}
  else driveSettings[i]=latest.tracks[i].drives;
 }
}
void AionProcessor::driveCleared(unsigned id,bool all,std::uint32_t request){std::lock_guard guard(midiSettingsMutex);for(unsigned i=0;i<driveSettings.size();++i)if(all||id==i){driveSettings[i]=aion::defaultDriveBank();drivePending[i]=request;driveClearing[i]=true;}}
void AionProcessor::driveSnapshot(juce::DynamicObject& object,unsigned id){
 std::lock_guard guard(midiSettingsMutex);
 if(driveSnapshotCache[id].isVoid()||driveSnapshotSettings[id]!=driveSettings[id]){
  auto* bank=new juce::DynamicObject;
  for(unsigned k=0;k<3;++k)bank->setProperty(text(aion::driveNames[k]),value(static_cast<aion::DriveKind>(k),driveSettings[id][k]));
  driveSnapshotCache[id]=juce::var(bank);driveSnapshotSettings[id]=driveSettings[id];
 }
 object.setProperty("drives",driveSnapshotCache[id]);
}
bool AionProcessor::driveRequest(const juce::var& request,juce::DynamicObject& result){
 const auto fail=[&](const char* why){result.setProperty("error",why);return false;};std::lock_guard guard(midiSettingsMutex);
 const auto id=request.getProperty("track",{});if(!id.isInt()||static_cast<int>(id)<0||static_cast<int>(id)>=static_cast<int>(driveSettings.size()))return fail("Invalid drive track");const auto track=static_cast<unsigned>(static_cast<int>(id));
 unsigned k=0;const auto name=request.getProperty("effect",{});for(;k<3&&name.toString()!=text(aion::driveNames[k]);++k){}if(k==3||!name.isString())return fail("Unknown drive effect");const auto kind=static_cast<aion::DriveKind>(k);
 auto next=driveSettings[track];auto& s=next[k];const auto action=request.getProperty("action","get").toString();
 if(action=="get"){
  result.setProperty("drive",value(kind,s));result.setProperty("schema",1);
  juce::Array<juce::var> controls,presets;
  for(const auto& c:aion::driveControls(kind))if(!c.name.empty()){auto* item=new juce::DynamicObject;item->setProperty("name",text(c.name));item->setProperty("initial",c.initial);item->setProperty("min",c.minimum);item->setProperty("max",c.maximum);item->setProperty("step",c.step);controls.add(juce::var(item));}
  for(const auto& p:aion::drivePresets(kind))presets.add(text(p.name));
  result.setProperty("controls",controls);result.setProperty("presets",presets);return true;
 }
 double expires=0;const auto now=static_cast<double>(juce::Time::currentTimeMillis());if(!number(request.getProperty("expiresAt",{}),expires)||expires<now||expires>now+5000)return fail("Drive command expired");
 const auto generation=request.getProperty("generation",{});if(!generation.isInt()||static_cast<int>(generation)!=static_cast<int>(engine.generation())||latest.generation!=engine.generation())return fail("Audio generation changed");
 if(eqRestorePending||compressorRestorePending||chainRestorePending||arpRestorePending||driveRestorePending||driveClearing[track])return fail("Wait for effects restore/clear acknowledgement");
 std::uint32_t mask=0;
 if(action=="toggle"){s.enabled=!s.enabled;mask=1u<<(k*6+5);}
 else if(action=="enable"){const auto enabled=request.getProperty("enabled",{});if(!enabled.isBool())return fail("Expected drive boolean");s.enabled=static_cast<bool>(enabled);mask=1u<<(k*6+5);}
 else if(action=="reset"){const bool enabled=s.enabled;s=aion::driveDefaults(kind);s.enabled=enabled;mask=31u<<(k*6);}
 else if(action=="preset"){const auto preset=request.getProperty("preset",{});if(!preset.isString())return fail("Expected preset name");const auto presets=aion::drivePresets(kind);unsigned p=0;for(;p<presets.size()&&preset.toString()!=text(presets[p].name);++p){}if(!aion::applyDrivePreset(kind,s,p+1))return fail("Unknown drive preset");mask=31u<<(k*6);}
 else if(action=="set"){
  const auto params=request.getProperty("values",{});const auto* values=params.getDynamicObject();if(!values||values->getProperties().size()<1||values->getProperties().size()>5)return fail("Expected bounded drive values");
  for(const auto& item:values->getProperties()){unsigned p=0;const auto controls=aion::driveControls(kind);for(;p<5&&text(controls[p].name)!=item.name.toString();++p){}double n=0;if(p==5||!number(item.value,n)||!aion::setDriveControl(kind,s,p,n))return fail("Unknown or invalid drive parameter");mask|=1u<<(k*6+p);}
 }else return fail("Unknown drive action");
 if(!aion::validDriveBank(next)||nextRequest==UINT32_MAX)return fail("Invalid drive settings/request exhausted");
 aion::LoopCommand command{aion::LoopAction::drive,track,nextRequest+1,engine.generation()};command.drives=next;command.driveMask=mask;command.driveClearEpoch=latest.tracks[track].clearEpoch;
 if(!engine.enqueueLoop(command))return fail("Drive command queue is full");
 driveSettings[track]=next;drivePending[track]=lastOrdinaryRequest=++nextRequest;result.setProperty("queued",true);result.setProperty("requestId",static_cast<juce::int64>(nextRequest));result.setProperty("drive",value(kind,s));return true;
}
