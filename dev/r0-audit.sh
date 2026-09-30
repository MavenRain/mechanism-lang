#!/bin/zsh
# Audit active Bend shape scope and the exact sources used by make build.
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
    baseline = json.loads((root / 'dev/BEND2-BASELINE.json').read_text())
    for required in ('_bend2/bin/mech.exe', 'bend2/kernel/shape.bend',
                     'bend2/kernel/check.bend', 'bend2/wasm/gc_encode.bend'):
        if not (root / required).is_file():
            fail('missing build/source file: ' + required)
    manifest = root / '_bend2/bend2-sources.sha256'
    recorded = {}
    for line in manifest.read_text().splitlines():
        digest, name = line.split(None, 1)
        name = name.lstrip(' *')
        if name in recorded:
            fail('duplicate build source record: ' + name)
        recorded[name] = digest
    actual = {str(p.relative_to(root)): p for p in (root / 'bend2').rglob('*')
              if p.is_file() and p.suffix in ('.bend', '.c', '.js')
              and 'tests' not in p.relative_to(root).parts}
    if set(recorded) != set(actual):
        fail('build source inventory is stale; run make build')
    for name, source in actual.items():
        if hashlib.sha256(source.read_bytes()).hexdigest() != recorded.get(name):
            fail('stale build source: ' + name)
    pattern = re.compile(r'SColl|SMu|SNu|SPar|SPi|SZk|SFhc|SMpc')
    for name, source in actual.items():
        if source.suffix == '.bend' and name.startswith(('bend2/kernel/', 'bend2/wasm/')):
            if pattern.search(source.read_text()) and name not in baseline['shape_modules']:
                fail('shape name escaped reviewed scope: ' + name)
    if baseline['review_status'] != 'reviewed':
        fail('Bend shape-scope migration baseline is unreviewed')
except (OSError, ValueError, KeyError) as error:
    fail(str(error))
if not failed:
    print('R0-AUDIT OK')
sys.exit(1 if failed else 0)
PY
