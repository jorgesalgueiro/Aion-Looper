#!/usr/bin/env python3
"""Isolated Windows REAPER VST3/CLAP state and settled stereo-render gate.
No plugin installation, user profile writes, physical I/O or packaging.
"""
import argparse
import base64
import xml.etree.ElementTree as ET
import ctypes
from ctypes import wintypes
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time
import uuid
import wave

ROOT = Path(__file__).resolve().parents[1]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def loaded_module(child, expected):
    """Prove the child loaded this build rather than another installed Aion."""
    api = ctypes.WinDLL('psapi', use_last_error=True)
    enum = api.EnumProcessModules
    enum.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.HMODULE), wintypes.DWORD, ctypes.POINTER(wintypes.DWORD)]
    enum.restype = wintypes.BOOL
    filename = api.GetModuleFileNameExW
    filename.argtypes = [wintypes.HANDLE, wintypes.HMODULE, wintypes.LPWSTR, wintypes.DWORD]
    filename.restype = wintypes.DWORD
    modules = (wintypes.HMODULE * 4096)()
    needed = wintypes.DWORD()
    assert enum(int(child._handle), modules, ctypes.sizeof(modules), ctypes.byref(needed)), 'Cannot inspect owned child modules'
    assert needed.value <= ctypes.sizeof(modules), 'Module list overflow'
    for module in modules[:needed.value // ctypes.sizeof(wintypes.HMODULE)]:
        name = ctypes.create_unicode_buffer(32768)
        if filename(int(child._handle), module, name, len(name)):
            if Path(name.value).resolve() == expected.resolve():
                return str(Path(name.value).resolve())
    return None

def pcm(path):
    data = path.read_bytes()
    assert data[:4] == b'RIFF' and data[8:12] == b'WAVE', 'Expected RIFF WAVE'
    chunks = {}
    pos = 12
    while pos + 8 <= len(data):
        key, size = struct.unpack_from('<4sI', data, pos)
        assert pos + 8 + size <= len(data), 'Truncated WAV'
        chunks[key] = data[pos+8:pos+8+size]
        pos += 8 + size + size % 2
    fmt, channels, rate, _, align, bits = struct.unpack_from('<HHIIHH', chunks[b'fmt '])
    assert channels == 2 and rate == 48000, (channels, rate)
    raw = chunks[b'data']
    assert len(raw) == 48000 * align, 'Expected exactly one second'
    if fmt == 1 and bits == 16:
        return [x[0] / 32768 for x in struct.iter_unpack('<h', raw)], 1 / 32768 + 1e-7
    if fmt == 1 and bits == 24:
        return [int.from_bytes(raw[i:i+3], 'little', signed=True) / 8388608
                for i in range(0,len(raw),3)], 1 / 8388608 + 1e-7
    if fmt == 3 and bits == 32:
        return [x[0] for x in struct.iter_unpack('<f', raw)], 1e-7
    raise AssertionError(('Unsupported render format', fmt, bits))

def midi_specs():
    return [('1',1,16),('2',2,16),('pc-repeat',3,16)]+[('pc-'+str(c),3,c) for c in range(1,16)]+[('3',3,16)]


def midi_fixtures(run):
    """Edit only this test plugin's public state, preserving opaque wrapper bytes.

    Equal-length XML edits leave JUCE/wrapper sizes and all surrounding data
    untouched. This fixture configures mappings, not GUI Learn or user settings.
    """
    source = run/'midi-source.b64'
    if not source.exists() or (run/'midi-1.b64').exists():
        return
    raw = base64.b64decode(source.read_text().strip(), validate=True)
    magic = b'VC2!'
    assert raw.count(magic) == 1, 'Expected exactly one JUCE state'
    pos = raw.index(magic)
    size, = struct.unpack_from('<I', raw, pos+4)
    assert size > 0 and pos+8+size < len(raw) and raw[pos+8+size] == 0, 'Truncated JUCE state'
    xml = raw[pos+8:pos+8+size]
    tree = ET.fromstring(xml)
    assert tree.tag == 'AionState', 'Wrong state root'
    settings = tree.find('MidiSettings')
    assert settings is not None and settings.get('version') == '3'
    assert all(settings.get('binding'+str(i)) is not None for i in range(93))
    assert tree.find("PARAM[@id='masterGain']").get('value') == '1.0', 'Expected unity fixture master'
    labels=[]
    for label,kind,channel in midi_specs():
        labels.append(label)
        binding=kind|(channel<<9)
        modified = xml
        for old,new in [(b'<MidiSettings enabled="0"',b'<MidiSettings enabled="1"'),
                        (b'binding73="0"',b'binding73="'+str(binding).encode()+b'"'),
                        (b'id="inputMonitor" value="1.0"',b'id="inputMonitor" value="0"'),
                        (b'id="syncMode" value="2.0"',b'id="syncMode" value="0"')]:
            assert modified.count(old) == 1, ('Unexpected state shape',old)
            modified = modified.replace(old,new)
        if kind==3 and label!='pc-repeat':
            old=b'binding0="145"';assert modified.count(old)==1
            modified=modified.replace(old,b'binding0="'+str(3|(127<<2)|(channel<<9)).encode()+b'"')
        # Short canonical numeric values reserve enough bytes for channel-specific
        # bindings; root whitespace preserves every opaque wrapper length field.
        padding=len(xml)-len(modified);assert padding>=0
        modified=modified.replace(b'</AionState>',b' '*padding+b'</AionState>')
        assert len(modified) == len(xml)
        state = ET.fromstring(modified)
        assert state.find('MidiSettings').get('binding73') == str(binding)
        changed = raw[:pos+8]+modified+raw[pos+8+size:]
        assert len(changed) == len(raw)
        # Publish the first file last: it is the host's fixture-ready signal.
        (run/('midi-'+label+'.b64.tmp')).write_text(base64.b64encode(changed).decode(),encoding='ascii')
    for label in reversed(labels):
        (run/('midi-'+label+'.b64.tmp')).replace(run/('midi-'+label+'.b64'))


def verify_midi(run, cases=None, plugin_format=None):
    if cases is None:cases = []
    for label,kind,channel in midi_specs()+[('project_reload',3,16)]:
        path = run/('midi-'+label+'.wav')
        audio,tolerance = pcm(path)
        assert all(math.isfinite(x) for x in audio), 'Nonfinite MIDI loop audio'
        windows = {}
        for name,start,end,audible in [('capture',0.08,0.15,False),
                ('playing',0.28,0.35,True),('muted',0.48,0.60,False),
                ('recovered',0.78,0.90,True)]:
            values=audio[round(start*48000)*2:round(end*48000)*2]
            peaks=[max(abs(x) for x in values[ch::2]) for ch in (0,1)]
            nonzero=[sum(x != 0 for x in values[ch::2]) for ch in (0,1)]
            assert all(p > 0.01 for p in peaks) if audible else all(p == 0 for p in peaks), (kind,name,peaks)
            if audible:
                assert all(n > len(values)*0.45 for n in nonzero), (kind,name,nonzero)
            windows[name]={'startSeconds':start,'endSeconds':end,'peaks':peaks,'nonzeroSamples':nonzero}
        if kind==3 and label!='pc-repeat':
            # PC program points supplied by this VST3 host are quantized to its
            # processing quantum. Identify the actual take period BEFORE mute;
            # never estimate it from the recovered signal being verified.
            anchor=round(.23*48000);candidates=[]
            for lag in range(6144,7681):
                if all(abs(audio[(anchor+j)*2]-audio[(anchor+j+lag)*2])<=tolerance for j in (0,117,239,479)):
                    if max(abs(audio[(anchor+j)*2]-audio[(anchor+j+lag)*2]) for j in range(480))<=tolerance:
                        candidates.append(lag)
            assert len(candidates)==1, (label,'ambiguous/nonperiodic pre-mute source',candidates)
            period=candidates[0]
            if plugin_format=='CLAP':assert period==7200, (label,'raw-MIDI capture timing changed',period)
            before=audio[anchor*2:(anchor+3360)*2]
            after=audio[(anchor+4*period)*2:(anchor+4*period+3360)*2]
        else:
            # Retain the original exact Note-timed 150ms/four-cycle gate.
            period=7200
            before=audio[round(.28*48000)*2:round(.35*48000)*2]
            after=audio[round(.88*48000)*2:round(.95*48000)*2]
        phase_error=max(abs(a-b) for a,b in zip(before,after))
        assert phase_error<=tolerance, (label,'mute altered loop source/phase',phase_error,tolerance)
        assert max(abs(audio[i]-audio[i+1]) for i in range(0,len(audio),2)) <= tolerance, 'Original center-pan loop downmix changed'
        cases.append({'name':label,'kind':kind,'channel':channel,'windows':windows,'recoveredPhaseMaxError':phase_error,
                      'tolerance':tolerance,'recordedPeriodFrames':period,'wavSha256':sha(path)})
    restored = base64.b64decode((run/'midi-restored.b64').read_text(),validate=True)
    assert restored.count(b'VC2!') == 1
    pos=restored.index(b'VC2!');size,=struct.unpack_from('<I',restored,pos+4)
    state=ET.fromstring(restored[pos+8:pos+8+size])
    mapping=state.find('MidiSettings')
    assert mapping.get('enabled') == '1' and mapping.get('version') == '3'
    assert mapping.get('binding73') == '8195' and mapping.get('binding0') == '8703', 'Reopened project lost PC MUTE assignment'
    assert all(mapping.get('binding'+str(i)) is not None for i in range(93))
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reaper', type=Path, default=Path('C:/Program Files/REAPER (x64)/reaper.exe'))
    parser.add_argument('--format', choices=['VST3', 'CLAP', 'both'], default='both')
    parser.add_argument('--midi', action='store_true', help='Also render main Note record/finish and mapped MUTE Note/CC/PC in the actual host')
    parser.add_argument("--full-speed", action="store_true", help="Test MIDI finalization without the default 1x render pacing")
    args = parser.parse_args()
    assert not args.full_speed or args.midi, "--full-speed requires --midi"
    assert sys.platform == 'win32', 'Windows REAPER required'
    assert args.reaper.is_file(), 'REAPER not installed'
    formats = ['VST3', 'CLAP'] if args.format == 'both' else [args.format]
    modules = {
        'VST3': ROOT/'build/windows/AionLooper_artefacts/Release/VST3/Aion-Looper.vst3/Contents/x86_64-win/Aion-Looper.vst3',
        'CLAP': ROOT/'build/windows/AionLooper_artefacts/Release/CLAP/Aion-Looper.clap'}
    for kind in formats:
        assert modules[kind].is_file(), f'Build {kind} first'
    output = ROOT/'build/reaper-daw'/ (time.strftime('%Y%m%dT%H%M%S') + '-' + uuid.uuid4().hex[:8])
    output.mkdir(parents=True)
    report = {'passed': False, 'host': str(args.reaper.resolve()), 'hostSha256': sha(args.reaper),
              'scriptSha256': sha(ROOT/'tests/ReaperDawProbe.lua'), 'runnerSha256': sha(Path(__file__)),
              'dirtyTracked': subprocess.check_output(['git','diff','--name-only'],cwd=ROOT,text=True).splitlines(),
              'midiRenderMode': 'full-speed' if args.full_speed else '1x', 'midiRequested': args.midi, 'midiScriptSha256': sha(ROOT/'tests/ReaperMidiProbe.lua'),
              'fixtureTestsSha256': sha(ROOT/'tests/ReaperMidiFixtureTests.py'),
              'sourceCommit': subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
              'limits': ['offline renders; no physical driver latency or 64/128 callback certification',
                         'monitor gate: settled gain only, excludes first 100ms smoothing envelope',
                         'MIDI gate checks '+('full-speed' if args.full_speed else 'paced 1x')+' offline rendering; not physical timing certification',
                         'track-state recall only; recorded-loop project storage is not implemented',
                         'no editor interaction, MIDI clock or host-follow scheduling acceptance'], 'formats': []}
    try:
        for kind in formats:
            run = output/kind
            run.mkdir()
            before = sha(modules[kind])
            result = {'format': kind, 'module': str(modules[kind]), 'sha256Before': before, 'passed': False}
            report['formats'].append(result)
            # Separate resource directory: no user ini, mappings, license or recent files copied.
            dirs = ROOT/'build/windows/AionLooper_artefacts/Release'
            (run/'reaper.ini').write_text('[reaper]\nvstpath64='+ (dirs/'VST3').as_posix()+
                '\nclap_path_win64='+(dirs/'CLAP').as_posix()+
                '\naudioclosestop=1\naudiocloseinactive=1\nrenderclosewhendone=4\n', encoding='utf-8')
            (run/'config.lua').write_text('return {format="'+kind+'",midi='+str(args.midi).lower()+',fullSpeed='+str(args.full_speed).lower()+'}\n', encoding='utf-8')
            shutil.copyfile(ROOT/'tests/ReaperMidiProbe.lua', run/'midi.lua')
            shutil.copyfile(ROOT/'tests/ReaperDawProbe.lua', run/'probe.lua')
            samples = []
            for n in range(48000):
                samples.extend((round(6000*math.sin(n*.031)), round(-3000*math.cos(n*.047))))
            with wave.open(str(run/'input.wav'), 'wb') as wav:
                wav.setparams((2,2,48000,0,'NONE','not compressed'))
                wav.writeframes(struct.pack('<'+'h'*len(samples), *samples))
            cmd = [str(args.reaper), '-newinst', '-nosplash', '-cfgfile', str(run/'reaper.ini'), str(run/'probe.lua')]
            result['command'] = cmd
            startup = subprocess.STARTUPINFO()
            startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startup.wShowWindow = 0
            child = subprocess.Popen(cmd, startupinfo=startup)
            result['pid'] = child.pid
            try:
                deadline = time.monotonic()+240
                while child.poll() is None:
                    events = (run/'events.txt').read_text() if (run/'events.txt').exists() else ''
                    if 'loadedModule' not in result and 'plugin=' in events:
                        loaded = loaded_module(child, modules[kind])
                        if loaded:
                            result['loadedModule'] = loaded
                    if args.midi:
                        midi_fixtures(run)
                    if 'FAIL=' in events:
                        raise AssertionError(events)
                    if time.monotonic() > deadline:
                        raise TimeoutError('Owned REAPER test exceeded 240s; inspect '+str(run))
                    time.sleep(.2)
                result['exitCode'] = child.returncode
                assert child.returncode == 0, 'REAPER exited unsuccessfully'
            finally:
                if child.poll() is None:
                    child.terminate()  # Only the child launched above; never an existing REAPER.
                    child.wait(timeout=15)
            events = (run/'events.txt').read_text()
            assert events.splitlines()[-1] == 'PASS', events
            assert 'loadedModule' in result, 'Expected plugin module was not observed in owned REAPER'
            result['hostEvidence'] = events.splitlines()
            result['sha256After'] = sha(modules[kind])
            assert result['sha256After'] == before, 'Plugin changed during test'
            result['cases'] = []
            for case, gain in [('default',0),('unity',1),('half',.5),('monitor_off',0),('master_zero',0),('recovery',1),('project_reload',1)]:
                audio, tolerance = pcm(run/(case+'.wav'))
                assert all(math.isfinite(x) for x in audio), 'Nonfinite render'
                # Existing 20ms smoothing is tested separately; this gate compares settled audio.
                errors = [abs(audio[i] - samples[i]/32768*gain) for i in range(9600,len(samples))]
                error = max(errors)
                assert error <= tolerance, (kind,case,error,tolerance)
                nonzero = sum(x != 0 for x in audio[9600:])
                assert (nonzero > 80000) if gain else (nonzero == 0), (case,nonzero)
                result['cases'].append({'name':case,'samplesCompared':len(errors),'maxError':error,
                                        'tolerance':tolerance,'nonzeroSamples':nonzero,'wavSha256':sha(run/(case+'.wav'))})
            if args.midi:
                result['midiSourceEventsSha256'] = {label:sha(run/('midi-'+label+'.events')) for label,_,_ in midi_specs()}
                result['midiCases'] = []
                verify_midi(run,result['midiCases'],kind)
                print(kind+': real-host Note record/finish and mapped MUTE Note/CC/PC silence/recovery/phase plus assignment project recall passed',flush=True)
            result['passed'] = True
            print(kind+': seven real REAPER renders, track-state and project recall passed',flush=True)
        report['passed'] = True
    except Exception as exc:
        report['error'] = str(exc)
        raise
    finally:
        (output/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        print('Evidence: '+str(output/'result.json'),flush=True)

if __name__ == '__main__':
    main()
