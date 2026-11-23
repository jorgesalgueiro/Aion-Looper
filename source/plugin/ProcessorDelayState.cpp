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
void AionProcessor::saveDelays(juce::ValueTree& state){
 std::lock_guard guard(midiSettingsMutex);
 serviceDelay(); // Drain exact edit receipts; protect accepted loads/clear intent.
 juce::ValueTree bank("LoopDelays");bank.setProperty("version",1,nullptr);
 for(unsigned id=0;id<delaySettings.size();++id){const auto& settings=delaySettings[id];juce::ValueTree row("Delay");
  row.setProperty("track",static_cast<int>(id),nullptr);row.setProperty("enabled",settings.enabled,nullptr);
  row.setProperty("preset",static_cast<int>(settings.preset),nullptr);
  row.setProperty("repeatMode",settings.repeatMode==aion::DelayRepeatMode::exact?"exact":"feedback",nullptr);
  row.setProperty("repeatCount",static_cast<int>(settings.repeatCount),nullptr);
  for(unsigned p=0;p<aion::delayControls.size();++p)row.setProperty(key(aion::delayControls[p].name),exact(settings.values[p]),nullptr);
  bank.appendChild(row,nullptr);
 }
 state.removeChild(state.getChildWithName("LoopDelays"),nullptr);state.appendChild(bank,nullptr);
}
bool AionProcessor::readDelays(const juce::ValueTree& state,aion::DelaySettingsOwner::Bank& output){
 aion::DelaySettingsOwner::Bank candidate{};const auto bank=state.getChildWithName("LoopDelays");
 if(!bank.isValid()){output=candidate;return true;} // Pre-D native sessions: original defaults, disabled.
 unsigned count=0;for(const auto& child:state)if(child.hasType("LoopDelays"))++count;
 if(count!=1||bank.getProperty("version").toString()!="1"||bank.getNumChildren()!=static_cast<int>(candidate.size()))return false;
 std::array<bool,aion::LoopBank::trackCount> seen{};
 for(const auto& row:bank){unsigned id=0,preset=0;
  if(!row.hasType("Delay")||row.getNumChildren()!=0||!number(row.getProperty("track"),id)||id>=candidate.size()||seen[id])return false;
  seen[id]=true;auto& settings=candidate[id];const auto enabled=row.getProperty("enabled").toString(),mode=row.getProperty("repeatMode").toString();
  if((enabled!="0"&&enabled!="1")||(mode!="feedback"&&mode!="exact")||!number(row.getProperty("preset"),preset)||preset>aion::delayPresetNames.size())return false;
  settings.enabled=enabled=="1";settings.preset=static_cast<std::uint8_t>(preset);
  settings.repeatMode=mode=="exact"?aion::DelayRepeatMode::exact:aion::DelayRepeatMode::feedback;
  if(!number(row.getProperty("repeatCount"),settings.repeatCount))return false;
  for(unsigned p=0;p<aion::delayControls.size();++p)if(!number(row.getProperty(key(aion::delayControls[p].name)),settings.values[p]))return false;
  if(!aion::validDelaySettings(settings))return false;
 }
 output=candidate;return true; // No state/owner/APVTS mutation until all banks validate.
}
