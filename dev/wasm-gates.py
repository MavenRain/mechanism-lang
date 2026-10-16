#!/usr/bin/env python3
"""Run the pinned WASM suite with mechanism's explicit golden overlays."""
from pathlib import Path
import subprocess
import sys
import tempfile
import os

# Hop 330 (USER ruling 1 on the memcap-blind python legs, D-329a): when MEMCAP
# names the memcap wrapper, every driver this harness spawns runs as
# `MEMCAP 3800 <secs> driver...`, so the cap sits on the driver, not on python3.
# <secs> is the call's own timeout less one second (memcap fires first and
# leaves a record), or MEMCAP_SECONDS when the call has no timeout.  Without
# MEMCAP the commands are unchanged.  memcap prints its record line on the
# driver's stdout; uncapped() moves such lines to this harness's stderr so the
# byte-for-byte stdout checks stay.
MEMCAP = os.environ.get("MEMCAP")
MEMCAP_SECONDS = int(os.environ.get("MEMCAP_SECONDS", "900"))


def capped(command, seconds=None):
    budget = MEMCAP_SECONDS if seconds is None else max(1, int(seconds) - 1)
    prefix = [MEMCAP, "3800", str(budget)] if MEMCAP else []
    return prefix + [str(part) for part in command]


def uncapped(result):
    stdout = getattr(result, "stdout", None)
    if not MEMCAP or stdout is None:
        return result
    marker = "memcap " if isinstance(stdout, str) else b"memcap "
    lines = stdout.splitlines(keepends=True)
    moved = [line for line in lines if line.startswith(marker)]
    kept = [line for line in lines if not line.startswith(marker)]
    if moved:
        text = marker[:0].join(moved)
        sys.stderr.write(text if isinstance(text, str) else text.decode("utf-8", "replace"))
        sys.stderr.flush()
    result.stdout = marker[:0].join(kept)
    return result


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
        (test / "fixtures").symlink_to(ROOT / "test/veil/pinned/test/fixtures",
                                       target_is_directory=True)
        pinned = ROOT / "test/veil/pinned/test/golden"
        if not OVERLAYS <= {path.name for path in pinned.iterdir()}:
            print("SUITE-WASM FAIL golden overlay has no pinned original")
            return 1
        for path in pinned.iterdir():
            source = overlay / path.name if path.name in OVERLAYS else path
            (golden / path.name).symlink_to(source)
        result = uncapped(subprocess.run(capped([ROOT / "_tcc/wasm_protocol", test,
                                 work / "wasm-suite"]), cwd=ROOT,
                                capture_output=True, text=True))
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
