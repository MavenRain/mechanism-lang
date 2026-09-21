"""Compare components certified by iterated whiskering laws on three hosts."""
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
GATE = "PRELUDE-ITERATED-WHISKERING-RUNTIME"
# Both gates use the existing 900-second CATEGORY watchdog.
TOTAL_BUDGET = 540
EMIT_BUDGET = 480
HOST_BUDGET = 30
FUNCTIONS = [
    ("rightLeftAt", 4, 32), ("rightRightAt", 4, 32),
    ("leftLeftAt", 101, 1314), ("leftRightAt", 101, 1314),
    ("mixedLeftAt", 101, 1317), ("mixedRightAt", 101, 1317),
]
PAYLOADS = [0, 37, 41]
SOURCES = [
    "prelude/cat/category-core.mech",
    "prelude/cat/heterogeneous-functor.mech",
    "prelude/cat/composable-functors.mech",
    "prelude/cat/heterogeneous-whiskering.mech",
    "prelude/cat/iterated-whiskering.mech",
    "test/fixtures/prelude/iterated-whiskering-runtime.mech",
]


def main():
    if len(sys.argv) != 1:
        print("usage: python3 -I test/iterated_whiskering_runtime.py", file=sys.stderr)
        return 64
    source = "\n".join((ROOT / name).read_text() for name in SOURCES)
    cases = []
    for payload in PAYLOADS:
        for function, scale, offset in FUNCTIONS:
            export = f"{function}{payload}"
            source += f"\ndef {export} : Nat := {function} {payload}\n"
            cases.append((export, payload * scale + offset))
    deadline = time.monotonic() + TOTAL_BUDGET

    def run(command, limit):
        remaining = min(limit, deadline - time.monotonic())
        if remaining <= 0:
            raise TimeoutError(f"timed out after the {TOTAL_BUDGET} s total budget")
        return subprocess.run(list(map(str, command)), cwd=ROOT, capture_output=True,
                              text=True, timeout=remaining)

    hosts = set()
    compared = set()
    with tempfile.TemporaryDirectory(prefix="mechanism-iterated-whiskering-") as directory:
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
                compared.add((name, host))
        unwritten = {name for name, _ in cases if not (work / f"{name}.wasm").is_file()}
    required = {(name, host) for name, _ in cases for host in ("node", "wasmtime")}
    if compared != required or unwritten or hosts != {"kernel", "node", "wasmtime"}:
        print(f"{GATE} FAIL unwritten={len(unwritten)} missing={len(required - compared)}")
        return 1
    print(f"{GATE} OK cases={len(FUNCTIONS)} hosts={len(hosts)} payloads={len(PAYLOADS)} "
          f"comparisons={len(compared)}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.SubprocessError, TimeoutError, ValueError) as error:
        print(f"{GATE} FAIL {error}")
        raise SystemExit(1)
