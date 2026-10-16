#!/usr/bin/env python3
"""Compare equality transport with fixed answers on the kernel and both hosts."""
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


def main():
    if len(sys.argv) != 1:
        print("usage: python3 -P test/equality_runtime.py", file=sys.stderr)
        return 64

    root = Path(__file__).resolve().parent.parent
    executable = root / "_tcc/bin/mech"
    prelude = (root / "prelude/init.mech").read_text()
    fixture = (root / "test/fixtures/prelude/equality-runtime.mech").read_text()
    original = "def equalityRuntimePayload : Nat := 37"
    replacement = "def equalityRuntimePayload : Nat := 41"
    if fixture.count(original) != 1:
        print("EQUALITY-RUNTIME FAIL mutation payload must occur exactly once")
        return 1

    failures = []

    def invoke(label, arguments, expected=""):
        result = uncapped(subprocess.run(capped(list(map(str, arguments)), 20), cwd=root,
                                capture_output=True, text=True, timeout=20))
        if result.returncode != 0 or result.stdout != expected or result.stderr != "":
            failures.append(label)
            print(f"EQUALITY-RUNTIME FAIL {label}: exit={result.returncode} "
                  f"stdout={result.stdout[:2000]!r} stderr={result.stderr[:2000]!r} "
                  f"expected={expected!r}")
            return False
        return True

    cases = [("equalityRuntimeLiteral", 37), ("equalityRuntimeStruct", 2),
             ("equalityRuntimeClosure", 42)]
    changed = [("equalityRuntimeLiteral", 41), ("equalityRuntimeStruct", 2),
               ("equalityRuntimeClosure", 46)]
    with tempfile.TemporaryDirectory(prefix="mechanism-equality-") as directory:
        work = Path(directory)
        variants = [("original", fixture, cases),
                    ("mutation", fixture.replace(original, replacement), changed)]
        for variant, body, answers in variants:
            source = work / f"{variant}.mech"
            source.write_text(prelude + "\n" + body)
            if not invoke(f"{variant}/check", [executable, "check", source]):
                continue
            invoke(f"{variant}/axioms", [executable, "axioms", source])
            for export, answer in answers:
                expected = f"{answer}\n"
                label = f"{variant}/{export}"
                wasm = work / f"{variant}-{export}.wasm"
                if not invoke(f"{label}/emit", [executable, "emit", source,
                                               "-o", wasm, "--export", export]):
                    continue
                invoke(f"{label}/node", ["node", root / "dev/run-node.mjs",
                                        wasm, export], expected)
                invoke(f"{label}/wasmtime", ["zsh", root / "dev/run-wasmtime.sh",
                                            wasm, export], expected)
                invoke(f"{label}/kernel", [executable, "run", source,
                                           "--export", export, "--host", "kernel"], expected)

    if failures:
        print(f"EQUALITY-RUNTIME FAIL failing_checks={len(failures)}")
        return 1
    print("EQUALITY-RUNTIME OK cases=3 hosts=3 mutation=1")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"EQUALITY-RUNTIME FAIL {error}", file=sys.stderr)
        sys.exit(2)
