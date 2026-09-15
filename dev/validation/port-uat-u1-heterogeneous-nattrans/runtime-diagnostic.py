"""Run the exact runtime comparisons with a diagnostic 900-second budget."""
import ast
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
HARNESS = ROOT / 'test/heterogeneous_nattrans_runtime.py'
if sys.argv[1:] == ['--child']:
    tree = ast.parse(HARNESS.read_text(), filename=str(HARNESS))
    deadlines = [node for node in ast.walk(tree)
                 if isinstance(node, ast.Constant) and type(node.value) is int and node.value == 110]
    if len(deadlines) != 2:
        raise SystemExit('Expected the total and compiler budget literals')
    for node in deadlines:
        node.value = 900
    sys.argv = [str(HARNESS)]
    exec(compile(tree, str(HARNESS), 'exec'), {'__file__': str(HARNESS), '__name__': '__main__'})
    raise SystemExit('Runtime harness did not return a verdict')
if len(sys.argv) != 2:
    raise SystemExit('usage: python3 -I runtime-diagnostic.py OUTPUT.json')
output = Path(sys.argv[1]).resolve()
if output.exists():
    raise SystemExit('Refusing to overwrite diagnostic evidence')
inputs = ['test/heterogeneous_nattrans_runtime.py',
          'prelude/cat/category-core.mech', 'prelude/cat/heterogeneous-functor.mech',
          'prelude/cat/heterogeneous-nattrans.mech',
          'test/fixtures/prelude/heterogeneous-nattrans-runtime.mech',
          '_build/default/test/prelude_runtime.exe']
sources = {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in inputs}
started = time.monotonic()
result = subprocess.run(['timeout', '930', sys.executable, '-I', __file__, '--child'],
                        cwd=ROOT, capture_output=True, text=True)
expected = 'PRELUDE-HETEROGENEOUS-NATTRANS-RUNTIME OK cases=5 hosts=3 mutation=1\n'
row = {'name': 'runtime-diagnostic', 'outer_timeout_seconds': 930,
       'internal_budget_seconds': 900, 'host_timeout_seconds': 10,
       'elapsed_ms': round((time.monotonic() - started) * 1000),
       'exit_code': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr,
       'passed': result.returncode == 0 and not result.stderr and result.stdout == expected}
for path, expected_hash in sources.items():
    if hashlib.sha256((ROOT / path).read_bytes()).hexdigest() != expected_hash:
        raise SystemExit('Runtime input changed during the diagnostic: ' + path)
output.write_text(json.dumps({'version': 1, 'sources': sources, 'check': row,
                             'runner_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                             'transformation': 'Only the two integer budget constants change from 110 to 900 in memory.'}, indent=2) + '\n')
print(f'{row["name"]}: passed={row["passed"]} exit={row["exit_code"]} elapsed_ms={row["elapsed_ms"]}', flush=True)
raise SystemExit(0 if row['passed'] else 1)
