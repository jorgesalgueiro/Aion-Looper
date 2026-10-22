#!/usr/bin/env python3
"""Public LV2 ABI stereo/parameter smoke test; no editor, device or RT certificate.

Uses the official LV2 core/urid/options/atom/patch layouts from the bundled SDK.
Python owns host buffers and URI mapping; no product headers or private API.
"""
import argparse
import ctypes as C
import json
import hashlib
import subprocess
import math
import os
from pathlib import Path
import platform
import re
import struct

BASE = 'http://lv2plug.in/ns/ext/'
URI = 'https://github.com/jorgesalgueiro/Aion-Looper/plugins/main'


class Feature(C.Structure):
    _fields_ = [('uri', C.c_char_p), ('data', C.c_void_p)]


MapFunction = C.CFUNCTYPE(C.c_uint32, C.c_void_p, C.c_char_p)


class Map(C.Structure):
    _fields_ = [('handle', C.c_void_p), ('map', MapFunction)]


class Option(C.Structure):
    _fields_ = [('context', C.c_uint32), ('subject', C.c_uint32), ('key', C.c_uint32),
                ('size', C.c_uint32), ('type', C.c_uint32), ('value', C.c_void_p)]


class Descriptor(C.Structure):
    _fields_ = [('uri', C.c_char_p)] + [(key, C.c_void_p) for key in
                ('instantiate', 'connect', 'activate', 'run', 'deactivate', 'cleanup', 'extension')]


def check(ok, message):
    if not ok:
        raise RuntimeError(message)


