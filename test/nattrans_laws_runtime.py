"""Compare components certified by pointwise transformation laws on three hosts."""
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
        return uncapped(subprocess.run(capped(list(map(str, command)), remaining), cwd=ROOT, capture_output=True,
                              text=True, timeout=remaining))

    hosts = set()
    completed = 0
    with tempfile.TemporaryDirectory(prefix="mechanism-nattrans-laws-") as directory:
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
