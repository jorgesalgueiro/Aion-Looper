"""External CLI into the production processor, never a headless DSP duplicate."""
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
assert cli('eq', '20', 'preset', '--preset', 'Radio')['applied']
assert cli('eq', '20', 'on')['applied']
assert cli('eq', '20', 'get')['equalizer']['preset'] == 'Radio'
assert cli('eq', '20', 'get')['equalizer']['enabled']
assert not cli('eq', '1', 'get')['equalizer']['enabled']
assert cli('eq', '20', 'set', '--values', '{"p1Gain":4,"p1Freq":1800,"p1Q":0.1}')['applied']
s = cli('eq', '20', 'get')['equalizer']
assert s['values']['p1Freq'] == 1800 and s['values']['p1Gain'] == 4 and not s['preset']
assert len(cli('eq', '20', 'curve')['points']) == 256
assert cli('eq', '20', 'width', '--band', '1', '--edge', '2500')['applied']
s = cli('eq', '20', 'get')['equalizer']
assert s['values']['p1Q'] > 1
assert abs(cli('eq', '20', 'curve')['ranges'][0][1] - 2500) < .01  # Float32 Q -> Hz edge.
assert not cli('eq', '20', 'set', '--values', '{"p1Gain":3,"p6Q":0}', code=1)['ok']
assert cli('eq', '20', 'get')['equalizer'] == s
assert cli('eq', '20', 'reset')['applied']
assert cli('eq', '20', 'off')['applied']
print('PASS external eq CLI: expanded track, preset/enable/XY/Q, bandwidth edge, curve, atomic rejection, reset/bypass')
