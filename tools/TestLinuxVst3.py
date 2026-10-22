#!/usr/bin/env python3
"""Opt-in Linux VST3 pluginval gate, including editor tests; no audio device."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import signal
import subprocess
import sys
import time
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / '.toolchain/pluginval/1.0.4-linux'
URL = 'https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Linux.zip'
ARCHIVE_HASH = 'c01c49d8063965c4c2dea8324468336768f5c9139e0b1caebde14c2400b55352'
EXECUTABLE_HASH = 'd3d342ea92293b0699a5c983937211e3faf2b0f38145b31cb8d78424a25e10ba'
OPTIONS = ('STRICTNESS_LEVEL', 'RANDOM_SEED', 'TIMEOUT_MS', 'VERBOSE',
           'SKIP_GUI_TESTS', 'DATA_FILE', 'OUTPUT_DIR', 'OUTPUT_FILENAME',
           'REPEAT', 'RANDOMISE', 'SAMPLE_RATES', 'BLOCK_SIZES',
           'VST3VALIDATOR', 'DISABLED_TESTS')
REQUIRED = ('Editor', 'Open editor whilst processing', 'Audio processing',
            'Plugin state', 'Automation', 'Editor Automation',
            'Automatable Parameters', 'Restoring default layout',
            'Non-releasing audio processing', 'Plugin state restoration',
            'Fuzz parameters', 'Parameters', 'Background thread state',
            'Parameter thread safety')
RATES, BLOCKS = (44100, 48000, 96000), (64, 128, 256, 512, 1024)


def sha(path):
    with path.open('rb') as stream:
        digest = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
        return digest.hexdigest()


def main(plugin_format='VST3'):
    if plugin_format not in ('VST3', 'LV2'):
        raise ValueError('Unsupported validation format')
    parser = argparse.ArgumentParser(description=f'Opt-in Linux {plugin_format} pluginval gate, including editor tests; no audio device.')
    parser.add_argument('--validator', type=Path, default=TOOL / 'pluginval')
    parser.add_argument('--plugin', type=Path, default=ROOT / f'build/linux/AionLooper_artefacts/Debug/{plugin_format}/Aion-Looper.{plugin_format.lower()}')
    parser.add_argument('--output-directory', type=Path, default=ROOT / ('build/linux-pluginval' if plugin_format == 'VST3' else 'build/linux-lv2-pluginval'))
    parser.add_argument('--download-tool', action='store_true')
    args = parser.parse_args()
    if platform.system() != 'Linux' or platform.machine() not in ('x86_64', 'AMD64'):
        parser.error('This pinned validator is for Linux x86_64 only.')
    if not os.environ.get('DISPLAY'):
        parser.error('Editor tests require an X display (for example WSLg or Xvfb).')
    if args.download_tool:
        if args.validator != TOOL / 'pluginval':
            parser.error('Use the default validator with --download-tool.')
        TOOL.mkdir(parents=True, exist_ok=True)
        archive = TOOL / 'pluginval_Linux.zip'
        if not archive.exists():
            with urllib.request.urlopen(URL, timeout=60) as response:
                archive.write_bytes(response.read())
        if sha(archive) != ARCHIVE_HASH:
            raise RuntimeError('Official pluginval Linux archive digest mismatch')
        with zipfile.ZipFile(archive) as packed:
            if packed.namelist() != ['pluginval']:
                raise RuntimeError('Unexpected pluginval archive layout')
            args.validator.write_bytes(packed.read('pluginval'))
        args.validator.chmod(0o755)
    validator, plugin = args.validator.resolve(strict=True), args.plugin.resolve(strict=True)
    if sha(validator) != EXECUTABLE_HASH:
        raise RuntimeError('Use the pinned official Linux pluginval 1.0.4 executable')
    if plugin.suffix != '.' + plugin_format.lower():
        parser.error('Expected a ' + plugin_format + ' bundle')
    binary = plugin / ('Contents/x86_64-linux/Aion-Looper.so' if plugin_format == 'VST3' else 'libAion-Looper.so')
    binary_hash = sha(binary)
    # An LV2 module is discovered through its RDF files, not the .so alone.
    bundle_files = [binary] + ([plugin / name for name in ('manifest.ttl', 'dsp.ttl', 'ui.ttl')] if plugin_format == 'LV2' else [])
    bundle_hashes = {str(path.relative_to(plugin)): sha(path) for path in bundle_files}
    run_dir = args.output_directory.resolve() / (time.strftime('%Y%m%dT%H%M%SZ', time.gmtime()) + '-' + os.urandom(4).hex())
    run_dir.mkdir(parents=True)
    command = [str(validator), '--validate-in-process', '--strictness-level', '9',
               '--random-seed', '1337', '--repeat', '1', '--timeout-ms', '30000',
               '--sample-rates', ','.join(map(str, RATES)),
               '--block-sizes', ','.join(map(str, BLOCKS)),
               '--output-dir', str(run_dir), '--validate', str(plugin)]
    result = dict(schemaVersion=1, platform=platform.platform(),
                  startedUtc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                  sourceCommit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  trackedChanges=subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=no'], cwd=ROOT, text=True).splitlines(),
                  harnessSha256=sha(Path(__file__)), format=plugin_format,
                  entryPoint=str(Path(sys.argv[0]).resolve()),
                  entryPointSha256=sha(Path(sys.argv[0]).resolve()), validatorVersion='1.0.4',
                  validatorSha256=EXECUTABLE_HASH, archiveSha256=ARCHIVE_HASH,
                  pluginPath=str(plugin), binarySha256=binary_hash, binarySha256After=None,
                  bundleSha256=bundle_hashes, bundleSha256After=None,
                  strictness=9, seed=1337, sampleRates=RATES, blockSizes=BLOCKS,
                  command=command, guiTestsRequired=True, requiredTests=REQUIRED,
                  missingTests=[], missingMatrix=[], allocationTestExecuted=False,
                  externalSteinbergValidatorExecuted=False, hardwareTested=False,
                  exitCode=None, passed=False, elapsedSeconds=None, error=None)
    start = time.monotonic()
    try:
        # Child-only environment; caller preferences cannot silently skip coverage.
        env = {key: value for key, value in os.environ.items() if key not in OPTIONS}
        with (run_dir / 'stdout.log').open('wb') as out, (run_dir / 'stderr.log').open('wb') as err:
            process = subprocess.Popen(command, cwd=run_dir, env=env, stdin=subprocess.DEVNULL,
                                       stdout=out, stderr=err, start_new_session=True)
            try:
                result['exitCode'] = process.wait(timeout=240)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                raise RuntimeError('Validator timed out; its process group was terminated')
        text = (run_dir / 'stdout.log').read_text(encoding='utf-8', errors='replace')
        stderr = (run_dir / 'stderr.log').read_text(encoding='utf-8', errors='replace')
        completed = set(re.findall(r'^Completed tests in pluginval / (.+)\r?$', text, re.MULTILINE))
        result['missingTests'] = sorted(set(REQUIRED) - completed)
        result['missingMatrix'] = [[rate, block] for rate in RATES for block in BLOCKS
                                   if f'Testing with sample rate [{rate}] and block size [{block}]' not in text]
        result['allocationTestExecuted'] = 'Allocations during process' in completed
        result['externalSteinbergValidatorExecuted'] = ('vst3 validator' in completed and 'Skipping vst3 validator' not in text)
        result['binarySha256After'] = sha(binary)
        result['bundleSha256After'] = {str(path.relative_to(plugin)): sha(path) for path in bundle_files}
        result['passed'] = (result['exitCode'] == 0 and re.search(r'^SUCCESS\r?$', text, re.MULTILINE) is not None
                            and re.search(r'^Num plugins found: 1\r?$', text, re.MULTILINE) is not None
                            and not result['missingTests'] and not result['missingMatrix']
                            and 'JUCE Assertion failure' not in text + stderr
                            and 'Leaked objects detected' not in text + stderr
                            and result['binarySha256After'] == binary_hash
                            and result['bundleSha256After'] == bundle_hashes
                            and re.search(r'^Testing plugin: ' + plugin_format + r'-Aion-Looper-', text, re.MULTILINE) is not None)
        if not result['passed']:
            raise RuntimeError('Linux ' + plugin_format + ' validation failed or incomplete; see ' + str(run_dir))
        print('PASS: actual Linux ' + plugin_format + ', pluginval 1.0.4 level 9, editor tests enabled, seed 1337')
        print('Evidence:', run_dir / 'result.json')
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        result['completedUtc'] = time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())
        result['elapsedSeconds'] = time.monotonic() - start
        (run_dir / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
