"""Compare heterogeneous functor computations on the kernel and both WASM hosts."""
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
GATE = "PRELUDE-HETEROGENEOUS-FUNCTOR-RUNTIME"


def main():
    if len(sys.argv) != 1:
        print("usage: python3 -I test/heterogeneous_functor_runtime.py", file=sys.stderr)
        return 64
    source = "".join((ROOT / path).read_text() for path in [
        "prelude/cat/category-core.mech",
        "prelude/cat/heterogeneous-functor.mech",
        "test/fixtures/prelude/heterogeneous-functor-runtime.mech",
    ])
    anchor = "def heterogeneousInput : Nat := 37"
    if source.count(anchor) != 1:
        print(f"{GATE} FAIL expected one payload anchor")
        return 1
    exports = ["objectValue", "mapValue", "compositionValue"]
    completed = 0
    hosts = set()
    deadline = time.monotonic() + 110

    def run(command, limit):
        remaining = min(limit, deadline - time.monotonic())
        if remaining <= 0:
            raise TimeoutError("runtime budget exhausted")
        return uncapped(subprocess.run(capped(list(map(str, command)), remaining), cwd=ROOT,
                              capture_output=True, text=True, timeout=remaining))

    with tempfile.TemporaryDirectory(prefix="mechanism-heterogeneous-functor-") as directory:
        work = Path(directory)
        for payload in [37, 41]:
            variant = work / str(payload)
            variant.mkdir()
            fixture = variant / "source.mech"
            fixture.write_text(source.replace(anchor, f"def heterogeneousInput : Nat := {payload}"))
            answers = [payload + 2, payload + 3, (payload + 3) * 2]
            expected = "".join(f"{name}\t{value}\n"
                               for name, value in zip(exports, answers, strict=True))
            emitted = run([ROOT / "_tcc/prelude_runtime", fixture,
                           variant, *exports], 30)
            if emitted.returncode or emitted.stderr or emitted.stdout != expected:
                print(f"{GATE} FAIL {payload}/kernel-emit exit={emitted.returncode} "
                      f"stdout={emitted.stdout[:1500]!r} stderr={emitted.stderr[:1500]!r}")
                return 1
            hosts.add("kernel")
            for export, value in zip(exports, answers, strict=True):
                wasm = variant / f"{export}.wasm"
                for host, command in [
                    ("node", ["node", ROOT / "dev/run-node.mjs", wasm, export]),
                    ("wasmtime", ["zsh", ROOT / "dev/run-wasmtime.sh", wasm, export]),
                ]:
                    result = run(command, 10)
                    if result.returncode or result.stderr or result.stdout != f"{value}\n":
                        print(f"{GATE} FAIL {payload}/{export}/{host} exit={result.returncode} "
                              f"stdout={result.stdout[:1500]!r} stderr={result.stderr[:1500]!r}")
                        return 1
                    hosts.add(host)
                completed += 1
    if completed != 2 * len(exports) or hosts != {"kernel", "node", "wasmtime"}:
        print(f"{GATE} FAIL incomplete comparisons")
        return 1
    print(f"{GATE} OK cases={len(exports)} hosts=3 mutation=1")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.SubprocessError, TimeoutError, ValueError) as error:
        print(f"{GATE} FAIL {error}")
        raise SystemExit(1)
