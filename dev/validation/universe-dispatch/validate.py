import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
EVIDENCE = ROOT / 'dev/validation/universe-dispatch'
TEST = ROOT / '_tcc'
parser = argparse.ArgumentParser()
parser.add_argument('--scope', choices=['kernel', 'frontend'], action='append')
parser.add_argument('--evidence', type=Path, default=EVIDENCE, help='directory for the per-scope JSON results')
args = parser.parse_args()
evidence = args.evidence.resolve()
evidence.mkdir(parents=True, exist_ok=True)


def unit_shard(row):
    if 'shard' in row:
        shard = row['shard']
        if not isinstance(shard, str) or not re.fullmatch(r'[a-z0-9-]+', shard):
            raise RuntimeError(f'invalid test shard: {shard!r}')
        return shard
    mode = row['mode']
    if mode.startswith(('kernel-', 'levels-')):
        return 'kernel'
    if mode.startswith('surface-'):
        return 'surface'
    if mode.startswith('wasm-'):
        return 'wasm'
    raise RuntimeError(f'no test shard for units: {mode}')


manifest = json.loads((ROOT / 'dev/cases/test-manifest.json').read_text())
subprocess.run(['make', '-C', str(ROOT), 'c-build'], cwd=ROOT, check=True)
results = []
for scope in args.scope or ['kernel', 'frontend']:
    shard = 'surface' if scope == 'frontend' else scope
    rows = []
    for row in manifest['units']:
        if unit_shard(row) != shard:
            continue
        standalone = TEST / row['mode'].replace('-', '_')
        command = [str(standalone)] if standalone.exists() else [str(TEST / 'runner'), 'unit', row['mode']]
        run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=120)
        passed = run.returncode == 0 and not run.stderr
        record = dict(mode=row['mode'], passed=passed, exit_code=run.returncode, stdout=run.stdout, stderr=run.stderr)
        rows.append(record)
        print(row['mode'], 'PASS' if passed else 'FAIL', flush=True)
    if not rows:
        raise ValueError('unit scope selected no groups: ' + scope)
    (evidence / (scope + '.json')).write_text(json.dumps(dict(results=rows), indent=2) + '\n')
    results.extend(rows)
    print(scope, 'groups', len(rows), 'passed', sum(r['passed'] for r in rows), flush=True)
sys.exit(0 if results and all(r['passed'] for r in results) else 1)
