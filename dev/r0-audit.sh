#!/bin/zsh
# Audit active C shape scope and the exact sources used by make build.
set -eu
root=${1:-${0:A:h}/..}
exec python3 -P - "$root" <<'PY'
import hashlib
import json
from pathlib import Path
import re
import sys
root = Path(sys.argv[1]).resolve()
failed = False
def fail(message):
    global failed
    print('R0-AUDIT FAIL: ' + message)
    failed = True
try:
    baseline = json.loads((root / 'dev/C-BASELINE.json').read_text())
    for required in ('_tcc/bin/mech', 'c/kernel/shape.c',
                     'c/kernel/check.c', 'c/wasm/gc_encode.c'):
        if not (root / required).is_file():
            fail('missing build/source file: ' + required)
    manifest = json.loads((root / '_tcc/c-build.json').read_text())
    recorded = manifest['sources']
    actual = {str(p.relative_to(root)): p for p in (root / 'c').rglob('*')
              if p.is_file() and p.suffix in ('.c', '.h')}
    actual.update({name: root / name for name in ('Makefile', 'dev/c-build.py')})
    if set(recorded) != set(actual):
        fail('build source inventory is stale; run make build')
    for name, source in actual.items():
        if hashlib.sha256(source.read_bytes()).hexdigest() != recorded.get(name):
            fail('stale build source: ' + name)
    required_artifacts = {'_tcc/bin/mech', '_tcc/bin/mech_cert'}
    if set(manifest['artifacts']) != required_artifacts:
        fail('production artifact inventory changed')
    for name, digest in manifest['artifacts'].items():
        if name not in required_artifacts or hashlib.sha256((root / name).read_bytes()).hexdigest() != digest:
            fail('stale build artifact: ' + name)
    compiler = manifest['compiler']
    if hashlib.sha256(Path(compiler['path']).read_bytes()).hexdigest() != compiler['sha256']:
        fail('build compiler changed')
    pattern = re.compile(r'\bSHAPE_(?:ARG|COLL|MU|NU|PAR|PI|ZK|FHC|MPC)\b')
    for name, source in actual.items():
        if name.startswith(('c/kernel/', 'c/wasm/')):
            if pattern.search(source.read_text()) and name not in baseline['shape_modules']:
                fail('shape name escaped reviewed scope: ' + name)
    if baseline['review_status'] != 'reviewed':
        fail('C shape-scope migration baseline is unreviewed')
except (OSError, ValueError, KeyError) as error:
    fail(str(error))
if not failed:
    print('R0-AUDIT OK')
sys.exit(1 if failed else 0)
PY
