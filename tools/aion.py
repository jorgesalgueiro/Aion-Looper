#!/usr/bin/env python3
"""Development CLI for the running Aion-Looper Standalone (Python stdlib only)."""
import argparse
import contextlib
import json
import os
from pathlib import Path
import sys
import time
import uuid

MAX_REQUEST = 32768
MAX_REPLY = 2097152

def strict_json(text):
    def invalid(value):
        raise ValueError('Non-finite JSON number: ' + value)
    return json.loads(text, parse_constant=invalid)

def default_root():
    if os.environ.get('AION_CLI_DIR'):
        root = Path(os.environ['AION_CLI_DIR'])
        if not root.is_absolute():
            raise ValueError('AION_CLI_DIR must be an absolute directory')
        return root
    if sys.platform == 'win32':
        base = Path(os.environ['APPDATA'])
    elif sys.platform == 'darwin':
        base = Path.home() / 'Library/Application Support'
    else:
        base = Path(os.environ.get('XDG_CONFIG_HOME', str(Path.home() / '.config')))
        if not base.is_absolute():
            raise ValueError('XDG_CONFIG_HOME must be an absolute directory')
    return base / 'Aion-Looper/cli'

def instances(root):
    found = []
    for path in sorted(root.glob('*/endpoint.json')):
        with path.open('rb') as stream:
            data = stream.read(2049)
        if len(data) > 2048:
            continue
        info = strict_json(data)
        if info.get('protocol') == 1 and info.get('instance') == path.parent.name:
            found.append((path.parent, info))
    return found

@contextlib.contextmanager
def client_lock(directory, timeout=5):
    # Kernel-owned locks are released on process death; no stale lock-directory
    # recovery or deletion of another client's ownership is needed.
    path = directory / 'client.lock'
    with path.open('a+b') as stream:
        stream.seek(0, 2)
        if stream.tell() == 0:
            stream.write(b'\0')
            stream.flush()
        deadline = time.monotonic() + timeout
        while True:
            try:
                stream.seek(0)
                if os.name == 'nt':
                    import msvcrt
                    msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
                else:
                    import fcntl
                    fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except OSError:
                if time.monotonic() >= deadline:
                    raise RuntimeError('CLI mailbox busy; no command was submitted')
                time.sleep(.01)
        try:
            yield
        finally:
            stream.seek(0)
            if os.name == 'nt':
                msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(stream, fcntl.LOCK_UN)

def invoke(endpoint, request, timeout=5):
    directory, info = endpoint
    with client_lock(directory):
        nonce = uuid.uuid4().hex
        packet = {'protocol': 1, 'token': info['token'], 'id': nonce,
                  'expiresAt': int(time.time() * 1000) + 4000, 'request': request}
        data = json.dumps(packet, ensure_ascii=False, allow_nan=False).encode('utf-8')
        if len(data) > MAX_REQUEST:
            raise ValueError('CLI request exceeds 32768 bytes; no command was submitted')
        temporary = directory / ('request-' + nonce + '.tmp')
        try:
            with temporary.open('xb') as stream:
                stream.write(data)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, directory / 'request.json')
        finally:
            temporary.unlink(missing_ok=True)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                with (directory / 'response.json').open('rb') as stream:
                    data = stream.read(MAX_REPLY + 1)
                if len(data) > MAX_REPLY:
                    raise RuntimeError('CLI reply exceeds byte limit')
                response = strict_json(data)
                if response.get('id') == nonce:
                    result = response.get('result')
                    if (type(response.get('protocol')) is not int or response['protocol'] != 1
                            or not isinstance(result, dict) or type(result.get('ok')) is not bool):
                        raise RuntimeError('Invalid CLI reply schema')
                    return result
            except (FileNotFoundError, PermissionError):
                pass
            time.sleep(.01)
        raise RuntimeError('CLI reply timed out; command may have executed. Inspect state before retrying')

