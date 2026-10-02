import hashlib
import json
from pathlib import Path
import sys

DIRECTORY = Path(__file__).resolve().parent
ROOT = DIRECTORY.parents[2]
EXTENSIONS = {'.bend', '.part', '.c', '.js', '.mjs', '.mech', '.json', '.py'}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def bundle():
    paths = [p for d in ['bend2', 'prelude', 'test/fixtures'] for p in (ROOT / d).rglob('*') if p.is_file() and p.suffix in EXTENSIONS]
    paths.extend([ROOT / 'dev/bend2-build.py', ROOT / 'dev/bend2/test-manifest.json'])
    hashes = {str(p.relative_to(ROOT)): digest(p) for p in sorted(set(paths))}
    return dict(files=len(hashes), sha256=hashlib.sha256(json.dumps(hashes, sort_keys=True).encode()).hexdigest())


def main():
    measured = bundle()
    recorded = json.loads((DIRECTORY / 'validation.json').read_text())['inputs']
    passed = measured == recorded
    print(json.dumps(dict(passed=passed, inputs=measured)))
    sys.exit(0 if passed else 1)


if __name__ == '__main__':
    main()
