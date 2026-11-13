// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/Processor.h"
#include <charconv>
#include <limits>
namespace {
juce::String key(std::string_view s){return juce::String::fromUTF8(s.data(),static_cast<int>(s.size()));}
template<class T> bool number(const juce::var& value,T& out){
 const auto text=value.toString().toStdString();T candidate{};
 const auto parsed=std::from_chars(text.data(),text.data()+text.size(),candidate);
 if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size())return false;
 out=candidate;return true;
}
juce::String exact(double value){
 char bytes[64];const auto result=std::to_chars(bytes,bytes+sizeof(bytes),value,std::chars_format::general,std::numeric_limits<double>::max_digits10);
 jassert(result.ec==std::errc{});return juce::String::fromUTF8(bytes,static_cast<int>(result.ptr-bytes));
}
}
void AionProcessor::saveArps(juce::ValueTree& state){
 std::lock_guard guard(midiSettingsMutex);
 arpOwner.service(latest); // Serialized non-audio owner: save must include already accepted MIDI even before the Timer.
 if(arpOwner.bound()&&!arpRestorePending&&!effectsRestore.active)for(unsigned id=0;id<arpSettings.size();++id)if(!arpOwner.track(id)->pending)arpSettings[id]=arpOwner.track(id)->committed;
 juce::ValueTree bank("LoopArps");bank.setProperty("version",1,nullptr);
 const auto timing=readArpTiming();const bool pendingTempo=timing!=arpTiming||arpTempoIntent!=arpTempoSeen;
 for(unsigned id=0;id<arpSettings.size();++id){auto original=arpSettings[id];
  if(pendingTempo&&original.controls.enabled)original.bpm=timing[0]; // Include not-yet-admitted automatic intent in immediate save.
  juce::ValueTree row("Arp");row.setProperty("track",static_cast<int>(id),nullptr);
  row.setProperty("enabled",original.controls.enabled,nullptr);row.setProperty("preset",static_cast<int>(original.controls.preset),nullptr);
  row.setProperty("bpm",exact(original.bpm),nullptr);row.setProperty("seed",juce::String(static_cast<juce::int64>(original.seed)),nullptr);
  for(unsigned p=0;p<aion::arpControls.size();++p)row.setProperty(key(aion::arpControls[p].name),exact(original.controls.values[p]),nullptr);
  bank.appendChild(row,nullptr);
 }
 state.removeChild(state.getChildWithName("LoopArps"),nullptr);state.appendChild(bank,nullptr);
}
bool AionProcessor::readArps(const juce::ValueTree& state,aion::ArpSettingsOwner::Bank& output){
 aion::ArpSettingsOwner::Bank candidate{};const auto bank=state.getChildWithName("LoopArps");
 if(!bank.isValid()){output=candidate;return true;} // Existing native sessions retain original defaults/off.
 unsigned count=0;for(const auto& child:state)if(child.hasType("LoopArps"))++count;
 if(count!=1||bank.getProperty("version").toString()!="1"||bank.getNumChildren()!=static_cast<int>(candidate.size()))return false;
 std::array<bool,aion::LoopBank::trackCount> seen{};
 for(const auto& row:bank){unsigned id=0,preset=0;
  if(!row.hasType("Arp")||!number(row.getProperty("track"),id)||id>=candidate.size()||seen[id])return false;
  seen[id]=true;auto& original=candidate[id];const auto enabled=row.getProperty("enabled").toString();
  if((enabled!="0"&&enabled!="1")||!number(row.getProperty("preset"),preset)||preset>aion::arpPresetNames.size())return false;
  original.controls.enabled=enabled=="1";original.controls.preset=static_cast<std::uint8_t>(preset);
  if(!number(row.getProperty("bpm"),original.bpm)||!number(row.getProperty("seed"),original.seed))return false;
  for(unsigned p=0;p<aion::arpControls.size();++p)if(!number(row.getProperty(key(aion::arpControls[p].name)),original.controls.values[p]))return false;
  aion::ArpRenderSettings render;if(!aion::compileArpSettings(original.controls,original.bpm,original.seed,render))return false;
 }
 output=candidate;return true; // Complete validation precedes any product intent mutation.
}
