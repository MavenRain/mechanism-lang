"""Compare heterogeneous left Kan extension computations on the kernel and both WASM hosts."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
GATE = "PRELUDE-HETEROGENEOUS-LEFT-KAN-RUNTIME"


def main():
    if len(sys.argv) != 1:
        print("usage: python3 -I test/heterogeneous_left_kan_runtime.py", file=sys.stderr)
        return 64
    source = "".join((ROOT / path).read_text() for path in [
        "prelude/cat/category-core.mech",
        "prelude/cat/composable-functors.mech",
        "prelude/cat/heterogeneous-whiskering.mech",
        "prelude/cat/heterogeneous-left-kan.mech",
        "test/fixtures/prelude/heterogeneous-left-kan-runtime.mech",
    ])
    anchor = "def heterogeneousLanInput : Nat := 37"
    if source.count(anchor) != 1:
        print(f"{GATE} FAIL expected one payload anchor")
        return 1
    exports = ["lanUnitResult", "lanMapResult", "lanDescFirst", "lanDescSecond"]
    # Both payloads share one checked set of category and functor definitions.
    alternate_input = "heterogeneousLanInput41"
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
        if len(re.findall(r"\bheterogeneousLanInput\b", body)) != 1:
            raise ValueError(f"expected one payload reference in {name}")
        if any(re.search(r"\b" + export + r"\b", body) for export in exports):
            raise ValueError(f"export dependency needs explicit specialization: {name}")
        alternate = block.replace(f"def {name} : Nat :=", f"def {name}_41 : Nat :=", 1)
        blocks.append(re.sub(r"\bheterogeneousLanInput\b", alternate_input, alternate))
    if seen != set(exports):
        raise ValueError("runtime export inventory changed")
    source = "".join(blocks)
    cases = []
    for payload, suffix in [(37, ""), (41, "_41")]:
        # The unit, mapped arrow and two cocone mediators have distinct results.
        answers = [payload + 1, payload + 3, payload + 5, payload + 13]
        cases.extend((payload, name, name + suffix, value)
                     for name, value in zip(exports, answers, strict=True))
    completed = 0
    hosts = set()
    deadline = time.monotonic() + 110

    def run(command, limit):
        remaining = min(limit, deadline - time.monotonic())
        if remaining <= 0:
            raise TimeoutError("runtime budget exhausted")
        return subprocess.run(list(map(str, command)), cwd=ROOT,
                              capture_output=True, text=True, timeout=remaining)

    with tempfile.TemporaryDirectory(prefix="mechanism-heterogeneous-left-kan-") as directory:
        work = Path(directory)
        fixture = work / "source.mech"
        fixture.write_text(source)
        expected = "".join(f"{actual}\t{value}\n" for _, _, actual, value in cases)
        emitted = run([ROOT / "_build/default/test/prelude_runtime.exe", fixture,
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
