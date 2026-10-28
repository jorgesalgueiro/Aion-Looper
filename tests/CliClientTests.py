"""External CLI -> local mailbox -> actual processor, driven by JUCE test loop."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import threading
import uuid

root = Path(sys.argv[1])
first, second = sys.argv[2:4]
client = Path(__file__).resolve().parent.parent / 'tools/aion.py'
spec = importlib.util.spec_from_file_location('aion_cli', client)
api = importlib.util.module_from_spec(spec)
spec.loader.exec_module(api)

def cli(*args, instance=first, code=0):
    command = [sys.executable, str(client), '--root', str(root)]
    if instance:
        command += ['--instance', instance]
    process = subprocess.run(command + list(args), capture_output=True, encoding='utf-8', timeout=8)
    assert process.returncode == code, (command, args, process.returncode, process.stdout, process.stderr)
    return json.loads(process.stdout if process.stdout else process.stderr)

def state(expected):
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        view = cli('status')
        if view['tracks'][0]['state'] == expected:
            return view
        time.sleep(.02)
    raise AssertionError(('state timeout', expected, view['tracks'][0]))

listed = cli('instances', instance=None)
assert {item['instance'] for item in listed} == {first, second}
assert not cli('status', instance=None, code=2)['ok']
assert cli('parameter', 'masterGain', '0.5')['ok']
assert cli('status')['masterGain'] == .5
assert cli('status', instance=second)['masterGain'] == 0
assert not cli('parameter', 'masterGain', '3', code=1)['ok']
assert cli('parameter', 'syncMode', '0')['ok']
assert cli('parameter', 'autoPlayAfterRecord', '0')['ok']
assert not cli('loop', 'play', '1', code=1)['ok']  # Queue acceptance is not application success.
assert cli('loop', 'record', '1')['applied']
state('recording');time.sleep(.12)
assert cli('loop', 'finish', '1')['applied']
assert state('stopped')['tracks'][0]['length'] > 0
assert cli('rename', '1', 'CLI Ω loop')['ok']
assert cli('status')['tracks'][0]['name'] == 'CLI Ω loop'
assert cli('loop', 'play', '1')['applied'];state('playing')
assert cli('loop', 'stopImmediate', '1')['applied'];state('stopped')
assert cli('midi', 'enable', '--data', '{"enabled":false}')['ok']
assert cli('midi', 'clear', '--data', '{"target":0}')['ok']
assert not cli('request', '{"operation":"not-a-feature"}', code=1)['ok']
assert not cli('request', '{"operation":"loop","action":"clear","track":0,"generation":0,"expiresAt":0}', code=1)['ok']
endpoints = {info['instance']: (path, info) for path, info in api.instances(root)}
directory, metadata = endpoints[first]
with api.client_lock(directory):
    try:
        with api.client_lock(directory, timeout=.03):
            raise AssertionError('Concurrent client acquired occupied mailbox')
    except RuntimeError as error:
        assert 'busy' in str(error)
try:
    api.invoke(endpoints[first], {'operation':'renameLoop', 'track':0, 'name':'x' * api.MAX_REQUEST})
    raise AssertionError('Oversized client request accepted')
except ValueError:
    pass
dead = root / 'unanswered-test'
dead.mkdir()
try:
    api.invoke((dead, metadata), {'operation':'snapshot'}, timeout=.1)
    raise AssertionError('Unanswered request did not time out')
except RuntimeError as error:
    assert 'may have executed' in str(error)
finally:
    for name in ('client.lock', 'request.json'):
        (dead / name).unlink(missing_ok=True)
    dead.rmdir()
for forged in ({'protocol':True, 'result':{'ok':True}}, {'protocol':1, 'result':[]}, {'protocol':1, 'result':{'ok':1}}):
    dead.mkdir();errors=[]
    def respond():
        try:
            deadline=time.monotonic()+5
            while time.monotonic()<deadline:
                try:
                    request=json.loads((dead/'request.json').read_text(encoding='utf-8'))
                    break
                except (FileNotFoundError, PermissionError):
                    time.sleep(.001)
            else:
                raise AssertionError('Mock reply fixture never received its request')
            temporary=dead/'response-test.tmp'
            temporary.write_text(json.dumps(dict(forged,id=request['id'])),encoding='utf-8')
            os.replace(temporary,dead/'response.json')
        except Exception as error:
            errors.append(error)
    responder=threading.Thread(target=respond);responder.start()
    try:
        api.invoke((dead,metadata),{'operation':'snapshot'},timeout=2)
        raise AssertionError('Invalid reply schema was accepted')
    except RuntimeError as error:
        assert 'schema' in str(error)
    finally:
        responder.join();assert not errors,errors
        for name in ('client.lock','request.json','response.json'):
            (dead/name).unlink(missing_ok=True)
        dead.rmdir()

def raw(packet):
    with api.client_lock(directory):
        (directory / 'response.json').unlink(missing_ok=True)
        temporary = directory / 'test-envelope.tmp'
        temporary.write_text(json.dumps(packet), encoding='utf-8')
        os.replace(temporary, directory / 'request.json')
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            try:
                reply = json.loads((directory / 'response.json').read_text(encoding='utf-8'))
                if reply['id'] == packet['id']:
                    return reply['result']
            except (FileNotFoundError, PermissionError):
                pass
            time.sleep(.01)
        raise AssertionError('Raw envelope not answered')

packet = {'protocol':1, 'id':uuid.uuid4().hex, 'token':metadata['token'],
          'expiresAt':int(time.time()*1000)+4000, 'request':{'operation':'toggleMetronome'}}
result = raw(packet)
assert result['ok']
assert raw(packet) == result  # Exact retry nonce does not toggle twice.
assert cli('status')['metronomeEnabled'] == result['enabled']
packet['id'] = uuid.uuid4().hex;packet['expiresAt'] = 1
assert not raw(packet)['ok']
packet['id'] = uuid.uuid4().hex;packet['expiresAt'] = int(time.time()*1000)+4000;packet['protocol'] = 2
assert not raw(packet)['ok']
packet['protocol'] = 1
packet['id'] = uuid.uuid4().hex;packet['protocol'] = True
assert not raw(packet)['ok']
packet['protocol'] = 1
packet['id'] = uuid.uuid4().hex;packet['expiresAt'] = int(time.time()*1000)+4000;packet['token'] = 'wrong'
assert not raw(packet)['ok']
for malformed in (' ' * (api.MAX_REQUEST + 1), '[' * 40 + '0' + ']' * 40, b'\xff', b'{}\x00{}'):
    with api.client_lock(directory):
        (directory / 'response.json').unlink(missing_ok=True)
        temporary = directory / 'test-envelope.tmp'
        temporary.write_bytes(malformed if isinstance(malformed, bytes) else malformed.encode('utf-8'))
        os.replace(temporary, directory / 'request.json')
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            try:
                assert not json.loads((directory / 'response.json').read_text())['result']['ok']
                break
            except (FileNotFoundError, PermissionError):
                time.sleep(.01)
        else:
            raise AssertionError('Malformed envelope not answered')
assert cli('status')['masterGain'] == .5
assert cli('status')['metronomeEnabled'] == result['enabled']
assert cli('status')['tracks'][0]['name'] == 'CLI Ω loop'
assert cli('status', instance=second)['tracks'][0]['state'] == 'empty'
print('PASS: external CLI instance routing, production parameter/record/play/stop/MIDI/name actions, Unicode, native rejection, expiry/auth/size/depth and duplicate-nonce isolation')
