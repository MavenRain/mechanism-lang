#!/usr/bin/env python3
"""Exercise the pinned Veil examples through Mechanism's libraries and driver."""
import difflib
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
VEIL = ROOT / "vendor/veil"
MECH = ROOT / "_build/default/bin/mech.exe"

# Veil's CIRCUIT leg diffs these fixtures as well as the three packs,
# because the rule for a field of an introduction at SMu decides whether
# their rows read a depth or a refusal.
CIRCUIT_FIXTURES = (("mu-dependent-layout", "test/fixtures/mu-dependent-layout.kan"),
                    ("one-fields", "test/fixtures/one-fields.kan"),
                    ("spine", "test/circuit-spine.kan"))


def run(*args, expected_code=0):
    """Run a command and give its stdout and its stderr.

    An `expected_code` of None ignores the exit code, because Veil's
    circuit verb exits 1 on a refused row by design, so the leg reads
    stdout. stderr is never the verdict; `compare` prints it in the
    failure message.
    """
    result = subprocess.run([str(arg) for arg in args], cwd=ROOT,
                            capture_output=True, timeout=60, check=False)
    if expected_code is not None and result.returncode != expected_code:
        raise ValueError(f"{args}: exit={result.returncode}\n"
                         + result.stdout.decode(errors="replace")
                         + result.stderr.decode(errors="replace"))
    return (result.stdout, result.stderr)


def compare(label, expected, streams):
    """Require the golden bytes on stdout, and show a diff on a failure."""
    stdout, stderr = streams
    if expected and stdout == expected:
        return
    delta = "".join(difflib.unified_diff(
        expected.decode(errors="replace").splitlines(keepends=True),
        stdout.decode(errors="replace").splitlines(keepends=True),
        fromfile="golden", tofile="run"))
    raise ValueError(f"{label} differs from the Veil pin\n" + delta
                     + stderr.decode(errors="replace"))


def main():
    work = ROOT / ".gatework"
    work.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="veil-", dir=work) as directory:
        for name in ("zk-pack", "fhc-pack", "mpc-pack"):
            source = VEIL / "test/shapes" / f"{name}.kan"
            for command, suffix, code in ((["check", "--print"], "checked", 0),
                                          (["circuit"], "circuit", None),
                                          (["axioms"], "axioms", 0)):
                expected = (VEIL / "test/golden" / f"{name}.{suffix}").read_bytes()
                compare(f"{name}: {suffix}", expected,
                        run(MECH, *command, source, expected_code=code))
            # The three pack host fixtures carry the checked, circuit and
            # disclosure goldens and run the plaintext reactor. Veil's HOST
            # and HOST-NAT legs drive host-nat.kan, nat-bytes.kan and
            # zk-instance.kan, and this gate does not inherit them;
            # dev/VEIL-KERNEL.md records that cut.
            wasm = Path(directory) / f"{name}.wasm"
            run(MECH, "build", VEIL / "runtime/reactor.kan",
                VEIL / "test/host" / f"{name}.kan", "-o", wasm, "--export", "main")
            expected = (VEIL / "test/golden" / f"{name}.run").read_bytes()
            for runner in ("run-node.mjs", "run-wasmtime.sh"):
                command = "node" if runner.endswith(".mjs") else "zsh"
                compare(f"{name}: runtime on {runner}", expected,
                        run(command, ROOT / "dev" / runner, wasm, "main"))
        for fixture, path in CIRCUIT_FIXTURES:
            expected = (VEIL / "test/golden" / f"circuit-{fixture}.circuit").read_bytes()
            compare(f"circuit-{fixture}", expected,
                    run(MECH, "circuit", VEIL / path, expected_code=None))
    print("VEIL-KERNEL OK shapes=3 hosts=2")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"VEIL-KERNEL FAIL {error}", file=sys.stderr)
        sys.exit(1)
