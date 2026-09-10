#!/usr/bin/env python3
"""Compare textual prenex specialization on the kernel and both hosts."""
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    if len(sys.argv) != 1:
        print("usage: python3 -P test/prenex_runtime.py", file=sys.stderr)
        return 64

    root = Path(__file__).resolve().parent.parent
    executable = root / "_build/default/bin/mech.exe"
    fixture = (root / "test/fixtures/prelude/prenex-runtime.mech").read_text()
    original = "def prenexRuntime : Nat := runtimeIdentity Nat 37"
    replacement = "def prenexRuntime : Nat := runtimeIdentity Nat 41"
    if fixture.count(original) != 1:
        print("PRENEX-RUNTIME FAIL mutation payload must occur exactly once")
        return 1

    failures = []

    def invoke(label, arguments, expected=""):
        result = subprocess.run(list(map(str, arguments)), cwd=root,
                                capture_output=True, text=True, timeout=20)
        if result.returncode != 0 or result.stdout != expected or result.stderr != "":
            failures.append(label)
            print(f"PRENEX-RUNTIME FAIL {label}: exit={result.returncode} "
                  f"stdout={result.stdout[:2000]!r} stderr={result.stderr[:2000]!r} "
                  f"expected={expected!r}")
            return False
        return True

    cases = [("prenexRuntime", 37), ("prenexClosure", 12)]
    changed = [("prenexRuntime", 41), ("prenexClosure", 12)]
    with tempfile.TemporaryDirectory(prefix="mechanism-prenex-") as directory:
        work = Path(directory)
        variants = [("original", fixture, cases),
                    ("mutation", fixture.replace(original, replacement), changed)]
        for variant, body, answers in variants:
            source = work / f"{variant}.mech"
            source.write_text(body)
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
        print(f"PRENEX-RUNTIME FAIL failing_checks={len(failures)}")
        return 1
    print("PRENEX-RUNTIME OK cases=2 hosts=3 mutation=1")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"PRENEX-RUNTIME FAIL {error}", file=sys.stderr)
        sys.exit(2)
