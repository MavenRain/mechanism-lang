#!/usr/bin/env python3
"""Check the natural-number boundary of the template composition API."""
from pathlib import Path
import hashlib
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent


def main():
    candidate = os.environ.get("BEND") or shutil.which("bend") or str(Path.home() / ".bend/bin/bend")
    bend = str(Path(shutil.which(candidate) or candidate).resolve())
    try:
        version = subprocess.run([bend, "version"], capture_output=True, text=True, timeout=10)
        if version.returncode != 0 or version.stdout.strip() != "bend 2.0.27":
            print("TEMPLATE-ARITY-FAIL expected Bend 2.0.27", file=sys.stderr)
            return 1
        fixtures = [ROOT / "test/fixtures/bend2-template-arity/positive.bend",
                    ROOT / "test/neg/bend2-template-arity/negative.bend"]
        pins = {path: hashlib.sha256(path.read_bytes()).hexdigest() for path in [Path(bend), *fixtures]}
        results = [subprocess.run([bend, str(path), "--check-only"], cwd=ROOT,
                                 capture_output=True, text=True, timeout=50)
                   for path in fixtures]
        positive, negative = results
        diagnostic = negative.stdout + negative.stderr
        if (positive.returncode != 0 or negative.returncode != 1
                or "- expected : Nat\n" not in diagnostic
                or "/kernel/bignum.T\n" not in diagnostic
                or "Location: probe\n" not in diagnostic):
            print("TEMPLATE-ARITY-FAIL positive control or signed-value refusal", file=sys.stderr)
            for result in results:
                print(result.stdout + result.stderr, file=sys.stderr)
            return 1
        if any(hashlib.sha256(path.read_bytes()).hexdigest() != pin for path, pin in pins.items()):
            print("TEMPLATE-ARITY-FAIL inputs changed during compiler checks", file=sys.stderr)
            return 1
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"TEMPLATE-ARITY-FAIL {error}", file=sys.stderr)
        return 1
    print("TEMPLATE-ARITY-OK positive=1 signed-refusal=1")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
