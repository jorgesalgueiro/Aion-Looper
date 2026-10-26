from pathlib import Path
import importlib.util
import sys

root = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('clock_patch', root / 'tools/patch_standalone_clock.py')
patch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patch)

fixture = '/* retained license */\n' + patch.PLAYER + patch.ANCHOR + '        originalCallback();\n    }\n'
result = patch.patch_header(fixture)
assert result.replace(patch.INJECTION, '', 1).replace(patch.BINDING, '', 1) == fixture
crlf = fixture.replace('\n', '\r\n')
assert patch.patch_header(crlf).replace(patch.injection_for(crlf), '', 1).replace(patch.BINDING.replace('\n','\r\n'), '', 1) == crlf
for bad in ('', fixture + fixture, result):
    try:
        patch.patch_header(bad)
    except RuntimeError:
        pass
    else:
        raise AssertionError('Header drift/double injection must fail closed')
wrapper = '/* retained license */\n' + patch.INCLUDE + '\noriginalApplication();\n'
assert patch.patch_wrapper(wrapper).replace(patch.FACADE, patch.INCLUDE) == wrapper
for bad in ('', wrapper + wrapper, patch.patch_wrapper(wrapper)):
    try:
        patch.patch_wrapper(bad)
    except RuntimeError:
        pass
    else:
        raise AssertionError('Wrapper drift/double injection must fail closed')
if len(sys.argv) > 1:
    module, generated = map(Path, sys.argv[1:])
    original = (module / 'Standalone/juce_StandaloneFilterWindow.h').read_bytes().decode('utf-8')
    output = (generated / 'aion_StandaloneFilterWindow.h').read_bytes().decode('utf-8')
    assert output == patch.patch_header(original)
    binding = patch.BINDING.replace('\n', '\r\n') if '\r\n' in original else patch.BINDING
    assert output.replace(patch.injection_for(original), '', 1).replace(binding, '', 1) == original
    original = (module / 'juce_audio_plugin_client_Standalone.cpp').read_bytes().decode('utf-8')
    output = (generated / 'aion_standalone_clock.cpp').read_bytes().decode('utf-8')
    assert output == patch.patch_wrapper(original)
    assert output.replace(patch.FACADE, patch.INCLUDE) == original
print('PASS: Standalone clock hook preserves original holder/application bytes except scoped entry, lifecycle binding and include; drift fails closed')
