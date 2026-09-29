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

BUILD = ROOT / 'build/bend2-pipeline'

CASES = ROOT / 'dev/bend2/pipeline-cases.json'

def run(command):
    return subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=120)

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--native', type=Path, default=ROOT / '_bend2/test/import_pipeline.exe')
    args = parser.parse_args()
    evidence = json.loads(CASES.read_text())
    failures = []
    for index, case in enumerate(evidence['cases']):
        command = ['node', '--stack-size=16384', str(args.native.resolve()), '--'] if args.native.suffix == '.js' else [str(args.native.resolve())]
        try:
            result = run([*command, case['mode'], case['source'], case['targets']])
            actual = dict(code=result.returncode, stdout=result.stdout, stderr=result.stderr)
        except subprocess.TimeoutExpired:
            actual = dict(code='timeout', stdout='', stderr='exceeded replay timeout')
        expected = {key: case[key] for key in actual}
        if actual != expected:
            failures.append(dict(index=index, mode=case['mode'], expected=expected, actual=actual))
    BUILD.mkdir(parents=True, exist_ok=True)
    report = BUILD / 'report.json'
    report.write_text(json.dumps(dict(cases=len(evidence['cases']), failures=failures), ensure_ascii=True, indent=2) + '\n')
    print(json.dumps(dict(cases=len(evidence['cases']), failed=len(failures), report=str(report))))
    return bool(failures)

if __name__ == '__main__':
    sys.exit(main())