def exercise(library, plugin, rate, block):
    urids = {}

    def map_uri(uri):
        uri = uri.encode() if isinstance(uri, str) else uri
        if uri not in urids:
            urids[uri] = len(urids) + 1
        return urids[uri]

    mapper = MapFunction(lambda handle, uri: map_uri(uri))
    mapping = Map(None, mapper)
    max_block = C.c_int32(block)
    options = (Option * 2)(Option(0, 0, map_uri(BASE + 'buf-size#maxBlockLength'),
                                  4, map_uri(BASE + 'atom#Int'), C.addressof(max_block)), Option())
    features = (Feature * 3)(Feature((BASE + 'urid#map').encode(), C.addressof(mapping)),
                             Feature((BASE + 'buf-size#boundedBlockLength').encode(), None),
                             Feature((BASE + 'options#options').encode(), C.addressof(options)))
    pointers = (C.POINTER(Feature) * 4)(*[C.pointer(feature) for feature in features], None)
    descriptor = library.lv2_descriptor(0)
    check(bool(descriptor) and descriptor.contents.uri.decode() == URI, 'Unexpected LV2 descriptor URI')
    check(not library.lv2_descriptor(1), 'Expected exactly one LV2 descriptor')
    d = descriptor.contents
    instantiate = C.CFUNCTYPE(C.c_void_p, C.POINTER(Descriptor), C.c_double, C.c_char_p,
                             C.POINTER(C.POINTER(Feature)))(d.instantiate)
    connect = C.CFUNCTYPE(None, C.c_void_p, C.c_uint32, C.c_void_p)(d.connect)
    activate = C.CFUNCTYPE(None, C.c_void_p)(d.activate)
    run = C.CFUNCTYPE(None, C.c_void_p, C.c_uint32)(d.run)
    deactivate = C.CFUNCTYPE(None, C.c_void_p)(d.deactivate)
    cleanup = C.CFUNCTYPE(None, C.c_void_p)(d.cleanup)
    instance = instantiate(descriptor, rate, (str(plugin) + '/').encode(), pointers)
    check(bool(instance), 'LV2 instantiate failed')
    buffers = [(C.c_float * block)() for _ in range(4)]
    incoming, outgoing = C.create_string_buffer(16384), C.create_string_buffer(16384)
    latency, freewheel, enabled = C.c_float(), C.c_float(0), C.c_float(1)
    frame, maximum, nonzero = 0, 0.0, 0
    active = False

    def patch(name, value):
        # Object body + two padded LV2_Atom_Property_Body values, then timestamp.
        prop = struct.pack('=IIIIII', map_uri(BASE + 'patch#property'), 0, 4,
                           map_uri(BASE + 'atom#URID'), map_uri(URI + ':' + name), 0)
        val = struct.pack('=IIII', map_uri(BASE + 'patch#value'), 0, 4,
                          map_uri(BASE + 'atom#Float')) + struct.pack('=fI', value, 0)
        body = struct.pack('=II', 0, map_uri(BASE + 'patch#Set')) + prop + val
        return struct.pack('=qII', 0, len(body), map_uri(BASE + 'atom#Object')) + body

    def render(changes=()):
        nonlocal frame
        events = b''.join(patch(name, value) for name, value in changes)
        sequence = struct.pack('=IIII', 8 + len(events), map_uri(BASE + 'atom#Sequence'), 0, 0) + events
        C.memmove(incoming, sequence, len(sequence))
        header = struct.pack('=IIII', C.sizeof(outgoing) - 8, map_uri(BASE + 'atom#Sequence'), 0, 0)
        C.memmove(outgoing, header, len(header))
        for i in range(block):
            buffers[0][i] = 0.2 * math.sin((frame + i + 1) * 0.031)
            buffers[1][i] = -0.1 * math.cos((frame + i + 1) * 0.047)
            buffers[2][i] = buffers[3][i] = float('nan')
        run(instance, block)
        frame += block
        check(all(math.isfinite(value) for channel in buffers[2:] for value in channel),
              'LV2 failed to write finite stereo output')
        check(latency.value == 0, 'Unexpected declared LV2 latency')

    def compare(gain):
        nonlocal maximum, nonzero
        for c in range(2):
            for i in range(block):
                expected = C.c_float(buffers[c][i] * gain).value
                value = buffers[c + 2][i]
                maximum = max(maximum, abs(value - expected))
                nonzero += value != 0
                check(abs(value - expected) <= 1e-7,
                      f'LV2 stereo/parameter mismatch rate={rate} block={block} channel={c} sample={i}: {value} != {expected}')

    try:
        for index, buffer in enumerate(buffers):
            connect(instance, index, C.addressof(buffer))
        for index, buffer in [(4, incoming), (5, outgoing), (6, latency), (7, freewheel), (8, enabled)]:
            connect(instance, index, C.addressof(buffer))
        activate(instance)
        active = True
        render()
        compare(0)  # Product defaults stay muted, with monitor disabled.
        stages = [(1.0, [('inputMonitor', 1), ('masterGain', 1)]),
                  (0.5, [('masterGain', 0.5)]), (0.0, [('inputMonitor', 0)]),
                  (0.0, [('inputMonitor', 1), ('masterGain', 0)]),
                  (1.0, [('masterGain', 1)])]
        for gain, changes in stages:
            render(changes)
            # Observe settled PCM after the existing 20ms master smoothing ramp.
            for _ in range(math.ceil(rate * 0.025 / block)):
                render()
            for _ in range(4):
                render()
                compare(gain)
        check(nonzero > 0, 'A muted-only render is not audio acceptance')
        return dict(sampleRate=rate, blockSize=block, comparedSamples=block * 2 * 21,
                    nonzeroSamples=nonzero, maximumError=maximum, tolerance=1e-7)
    finally:
        if active:
            deactivate(instance)
        cleanup(instance)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plugin', type=Path, required=True)
    args = parser.parse_args()
    check(platform.system() == 'Linux' and platform.machine() == 'x86_64', 'Linux x86_64 ABI required')
    plugin = args.plugin.resolve(strict=True)
    ttl = (plugin / 'dsp.ttl').read_text()
    symbols = {}
    for port in re.findall(r'\[\s*a lv2:(?:InputPort|OutputPort).*?\]', ttl, re.DOTALL):
        symbols[re.search(r'lv2:symbol "([^"]+)"', port)[1]] = int(re.search(r'lv2:index (\d+)', port)[1])
    check(symbols == dict(audio_in_1=0, audio_in_2=1, audio_out_1=2, audio_out_2=3,
                          **{'in': 4, 'out': 5, 'latency': 6, 'freeWheeling': 7, 'enabled': 8}),
          'Unexpected LV2 port metadata; update the host explicitly')
    def hashes():
        return {name: hashlib.sha256((plugin / name).read_bytes()).hexdigest()
                for name in ('libAion-Looper.so', 'manifest.ttl', 'dsp.ttl', 'ui.ttl')}

    before = hashes()
    library = C.CDLL(str(plugin / 'libAion-Looper.so'), mode=os.RTLD_NOW | os.RTLD_LOCAL)
    library.lv2_descriptor.argtypes = [C.c_uint32]
    library.lv2_descriptor.restype = C.POINTER(Descriptor)
    cases = [exercise(library, plugin, rate, block) for rate in (44100, 48000, 96000) for block in (64, 128)]
    after = hashes()
    check(before == after, 'LV2 module/metadata changed during audio verification')
    root = Path(__file__).resolve().parents[1]
    print(json.dumps(dict(passed=True, pluginPath=str(plugin), bundleSha256=before, bundleSha256After=after,
                          harnessSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                          sourceCommit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
                          trackedChanges=subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=no'], cwd=root, text=True).splitlines(), scope='Public LV2 ABI, patch:Set master/monitor, settled stereo PCM; no GUI/device/loop-session/allocation claim', cases=cases), indent=2))


if __name__ == '__main__':
    main()
