"""Compare complete and reachable erasure, including shared exports and refusals."""
from pathlib import Path
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
GATE = "PRELUDE-RUNTIME-REACHABLE"
SOURCE = """def hidden : Type := Nat
def invoke : (Nat -> Nat) -> Nat := fun (f : Nat -> Nat) => f 0
def captured : Nat -> Nat -> Nat := fun (n : Nat) (ignored : Nat) => n
def helper : Nat -> Nat := fun (n : Nat) => invoke (fun (ignored : Nat) => invoke (captured n))
def unused : Nat := 99
def alias : Nat -> Nat := helper
def one : Nat := alias 7
def two : Nat := alias 13
"""


def main():
    deadline = time.monotonic() + 120

    def run(command):
        remaining = min(30, deadline - time.monotonic())
        if remaining <= 0:
            raise TimeoutError("timed out after the 120 s total budget")
        return subprocess.run(list(map(str, command)), cwd=ROOT, capture_output=True,
                              text=True, timeout=remaining)

    driver = ROOT / "_bend2/test/prelude_runtime.exe"
    selected = run([driver, "--slice-self-test"])
    if selected.returncode or selected.stderr or selected.stdout != "RUNTIME-SLICE-OK\n":
        raise ValueError(f"selection self-test: {selected.stdout!r} {selected.stderr!r}")
    cases = [(False, ["one", "two"]), (True, ["one", "two"]),
             (True, ["two", "one"]), (True, ["one", "one", "two"])]
    answers = {"one": 7, "two": 13}
    comparisons = 0
    with tempfile.TemporaryDirectory(prefix="mechanism-runtime-reachable-") as directory:
        work = Path(directory)
        source = work / "source.mech"
        source.write_text(SOURCE)
        for index, (reachable, names) in enumerate(cases):
            output = work / str(index)
            output.mkdir()
            emitted = run([driver, *(["--reachable"] if reachable else []), source, output, *names])
            expected = "".join(f"{name}\t{answers[name]}\n" for name in names)
            if emitted.returncode or emitted.stderr or emitted.stdout != expected:
                raise ValueError(f"program {index}: {emitted.stdout!r} {emitted.stderr!r}")
            comparisons += len(names)
            for name in names:
                wasm = output / f"{name}.wasm"
                for host, command in [
                    ("node", ["node", ROOT / "dev/run-node.mjs", wasm, name]),
                    ("wasmtime", ["zsh", ROOT / "dev/run-wasmtime.sh", wasm, name]),
                ]:
                    result = run(command)
                    if result.returncode or result.stderr or result.stdout != f"{answers[name]}\n":
                        raise ValueError(f"program {index}/{name}/{host}: {result.stdout!r} {result.stderr!r}")
                    comparisons += 1
        refusals = [
            (SOURCE, "missing", "missing runtime dependency: missing"),
            (SOURCE, "../one", "invalid export path"),
            ("axiom forbidden : Nat\n" + SOURCE, "one", "runtime fixture declares axioms"),
            ("def invalid : Nat := Type\n" + SOURCE, "one",
             "mismatch: the term has type Type 2 and the expected type is Nat"),
        ]
        for index, (text, name, diagnostic) in enumerate(refusals):
            source.write_text(text)
            output = work / f"negative-{index}"
            output.mkdir()
            result = run([driver, "--reachable", source, output, name])
            if result.returncode != 1 or list(output.iterdir()):
                raise ValueError(f"refusal {index} accepted or emitted a module")
            if diagnostic not in result.stdout + result.stderr:
                raise ValueError(f"refusal {index}: {result.stdout!r} {result.stderr!r}")
    exports = sum(len(names) for _, names in cases)
    if comparisons != 3 * exports:
        raise ValueError(f"incomplete comparisons: {comparisons}")
    print(f"{GATE} OK programs={len(cases)} exports={exports} hosts=3 "
          f"comparisons={comparisons} negatives={len(refusals)}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.SubprocessError, TimeoutError, ValueError) as error:
        print(f"{GATE} FAIL {error}")
        raise SystemExit(1)
