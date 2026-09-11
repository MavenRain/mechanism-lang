#!/usr/bin/env python3
"""Run the pinned WASM suite with mechanism's explicit golden overlays."""
from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
OVERLAYS = {
    "d06-closure-capture.wat", "d07-partial-over.wat",
    "d13-function-case.wat", "d14-generic-capture.wat",
    "d15-generic-aggregates.wat", "nat-runtime-edges.wat",
    "one-runtime-capture.wat", "one-shadowing.wat",
}


def main():
    overlay = ROOT / "test/golden/wasm"
    if {path.name for path in overlay.iterdir()} != OVERLAYS:
        print("SUITE-WASM FAIL golden overlay inventory")
        return 1
    work = ROOT / ".gatework"
    work.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="wasm-input-", dir=work) as directory:
        repo = Path(directory)
        test = repo / "test"
        golden = test / "golden"
        golden.mkdir(parents=True)
        (repo / "dev").symlink_to(ROOT / "dev", target_is_directory=True)
        (test / "fixtures").symlink_to(ROOT / "vendor/kanon/test/fixtures",
                                       target_is_directory=True)
        pinned = ROOT / "vendor/kanon/test/golden"
        if not OVERLAYS <= {path.name for path in pinned.iterdir()}:
            print("SUITE-WASM FAIL golden overlay has no pinned original")
            return 1
        for path in pinned.iterdir():
            source = overlay / path.name if path.name in OVERLAYS else path
            (golden / path.name).symlink_to(source)
        result = subprocess.run([ROOT / "_build/default/test/wasm.exe", test,
                                 work / "wasm-suite"], cwd=ROOT,
                                capture_output=True, text=True)
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        if result.returncode != 0:
            return result.returncode
        # The suite skips a fixture that stops elaborating without a line,
        # so a green verdict alone does not prove the overlays were read.
        lines = set(result.stdout.splitlines())
        skipped = sorted(name for name in OVERLAYS
                         if f"EMIT {Path(name).stem} OK" not in lines)
        if skipped:
            print("SUITE-WASM FAIL golden overlay not compared: "
                  + " ".join(skipped))
            return 1
        return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except OSError as error:
        print(f"SUITE-WASM FAIL {error}", file=sys.stderr)
        sys.exit(2)
