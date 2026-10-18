"""Compile paired C probes for the unsigned universe and arity interfaces."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def run(cases, output, marker, historical_sources, success=None):
    compiler = shlex.split(os.environ.get("TCC", "tcc"))
    executable = shutil.which(compiler[0]) if compiler else None
    if executable is None:
        print(f"{marker}-FAIL tcc is required", file=sys.stderr)
        return 1
    compiler[0] = str(Path(executable).resolve())
    paths = [Path(compiler[0]), ROOT / "c/kernel/kernel.h", ROOT / "c/surface/surface.h",
             ROOT / "c/rt/rt.h"]
    paths.extend(ROOT / "test/fixtures/c-boundaries" / f"{case}.c" for case in cases)
    pins = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
    rows = []
    with tempfile.TemporaryDirectory(prefix="c-boundaries-") as directory:
        for case in cases:
            pair = {}
            for variant in ("positive", "negative"):
                source = ROOT / "test/fixtures/c-boundaries" / f"{case}.c"
                defines = ["-DBOUNDARY_SIGNED"] if variant == "negative" else []
                command = [*compiler, *defines, "-Wall", "-Werror", "-I", str(ROOT / "c/rt"),
                           "-I", str(ROOT / "c/kernel"), "-I", str(ROOT / "c/surface"),
                           "-c", str(source), "-o", str(Path(directory) / "probe.o")]
                result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
                diagnostics = result.stdout + result.stderr
                passed = result.returncode == 0 if variant == "positive" else (
                    result.returncode == 1 and "cannot convert" in diagnostics
                    and f"{source}:" in diagnostics)
                pair[variant] = {"exit": result.returncode, "passed": passed,
                                 "source": str(source.relative_to(ROOT)),
                                 "source_sha256": pins[str(source)],
                                 "stdout": result.stdout, "stderr": result.stderr}
            rows.append({"name": case, "passed": all(x["passed"] for x in pair.values()), **pair})
    unchanged = all(hashlib.sha256(Path(path).read_bytes()).hexdigest() == digest
                    for path, digest in pins.items())
    passed = unchanged and all(row["passed"] for row in rows)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({"passed": passed, "compiler": compiler, "pins": pins,
                                 "inputs_unchanged": unchanged,
                                 "historical_sources": historical_sources, "revision": "af5b7f0",
                                 "mapping": "signed Big values are refused by the uint32_t/Nat C interfaces",
                                 "rows": rows}, indent=2) + "\n")
    print(success if passed and success else f"{marker}-{'OK' if passed else 'FAIL'} assertions={len(rows)}")
    return 0 if passed else 1
