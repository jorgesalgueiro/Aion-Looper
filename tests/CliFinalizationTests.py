"""Client admission tests; production mailbox/processor coverage remains separate."""
import importlib.util
from pathlib import Path
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('aion_cli', Path(__file__).resolve().parents[1] / 'tools/aion.py')
api = importlib.util.module_from_spec(spec)
spec.loader.exec_module(api)

for action in ('applyStart', 'quantizeStart', 'nudgeStart', 'speed', 'retrigger', 'normalize', 'undo', 'redo', 'undoGlobal', 'redoGlobal', 'multiply'):
    calls = []
    states = iter(('finalizing', 'finalizing', 'playing'))
    def invoke(endpoint, request):
        calls.append(request.copy())
        if request['operation'] == 'snapshot':
            return {'ok': True, 'generation': 7, 'tracks': [{'state': next(states), 'clearEpoch': 4}]}
        assert request['operation'] == 'loop'
        return {'ok': True}  # Admission policy only, not an audio-application claim.
    with patch.object(api, 'instances', return_value=[('endpoint', {'instance': 'test'})]), patch.object(api, 'invoke', invoke), patch.object(api.time, 'sleep'):
        assert api.main(['loop', action, '1', '-1' if action == 'nudgeStart' else '0.5' if action == 'speed' else '0'])['ok']
    assert [c['operation'] for c in calls] == ['snapshot'] * 3 + ['loop']

# Global history must also wait for a worker on a different track. Its audio
# owner resolves the newest history target; the CLI never guesses that target.
for action in ('undoGlobal', 'redoGlobal'):
    calls = []
    def invoke(endpoint, request):
        calls.append(request.copy())
        if request['operation'] == 'snapshot':
            tracks = [{'state': 'stopped', 'clearEpoch': 4} for _ in range(20)]
            tracks[19]['state'] = 'finalizing' if len(calls) == 1 else 'stopped'
            return {'ok': True, 'generation': 7, 'tracks': tracks}
        return {'ok': True}
    with patch.object(api, 'instances', return_value=[('endpoint', {'instance': 'test'})]), patch.object(api, 'invoke', invoke), patch.object(api.time, 'sleep'):
        assert api.main(['loop', action])['ok']
    assert [c['operation'] for c in calls] == ['snapshot', 'snapshot', 'loop']
    assert calls[-1]['track'] == 0 and calls[-1]['action'] == action

for changed in ('generation', 'clear', 'timeout', 'late-ready'):
    calls = []
    def invoke(endpoint, request):
        calls.append(request.copy())
        return {'ok': True, 'generation': 8 if changed == 'generation' and len(calls) > 1 else 7, 'tracks': [{'state': 'playing' if changed == 'late-ready' and len(calls) > 1 else 'finalizing', 'clearEpoch': 5 if changed == 'clear' and len(calls) > 1 else 4}]}
    with patch.object(api, 'instances', return_value=[('endpoint', {'instance': 'test'})]), patch.object(api, 'invoke', invoke), patch.object(api.time, 'sleep'), patch.object(api.time, 'monotonic', side_effect=(0, 1, 4, 4)):
        reply = api.main(['loop', 'applyStart', '1'])
    assert not reply['ok'] and not reply['applied']
    assert all(c['operation'] == 'snapshot' for c in calls), 'No blind admission/replay after generation change or timeout'
print('PASS: CLI finalization admission for eleven source/history actions, bounded timeout, generation/DEL fencing and no admitted-command replay')
