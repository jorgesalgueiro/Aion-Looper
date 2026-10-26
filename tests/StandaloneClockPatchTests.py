from pathlib import Path
import importlib.util
import sys

root = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('clock_patch', root / 'tools/patch_standalone_clock.py')
patch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patch)
split_spec = importlib.util.spec_from_file_location('device_splitter', root / 'tools/extract_device_splitter.py')
splitter = importlib.util.module_from_spec(split_spec)
split_spec.loader.exec_module(splitter)
delivery_spec = importlib.util.spec_from_file_location('device_delivery', root / 'tools/patch_device_delivery.py')
delivery = importlib.util.module_from_spec(delivery_spec)
delivery_spec.loader.exec_module(delivery)
delivery_fixture = ('/* retained license */\n' + delivery.START + '\n{\n'
                    + '\n'.join(before for before,after in delivery.REPLACEMENTS[:-1])
                    + '\n};\n' + delivery.END + '\n{\n' + delivery.REPLACEMENTS[-1][0] + '\n};\n')
for fixture_source in (delivery_fixture, delivery_fixture.replace('\n','\r\n')):
    assert delivery.unpatch_manager(delivery.patch_manager(fixture_source)) == fixture_source
for bad in ('',delivery_fixture+delivery_fixture,delivery.patch_manager(delivery_fixture),delivery_fixture.replace('int maximumSize = 0;', 'int maximumSize = 1;')):
    try:
        delivery.patch_manager(bad)
    except RuntimeError:
        pass
    else:
        raise AssertionError('Device delivery hook drift/double injection must fail closed')
for before,after in ((delivery.MODULE_FROM,delivery.MODULE_TO),(delivery.MM_FROM,delivery.MM_TO)):
    for endline in ('\n','\r\n'):
        fixture_source='/* license */'+endline+before+endline
        generated_source=delivery.replace_once(fixture_source,before,after)
        assert generated_source.replace(delivery.newline(fixture_source,after),before)==fixture_source
split_fixture = '/* retained license */\n' + splitter.START + '\n{ originalSplitter(); };\n\n' + splitter.END + '\n{};\n'
for fixture_source in (split_fixture, split_fixture.replace('\n', '\r\n')):
    extracted = splitter.extract(fixture_source)
    assert fixture_source[fixture_source.index(splitter.START):fixture_source.index('//=====')] in extracted
    assert '/* retained license */' in extracted
for bad in ('', split_fixture + split_fixture, split_fixture.replace(splitter.START, 'changed')):
    try:
        splitter.extract(bad)
    except RuntimeError:
        pass
    else:
        raise AssertionError('Splitter extraction drift must fail closed')

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
    device_source = module.parent / 'juce_audio_devices/audio_io/juce_AudioDeviceManager.cpp'
    device_original = device_source.read_bytes().decode('utf-8')
    device_generated = (generated / 'aion_AudioDeviceManager.cpp').read_bytes().decode('utf-8')
    assert device_generated == delivery.patch_manager(device_original)
    assert delivery.unpatch_manager(device_generated) == device_original
    test_header = '#include "core/DeviceDeliveryScope.h"\n'+splitter.extract(device_generated).replace('namespace aion_split_test {','namespace aion_delivery_split_test {',1)
    assert (generated / 'aion_device_delivery_splitter_test.h').read_bytes() == test_header.encode('utf-8')
    for name,before,after in (('cpp',delivery.MODULE_FROM,delivery.MODULE_TO),('mm',delivery.MM_FROM,delivery.MM_TO)):
        original_source=(module.parent / f'juce_audio_devices/juce_audio_devices.{name}').read_bytes().decode('utf-8')
        assert (generated / f'aion_audio_devices.{name}').read_bytes() == delivery.replace_once(original_source,before,after).encode('utf-8')
    assert (generated / 'aion_device_splitter_test.h').read_bytes() == splitter.extract(device_source.read_bytes().decode('utf-8')).encode('utf-8')
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
