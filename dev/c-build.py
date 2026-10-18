#!/usr/bin/env python3
"""Build the C artifacts and pin the inputs and production executables."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def sources():
    paths = [p for p in (ROOT / "c").rglob("*") if p.is_file() and p.suffix in {".c", ".h"}]
    paths.extend([ROOT / "Makefile", Path(__file__).resolve()])
    return {str(p.relative_to(ROOT)): digest(p) for p in sorted(paths)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tcc", default="tcc")
    parser.add_argument("--cflags", default="")
    parser.add_argument("--ldflags", default="")
    args = parser.parse_args()
    command = shlex.split(args.tcc)
    executable = shutil.which(command[0]) if command else None
    if executable is None:
        parser.error("tcc is required")
    tool = Path(executable).resolve()
    before, tool_hash = sources(), digest(tool)
    compiler = {"path": str(tool), "sha256": tool_hash, "command": command,
                "cflags": args.cflags, "ldflags": args.ldflags,
                "environment": {name: os.environ.get(name) for name in
                                ("CPATH", "C_INCLUDE_PATH", "LIBRARY_PATH", "SDKROOT",
                                 "MACOSX_DEPLOYMENT_TARGET")}}
    target = ROOT / "_tcc/c-build.json"
    try:
        previous = json.loads(target.read_text())
        recorded_sources, recorded_artifacts = previous['sources'], previous['artifacts']
        # The guard is recorded for audit, but it is not a C compilation input.
        compile_inputs = {name: value for name, value in before.items() if name != 'dev/c-build.py'}
        reusable = (isinstance(recorded_sources, dict) and isinstance(recorded_artifacts, dict)
                    and {name: value for name, value in recorded_sources.items()
                         if name != 'dev/c-build.py'} == compile_inputs
                    and previous['compiler'] == compiler
                    and set(recorded_artifacts) == {'_tcc/bin/mech', '_tcc/bin/mech_cert'}
                    and all(digest(ROOT / name) == value for name, value in recorded_artifacts.items()))
    except (OSError, ValueError, KeyError, TypeError):
        reusable = False
    force = [] if reusable else ["-B"]
    result = subprocess.run(["make", "--no-print-directory", *force, "c-build-artifacts",
                             f"TCC={args.tcc}", f"CFLAGS={args.cflags}", f"LDFLAGS={args.ldflags}"], cwd=ROOT)
    if result.returncode:
        return result.returncode
    if sources() != before or digest(tool) != tool_hash:
        raise SystemExit("C BUILD FAIL: sources or compiler changed during the build")
    artifacts = {name: digest(ROOT / name) for name in ("_tcc/bin/mech", "_tcc/bin/mech_cert")}
    record = {"sources": before, "compiler": compiler, "artifacts": artifacts}
    temporary = target.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(record, indent=2) + "\n")
    temporary.replace(target)
    print("C BUILD PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
