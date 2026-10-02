import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--bend', type=Path, required=True)
    args = parser.parse_args()
    directory = Path(__file__).resolve().parent
    root = directory.parents[2]
    output = root / '_bend2/universe-dispatch'
    output.mkdir(parents=True, exist_ok=True)
    version = subprocess.run([args.bend, 'version'], check=True, capture_output=True, text=True).stdout.strip()
    if not re.search(r'(?<![\d.])2\.0\.27(?![\d.])', version):
        raise ValueError('expected Bend 2.0.27')
    build = subprocess.run([args.bend, directory / 'polls.bend', '-o', output / 'polls.js'], capture_output=True, text=True, timeout=300)
    if build.returncode:
        raise RuntimeError(build.stderr)
    run = subprocess.run(['node', '--stack-size=16384', output / 'polls.js'], check=True, capture_output=True, text=True, timeout=60)
    measurements = dict(re.findall(r'^(previous|universe|structural) remaining=(\d+)$', run.stdout, re.M))
    if len(measurements) != 3 or run.stderr:
        raise ValueError('incomplete conversion measurements')
    polls = {name: 100 - int(remaining) for name, remaining in measurements.items()}
    if not polls['previous'] == polls['universe'] == polls['structural']:
        raise ValueError('universe dispatch changed the comparison poll budget')
    sources = [root / 'bend2/kernel/check_engine.bend', root / 'bend2/tests/kernel_conversion_budget.bend', directory / 'polls.bend']
    record = dict(version=1, compiler=dict(version=version, sha256=hashlib.sha256(args.bend.read_bytes()).hexdigest()),
                  sources={str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
                  polls=polls, stdout=run.stdout, stderr=run.stderr, passed=True)
    (directory / 'polls.json').write_text(json.dumps(record, indent=2) + '\n')
    print(json.dumps(dict(passed=True, polls=polls)))


if __name__ == '__main__':
    main()
