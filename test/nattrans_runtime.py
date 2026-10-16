"""Check each nattrans variant once and compare all exports on three hosts."""
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
GATE = "PRELUDE-NATTRANS-RUNTIME"
# The category prelude is checked and erased for both payloads.
# Keep the whole run below the 900-second CATEGORY watchdog.
DEADLINE = 870
BATCH = 210
HOST = 20


def main():
    if len(sys.argv) != 1:
        print("usage: python3 -P test/nattrans_runtime.py", file=sys.stderr)
        return 64
    source = (ROOT / "prelude/cat/category.mech").read_text()
    source += (ROOT / "test/fixtures/prelude/nattrans-runtime.mech").read_text()
    anchor = "def nattransInput : Nat := 37"
    if source.count(anchor) != 1:
        print(f"{GATE} FAIL expected one payload anchor")
        return 1
    exports = ["componentValue", "identityValue", "verticalForward", "verticalReverse",
               "verticalNested", "rightValue", "leftValue", "leftOther"]
    hosts = set()
    completed = 0
    end = time.monotonic() + DEADLINE

    def budget(limit):
        return min(limit, end - time.monotonic())

    with tempfile.TemporaryDirectory(prefix="mechanism-nattrans-") as directory:
        work = Path(directory)
        for variant, payload in [("original", 37), ("mutation", 41)]:
            variant_dir = work / variant
            variant_dir.mkdir()
            fixture = variant_dir / "source.mech"
            fixture.write_text(source.replace(anchor, f"def nattransInput : Nat := {payload}"))
            answers = [payload, payload, 2 * payload, payload, 3 * payload,
                       payload, payload, payload]
            expected = "".join(f"{name}\t{value}\n" for name, value in zip(exports, answers, strict=True))
            if budget(BATCH) <= 0:
                print(f"{GATE} FAIL {variant}/kernel-emit out of time")
                return 1
            emitted = uncapped(subprocess.run(
                capped([str(ROOT / "_tcc/prelude_runtime"), str(fixture),
                 str(variant_dir), *exports], budget(BATCH)), cwd=ROOT, capture_output=True, text=True,
                timeout=budget(BATCH)))
            if emitted.returncode or emitted.stderr or emitted.stdout != expected:
                print(f"{GATE} FAIL {variant}/kernel-emit exit={emitted.returncode} "
                      f"stdout={emitted.stdout[:2000]!r} stderr={emitted.stderr[:2000]!r}")
                return 1
            hosts.add("kernel")
            for export, value in zip(exports, answers, strict=True):
                wasm = variant_dir / f"{export}.wasm"
                for host, command in [
                    ("node", ["node", ROOT / "dev/run-node.mjs", wasm, export]),
                    ("wasmtime", ["zsh", ROOT / "dev/run-wasmtime.sh", wasm, export]),
                ]:
                    if budget(HOST) <= 0:
                        print(f"{GATE} FAIL {variant}/{export}/{host} out of time")
                        return 1
                    result = uncapped(subprocess.run(capped(list(map(str, command)), budget(HOST)), cwd=ROOT,
                                            capture_output=True, text=True,
                                            timeout=budget(HOST)))
                    if result.returncode or result.stderr or result.stdout != f"{value}\n":
                        print(f"{GATE} FAIL {variant}/{export}/{host} exit={result.returncode} "
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
