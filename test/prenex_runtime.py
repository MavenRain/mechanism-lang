#!/usr/bin/env python3
"""Compare textual prenex specialization on the kernel and both hosts."""
from pathlib import Path
import subprocess
import sys
import tempfile

GATES = {(): "PRENEX-RUNTIME", ("--families",): "PRENEX-FAMILIES-RUNTIME",
         ("--groups",): "PRENEX-GROUPS-RUNTIME"}


def gate_name(arguments):
    """The gate label of one mode, for the run and for an abort."""
    return GATES.get(tuple(arguments), "PRENEX-RUNTIME")


def main():
    if sys.argv[1:] not in ([], ["--families"], ["--groups"]):
        print("usage: python3 -P test/prenex_runtime.py [--families|--groups]", file=sys.stderr)
        return 64

    root = Path(__file__).resolve().parent.parent
    executable = root / "_build/default/bin/mech.exe"
    families = sys.argv[1:] == ["--families"]
    gate = gate_name(sys.argv[1:])
    fixture_name = "prenex-families-runtime.mech" if families else "prenex-runtime.mech"
    fixture = (root / "test/fixtures/prelude" / fixture_name).read_text()
    original = ("def familyPayload : Nat := unbox (box 37)" if families
                else "def prenexRuntime : Nat := runtimeIdentity Nat 37")
    replacement = ("def familyPayload : Nat := unbox (box 41)" if families
                   else "def prenexRuntime : Nat := runtimeIdentity Nat 41")
    groups = sys.argv[1:] == ["--groups"]
    if groups:
        fixture = (root / "test/fixtures/prelude/prenex-groups-runtime.mech").read_text()
        original = "def groupPayload : Nat := Run_unpack Nat (Run_pack Nat 37)"
        replacement = "def groupPayload : Nat := Run_unpack Nat (Run_pack Nat 41)"
    if fixture.count(original) != 1:
        print(f"{gate} FAIL mutation payload must occur exactly once")
        return 1

    failures = []

    def invoke(label, arguments, expected=""):
        result = subprocess.run(list(map(str, arguments)), cwd=root,
                                capture_output=True, text=True, timeout=20)
        if result.returncode != 0 or result.stdout != expected or result.stderr != "":
            failures.append(label)
            print(f"{gate} FAIL {label}: exit={result.returncode} "
                  f"stdout={result.stdout[:2000]!r} stderr={result.stderr[:2000]!r} "
                  f"expected={expected!r}")
            return False
        return True

    payload, other = ("familyPayload", "familyRecursive") if families else ("prenexRuntime", "prenexClosure")
    if groups:
        payload, other = "groupPayload", "groupOther"
    cases = [(payload, 37), (other, 12)]
    changed = [(payload, 41), (other, 12)]
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
        print(f"{gate} FAIL failing_checks={len(failures)}")
        return 1
    print(f"{gate} OK cases=2 hosts=3 mutation=1")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"{gate_name(sys.argv[1:])} FAIL {error}", file=sys.stderr)
        sys.exit(2)
