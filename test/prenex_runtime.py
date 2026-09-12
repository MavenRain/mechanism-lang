#!/usr/bin/env python3
"""Compare textual prenex specialization on the kernel and both hosts."""
from pathlib import Path
import subprocess
import sys
import tempfile

GATES = {(): "PRENEX-RUNTIME", ("--families",): "PRENEX-FAMILIES-RUNTIME",
         ("--groups",): "PRENEX-GROUPS-RUNTIME", ("--category",): "PRELUDE-CATEGORY-RUNTIME",
         ("--category-accessors",): "PRELUDE-CATEGORY-ACCESSORS",
         ("--closures",): "DEPENDENT-CLOSURE-RUNTIME"}


def gate_name(arguments):
    """The gate label of one mode, for the run and for an abort."""
    return GATES.get(tuple(arguments), "PRENEX-RUNTIME")


def main():
    if tuple(sys.argv[1:]) not in GATES:
        print("usage: python3 -P test/prenex_runtime.py "
              "[--families|--groups|--category|--category-accessors|--closures]",
              file=sys.stderr)
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
    category = sys.argv[1:] in (["--category"], ["--category-accessors"])
    if category:
        fixture = (root / "prelude/cat/category.mech").read_text()
        category_fixture = ("category-accessor-runtime.mech"
                            if sys.argv[1:] == ["--category-accessors"]
                            else "category-runtime.mech")
        fixture += (root / "test/fixtures/prelude" / category_fixture).read_text()
        original = "def categoryInput : Nat := 37"
        replacement = "def categoryInput : Nat := 41"
    closures = sys.argv[1:] == ["--closures"]
    if closures:
        fixture = (root / "test/fixtures/prelude/dependent-closure-runtime.mech").read_text()
        original = "def closureInput : Nat := 37"
        replacement = "def closureInput : Nat := 41"
    if fixture.count(original) != 1:
        print(f"{gate} FAIL mutation payload must occur exactly once")
        return 1

    failures = []
    hosts = set()

    def invoke(label, arguments, expected="", timeout=20):
        result = subprocess.run(list(map(str, arguments)), cwd=root,
                                capture_output=True, text=True, timeout=timeout)
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
    if category:
        payload, other = "categoryPayload", "categoryOther"
    cases = [(payload, 37), (other, 12)]
    changed = [(payload, 41), (other, 12)]
    if closures:
        exports = ["nullaryPayload", "functionPayload", "capturedPayload",
                   "aliasPayload", "partialPayload", "extraPayload",
                   "exactPayload", "capturedMapPayload", "nonTailPayload"]
        cases = [(name, 37) for name in exports]
        changed = [(name, 41) for name in exports]
    if category:
        # Both category modes share a second export that composes the
        # identity with a constant function, so its answer is 12 before
        # and after the mutation. That export cannot observe the
        # mutation. The mutation variant runs the payload export only,
        # which is the export whose answer depends on the mutated
        # definition.
        changed = [(payload, 41)]
    with tempfile.TemporaryDirectory(prefix="mechanism-prenex-") as directory:
        work = Path(directory)
        variants = [("original", fixture, cases),
                    ("mutation", fixture.replace(original, replacement), changed)]
        # The batch helper checks and erases each category variant once,
        # audits its axioms, then evaluates and emits every requested
        # export through the same APIs as the CLI.
        for variant, body, answers in variants:
            source = work / f"{variant}.mech"
            source.write_text(body)
            if category:
                expected = "".join(f"{name}\t{answer}\n" for name, answer in answers)
                if not invoke(f"{variant}/kernel-emit",
                              [root / "_build/default/test/prelude_runtime.exe", source,
                               work, *(name for name, _ in answers)], expected, timeout=60):
                    continue
                hosts.add("kernel")
            if not category:
                if not invoke(f"{variant}/check",
                              [executable, "check", source]):
                    continue
                invoke(f"{variant}/axioms", [executable, "axioms", source])
            for export, answer in answers:
                expected = f"{answer}\n"
                label = f"{variant}/{export}"
                wasm = work / (f"{export}.wasm" if category else f"{variant}-{export}.wasm")
                if not category and not invoke(f"{label}/emit", [executable, "emit", source,
                                                                "-o", wasm, "--export", export]):
                    continue
                # The OK line reports the hosts that ran, never a
                # literal, so a dropped host shows in the count.
                host_runs = (
                    ("node", ["node", root / "dev/run-node.mjs", wasm, export]),
                    ("wasmtime", ["zsh", root / "dev/run-wasmtime.sh", wasm,
                                  export]),
                )
                if not category:
                    host_runs += (("kernel", [executable, "run", source, "--export", export,
                                               "--host", "kernel"]),)
                for host, arguments in host_runs:
                    hosts.add(host)
                    invoke(f"{label}/{host}", arguments, expected)

    # A host failure and a kernel regression are separate outcomes.
    kernel_failures = [label for label in failures
                       if label.endswith(("/check", "/axioms", "/emit", "/kernel", "/kernel-emit"))]
    if kernel_failures:
        print(f"{gate} FAIL kernel_checks={len(kernel_failures)} "
              f"failing_checks={len(failures)}")
        return 1
    if failures:
        print(f"{gate} FAIL host_checks={len(failures)} "
              f"failing_checks={len(failures)}")
        return 1
    print(f"{gate} OK cases={len(cases)} hosts={len(hosts)} mutation=1")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"{gate_name(sys.argv[1:])} FAIL {error}", file=sys.stderr)
        sys.exit(2)
