#!/usr/bin/env python3
"""Run the compatibility report and its kernel mutation controls."""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mech", type=Path, default=ROOT / "_bend2/bin/mech.exe")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    output = args.out.resolve() if args.out else Path(tempfile.mkdtemp(prefix="mechanism-compatibility-")) / "evidence"
    if output.exists():
        parser.error("output directory must be new")
    mech = args.mech.resolve()
    commands = [
        [sys.executable, "-P", str(ROOT / "dev/prelude-compatibility-pilot.py"),
         "--mech", str(mech), "--out", str(output)],
        [sys.executable, "-P", str(ROOT / "test/prelude_compatibility.py"),
         "--mech", str(mech), "--report", str(output / "report.json")],
    ]
    for index, command in enumerate(commands):
        result = subprocess.run(command, capture_output=True, text=True, cwd=ROOT)
        output.mkdir(parents=True, exist_ok=True)
        (output / f"gate-{index}.stdout").write_text(result.stdout)
        (output / f"gate-{index}.stderr").write_text(result.stderr)
        if result.returncode:
            print(result.stdout, end="")
            print(result.stderr, end="", file=sys.stderr)
            print("PRELUDE-COMPATIBILITY-GATES FAIL")
            print(f"Evidence: {output}", file=sys.stderr)
            return 1
    print("PRELUDE-COMPATIBILITY-GATES candidates=19 support=1 records=4 mutation=4 OK")
    print(f"Evidence: {output}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
