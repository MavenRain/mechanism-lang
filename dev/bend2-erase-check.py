#!/usr/bin/env python3
"""Replay the original erased-program corpus without an OCaml dependency."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--driver", type=Path, default=ROOT / "_bend2/test/kernel_erase_driver.exe")
    args = parser.parse_args()
    driver = args.driver.resolve()
    command = ["node", "--stack-size=16384", str(driver), "--"] if driver.suffix == ".js" else [str(driver)]
    evidence = json.loads((ROOT / "dev/bend2/erase-cases.json").read_text())
    failures = []
    for case in evidence["cases"]:
        try:
            result = subprocess.run([*command, case["source"]], capture_output=True, text=True, timeout=120)
            actual = dict(code=result.returncode, stdout=result.stdout, stderr=result.stderr)
        except subprocess.TimeoutExpired:
            actual = dict(code="timeout", stdout="", stderr="exceeded 120 seconds")
        expected = {key: case[key] for key in actual}
        if actual != expected:
            failures.append(dict(fixture=case["fixture"], expected=expected, actual=actual))
    report = ROOT / "build/bend2-erasure/report.json"
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(dict(cases=len(evidence["cases"]), failures=failures,
                                     driver_sha256=hashlib.sha256(driver.read_bytes()).hexdigest()), indent=2) + "\n")
    print(json.dumps(dict(cases=len(evidence["cases"]), failed=len(failures), report=str(report))))
    return bool(failures)


if __name__ == "__main__":
    sys.exit(main())
