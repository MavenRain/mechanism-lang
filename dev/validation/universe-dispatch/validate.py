import argparse
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
EVIDENCE = ROOT / 'dev/validation/universe-dispatch'
parser = argparse.ArgumentParser()
parser.add_argument('--bend', type=Path, required=True)
parser.add_argument('--scope', choices=['kernel', 'frontend'], action='append')
args = parser.parse_args()
BEND = args.bend.resolve()
env = dict(os.environ, BEND=str(BEND))
EVIDENCE.mkdir(parents=True, exist_ok=True)
spec = importlib.util.spec_from_file_location('build', ROOT / 'dev/bend2-build.py')
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)
manifest = json.loads((ROOT / 'dev/bend2/test-manifest.json').read_text())
results = []
for scope in args.scope or ['kernel', 'frontend']:
    shard = 'surface' if scope == 'frontend' else scope
    rows = []
    for row in manifest['units']:
        if build.test_shard('units', row) != shard:
            continue
        subprocess.run([sys.executable, '-P', ROOT / 'dev/bend2-build.py', '--target', 'tests', '--test-mode', 'unit-' + row['mode']], cwd=ROOT, env=env, check=True)
        command = [str(ROOT / '_bend2/test' / ('unit_' + row['mode'] + '.exe'))]
        run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=120)
        passed = run.returncode == 0 and not run.stderr
        record = dict(mode=row['mode'], passed=passed, exit_code=run.returncode, stdout=run.stdout, stderr=run.stderr)
        rows.append(record)
        print(row['mode'], 'PASS' if passed else 'FAIL', flush=True)
    if not rows:
        raise ValueError('unit scope selected no groups: ' + scope)
    (EVIDENCE / (scope + '.json')).write_text(json.dumps(dict(results=rows), indent=2) + '\n')
    results.extend(rows)
    print(scope, 'groups', len(rows), 'passed', sum(r['passed'] for r in rows), flush=True)
sys.exit(0 if results and all(r['passed'] for r in results) else 1)
