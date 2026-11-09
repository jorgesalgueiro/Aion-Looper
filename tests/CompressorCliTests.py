"""External CLI into the same production compressor state/audio owner."""
import json
from pathlib import Path
import subprocess
import sys
root, instance = sys.argv[1:3]
client = Path(__file__).resolve().parent.parent / 'tools/aion.py'
def cli(*args, code=0):
    p = subprocess.run([sys.executable, str(client), '--root', root, '--instance', instance, *args], capture_output=True, text=True, timeout=8)
    assert p.returncode == code, (args, p.stdout, p.stderr)
    return json.loads(p.stdout)
assert cli('comp', '20', 'preset', '--preset', 'sustain')['applied']
assert cli('comp', '20', 'on')['applied']
assert cli('comp', '20', 'get')['compressor']['preset'] == 'sustain'
assert cli('comp', '20', 'get')['compressor']['enabled']
assert not cli('comp', '1', 'get')['compressor']['enabled']
assert cli('comp', '20', 'set', '--values', '{"threshold":-30,"gain":3,"attack":0}')['applied']
s = cli('comp', '20', 'get')['compressor']
assert s['values']['threshold'] == -30 and s['values']['gain'] == 3 and not s['preset']
assert len(cli('comp', '20', 'curve')['points']) == 321
assert not cli('comp', '20', 'set', '--values', '{"threshold":-40,"ratio":0}', code=1)['ok']
assert cli('comp', '20', 'get')['compressor'] == s
assert cli('comp', '20', 'reset')['applied']
assert cli('comp', '20', 'off')['applied']
assert cli('midi', 'eqTrack', '--data', '{"track":19}')['ok']
assert cli('midi', 'learn', '--data', '{"target":122}')['ok']
assert cli('status')['midi']['learnTarget'] == 122
assert cli('midi', 'cancelLearn')['ok']
assert cli('midi', 'clear', '--data', '{"target":122}')['ok']
print('PASS external compressor CLI: expanded track, preset/enable/controls/curve, atomic rejection, reset/bypass and MIDI learn')
