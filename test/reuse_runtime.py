"""Compare shared category computations on the kernel and both WASM hosts."""
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
GATE = "TEMPLATE-REUSE-RUNTIME"


def main():
    if len(sys.argv) != 1:
        print("usage: python3 -I test/reuse_runtime.py", file=sys.stderr)
        return 64
    source = "\n".join((ROOT / path).read_text() for path in [
        "prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech",
        "prelude/cat/composable-functors.mech", "prelude/cat/identity-functor.mech",
        "test/fixtures/prelude/reuse-functors.mech",
    ])
    anchor = "def reuseInput : Nat := 37"
    if source.count(anchor) != 1:
        print(f"{GATE} FAIL expected one payload anchor")
        return 1
    exports = ["repeatedObject", "repeatedArrow", "identityObject", "identityArrow"]
    completed = 0
    hosts = set()
    deadline = time.monotonic() + 110
    with tempfile.TemporaryDirectory(prefix="mechanism-reuse-") as directory:
        work = Path(directory)
        for payload in [37, 41]:
            variant = work / str(payload)
            variant.mkdir()
            fixture = variant / "source.mech"
            fixture.write_text(source.replace(anchor, f"def reuseInput : Nat := {payload}"))
            answers = [(payload + 2) * 2 + 5, (payload + 3) * 2, payload, payload + 7]
            expected = "".join(f"{name}\t{value}\n"
                               for name, value in zip(exports, answers, strict=True))
            remaining = min(50, deadline - time.monotonic())
            if remaining <= 0:
                print(f"{GATE} FAIL {payload}/kernel-emit out of time")
                return 1
            result = subprocess.run(
                [str(ROOT / "_build/default/test/prelude_runtime.exe"), str(fixture),
                 str(variant), *exports], cwd=ROOT, capture_output=True, text=True, timeout=remaining)
            if result.returncode or result.stderr or result.stdout != expected:
                print(f"{GATE} FAIL {payload}/kernel-emit exit={result.returncode} "
                      f"stdout={result.stdout[:1500]!r} stderr={result.stderr[:1500]!r}")
                return 1
            hosts.add("kernel")
            for export, value in zip(exports, answers, strict=True):
                wasm = variant / f"{export}.wasm"
                for host, command in [
                    ("node", ["node", ROOT / "dev/run-node.mjs", wasm, export]),
                    ("wasmtime", ["zsh", ROOT / "dev/run-wasmtime.sh", wasm, export]),
                ]:
                    remaining = min(10, deadline - time.monotonic())
                    if remaining <= 0:
                        print(f"{GATE} FAIL {payload}/{export}/{host} out of time")
                        return 1
                    result = subprocess.run(list(map(str, command)), cwd=ROOT,
                                            capture_output=True, text=True, timeout=remaining)
                    if result.returncode or result.stderr or result.stdout != f"{value}\n":
                        print(f"{GATE} FAIL {payload}/{export}/{host} exit={result.returncode} "
                              f"stdout={result.stdout[:1500]!r} stderr={result.stderr[:1500]!r}")
                        return 1
                    hosts.add(host)
                completed += 1
    if completed != 2 * len(exports) or hosts != {"kernel", "node", "wasmtime"}:
        print(f"{GATE} FAIL incomplete comparisons")
        return 1
    print(f"{GATE} OK cases={len(exports)} hosts={len(hosts)} mutation=1")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"{GATE} FAIL {error}", file=sys.stderr)
        sys.exit(2)
