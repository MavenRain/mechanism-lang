"""Compare left Kan extension projections and mediators on three hosts."""
from pathlib import Path
import subprocess
import sys
import tempfile
import time

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
        emitted = subprocess.run(
            [str(ROOT / "_build/default/test/prelude_runtime.exe"), str(fixture),
             str(work), *(name for name, _ in cases)], cwd=ROOT,
            capture_output=True, text=True, timeout=budget(BATCH))
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
                result = subprocess.run(list(map(str, command)), cwd=ROOT,
                                        capture_output=True, text=True,
                                        timeout=budget(HOST))
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
