"""Drive commands into the running production processor, with no second engine."""
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
for effect, preset, parameter in [('distortion', 'tube-warmth', 'amount'), ('overdrive', 'American Sparkle', 'drive'), ('fuzz', 'Big Muff', 'gain')]:
    assert cli(effect, '20', 'preset', '--preset', preset)['applied']
    assert cli(effect, '20', 'on')['applied']
    s = cli(effect, '20', 'get')['drive']
    assert s['enabled'] and s['preset'] == preset
    assert not cli(effect, '20', 'set', '--values', json.dumps({parameter: 10, 'tone': -1}), code=1)['ok']
    assert cli(effect, '20', 'get')['drive'] == s
    assert cli(effect, '20', 'set', '--values', json.dumps({parameter: 12, 'mix': .5}))['applied']
    assert not cli(effect, '20', 'get')['drive']['preset']
    assert cli(effect, '20', 'preset', '--preset', preset)['applied']
    assert cli(effect, '20', 'off')['applied']
assert cli('chain', '20', 'set', 'FOT')['applied']
print('PASS shared distortion/overdrive/fuzz CLI presets, controls, rejection, bypass and chain')
