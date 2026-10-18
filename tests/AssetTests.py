import hashlib, json, pathlib, shutil, subprocess, sys, tempfile, zipfile
root = pathlib.Path(__file__).resolve().parents[1]
manifest = root / 'resources/reference-manifest.json'
source = root / 'resources/ui'
files = json.loads(manifest.read_text(encoding='utf-8'))['files']
with tempfile.TemporaryDirectory(prefix='aion-ui-test-') as folder:
    temp = pathlib.Path(folder)
    def pack(src, dest):
        return subprocess.run([sys.executable, str(root/'tools/pack_ui.py'), '--source', str(src), '--manifest', str(manifest), '--output', str(dest)], capture_output=True)
    first, second = temp/'first.zip', temp/'second.zip'
    assert pack(source, first).returncode == 0
    assert pack(source, second).returncode == 0
    assert first.read_bytes() == second.read_bytes(), 'archive reproducibility'
    with zipfile.ZipFile(first) as archive:
        assert set(archive.namelist()) == set(files)
        for name, expected in files.items():
            assert hashlib.sha256(archive.read(name)).hexdigest() == expected
            reference = root/'.reference/Ascii-Looper'/name
            if reference.exists(): assert reference.read_bytes() == archive.read(name), name
    altered = temp/'altered'
    shutil.copytree(source, altered)
    (altered/'main.js').write_bytes(b'changed')
    assert pack(altered, temp/'bad.zip').returncode != 0, 'modified UI must reject'
    shutil.copyfile(source/'main.js', altered/'main.js')
    (altered/'unexpected.js').write_bytes(b'new script')
    assert pack(altered, temp/'bad.zip').returncode != 0, 'unknown scripts must reject'
print('PASS: original UI byte identity, deterministic embedding and tamper rejection')
