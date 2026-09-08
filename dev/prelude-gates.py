#!/usr/bin/env python3
"""Check the prelude, its axiom boundary, and the reproducible inventory."""

from pathlib import Path
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
MECH = ROOT / "_build/default/bin/mech.exe"
AUDIT = ROOT / "_build/default/test/prelude.exe"
CORPUS = Path(os.environ.get("MECHANISM_UAT_CORPUS", "/Users/oobi/Documents/kanon-m2-corpus"))
EXPORT = CORPUS / "corpus/lean-parity/uat/uat.export"


def run(*args):
    return subprocess.run([str(arg) for arg in args], cwd=ROOT,
                          text=True, capture_output=True, timeout=30, check=False)


def run_bytes(*args):
    return subprocess.run([str(arg) for arg in args], cwd=ROOT,
                          capture_output=True, timeout=30, check=False)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def axioms():
    prelude = ROOT / "prelude/init.mech"
    result = run(MECH, "axioms", prelude)
    require(result.returncode == 0 and result.stdout == "" and result.stderr == "",
            f"prelude axioms: {result.stdout}{result.stderr}")
    result = run(AUDIT, "--audit", prelude)
    require(result.returncode == 0 and result.stdout == "PRELUDE-AXIOMS OK\n",
            f"empty-environment audit: {result.stdout}{result.stderr}")
    with tempfile.TemporaryDirectory(prefix="mech-axioms-") as directory:
        fixture = Path(directory) / "one-axiom.mech"
        fixture.write_text("axiom StageCAxiom : Prop\n")
        result = run(MECH, "axioms", fixture)
        require(result.returncode == 0 and result.stdout == "StageCAxiom\n",
                f"axiom disclosure: {result.stdout}{result.stderr}")
        fixture.write_text(prelude.read_text() + "\naxiom CounterfeitInit : Type 0\n")
        result = run(AUDIT, "--audit", fixture)
        require(result.returncode == 1 and "prelude postulate: CounterfeitInit" in result.stderr,
                "a postulated Init type escaped the empty-environment audit")
        original = "mu MechUnit : Type 0 with\n| mechUnit : MechUnit"
        source = prelude.read_text()
        require(source.count(original) == 1, "the Init replacement mutation lost its target")
        fixture.write_text(source.replace(original,
            "axiom MechUnit : Type 0\naxiom mechUnit : MechUnit", 1))
        result = run(AUDIT, "--audit", fixture)
        require(result.returncode == 1 and "prelude postulate: MechUnit" in result.stderr,
                "postulating the Unit family escaped the axiom gate")
    print("AXIOMS OK prelude=0 fixture=1 hidden_builtins=0")


def mapping():
    result = run(ROOT / "_build/default/test/mapping.exe")
    require(result.returncode == 0 and "MAPPING-OK\n" in result.stdout,
            f"mapping tests: {result.stdout}{result.stderr}")
    for name, flags in [("prelude.map.tsv", []), ("NEVER.tsv", ["--never"])]:
        result = run_bytes(MECH, "map-inventory", "--export", EXPORT, *flags)
        require(result.returncode == 0, f"inventory: {result.stdout}{result.stderr}")
        require(result.stdout == (ROOT / "map" / name).read_bytes(),
                f"{name} does not match the source inventory")
    print("MAP-INVENTORY OK")


def main():
    if sys.argv[1:] == ["axioms"]:
        axioms()
    elif sys.argv[1:] == ["mapping"]:
        mapping()
    else:
        print("usage: prelude-gates.py axioms|mapping", file=sys.stderr)
        return 64
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"PRELUDE-GATE FAIL: {error}", file=sys.stderr)
        sys.exit(1)
