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

BUILD = ROOT / 'build' / 'bend2-translate'

CASES = ROOT / 'dev' / 'bend2' / 'translate-cases.json'

def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, text=True, capture_output=True, timeout=30)

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--native', type=Path, default=ROOT / '_bend2/test/import_translate.exe')
    args = parser.parse_args()
    evidence = json.loads(CASES.read_text())
    failures = []
    for index, case in enumerate(evidence['cases']):
        result = run([str(args.native.resolve()), case['source']])
        actual = dict(code=result.returncode, stdout=result.stdout, stderr=result.stderr)
        expected = {key: case[key] for key in actual}
        if actual != expected:
            failures.append(dict(index=index, expected=expected, actual=actual))
    BUILD.mkdir(parents=True, exist_ok=True)
    report = BUILD / 'report.json'
    report.write_text(json.dumps(dict(cases=len(evidence['cases']), failures=failures), ensure_ascii=True, indent=2) + '\n')
    print(json.dumps(dict(cases=len(evidence['cases']), failed=len(failures), report=str(report))))
    return bool(failures)

if __name__ == '__main__':
    sys.exit(main())
