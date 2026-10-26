"""Generate reversible scoped device-delivery hooks; leave the JUCE SDK intact."""
from pathlib import Path
import argparse

START = 'class CallbackMaxSizeEnforcer  : public AudioIODeviceCallback'
END = '//==============================================================================\nclass AudioDeviceManager::CallbackHandler'
REPLACEMENTS = [
    ('CallbackMaxSizeEnforcer (AudioIODeviceCallback& callbackIn)\n        : inner (callbackIn) {}',
     'CallbackMaxSizeEnforcer (AudioIODeviceCallback& callbackIn, const void* aionOwnerIn)\n        : inner (callbackIn), aionOwner (aionOwnerIn) {}'),
    ('        maximumSize = device->getCurrentBufferSizeSamples();',
     '        aionDeliveryBinding.advance();\n        Time::getMillisecondCounterHiRes();\n        maximumSize = device->getCurrentBufferSizeSamples();'),
    ('        jassert ((int) storedInputChannels.size()  == numInputChannels);',
     '        aion::DeviceDeliveryScope aionDelivery { aionOwner, &aionDeliveryBinding, numSamples, aionDeliveryBinding.epoch(), aion::DeviceCallbackBinding::configurationEpoch(), Time::getMillisecondCounterHiRes() * 0.001 };\n        jassert ((int) storedInputChannels.size()  == numInputChannels);'),
    ('            inner.audioDeviceIOCallbackWithContext (storedInputChannels.data(),',
     '            aion::DeviceDeliveryScope::Block aionBlock { position, blockLength };\n            inner.audioDeviceIOCallbackWithContext (storedInputChannels.data(),'),
    ('        inner.audioDeviceStopped();',
     '        aionDeliveryBinding.advance();\n        inner.audioDeviceStopped();'),
    ('    int maximumSize = 0;',
     '    int maximumSize = 0;\n    const void* aionOwner;\n    aion::DeviceCallbackBinding aionDeliveryBinding;'),
    ('    CallbackMaxSizeEnforcer enforcer { *this };',
     '    CallbackMaxSizeEnforcer enforcer { *this, &owner };')]
MODULE_FROM = '#include "audio_io/juce_AudioDeviceManager.cpp"'
MODULE_TO = '#include "core/DeviceDeliveryScope.h"\n#include "aion_AudioDeviceManager.cpp"'
MM_FROM = '#include "juce_audio_devices.cpp"'
MM_TO = '#include "aion_audio_devices.cpp"'

def newline(source, text):
    return text.replace('\n','\r\n') if '\r\n' in source else text

def replace_once(source, before, after):
    before,after = newline(source,before),newline(source,after)
    if source.count(before) != 1 or after in source:
        raise RuntimeError('JUCE device-delivery source layout changed')
    return source.replace(before,after,1)

def patch_manager(source):
    end = newline(source, END)
    if source.count(START) != 1 or source.count(end) != 1:
        raise RuntimeError('JUCE device splitter boundaries changed')
    begin,finish = source.index(START),source.index(end)
    body = source[begin:finish]
    for before,after in REPLACEMENTS[:-1]:
        body = replace_once(body,before,after)
    output = source[:begin]+body+source[finish:]
    return replace_once(output,*REPLACEMENTS[-1])

def unpatch_manager(source):
    for before,after in reversed(REPLACEMENTS):
        after,before = newline(source,after),newline(source,before)
        if source.count(after) != 1:
            raise RuntimeError('Generated JUCE device-delivery hook drift')
        source = source.replace(after,before,1)
    return source

def write_changed(path, text):
    data = text.encode('utf-8')
    path.parent.mkdir(parents=True,exist_ok=True)
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('module',type=Path)
    parser.add_argument('output',type=Path)
    args = parser.parse_args()
    manager = (args.module/'audio_io/juce_AudioDeviceManager.cpp').read_bytes().decode('utf-8')
    generated = patch_manager(manager)
    assert unpatch_manager(generated) == manager
    write_changed(args.output/'aion_AudioDeviceManager.cpp',generated)
    for name,before,after in [('juce_audio_devices.cpp',MODULE_FROM,MODULE_TO),('juce_audio_devices.mm',MM_FROM,MM_TO)]:
        write_changed(args.output/name.replace('juce_','aion_',1),replace_once((args.module/name).read_bytes().decode('utf-8'),before,after))
    # Tests compile the very same instrumented splitter body in a separate namespace.
    from extract_device_splitter import extract
    test = extract(generated).replace('namespace aion_split_test {','namespace aion_delivery_split_test {',1)
    write_changed(args.output/'aion_device_delivery_splitter_test.h','#include "core/DeviceDeliveryScope.h"\n'+test)
