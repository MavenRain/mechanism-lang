#!/usr/bin/env python3
"""Replay frozen reference observations without a reference compiler."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

BUILD = ROOT / 'build' / 'bend2-parser'

CASES = ROOT / 'dev' / 'bend2' / 'parser-cases.json'

def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, capture_output=True, text=True, timeout=30)

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--native', type=Path, default=ROOT / '_bend2/test/surface_driver.exe')
    args = parser.parse_args()
    evidence = json.loads(CASES.read_text())
    failures = []
    for index, case in enumerate(evidence['cases']):
        if 'path' in case:
            path = ROOT / case['path']
            if sha(path) != case['sha256']:
                raise RuntimeError(f'stale oracle evidence for {path}')
            source = path.read_text()
        else:
            source = case['source']
        result = run([str(args.native.resolve()), source])
        actual = dict(code=result.returncode, stdout=result.stdout, stderr=result.stderr)
        expected = {key: case[key] for key in actual}
        if actual != expected:
            failures.append(dict(index=index, path=case.get('path'), source=source, expected=expected, actual=actual))
    BUILD.mkdir(parents=True, exist_ok=True)
    report = BUILD / 'report.json'
    report.write_text(json.dumps(dict(cases=len(evidence['cases']), failures=failures), ensure_ascii=True, indent=2) + '\n')
    acceptance = sum((f['actual']['code'] != f['expected']['code'] for f in failures))
    print(json.dumps(dict(cases=len(evidence['cases']), failed=len(failures), acceptance_mismatches=acceptance, report=str(report))))
    return bool(failures)

if __name__ == '__main__':
    sys.exit(main())
