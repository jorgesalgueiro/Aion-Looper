#!/usr/bin/env python3
"""Opt-in official Ubuntu CLAP validation; no package, installation or audio device."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import signal
import subprocess
import tarfile
import time
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / '.toolchain/clap-validator/0.4.1-linux'
ARCHIVE_HASH = '49edadcfb407ea0dd946ce418300e853fbd2660fa4b0d00e4f19ff8eef24ad90'
EXECUTABLE_HASH = 'd9a78bfe781719de450bc5013141496eb13e1d9a538c67de1b8d325628670dbf'
URL = 'https://github.com/free-audio/clap-validator/releases/download/0.4.1/clap-validator-0.4.1-127-g152b982-ubuntu-22.04.zip'
EXPECTED = set("""plugin-library:query-nonexistent-factory
plugin-library:create-id-with-trailing-garbage
plugin-library:scan-rtld-now
plugin-library:scan-time
plugin-library:preset-discovery-crawl
plugin-library:preset-discovery-descriptor-consistency
plugin-library:preset-discovery-load
plugin-instance:descriptor-consistency
plugin-instance:features-categories
plugin-instance:features-duplicates
plugin-instance:process-audio-basic-out-of-place
plugin-instance:process-audio-basic-in-place
plugin-instance:process-audio-double-out-of-place
plugin-instance:process-audio-double-in-place
plugin-instance:process-audio-denormals
plugin-instance:process-sleep-constant-mask
plugin-instance:process-sleep-process-status
plugin-instance:process-note-out-of-place-basic
plugin-instance:process-note-inconsistent
plugin-instance:process-note-wildcard
plugin-instance:process-varying-sample-rates
plugin-instance:process-varying-block-sizes
plugin-instance:process-random-block-sizes
plugin-instance:process-reset-reactivate
plugin-instance:layout-audio-ports-activation
plugin-instance:layout-audio-ports-config
plugin-instance:layout-configurable-audio-ports
plugin-instance:param-set-events
plugin-instance:param-set-no-cookies
plugin-instance:param-set-wrong-namespace
plugin-instance:param-fuzz-basic
plugin-instance:param-fuzz-bounds
plugin-instance:param-fuzz-sample-accurate
plugin-instance:param-fuzz-modulation
plugin-instance:param-conversions
plugin-instance:param-default-values
plugin-instance:state-invalid-empty
plugin-instance:state-invalid-random
plugin-instance:state-reproducibility-basic
plugin-instance:state-reproducibility-binary
plugin-instance:state-reproducibility-buffered
plugin-instance:transport-null
plugin-instance:transport-fuzz
plugin-instance:transport-fuzz-sample-accurate""".splitlines())
# Unix RTLD_NOW must execute here; Windows's platform skip is not allowed.
SKIPS = {'preset-discovery-crawl', 'preset-discovery-descriptor-consistency',
         'preset-discovery-load', 'process-audio-double-out-of-place',
         'process-audio-double-in-place', 'process-note-wildcard',
         'layout-audio-ports-activation', 'layout-audio-ports-config',
         'layout-configurable-audio-ports'}
WARNING = 'The plugin loaded random bytes successfully, which is unexpected, but the plugin did not crash.'


def sha(path):
    with path.open('rb') as stream:
        return hashlib.sha256(stream.read()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--validator', type=Path, default=TOOL / 'clap-validator')
    parser.add_argument('--plugin', type=Path, default=ROOT / 'build/linux/AionLooper_artefacts/Debug/CLAP/Aion-Looper.clap')
    parser.add_argument('--output-directory', type=Path, default=ROOT / 'build/linux-clap-validator')
    parser.add_argument('--download-tool', action='store_true')
    args = parser.parse_args()
    if platform.system() != 'Linux' or platform.machine() not in ('x86_64', 'AMD64'):
        parser.error('This pinned tool is for Linux x86_64 only.')
    if args.download_tool:
        if args.validator != TOOL / 'clap-validator':
            parser.error('Use the default validator path with --download-tool.')
        TOOL.mkdir(parents=True, exist_ok=True)
        archive = TOOL / 'clap-validator-ubuntu.zip'
        if not archive.exists():
            with urllib.request.urlopen(URL, timeout=60) as response:
                archive.write_bytes(response.read())
        if sha(archive) != ARCHIVE_HASH:
            raise RuntimeError('Official CLAP validator archive digest mismatch')
        # The official zip wraps a tarball with a historical 0.3.2 filename.
        # Read only the expected binary; never extract arbitrary archive paths.
        with zipfile.ZipFile(archive) as zipped:
            members = [name for name in zipped.namelist() if name.endswith('.tar.gz')]
            if len(members) != 1:
                raise RuntimeError('Unexpected validator archive layout')
            with tarfile.open(fileobj=io.BytesIO(zipped.read(members[0])), mode='r:gz') as packed:
                member = packed.getmember('clap-validator')
                if not member.isfile():
                    raise RuntimeError('Validator must be a regular archive member')
                with packed.extractfile(member) as binary:
                    args.validator.write_bytes(binary.read())
        args.validator.chmod(0o755)
    validator, plugin = args.validator.resolve(strict=True), args.plugin.resolve(strict=True)
    if sha(validator) != EXECUTABLE_HASH:
        raise RuntimeError('Use the pinned official Ubuntu CLAP validator 0.4.1')
    run_dir = args.output_directory.resolve() / (time.strftime('%Y%m%dT%H%M%SZ', time.gmtime()) + '-' + os.urandom(4).hex())
    run_dir.mkdir(parents=True)
    for directory in [run_dir, *run_dir.parents]:
        if (directory / 'clap-validator.toml').exists():
            raise RuntimeError('Remove external test filtering configuration: ' + str(directory))
    binary_hash = sha(plugin)
    command = [str(validator), 'validate', '--json', '--hide-output', '--jobs', '1', str(plugin)]
    result = dict(schemaVersion=1, platform=platform.platform(), startedUtc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()), sourceCommit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  trackedChanges=subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=no'], cwd=ROOT, text=True).splitlines(),
                  validatorVersion='0.4.1', validatorSha256=EXECUTABLE_HASH, archiveSha256=ARCHIVE_HASH,
                  harnessSha256=sha(Path(__file__)), pluginPath=str(plugin), binarySha256=binary_hash,
                  binarySha256After=None, command=command, guiTested=False, allocationTested=False,
                  hardwareTested=False, passed=False, successes=[], skipped=[], warnings=[], failures=[],
                  missing=[], exitCode=None, elapsedSeconds=None, error=None)
    start = time.monotonic()
    try:
        env = dict(os.environ, CLAP_VALIDATOR_VERBOSITY='info')
        with (run_dir / 'stdout.json').open('wb') as out, (run_dir / 'stderr.log').open('wb') as err:
            process = subprocess.Popen(command, cwd=run_dir, env=env, stdin=subprocess.DEVNULL,
                                       stdout=out, stderr=err, start_new_session=True)
            try:
                result['exitCode'] = process.wait(timeout=240)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                raise RuntimeError('Validator timed out; its process group was terminated')
        report = json.loads((run_dir / 'stdout.json').read_text(encoding='utf-8'))
        seen = set()
        for entry in report['results']:
            ((kind, test),) = entry['test'].items()
            key = kind + ':' + test['test']
            if key not in EXPECTED or key in seen or test['path'] != str(plugin):
                raise RuntimeError('Unexpected/duplicate test or wrong plugin path: ' + key)
            if kind == 'plugin-instance' and test['plugin_id'] != 'com.jorgesalgueiro.aionlooper':
                raise RuntimeError('Unexpected plugin ID')
            seen.add(key)
            code, details = entry['status']['code'], entry['status']['details']
            if code == 'success':
                result['successes'].append(key)
            elif code == 'skipped' and test['test'] in SKIPS:
                result['skipped'].append(dict(test=key, reason=details))
            elif code == 'warning' and test['test'] == 'state-invalid-random' and details == WARNING:
                result['warnings'].append(dict(test=key, reason=details))
            else:
                result['failures'].append(entry)
        result['missing'] = sorted(EXPECTED - seen)
        result['binarySha256After'] = sha(plugin)
        result['passed'] = (result['exitCode'] == 0 and not result['failures'] and not result['missing']
                            and result['binarySha256After'] == binary_hash)
        if not result['passed']:
            raise RuntimeError('CLAP validation failed or incomplete; see ' + str(run_dir))
        print('PASS: Linux CLAP: {} passed, {} unsupported skips, {} retained warnings; all 44 tests accounted for'.format(
            len(result['successes']), len(result['skipped']), len(result['warnings'])))
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
