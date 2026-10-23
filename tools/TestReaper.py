#!/usr/bin/env python3
"""Isolated Windows REAPER VST3/CLAP state and settled stereo-render gate.
No plugin installation, user profile writes, physical I/O or packaging.
"""
import argparse
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

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reaper', type=Path, default=Path('C:/Program Files/REAPER (x64)/reaper.exe'))
    parser.add_argument('--format', choices=['VST3', 'CLAP', 'both'], default='both')
    args = parser.parse_args()
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
              'sourceCommit': subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
              'limits': ['offline renders; no physical driver latency or 64/128 callback certification',
                         'settled gain only, excludes first 100ms smoothing envelope',
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
            (run/'config.lua').write_text('return {format="'+kind+'"}\n', encoding='utf-8')
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
            result['sha256After'] = sha(modules[kind])
            assert result['sha256After'] == before, 'Plugin changed during test'
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
