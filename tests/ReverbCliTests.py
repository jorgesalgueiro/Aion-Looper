"""External CLI against the production reverb request/receipt owner."""
import json
from pathlib import Path
import subprocess
import sys

root, instance = sys.argv[1:3]
client = Path(__file__).resolve().parent.parent / 'tools/aion.py'


def cli(*args, code=0):
    result = subprocess.run(
        [sys.executable, str(client), '--root', root, '--instance', instance, *args],
        capture_output=True, text=True, timeout=8)
    assert result.returncode == code, (args, result.stdout, result.stderr)
    return json.loads(result.stdout)


assert cli('reverb', '20', 'preset', '--preset', 'small')['applied']
assert cli('reverb', '20', 'on')['applied']
assert cli('reverb', '20', 'get')['reverb']['preset'] == 'small'
assert not cli('reverb', '1', 'get')['reverb']['enabled']
assert cli('reverb', '20', 'set', '--parameter', 'mix', '--value', '0.12345678901234566')['applied']
assert cli('reverb', '20', 'get')['reverb']['values']['mix'] == 0.12345678901234566
assert cli('reverb', '20', 'room', '--room', 'studio')['applied']
settings = cli('reverb', '20', 'get')['reverb']
assert settings['room'] == 'studio' and settings['preset'] == ''
assert settings['values']['duration'] == 1.2 and settings['values']['decay'] == 1.8
for args in (('set', '--parameter', 'mix', '--value', '2'),
             ('set', '--parameter', 'mix', '--value', 'true'),
             ('set', '--parameter', 'missing', '--value', '0.2'),
             ('room', '--room', 'missing'), ('preset', '--preset', 'missing')):
    assert not cli('reverb', '20', *args, code=1)['ok']
    assert cli('reverb', '20', 'get')['reverb'] == settings
assert cli('reverb', '20', 'reset')['applied']
assert cli('reverb', '20', 'get')['reverb']['enabled']
assert cli('reverb', '20', 'off')['applied']
assert cli('reverb', '20', 'toggle')['applied']
assert cli('reverb', '20', 'off')['applied']
print('PASS external Reverb CLI: exact receipts, preset/room, precision, strict rollback, reset/bypass and expanded track isolation')
