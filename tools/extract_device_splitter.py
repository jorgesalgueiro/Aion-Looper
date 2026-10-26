"""Test-only copy of JUCE's actual splitter; never edits or replaces the SDK."""
from pathlib import Path
import argparse

START = 'class CallbackMaxSizeEnforcer  : public AudioIODeviceCallback'
END = '//==============================================================================\nclass AudioDeviceManager::CallbackHandler'

def extract(source):
    newline = '\r\n' if '\r\n' in source else '\n'
    end = END.replace('\n', newline)
    if source.count(START) != 1 or source.count(end) != 1 or not source.startswith('/*'):
        raise RuntimeError('JUCE device splitter layout changed')
    license_end = source.find('*/') + 2
    begin = source.index(START)
    finish = source.index(end)
    if license_end < 2 or begin < license_end or finish <= begin:
        raise RuntimeError('JUCE device splitter boundaries changed')
    return (source[:license_end] + newline + '#pragma once' + newline
            + '#include <juce_audio_devices/juce_audio_devices.h>' + newline
            + 'namespace aion_split_test {' + newline + 'using namespace juce;' + newline
            + source[begin:finish] + '}' + newline)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    data = extract(args.source.read_bytes().decode('utf-8')).encode('utf-8')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_bytes() != data:
        args.output.write_bytes(data)
