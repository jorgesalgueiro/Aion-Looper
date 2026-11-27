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
void AionProcessor::saveReverbs(juce::ValueTree& state){
 std::lock_guard guard(midiSettingsMutex);
 serviceReverb(); // Drain exact edit/bank receipts; protect accepted loads/clear intent.
 juce::ValueTree bank("LoopReverbs");bank.setProperty("version",1,nullptr);
 for(unsigned id=0;id<reverbSettings.size();++id){const auto& settings=reverbSettings[id];juce::ValueTree row("Reverb");
  row.setProperty("track",static_cast<int>(id),nullptr);row.setProperty("enabled",settings.enabled,nullptr);
  row.setProperty("room",key(aion::reverbRooms[static_cast<unsigned>(settings.room)].name),nullptr);
  row.setProperty("preset",static_cast<int>(settings.preset),nullptr);
  for(unsigned p=0;p<aion::reverbControls.size();++p)row.setProperty(key(aion::reverbControls[p].name),exact(settings.values[p]),nullptr);
  bank.appendChild(row,nullptr);
 }
 state.removeChild(state.getChildWithName("LoopReverbs"),nullptr);state.appendChild(bank,nullptr);
}
bool AionProcessor::readReverbs(const juce::ValueTree& state,aion::ReverbSettingsOwner::Bank& output){
 aion::ReverbSettingsOwner::Bank candidate{};const auto bank=state.getChildWithName("LoopReverbs");
 if(!bank.isValid()){output=candidate;return true;} // Pre-B native sessions: original defaults, disabled.
 unsigned count=0;for(const auto& child:state)if(child.hasType("LoopReverbs"))++count;
 if(count!=1||bank.getProperty("version").toString()!="1"||bank.getNumChildren()!=static_cast<int>(candidate.size()))return false;
 std::array<bool,aion::LoopBank::trackCount> seen{};
 for(const auto& row:bank){unsigned id=0,preset=0;
  if(!row.hasType("Reverb")||row.getNumChildren()!=0||!number(row.getProperty("track"),id)||id>=candidate.size()||seen[id])return false;
  seen[id]=true;auto& settings=candidate[id];const auto enabled=row.getProperty("enabled").toString();
  if(enabled!="0"&&enabled!="1")return false;settings.enabled=enabled=="1";
  const auto room=row.getProperty("room").toString();bool roomFound=false;
  for(unsigned r=0;r<aion::reverbRooms.size();++r)if(room==key(aion::reverbRooms[r].name)){settings.room=static_cast<aion::ReverbRoom>(r);roomFound=true;break;}
  if(!roomFound||!number(row.getProperty("preset"),preset)||preset>aion::reverbPresetNames.size())return false;
  settings.preset=static_cast<std::uint8_t>(preset);
  for(unsigned p=0;p<aion::reverbControls.size();++p)if(!number(row.getProperty(key(aion::reverbControls[p].name)),settings.values[p]))return false;
  if(!aion::validReverbSettings(settings))return false;
 }
 output=candidate;return true; // No state/owner/APVTS mutation until the bank validates.
}
