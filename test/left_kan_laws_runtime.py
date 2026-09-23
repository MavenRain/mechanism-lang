"""Compare certified left Kan mediator laws on three hosts."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
GATE = "PRELUDE-LEFT-KAN-LAWS-RUNTIME"
# The CATEGORY watchdog of dev/gates.sh is 900 s, so the total stays under it and
# every expiry reports "timed out after", which the gate ladder can recheck.
TOTAL_BUDGET = 540
EMIT_BUDGET = 480
HOST_BUDGET = 30


def main():
    if len(sys.argv) != 1:
        print("usage: python3 -I test/left_kan_laws_runtime.py", file=sys.stderr)
        return 64
    source = "\n".join((ROOT / path).read_text() for path in [
        "prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech",
        "prelude/cat/composable-functors.mech", "prelude/cat/heterogeneous-nattrans.mech",
        "prelude/cat/heterogeneous-whiskering.mech", "prelude/cat/shared-nattrans.mech",
        "prelude/cat/heterogeneous-left-kan.mech", "prelude/cat/shared-left-kan.mech",
        "prelude/cat/left-kan-laws.mech",
        "test/fixtures/prelude/shared-left-kan-runtime.mech",
        "test/fixtures/prelude/left-kan-laws-runtime.mech",
    ])
    anchor = "def sharedLanInput : Nat := 37"
    alternate_input = "sharedLanInput41"
    if source.count(anchor) != 1 or re.search(r"\b" + alternate_input + r"\b", source):
        raise ValueError("expected one payload anchor and a fresh alternate name")
    source = source.replace(anchor, anchor + f"\ndef {alternate_input} : Nat := 41")
    exports = ["lanIdentity", "lanIdentityReference", "lanCongr", "lanCongrReference",
               "lanOtherCongr", "lanOtherCongrReference"]
    seen = set()
    blocks = []
    for block in re.split(r"(?m)(?=^def )", source):
        blocks.append(block)
        declaration = re.match(r"def (\w+) : Nat :=", block)
        if declaration is None or declaration.group(1) not in exports:
            continue
        name = declaration.group(1)
        if name in seen:
            raise ValueError(f"duplicate export name: {name}")
        if re.search(r"\b" + name + r"_41\b", source):
            raise ValueError(f"reserved alternate name already present: {name}_41")
        seen.add(name)
        body = block.split(":=", 1)[1]
        references = 1
        if len(re.findall(r"\bsharedLanInput\b", body)) != references:
            raise ValueError(f"unexpected payload references in {name}")
        if any(re.search(r"\b" + export + r"\b", body) for export in exports):
            raise ValueError(f"export dependency needs explicit specialization: {name}")
        alternate = block.replace(f"def {name} : Nat :=", f"def {name}_41 : Nat :=", 1)
        blocks.append(re.sub(r"\bsharedLanInput\b", alternate_input, alternate))
    if seen != set(exports):
        raise ValueError("runtime export inventory changed")
    source = "".join(blocks)
    cases = []
    for payload, suffix in [(37, ""), (41, "_41")]:
        # The identity and two indexed cocones have independent arithmetic oracles.
        identity = 102 * payload + 718
        alpha = 103 * payload + 11
        beta = 103 * payload + 1126
        answers = [identity, identity, alpha, alpha, beta, beta]
        cases.extend((payload, name + suffix, value)
                     for name, value in zip(exports, answers, strict=True))
    deadline = time.monotonic() + TOTAL_BUDGET

    def run(command, limit):
        remaining = min(limit, deadline - time.monotonic())
        if remaining <= 0:
            raise TimeoutError(f"timed out after the {TOTAL_BUDGET} s total budget")
        return subprocess.run(list(map(str, command)), cwd=ROOT,
                              capture_output=True, text=True, timeout=remaining)

    completed = 0
    hosts = set()
    selected = run([ROOT / "_build/default/test/prelude_runtime.exe", "--slice-self-test"], HOST_BUDGET)
    if selected.returncode or selected.stderr or selected.stdout != "RUNTIME-SLICE-OK\n":
        print(f"{GATE} FAIL runtime selection check stdout={selected.stdout!r} stderr={selected.stderr!r}")
        return 1
    with tempfile.TemporaryDirectory(prefix="mechanism-left-kan-laws-") as directory:
        work = Path(directory)
        fixture = work / "source.mech"
        fixture.write_text(source)
        expected = "".join(f"{actual}\t{value}\n" for _, actual, value in cases)
        emitted = run([ROOT / "_build/default/test/prelude_runtime.exe", "--reachable", fixture,
                       work, *(actual for _, actual, _ in cases)], EMIT_BUDGET)
        if emitted.returncode or emitted.stderr or emitted.stdout != expected:
            print(f"{GATE} FAIL kernel-emit exit={emitted.returncode} "
                  f"stdout={emitted.stdout[:1500]!r} stderr={emitted.stderr[:1500]!r}")
            return 1
        hosts.add("kernel")
        for payload, actual, value in cases:
            wasm = work / f"{actual}.wasm"
            for host, command in [
                ("node", ["node", ROOT / "dev/run-node.mjs", wasm, actual]),
                ("wasmtime", ["zsh", ROOT / "dev/run-wasmtime.sh", wasm, actual]),
            ]:
                result = run(command, HOST_BUDGET)
                if result.returncode or result.stderr or result.stdout != f"{value}\n":
                    print(f"{GATE} FAIL {payload}/{actual}/{host} exit={result.returncode} "
                          f"stdout={result.stdout[:1500]!r} stderr={result.stderr[:1500]!r}")
                    return 1
                hosts.add(host)
            completed += 1
    if completed != 2 * len(exports) or hosts != {"kernel", "node", "wasmtime"}:
        print(f"{GATE} FAIL incomplete comparisons")
        return 1
    print(f"{GATE} OK cases={len(exports)} hosts={len(hosts)} payloads=2 comparisons={completed * 3}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.SubprocessError, TimeoutError, ValueError) as error:
        print(f"{GATE} FAIL {error}")
        raise SystemExit(1)
