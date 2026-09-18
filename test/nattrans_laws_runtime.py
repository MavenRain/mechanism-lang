"""Compare components certified by pointwise transformation laws on three hosts."""
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
GATE = "PRELUDE-NATTRANS-LAWS-RUNTIME"
TOTAL_BUDGET = 290
EMIT_BUDGET = 240
HOST_BUDGET = 30
FUNCTIONS = [
    ("leftIdentityAt", 1), ("rightIdentityAt", 1),
    ("assocLeftAt", 3), ("assocRightAt", 3),
    ("congrLeftAt", 2), ("congrRightAt", 2),
]
PAYLOADS = [37, 41]
SOURCES = [
    "prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech",
    "prelude/cat/heterogeneous-nattrans.mech", "prelude/cat/nattrans-laws.mech",
    "test/fixtures/prelude/heterogeneous-nattrans-runtime.mech",
    "test/fixtures/prelude/nattrans-laws-runtime.mech",
]


def main():
    if len(sys.argv) != 1:
        print("usage: python3 -I test/nattrans_laws_runtime.py", file=sys.stderr)
        return 64
    source = "\n".join((ROOT / name).read_text() for name in SOURCES)
    cases = []
    for payload in PAYLOADS:
        for function, factor in FUNCTIONS:
            export = f"{function}{payload}"
            source += f"\ndef {export} : Nat := {function} {payload}\n"
            cases.append((export, payload * factor))
    deadline = time.monotonic() + TOTAL_BUDGET

    def run(command, limit):
        remaining = min(limit, deadline - time.monotonic())
        if remaining <= 0:
            raise TimeoutError(f"timed out after the {TOTAL_BUDGET} s total budget")
        return subprocess.run(list(map(str, command)), cwd=ROOT, capture_output=True,
                              text=True, timeout=remaining)

    hosts = set()
    completed = 0
    with tempfile.TemporaryDirectory(prefix="mechanism-nattrans-laws-") as directory:
        work = Path(directory)
        fixture = work / "source.mech"
        fixture.write_text(source)
        expected = "".join(f"{name}\t{value}\n" for name, value in cases)
        emitted = run([ROOT / "_build/default/test/prelude_runtime.exe", "--reachable",
                       fixture, work, *(name for name, _ in cases)], EMIT_BUDGET)
        if emitted.returncode or emitted.stderr or emitted.stdout != expected:
            print(f"{GATE} FAIL kernel-emit exit={emitted.returncode} "
                  f"stdout={emitted.stdout[:1500]!r} stderr={emitted.stderr[:1500]!r}")
            return 1
        hosts.add("kernel")
        for name, value in cases:
            wasm = work / f"{name}.wasm"
            for host, command in [
                ("node", ["node", ROOT / "dev/run-node.mjs", wasm, name]),
                ("wasmtime", ["zsh", ROOT / "dev/run-wasmtime.sh", wasm, name]),
            ]:
                result = run(command, HOST_BUDGET)
                if result.returncode or result.stderr or result.stdout != f"{value}\n":
                    print(f"{GATE} FAIL {name}/{host} exit={result.returncode} "
                          f"stdout={result.stdout[:1500]!r} stderr={result.stderr[:1500]!r}")
                    return 1
                hosts.add(host)
            completed += 1
    if completed != len(FUNCTIONS) * len(PAYLOADS) or hosts != {"kernel", "node", "wasmtime"}:
        print(f"{GATE} FAIL incomplete comparisons")
        return 1
    print(f"{GATE} OK cases={len(FUNCTIONS)} hosts={len(hosts)} payloads={len(PAYLOADS)}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.SubprocessError, TimeoutError, ValueError) as error:
        print(f"{GATE} FAIL {error}")
        raise SystemExit(1)
