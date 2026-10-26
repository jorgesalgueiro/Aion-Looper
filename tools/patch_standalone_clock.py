"""Generate a scoped JUCE Standalone IO clock hook without editing the SDK."""
from pathlib import Path
import argparse

ANCHOR = '''    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const AudioIODeviceCallbackContext& context) override
    {
'''
INJECTION = '        aion::DeviceCallbackScope aionDeviceClock { &aionDeviceClockBinding, numSamples, aionDeviceClockBinding.epoch(), Time::getMillisecondCounterHiRes() * 0.001 };\n'
PLAYER = '    AudioProcessorPlayer player;\n'
BINDING = '    aion::DeviceCallbackBinding aionDeviceClockBinding;\n'
INCLUDE = '#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>'
FACADE = '#include "plugin/StandaloneHolder.h"'

def injection_for(source):
    return INJECTION.replace('\n', '\r\n') if '\r\n' in source else INJECTION

def patch_header(source):
    anchor = ANCHOR.replace('\n', '\r\n') if '\r\n' in source else ANCHOR
    injection = injection_for(source)
    player = PLAYER.replace('\n', '\r\n') if '\r\n' in source else PLAYER
    binding = BINDING.replace('\n', '\r\n') if '\r\n' in source else BINDING
    if source.count(anchor) != 1 or injection in source or source.count(player) != 1 or binding in source:
        raise RuntimeError('JUCE Standalone callback layout changed')
    return source.replace(anchor, anchor + injection).replace(player, binding + player)

def patch_wrapper(source):
    if source.count(INCLUDE) != 1 or FACADE in source:
        raise RuntimeError('JUCE Standalone wrapper include layout changed')
    return source.replace(INCLUDE, FACADE)

def write_changed(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    data = text.encode('utf-8')
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('module', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    write_changed(args.output / 'aion_StandaloneFilterWindow.h', patch_header(
        (args.module / 'Standalone/juce_StandaloneFilterWindow.h').read_bytes().decode('utf-8')))
    write_changed(args.output / 'aion_standalone_clock.cpp', patch_wrapper(
        (args.module / 'juce_audio_plugin_client_Standalone.cpp').read_bytes().decode('utf-8')))
