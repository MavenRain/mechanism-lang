"""Compare left Kan extension projections and mediators on three hosts."""
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
GATE = "PRELUDE-LEFT-KAN-RUNTIME"
# The leg runs under the 300-second SUITE watchdog.  The whole run keeps
# 270 seconds, so a slow command reports its own label before the watchdog.
DEADLINE = 270
# One batch shares the identity witness across both payloads.
BATCH = 210
HOST = 20


def main():
    if len(sys.argv) != 1:
        print("usage: python3 -P test/left_kan_runtime.py", file=sys.stderr)
        return 64
    source = (ROOT / "prelude/cat/category.mech").read_text()
    source += (ROOT / "test/fixtures/prelude/nattrans-runtime.mech").read_text()
    source += (ROOT / "test/fixtures/prelude/left-kan-identity.mech").read_text()
    source += (ROOT / "test/fixtures/prelude/left-kan-runtime.mech").read_text()
    exports = ["lanUnitValue", "lanMapValue", "lanDescValue", "lanDescOther",
               "lanDescObject", "lanDescSecond"]
    functions = ["lanUnitAt", "lanMapAt", "lanDescAt", "lanOtherAt",
                 "lanObjectAt", "lanSecondAt"]
    cases = []
    for suffix, payload in [("", 37), ("Changed", 41)]:
        answers = [payload, 2 * payload, payload, 2 * payload, 2 * payload,
                   payload]
        for name, function, value in zip(exports, functions, answers, strict=True):
            export = name + suffix
            source += f"\ndef {export} : Nat := {function} {payload}\n"
            cases.append((export, value))
    hosts = set()
    completed = 0
    end = time.monotonic() + DEADLINE

    def budget(limit):
        return min(limit, end - time.monotonic())

    with tempfile.TemporaryDirectory(prefix="mechanism-left-kan-") as directory:
        work = Path(directory)
        fixture = work / "source.mech"
        fixture.write_text(source)
        expected = "".join(f"{name}\t{value}\n" for name, value in cases)
        if budget(BATCH) <= 0:
            print(f"{GATE} FAIL kernel-emit out of time")
            return 1
        emitted = uncapped(subprocess.run(
            capped([str(ROOT / "_tcc/prelude_runtime"), str(fixture),
             str(work), *(name for name, _ in cases)], budget(BATCH)), cwd=ROOT,
            capture_output=True, text=True, timeout=budget(BATCH)))
        if emitted.returncode or emitted.stderr or emitted.stdout != expected:
            print(f"{GATE} FAIL kernel-emit exit={emitted.returncode} "
                  f"stdout={emitted.stdout[:2000]!r} stderr={emitted.stderr[:2000]!r}")
            return 1
        hosts.add("kernel")
        for export, value in cases:
            wasm = work / f"{export}.wasm"
            for host, command in [
                ("node", ["node", ROOT / "dev/run-node.mjs", wasm, export]),
                ("wasmtime", ["zsh", ROOT / "dev/run-wasmtime.sh", wasm, export]),
            ]:
                if budget(HOST) <= 0:
                    print(f"{GATE} FAIL {export}/{host} out of time")
                    return 1
                result = uncapped(subprocess.run(capped(list(map(str, command)), budget(HOST)), cwd=ROOT,
                                        capture_output=True, text=True,
                                        timeout=budget(HOST)))
                if result.returncode or result.stderr or result.stdout != f"{value}\n":
                    print(f"{GATE} FAIL {export}/{host} exit={result.returncode} "
                          f"stdout={result.stdout[:2000]!r} stderr={result.stderr[:2000]!r}")
                    return 1
                hosts.add(host)
            completed += 1
    if completed != 2 * len(exports):
        print(f"{GATE} FAIL completed={completed}")
        return 1
    print(f"{GATE} OK cases={len(exports)} hosts={len(hosts)} mutation=1")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"{GATE} FAIL {error}", file=sys.stderr)
        sys.exit(2)
