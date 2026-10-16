"""Compare both horizontal associations, certified by their law, on three hosts."""
from pathlib import Path
import subprocess
import sys
import tempfile
import time
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

ROOT = Path(__file__).resolve().parents[1]
GATE = "PRELUDE-HORIZONTAL-ASSOCIATIVITY-RUNTIME"
# Both gates use the existing 900-second CATEGORY watchdog.
TOTAL_BUDGET = 540
EMIT_BUDGET = 480
HOST_BUDGET = 30
FUNCTIONS = [
    ("assocLeftAt", 103, 814), ("assocRightAt", 103, 814),
]
PAYLOADS = [0, 37, 41]
SOURCES = [
    "prelude/cat/category-core.mech",
    "prelude/cat/heterogeneous-functor.mech",
    "prelude/cat/composable-functors.mech",
    "prelude/cat/heterogeneous-nattrans.mech",
    "prelude/cat/heterogeneous-whiskering.mech",
    "prelude/cat/shared-nattrans.mech",
    "prelude/cat/horizontal-associativity.mech",
    "test/fixtures/prelude/horizontal-associativity-runtime.mech",
]


def main():
    if len(sys.argv) != 1:
        print("usage: python3 -I test/horizontal_associativity_runtime.py", file=sys.stderr)
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
        return uncapped(subprocess.run(capped(list(map(str, command)), remaining), cwd=ROOT, capture_output=True,
                              text=True, timeout=remaining))

    hosts = set()
    compared = set()
    with tempfile.TemporaryDirectory(prefix="mechanism-horizontal-associativity-") as directory:
        work = Path(directory)
        fixture = work / "source.mech"
        fixture.write_text(source)
        expected = "".join(f"{name}\t{value}\n" for name, value in cases)
        emitted = run([ROOT / "_tcc/prelude_runtime", "--reachable",
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
