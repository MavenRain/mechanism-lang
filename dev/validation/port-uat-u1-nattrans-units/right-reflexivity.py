"""Replay a horizontal unit refusal in an isolated universe specialization.

Usage: python3 right-reflexivity.py [right|left]
  right (default): UnitsWide (3, 2, 1, 0) with right-reflexivity.mech
  left: UnitsMixed (0, 1, 2, 3) with left-reflexivity.mech
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
SOURCES = [
    "category-core", "heterogeneous-functor", "composable-functors",
    "heterogeneous-nattrans", "heterogeneous-whiskering", "shared-nattrans",
    "identity-functor", "nattrans-units",
]


CONTROLS = {
    "right": ("(3, 2, 1, 0)", "UnitsWide", "right-reflexivity"),
    "left": ("(0, 1, 2, 3)", "UnitsMixed", "left-reflexivity"),
}


def replay(levels, instance, negative):
    source = "\n".join((ROOT / "prelude/cat" / f"{name}.mech").read_text() for name in SOURCES)
    source += f"\nspecialize MechNatTransUnits {levels} as {instance}\n"
    source += (ROOT / "test/neg/nattrans-units" / f"{negative}.mech").read_text()
    with tempfile.TemporaryDirectory(prefix="mechanism-unit-refusal-") as directory:
        path = Path(directory) / "control.mech"
        path.write_text(source)
        result = subprocess.run([ROOT / "_build/default/bin/mech.exe", "check", path],
                                cwd=ROOT, capture_output=True, text=True, timeout=480)
    expected = f"mismatch: the constructor categoryRefl of {instance}_Target"
    if result.returncode != 1 or result.stdout or not result.stderr.startswith(expected):
        print(json.dumps({"status": "failed", "control": negative, "exit": result.returncode,
                          "stdout": result.stdout, "stderr": result.stderr}))
        return 1
    message = result.stderr.rstrip("\n")
    print(json.dumps({"status": "refused", "control": negative, "exit": result.returncode,
                      "excerpt": message[:256],
                      "excerpt_digest": hashlib.md5(message[:256].encode()).hexdigest(),
                      "digest": hashlib.md5(message.encode()).hexdigest()}))
    return 0


def main(argv):
    control = CONTROLS.get(argv[1] if len(argv) > 1 else "right")
    if control is None or len(argv) > 2:
        print(json.dumps({"status": "failed", "reason": "usage: right-reflexivity.py [right|left]"}))
        return 1
    return replay(*control)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
