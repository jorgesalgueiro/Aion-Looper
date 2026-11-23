"""External development CLI, actual production Delay request/receipt owner."""
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
assert cli('delay', '20', 'preset', '--preset', 'dub')['applied']
assert cli('delay', '20', 'on')['applied']
assert cli('delay', '20', 'get')['delay']['preset'] == 'dub'
assert not cli('delay', '1', 'get')['delay']['enabled']
assert cli('delay', '20', 'set', '--parameter', 'time', '--value', '0.12345678901234566')['applied']
assert cli('delay', '20', 'get')['delay']['values']['time'] == 0.12345678901234566
for count in (1, 2, 3, 10):
    assert cli('delay', '20', 'repeats', '--mode', 'exact', '--count', str(count))['applied']
    s = cli('delay', '20', 'get')['delay']
    assert s['repeatMode'] == 'exact' and s['repeatCount'] == count
assert cli('delay', '20', 'repeats', '--mode', 'feedback', '--count', '3')['applied']
assert cli('delay', '20', 'set', '--parameter', 'repetitions', '--value', '6')['applied']
s = cli('delay', '20', 'get')['delay']
assert not cli('delay', '20', 'repeats', '--mode', 'exact', '--count', '11', code=1)['ok']
assert not cli('delay', '20', 'set', '--parameter', 'mix', '--value', '2', code=1)['ok']
assert cli('delay', '20', 'get')['delay'] == s
assert cli('delay', '20', 'reset')['applied']
assert cli('delay', '20', 'get')['delay']['enabled']
assert cli('delay', '20', 'off')['applied']
assert cli('delay', '20', 'toggle')['applied']
assert cli('delay', '20', 'off')['applied']
print('PASS external Delay CLI: exact receipts, preset, precision, Feedback/exact 1/2/3/10, rollback, reset/bypass and expanded track isolation')
for channel, target in ((1, 147), (16, 150)):
    assert cli('midi', 'learn', '--data', json.dumps({'target': target, 'channel': channel}))['ok']
    s = cli('status')['midi']
    assert s['learnTarget'] == target and s['learnChannel'] == channel
    assert not cli('midi', 'learn', '--data', '{"target":148,"channel":17}', code=1)['ok']
    s = cli('status')['midi']
    assert s['learnTarget'] == target and s['learnChannel'] == channel
    assert cli('midi', 'cancelLearn')['ok']
assert cli('midi', 'learn', '--data', '{"target":147}')['ok']
assert cli('status')['midi']['learnChannel'] == 0
assert cli('midi', 'cancelLearn')['ok']
print('PASS external MIDI CLI: channel-scoped Learn, invalid-filter rollback, cancellation and legacy omni default')
