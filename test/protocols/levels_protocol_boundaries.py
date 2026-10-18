#!/usr/bin/env python3
"""Verify signed-value refusals at the C universe interfaces."""
from pathlib import Path
import runpy
import sys

ROOT = Path(__file__).resolve().parents[2]
run = runpy.run_path(str(Path(__file__).with_name("c_boundaries.py")))["run"]
CASES = ("negative-level", "negative-variable", "negative-scope", "negative-scheme-arity")


if __name__ == "__main__":
    output = Path(sys.argv[1]) if len(sys.argv) == 2 else ROOT / "build/levels-boundaries-report.json"
    raise SystemExit(run(CASES, output, "LEVELS-BOUNDARIES", ["test/levels.ml"]))
