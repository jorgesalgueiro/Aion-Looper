"""Verify original runtime bytes and generate a reproducible embedded archive."""
import argparse, hashlib, json, pathlib, zipfile
p = argparse.ArgumentParser()
p.add_argument('--source', type=pathlib.Path, required=True)
p.add_argument('--manifest', type=pathlib.Path, required=True)
p.add_argument('--output', type=pathlib.Path, required=True)
a = p.parse_args()
files = json.loads(a.manifest.read_text(encoding='utf-8'))['files']
actual = {f.relative_to(a.source).as_posix() for f in a.source.rglob('*') if f.is_file()}
if actual != set(files): raise SystemExit('UI file set differs from preserved reference manifest')
a.output.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(a.output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for name, expected in sorted(files.items()):
        path = pathlib.PurePosixPath(name)
        if path.is_absolute() or '..' in path.parts: raise SystemExit('Unsafe UI archive path')
        data = (a.source / name).read_bytes()
        if hashlib.sha256(data).hexdigest() != expected: raise SystemExit(f'UI reference changed: {name}')
        info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o100644 << 16
        z.writestr(info, data)
print(f'Preserved {len(files)} reference files, verified SHA256, deterministic archive')
