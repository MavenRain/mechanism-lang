#!/usr/bin/env python3
"""Check the natural-number boundary of the C template composition API."""
from pathlib import Path
import runpy

ROOT = Path(__file__).resolve().parents[1]
run = runpy.run_path(str(ROOT / "test/protocols/c_boundaries.py"))["run"]


if __name__ == "__main__":
    raise SystemExit(run(("template-arity",), ROOT / "build/template-arity-report.json",
                         "TEMPLATE-ARITY", ["test/template_composition.ml"],
                         success="TEMPLATE-ARITY-OK positive=1 signed-refusal=1"))