def track(value):
    number = int(value)
    if not 1 <= number <= 20:
        raise argparse.ArgumentTypeError('Track must be 1..20')
    return number - 1

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=default_root())
    parser.add_argument('--instance', help='Instance ID from the instances command')
    commands = parser.add_subparsers(dest='command', required=True)
    commands.add_parser('instances')
    commands.add_parser('status')
    parameter = commands.add_parser('parameter')
    parameter.add_argument('id'); parameter.add_argument('value', type=strict_json)
    loop = commands.add_parser('loop')
    loop.add_argument('action'); loop.add_argument('track', type=track, nargs='?', default=0)
    loop.add_argument('value', type=strict_json, nargs='?', default=0)
    rename = commands.add_parser('rename')
    rename.add_argument('track', type=track); rename.add_argument('name')
    midi = commands.add_parser('midi')
    midi.add_argument('action'); midi.add_argument('--data', type=strict_json, default={})
    for name in ('metronome', 'release-stutter', 'cancel-export'):
        commands.add_parser(name)
    export = commands.add_parser('export')
    export.add_argument('track', type=track); export.add_argument('filename')
    raw = commands.add_parser('request', help='Any existing native JSON operation, with unchanged native field conventions')
    raw.add_argument('json', type=strict_json)
    args = parser.parse_args(argv)
    found = instances(args.root)
    if args.command == 'instances':
        return [{'instance': info['instance'], 'directory': str(path)} for path, info in found]
    if args.instance:
        found = [entry for entry in found if entry[1]['instance'] == args.instance]
    if len(found) != 1:
        raise RuntimeError('Select exactly one running Standalone with --instance; use instances to list IDs')
    endpoint = found[0]
    if args.command == 'status':
        request = {'operation': 'snapshot'}
    elif args.command == 'parameter':
        request = {'operation': 'setParameter', 'id': args.id, 'value': args.value}
    elif args.command in ('loop', 'export'):
        status = invoke(endpoint, {'operation': 'snapshot'})
        if not status.get('ok'):
            return status
        request = {'operation': 'loop' if args.command == 'loop' else 'exportLoop',
                   'track': args.track, 'generation': status['generation']}
        if args.command == 'loop':
            request.update(action=args.action, value=args.value, expiresAt=int(time.time() * 1000) + 4000)
        else:
            request.update(waveformId=status['tracks'][args.track]['waveformId'], filename=args.filename)
    elif args.command == 'rename':
        request = {'operation': 'renameLoop', 'track': args.track, 'name': args.name}
    elif args.command == 'midi':
        if not isinstance(args.data, dict):
            raise ValueError('MIDI --data must be a JSON object')
        request = dict(args.data, operation='midiConfig', action=args.action)
    elif args.command == 'request':
        request = args.json
    else:
        request = {'operation': {'metronome': 'toggleMetronome', 'release-stutter': 'releaseStutter',
                                 'cancel-export': 'cancelExport'}[args.command]}
    if not isinstance(request, dict):
        raise ValueError('Native request must be a JSON object')
    reply = invoke(endpoint, request)
    if args.command == 'loop' and reply.get('ok') and reply.get('queued'):
        deadline = time.monotonic() + 4
        while time.monotonic() < deadline:
            observed = invoke(endpoint, {'operation': 'snapshot'})
            if not observed.get('ok') or observed.get('generation') != request['generation']:
                break
            seen = observed.get('lastRequestId', 0)
            if seen == reply['requestId']:
                if observed.get('rejected'):
                    return dict(reply, ok=False, error='Action is unavailable in the current loop state', applied=False)
                return dict(reply, queued=False, applied=True)
            if seen > reply['requestId']:
                break  # Another controller's later outcome cannot prove ours.
            time.sleep(.01)
        return dict(reply, ok=False, error='Command outcome is unconfirmed; inspect status before retrying')
    return reply

if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    sys.stderr.reconfigure(encoding='utf-8')
    try:
        result = main()
        print(json.dumps(result, ensure_ascii=False, allow_nan=False))
        sys.exit(1 if isinstance(result, dict) and not result.get('ok', False) else 0)
    except Exception as error:
        print(json.dumps({'ok': False, 'error': str(error)}, ensure_ascii=False), file=sys.stderr)
        sys.exit(2)
