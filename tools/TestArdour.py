#!/usr/bin/env python3
"""Real Ardour/libardour LV2 property automation, synthetic audio and session recall.
Runs in Ubuntu with its own home/config and Dummy backend, never physical I/O.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import uuid

ROOT=Path(__file__).resolve().parents[1]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    assert sys.platform=='linux', 'Run in Ubuntu'
    host=shutil.which('ardour8-lua')
    assert host, 'Install Ardour (including ardour8-lua) first'
    assert os.environ.get('DISPLAY'), 'JUCE LV2 requires a display even without opening its editor'
    bundle=ROOT/'build/linux/AionLooper_artefacts/Debug/LV2/Aion-Looper.lv2'
    files=[bundle/n for n in ['libAion-Looper.so','manifest.ttl','dsp.ttl','ui.ttl']]
    assert all(p.is_file() for p in files), 'Build the LV2 bundle first'
    out=ROOT/'build/ardour-daw'/(time.strftime('%Y%m%dT%H%M%S')+'-'+uuid.uuid4().hex[:8])
    out.mkdir(parents=True)
    before={p.name:sha(p) for p in files}
    report={'passed':False,'host':host,'version':subprocess.check_output([host,'-V'],text=True),
            'scriptSha256':sha(ROOT/'tests/ArdourDawProbe.lua'),'runnerSha256':sha(Path(__file__)),
            'dirtyTracked':subprocess.check_output(['git','diff','--name-only'],cwd=ROOT,text=True).splitlines(),
            'sourceCommit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
            'bundle':str(bundle),'sha256Before':before,'cases':[],
            'limits':['libardour command-line session, not GTK editor interaction',
                      '48kHz synthetic Dummy backend; no physical JACK/ALSA latency certification',
                      'stereo peak gain/mute/recall checks, not sample-by-sample PCM or envelope comparison',
                      'no recorded-loop storage, host-follow scheduling or MIDI clock acceptance',
                      'Ardour 8.4 has no CLAP support; use a compatible CLAP host separately']}
    try:
        for block in (64,128):
            run=out/str(block);run.mkdir()
            home=run/'home';home.mkdir()
            env=os.environ.copy()
            env.update(HOME=str(home),XDG_CONFIG_HOME=str(home/'config'),XDG_CACHE_HOME=str(home/'cache'),
                       XDG_DATA_HOME=str(home/'data'),LV2_PATH=str(bundle.parent))
            cmd=[host,str(ROOT/'tests/ArdourDawProbe.lua'),str(run),str(block)]
            result=subprocess.run(cmd,cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
            (run/'output.log').write_text(result.stdout,encoding='utf-8')
            assert result.returncode==0, result.stdout
            assert 'Error:' not in result.stdout and 'JUCE Assertion failure' not in result.stdout and 'Leaked objects detected' not in result.stdout,result.stdout
            lines=result.stdout.splitlines()
            assert lines[-1]=='PASS',result.stdout  # luasession can exit zero after Lua error
            audio=[line.split() for line in lines if line.startswith('AUDIO')]
            assert [v[1] for v in audio]==['default','unity','half','monitor_off','master_zero','recovery','session_reload'],result.stdout
            assert any(len(v)==3 and v[0]=='BACKEND' and int(v[1])==block and float(v[2])==48000
                       for v in (line.split() for line in lines)),result.stdout
            report['cases'].append({'blockSize':block,'sampleRate':48000,'audioPeaksDb':audio,'passed':True})
            print(f'Ardour LV2: {block} sample stereo gain/mute and session recall passed',flush=True)
        report['sha256After']={p.name:sha(p) for p in files}
        assert report['sha256After']==before,'LV2 bundle changed during test'
        report['passed']=True
    except Exception as exc:
        report['error']=str(exc)
        raise
    finally:
        (out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        print('Evidence: '+str(out/'result.json'),flush=True)

if __name__=='__main__':
    main()
