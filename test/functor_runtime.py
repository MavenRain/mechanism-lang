"""Check each functor variant once and compare all exports on three hosts."""
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
GATE = "PRELUDE-FUNCTOR-RUNTIME"
# The leg runs under the 300-second SUITE watchdog.  The whole run keeps
# 270 seconds, so a slow command reports its own label before the watchdog.
DEADLINE = 270
# The functor batch checks and emits six exports, so it keeps 90 seconds.
BATCH = 90
HOST = 20


def main():
    if len(sys.argv) != 1:
        print("usage: python3 -P test/functor_runtime.py", file=sys.stderr)
        return 64
    source = (ROOT / "prelude/cat/category.mech").read_text()
    source += (ROOT / "test/fixtures/prelude/functor-runtime.mech").read_text()
    anchor = "def functorInput : Nat := 37"
    if source.count(anchor) != 1:
        print(f"{GATE} FAIL expected one payload anchor")
        return 1
    exports = ["objectIdentity", "objectForward", "objectReverse",
               "mapIdentity", "mapForward", "mapReverse"]
    hosts = set()
    completed = 0
    end = time.monotonic() + DEADLINE

    def budget(limit):
        return min(limit, end - time.monotonic())

    with tempfile.TemporaryDirectory(prefix="mechanism-functor-") as directory:
        work = Path(directory)
        for variant, payload in [("original", 37), ("mutation", 41)]:
            variant_dir = work / variant
            variant_dir.mkdir()
            fixture = variant_dir / "source.mech"
            fixture.write_text(source.replace(anchor, f"def functorInput : Nat := {payload}"))
            answers = [payload, 12, payload, payload, payload, 12]
            expected = "".join(f"{name}\t{value}\n" for name, value in zip(exports, answers, strict=True))
            if budget(BATCH) <= 0:
                print(f"{GATE} FAIL {variant}/kernel-emit out of time")
                return 1
            emitted = subprocess.run(
                [str(ROOT / "_build/default/test/prelude_runtime.exe"), str(fixture),
                 str(variant_dir), *exports], cwd=ROOT, capture_output=True, text=True,
                timeout=budget(BATCH))
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
                    result = subprocess.run(list(map(str, command)), cwd=ROOT,
                                            capture_output=True, text=True,
                                            timeout=budget(HOST))
                    if result.returncode or result.stderr or result.stdout != f"{value}\n":
                        print(f"{GATE} FAIL {variant}/{export}/{host} exit={result.returncode} "
                              f"stdout={result.stdout[:2000]!r} stderr={result.stderr[:2000]!r}")
                        return 1
                    hosts.add(host)
                completed += 1
        refusals = 0
        for label, text, names, message in [
            ("axiom", source + "\naxiom runtimeAxiom : Nat\n", exports[:1],
             "runtime fixture declares axioms\n"),
            ("export-path", source, ["cat/objectIdentity"], "invalid export path\n"),
        ]:
            probe = work / f"probe-{label}"
            probe.mkdir()
            fixture = probe / "source.mech"
            fixture.write_text(text)
            if budget(BATCH) <= 0:
                print(f"{GATE} FAIL probe/{label} out of time")
                return 1
            refused = subprocess.run(
                [str(ROOT / "_build/default/test/prelude_runtime.exe"), str(fixture),
                 str(probe), *names], cwd=ROOT, capture_output=True, text=True,
                timeout=budget(BATCH))
            if refused.returncode != 1 or refused.stdout or refused.stderr != message:
                print(f"{GATE} FAIL probe/{label} exit={refused.returncode} "
                      f"stdout={refused.stdout[:2000]!r} stderr={refused.stderr[:2000]!r}")
                return 1
            refusals += 1
    if completed != 2 * len(exports):
        print(f"{GATE} FAIL completed={completed}")
        return 1
    print(f"{GATE} OK cases={len(exports)} hosts={len(hosts)} mutation=1 "
          f"refusals={refusals}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"{GATE} FAIL {error}", file=sys.stderr)
        sys.exit(2)
