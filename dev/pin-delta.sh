#!/bin/zsh
# Compare native Bend sources with the explicitly reviewed migration inventory.
# The origin PIN remains a separate immutable corpus/provenance gate.
set -eu
exec python3 -P - "${0:A:h}/.." <<'PY'
import hashlib
import json
from pathlib import Path
import sys
root = Path(sys.argv[1]).resolve()
failed = False
def fail(message):
    global failed
    print('PIN-DELTA ' + message + ' FAIL')
    failed = True
try:
    baseline = json.loads((root / 'dev/BEND2-BASELINE.json').read_text())
    if baseline['origin_pin'] != (root / 'PIN').read_text().strip():
        fail('origin PIN changed')
    expected = baseline['sources']
    actual = {str(p.relative_to(root)): p for p in (root / 'bend2').rglob('*')
              if p.is_file() and p.suffix in ('.bend', '.c', '.js')
              and 'tests' not in p.relative_to(root).parts}
    for name in sorted(set(expected) | set(actual)):
        if name not in expected:
            fail(name + ' UNRECORDED')
        elif name not in actual:
            fail(name + ' MISSING')
        else:
            contents = actual[name].read_bytes()
            digest = hashlib.sha256(contents).hexdigest()
            lines = len(contents.splitlines())
            if digest != expected[name]['sha256'] or lines != expected[name]['lines']:
                fail(f'{name} CHANGED lines={lines} recorded={expected[name]["lines"]}')
    print(f'PIN-DELTA language=bend files={len(actual)} review={baseline["review_status"]}')
    if baseline['review_status'] != 'reviewed':
        fail('migration baseline UNREVIEWED')
except (OSError, ValueError, KeyError) as error:
    fail(str(error))
if not failed:
    print('PIN-DELTA OK')
sys.exit(1 if failed else 0)
PY
