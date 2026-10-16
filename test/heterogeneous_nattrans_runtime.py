"""Compare heterogeneous natural transformation computations on the kernel and both WASM hosts."""
from pathlib import Path
import re
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
GATE = "PRELUDE-HETEROGENEOUS-NATTRANS-RUNTIME"


def main():
    if len(sys.argv) != 1:
        print("usage: python3 -I test/heterogeneous_nattrans_runtime.py", file=sys.stderr)
        return 64
    source = "".join((ROOT / path).read_text() for path in [
        "prelude/cat/category-core.mech",
        "prelude/cat/heterogeneous-functor.mech",
        "prelude/cat/heterogeneous-nattrans.mech",
        "test/fixtures/prelude/heterogeneous-nattrans-runtime.mech",
    ])
    anchor = "def heterogeneousNatInput : Nat := 37"
    if source.count(anchor) != 1:
        print(f"{GATE} FAIL expected one payload anchor")
        return 1
    exports = ["componentValue", "identityValue", "verticalForward", "verticalReverse", "verticalNested"]
    # Both payloads share one checked set of category and functor definitions.
    alternate_input = "heterogeneousNatInput41"
    if re.search(r"\b" + alternate_input + r"\b", source):
        raise ValueError("alternate payload name already exists")
    source = source.replace(anchor, anchor + f"\ndef {alternate_input} : Nat := 41")
    seen = set()
    blocks = []
    for block in re.split(r"(?m)(?=^def )", source):
        blocks.append(block)
        declaration = re.match(r"def (\w+) : Nat :=", block)
        if declaration is None or declaration.group(1) not in exports:
            continue
        name = declaration.group(1)
        if name in seen or re.search(r"\b" + name + r"_41\b", source):
            raise ValueError(f"duplicate export name: {name}")
        seen.add(name)
        body = block.split(":=", 1)[1]
        if len(re.findall(r"\bheterogeneousNatInput\b", body)) != 1:
            raise ValueError(f"expected one payload reference in {name}")
        if any(re.search(r"\b" + export + r"\b", body) for export in exports):
            raise ValueError(f"export dependency needs explicit specialization: {name}")
        alternate = block.replace(f"def {name} : Nat :=", f"def {name}_41 : Nat :=", 1)
        blocks.append(re.sub(r"\bheterogeneousNatInput\b", alternate_input, alternate))
    if seen != set(exports):
        raise ValueError("runtime export inventory changed")
    source = "".join(blocks)
    cases = []
    for payload, suffix in [(37, ""), (41, "_41")]:
        # identityValue adds 5 to the payload before the identity component.
        answers = [payload, payload + 5, payload * 2, payload, payload * 3]
        cases.extend((payload, name, name + suffix, value)
                     for name, value in zip(exports, answers, strict=True))
    completed = 0
    hosts = set()
    deadline = time.monotonic() + 110

    def run(command, limit):
        remaining = min(limit, deadline - time.monotonic())
        if remaining <= 0:
            raise TimeoutError("runtime budget exhausted")
        return uncapped(subprocess.run(capped(list(map(str, command)), remaining), cwd=ROOT,
                              capture_output=True, text=True, timeout=remaining))

    with tempfile.TemporaryDirectory(prefix="mechanism-heterogeneous-nattrans-") as directory:
        work = Path(directory)
        fixture = work / "source.mech"
        fixture.write_text(source)
        expected = "".join(f"{actual}\t{value}\n" for _, _, actual, value in cases)
        emitted = run([ROOT / "_tcc/prelude_runtime", fixture,
                       work, *(actual for _, _, actual, _ in cases)], 110)
        if emitted.returncode or emitted.stderr or emitted.stdout != expected:
            print(f"{GATE} FAIL kernel-emit exit={emitted.returncode} "
                  f"stdout={emitted.stdout[:1500]!r} stderr={emitted.stderr[:1500]!r}")
            return 1
        hosts.add("kernel")
        for payload, export, actual, value in cases:
            wasm = work / f"{actual}.wasm"
            for host, command in [
                ("node", ["node", ROOT / "dev/run-node.mjs", wasm, actual]),
                ("wasmtime", ["zsh", ROOT / "dev/run-wasmtime.sh", wasm, actual]),
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
