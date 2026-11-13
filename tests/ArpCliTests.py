"""External CLI into the production original Arp settings/receipt owner."""
import json
from pathlib import Path
import subprocess
import sys
root, instance = sys.argv[1:3]
client = Path(__file__).resolve().parent.parent / 'tools/aion.py'
def cli(*args, code=0):
    if args[0] != 'status':
        print('Arp CLI step:', args, file=sys.stderr, flush=True)
    p = subprocess.run([sys.executable, str(client), '--root', root, '--instance', instance, *args], capture_output=True, text=True, timeout=8)
    assert p.returncode == code, (args, p.stdout, p.stderr)
    return json.loads(p.stdout)
assert cli('arp', '20', 'preset', '--preset', 'sync-trip')['applied']
assert cli('arp', '20', 'on')['applied']
assert cli('parameter', 'syncBpm', '173.25')['ok']
import time
for _ in range(150):
    current = cli('status')
    if _ < 3:
        print('Arp tempo observation:', current['syncBpm'], current['tracks'][19]['arp']['bpm'], current['tracks'][19]['arpBusy'], file=sys.stderr, flush=True)
    if not current['tracks'][19]['arpBusy'] and current['tracks'][19]['arp']['bpm'] == current['syncBpm']:
        break
    time.sleep(.01)
else:
    raise AssertionError('External Master BPM did not reach active Arp')
assert abs(current['syncBpm'] - 173.25) < .001
assert cli('arp', '20', 'get')['arp']['preset'] == 'sync-trip'
assert cli('arp', '20', 'get')['arp']['enabled']
assert not cli('arp', '1', 'get')['arp']['enabled']
assert cli('arp', '20', 'set', '--parameter', 'time', '--value', '0.12345678901234566')['applied']
s = cli('arp', '20', 'get')['arp']
assert s['values']['time'] == 0.12345678901234566 and not s['preset'], s
assert not cli('arp', '20', 'set', '--parameter', 'time', '--value', '3', code=1)['ok']
assert cli('arp', '20', 'get')['arp'] == s
assert cli('arp', '20', 'off')['applied']
assert cli('arp', '20', 'toggle')['applied']
assert cli('arp', '20', 'off')['applied']
print('PASS external Arp CLI: expanded track, preset/worker enable, exact originals, atomic rejection, bypass and exact receipt confirmation')
