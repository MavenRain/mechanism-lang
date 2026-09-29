#!/usr/bin/env python3
"""Verify the negative composition arity excluded by the native Nat API boundary.

Each negative call must be rejected by Bend 2.0.27, and the same complete
call with zero must typecheck. These are compiler checks, not runtime passes.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "bend2/tests/fixtures/surface-boundaries"
CASES = ("negative-composition-arity", "negative-family-arity")


def main():
    bend = os.environ.get("BEND") or shutil.which("bend") or str(Path.home() / ".bend/bin/bend")
    version = subprocess.run([bend, "version"], capture_output=True, text=True, check=True).stdout.strip()
    if version != "bend 2.0.27":
        raise ValueError("boundary fixtures require Bend 2.0.27")
    output = Path(sys.argv[1]) if len(sys.argv) == 2 else ROOT / "build/surface-boundaries-report.json"
    rows = []
    for name in CASES:
        pair = {}
        for variant in ("positive", "negative"):
            source = FIXTURES / f"{name}-{variant}.bend"
            result = subprocess.run([bend, str(source), "--check-only"], cwd=ROOT,
                                    capture_output=True, text=True, timeout=60)
            diagnostics = result.stdout + result.stderr
            passed = (result.returncode == 0 and "All terms check" in diagnostics) if variant == "positive" else (
                result.returncode == 1 and "Error:" in diagnostics and "observed : '-'" in diagnostics)
            pair[variant] = {"exit": result.returncode, "passed": passed,
                             "source": str(source.relative_to(ROOT)),
                             "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                             "stdout": result.stdout, "stderr": result.stderr}
        rows.append({"name": name, "passed": all(x["passed"] for x in pair.values()), **pair})
    passed = all(row["passed"] for row in rows)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({"passed": passed, "compiler": version,
                                 "historical_sources": ["test/template_composition.ml", "test/family_poly.ml"], "revision": "af5b7f0",
                                 "mapping": "signed host integers are rejected at the U32/Nat source boundary",
                                 "rows": rows}, indent=2) + "\n")
    print(f"SURFACE-BOUNDARIES-{'OK' if passed else 'FAIL'} assertions={len(rows)}")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
