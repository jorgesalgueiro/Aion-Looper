"""Fail-closed generation/integrity gate for the actual pinned wrapper source."""
from pathlib import Path
import hashlib
import subprocess
import sys
import tempfile

source=Path(sys.argv[1]);built=Path(sys.argv[2]);root=Path(__file__).parents[1]
runner=root/'tools/patch_vst3_programs.py';before=source.read_bytes()
with tempfile.TemporaryDirectory() as tmp:
    output=Path(tmp)/'wrapper.cpp'
    args=[sys.executable,str(runner),str(source),str(output)]
    subprocess.run(args,check=True,capture_output=True)
    generated=output.read_bytes();stamp=output.stat().st_mtime_ns
    assert generated==built.read_bytes(),'Build does not use the current generated wrapper'
    assert b'Copyright (c) Raw Material Software Limited' in generated
    assert b'aion::vst3::decode' in generated and b'aionProgramBudget.take()' in generated
    subprocess.run(args,check=True,capture_output=True)
    assert output.stat().st_mtime_ns==stamp,'Idempotent configure must not rebuild wrapper'
    # Reject arbitrary drift even when critical replacement anchors still match.
    bad=Path(tmp)/'bad.cpp';bad.write_bytes(before+b'\n// drift\n')
    result=subprocess.run([sys.executable,str(runner),str(bad),str(output)],capture_output=True,text=True)
    assert result.returncode!=0 and 'Unpinned or modified' in result.stderr
    optimized=subprocess.run([sys.executable,'-O',str(runner),str(bad),str(output)],capture_output=True,text=True)
    assert optimized.returncode!=0 and 'Unpinned or modified' in optimized.stderr
    assert output.read_bytes()==generated,'Rejected source must not overwrite last good output'
    assert source.read_bytes()==before,'Dependency checkout was modified'
print('PASS: exact pinned wrapper, generated build identity, unchanged upstream/license, idempotence and fail-closed drift; upstream SHA256='+hashlib.sha256(before).hexdigest())
