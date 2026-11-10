"""External CLI uses the production processor's chain owner."""
import json
from pathlib import Path
import subprocess
import sys
root, instance = sys.argv[1:3]
client = Path(__file__).resolve().parent.parent / 'tools/aion.py'
def cli(*args):
    result = subprocess.run([sys.executable, str(client), '--root', root, '--instance', instance, *args], capture_output=True, text=True, timeout=8)
    assert result.returncode == 0, (args, result.stdout, result.stderr)
    return json.loads(result.stdout)
assert cli('chain', '20', 'set', ' c q a c ! ')['applied']
assert cli('chain', '20', 'get')['signalChain'] == 'CQA'
assert cli('status')['tracks'][19]['signalChain'] == 'CQA'
assert cli('chain', '1', 'get')['signalChain'] == 'QCATFODBVKZG'
assert cli('chain', '20', 'set', 'A')['applied']
assert cli('chain', '20', 'get')['signalChain'] == 'A'
assert cli('chain', '20', 'set', '')['applied']
assert cli('chain', '20', 'get')['signalChain'] == 'QCATFODBVKZG'
assert cli('chain', '20', 'set', 'CQ')['applied']
assert cli('chain', '20', 'reset')['applied']
assert cli('chain', '20', 'get')['signalChain'] == 'QCATFODBVKZG'
print('PASS external chain CLI: normalization, expanded track, shared state, omission and reset')
