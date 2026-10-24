"""Generate Aion's pinned JUCE VST3 Program Change routing wrapper.
Never modify dependency sources. Exact replacement counts fail on upstream drift.
"""
from pathlib import Path
import argparse
import hashlib


def transform(source):
    def replace(old,new,count=1):
        nonlocal source
        if source.count(old)!=count:
            raise RuntimeError(('JUCE wrapper drift',old[:90],source.count(old)))
        source=source.replace(old,new)
    replace(
        '''#include <juce_core/system/juce_TargetPlatform.h>''',
        '''#include "plugin/Vst3ProgramRouting.h"
#include <juce_core/system/juce_TargetPlatform.h>''')
    # These IDs are separate from all existing managed parameter IDs and JUCE CCs.
    replace('class JuceAudioProcessor final : public Vst::IUnitInfo',
        'static constexpr Vst::ParamID aionProgramBase = aion::vst3::programBase;\n'
        'static constexpr Vst::UnitID aionProgramUnitBase = aion::vst3::unitBase;\n'
        'static constexpr Vst::ProgramListID aionProgramListBase = aion::vst3::listBase;\n'
        'static bool aionProgramId (Vst::ParamID id) noexcept { return id >= aionProgramBase && id < aionProgramBase + 16; }\n\n'
        'class JuceAudioProcessor final : public Vst::IUnitInfo')
    replace('return parameterGroups.size() + 1;', 'return parameterGroups.size() + 1 + 16;')
    replace(
        '''    tresult PLUGIN_API getUnitInfo (Steinberg::int32 unitIndex, Vst::UnitInfo& info) override
    {
        if (unitIndex == 0)
        {''',
        '''    tresult PLUGIN_API getUnitInfo (Steinberg::int32 unitIndex, Vst::UnitInfo& info) override
    {
        const auto channel = unitIndex - (parameterGroups.size() + 1);
        if (isPositiveAndBelow (channel, 16))
        {
            info.id = aionProgramUnitBase + channel;
            info.parentUnitId = Vst::kRootUnitId;
            info.programListId = aionProgramListBase + channel;
            toString128 (info.name, "MIDI Program Ch " + String (channel + 1));
            return kResultTrue;
        }
        if (unitIndex == 0)
        {''')
    replace(
        '''        if (audioProcessor->getNumPrograms() > 0)
            return 1;

        return 0;''',
        '''        return (audioProcessor->getNumPrograms() > 0 ? 1 : 0) + 16;''')
    replace(
        '''        if (listIndex == 0)
        {''',
        '''        const auto channel = listIndex - (audioProcessor->getNumPrograms() > 0 ? 1 : 0);
        if (isPositiveAndBelow (channel, 16))
        {
            info.id = aionProgramListBase + channel;
            info.programCount = 128;
            toString128 (info.name, "MIDI Program Ch " + String (channel + 1));
            return kResultTrue;
        }
        if (listIndex == 0)
        {''')
    replace(
        '''        if (listId == static_cast<Vst::ProgramListID> (programParamID)
''',
        '''        if (listId >= aionProgramListBase && listId < aionProgramListBase + 16
            && isPositiveAndBelow (programIndex, 128))
        {
            toString128 (name, "MIDI PC " + String (programIndex));
            return kResultTrue;
        }
        if (listId == static_cast<Vst::ProgramListID> (programParamID)
''')
    replace(
        '''    tresult PLUGIN_API getUnitByBus (Vst::MediaType, Vst::BusDirection, Steinberg::int32, Steinberg::int32, Vst::UnitID& unitId) override
    {
        unitId = Vst::kRootUnitId;''',
        '''    tresult PLUGIN_API getUnitByBus (Vst::MediaType type, Vst::BusDirection direction, Steinberg::int32 bus, Steinberg::int32 channel, Vst::UnitID& unitId) override
    {
        if (type == Vst::kEvent && direction == Vst::kInput && bus == 0 && isPositiveAndBelow (channel, 16))
        {
            unitId = aionProgramUnitBase + channel;
            return kResultTrue;
        }
        unitId = Vst::kRootUnitId;''')
    replace(
        '''            audioProcessorChanged (pluginInstance, ChangeDetails().withParameterInfoChanged (true));
''',
        '''            for (int channel = 0; channel < 16; ++channel)
                parameters.addParameter (new Vst::Parameter (toString ("MIDI Program Ch " + String (channel + 1)),
                    aionProgramBase + static_cast<Vst::ParamID> (channel), nullptr, 127, 0,
                    Vst::ParameterInfo::kIsProgramChange | Vst::ParameterInfo::kIsHidden,
                    aionProgramUnitBase + channel));
            audioProcessorChanged (pluginInstance, ChangeDetails().withParameterInfoChanged (true));
''')
    replace(
        '''               #if JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS
                if (juceVST3EditController != nullptr && juceVST3EditController->isMidiControllerParamID (vstParamID))''',
        '''                if (aionProgramId (vstParamID))
                {
                    // Each queue point is a pulse, even if the value is unchanged.
                    // Do not invoke preset setters or message-thread IPC here.
                    for (Steinberg::int32 point = 0; point < numPoints; ++point)
                    {
                        if (! aionProgramBudget.take()) break;
                        if (const auto change = getPointFromQueue (paramQueue, point))
                            if (const auto event = aion::vst3::decode (vstParamID, change->offsetSamples, change->value))
                                midiBuffer.addEvent (MidiMessage::programChange (event->channel, event->number), event->offset);
                    }
                }
                else
               #if JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS
                if (juceVST3EditController != nullptr && juceVST3EditController->isMidiControllerParamID (vstParamID))''')
    replace('        const auto numParamsChanged = paramChanges.getParameterCount();',
        '        aion::vst3::PointBudget aionProgramBudget;\n        const auto numParamsChanged = paramChanges.getParameterCount();')
    replace(
        '''        }
    }

    void addParameterChangeToMidiBuffer''',
        '''        }
        if (aionProgramBudget.overflow)
            if (auto* sink = dynamic_cast<aion::vst3::OverflowSink*> (pluginInstance))
                sink->programInputOverflow();
    }

    void addParameterChangeToMidiBuffer''')
    replace('midiBuffer.ensureSize (2048);', 'midiBuffer.ensureSize (8192);')
    return source


def main():
    p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('destination',type=Path);args=p.parse_args()
    raw=args.source.read_bytes()
    # The dependency commit is independently checked by Dependencies.cmake.
    text=raw.decode('utf8').replace('\r\n','\n')
    if hashlib.sha256(text.encode('utf8')).hexdigest()!='91716a2225f034ed4c21e86a37edaaba71bdf3fa161de0938f4e754bb1e61874':
        raise RuntimeError('Unpinned or modified JUCE VST3 source')
    generated=transform(text).encode('utf8')
    args.destination.parent.mkdir(parents=True,exist_ok=True)
    if not args.destination.exists() or args.destination.read_bytes()!=generated:args.destination.write_bytes(generated)
    print('Generated VST3 program-routing wrapper; upstream SHA256='+hashlib.sha256(raw).hexdigest())

if __name__=='__main__':main()
