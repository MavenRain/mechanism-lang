"""Build and reject isolated equality-operation mutants, then restore the source."""

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit("usage: python3 -I dev/equality-ops-mutations.py NEW_WORK_DIRECTORY")
WORK = Path(sys.argv[1]).resolve()
if WORK.exists() or WORK == ROOT or ROOT in WORK.parents:
    raise SystemExit("the work directory must be new and outside the source repository")
WORK.mkdir(parents=True)
COPY = WORK / "copy"
shutil.copytree(ROOT, COPY, ignore=shutil.ignore_patterns(
    ".git", "_build", ".gatework", ".kanon-exec", ".kanon-wait", "__pycache__"))
ENV = dict(os.environ)
ENV.pop("OPAM_SWITCH_PREFIX", None)
ENV.pop("CAML_LD_LIBRARY_PATH", None)


def run(name, command):
    result = subprocess.run(command, cwd=COPY, env=ENV, capture_output=True, timeout=300)
    (WORK / f"{name}.stdout").write_bytes(result.stdout)
    (WORK / f"{name}.stderr").write_bytes(result.stderr)
    return result


def build(name):
    result = run(name, ["zsh", str(COPY / "dev/dunecho.sh"), "build"])
    if result.returncode != 0 or b"0 errors, 0 warnings" not in result.stdout:
        raise SystemExit(f"{name}: build failed; this is not a killed mutation")


def suite(name):
    return run(name, [str(COPY / "_build/default/test/prelude_equality_ops.exe"), str(COPY)])


build("baseline-build")
if suite("baseline").returncode != 0:
    raise SystemExit("baseline failed")

library = "prelude/equality.ml"
mismatch = "PRELUDE-EQUALITY-OPS-FAIL mismatch:"
controls = [
    ("C-OPS-M1", library,
     'eliminate ~scrut:(Term.Var 0) ~branch:(Term.Var 2) "MechEq"',
     'eliminate ~scrut:(Term.Var 0) ~branch:(Term.Var 1) "MechEq"',
     'PRELUDE-EQUALITY-OPS-FAIL quantity: the erased binder y is read in a runtime position'),
    ("C-OPS-M2", library,
     '(eq (Term.Var 5) (Term.Var 1) (Term.Var 4))) in',
     '(eq (Term.Var 5) (Term.Var 4) (Term.Var 1))) in', mismatch),
    ("C-OPS-M3", library,
     'eliminate ~scrut:(Term.Var 0) ~branch:(Term.Var 1) "MechEq" (Term.Var 2)',
     'eliminate ~scrut:(Term.Var 1) ~branch:(Term.Var 1) "MechEq" (Term.Var 2)', mismatch),
    ("C-OPS-M4", library,
     '(apply (Term.Var 5) (Term.Var 3) (Term.Var 1)))',
     '(apply (Term.Var 5) (Term.Var 3) (Term.Var 2)))', mismatch),
    ("C-OPS-M5", library,
     '(type_eq (Term.Var 1) (Term.Var 4))) in',
     '(type_eq (Term.Var 4) (Term.Var 1))) in', mismatch),
    ("C-OPS-M6", library,
     'eliminate ~scrut:(Term.Var 0) ~branch:(Term.Var 1) "MechTypeEq" (Term.Var 2)',
     'eliminate ~scrut:(Term.Var 0) ~branch:(reflexive "MechTypeEq" (Term.Var 2)) '
     '"MechTypeEq" (Term.Var 2)', mismatch),
    ("C-OPS-M7", "test/fixtures/prelude/equality-ops.mech",
     '(mechSucc mechZero) mechZero (OpsData_refl MechNat mechZero)',
     'mechZero mechZero (OpsData_refl MechNat mechZero)',
     'PRELUDE-EQUALITY-OPS-FAIL mismatch: the constructor opsIsOne'),
    ("C-OPS-M8", "test/prelude_equality_ops.ml",
     '"OpsData_refl MechNat mechZero", "OpsHigher_refl MechNat mechZero",',
     '"OpsData_refl MechNat mechZero", "OpsData_refl MechNat mechZero",',
     'PRELUDE-EQUALITY-OPS-FAIL mixed-instance: expected refusal:'),
    ("C-OPS-M9", "test/prelude_equality_ops.ml",
     '"opsDependent", Term.In', '"opsJ", Term.In',
     'PRELUDE-EQUALITY-OPS-FAIL wrong computation: opsJ'),
    ("C-OPS-M10", library,
     '(at_motive (Term.Var 7) (Term.Var 6) (Term.Var 5) (Term.Var 1) (Term.Var 0))',
     '(at_motive (Term.Var 7) (Term.Var 6) (Term.Var 5) (Term.Var 0) (Term.Var 1))',
     'PRELUDE-EQUALITY-OPS-FAIL mismatch: the term has type (Lan SMu MechEq [right]'),
]

reports = []
for name, relative, old, new, diagnostic in controls:
    path = COPY / relative
    original = path.read_bytes()
    text = original.decode()
    if text.count(old) != 1:
        raise SystemExit(f"{name}: mutation anchor is not unique")
    try:
        path.write_text(text.replace(old, new))
        mutated = hashlib.sha256(path.read_bytes()).hexdigest()
        build(name + "-build")
        result = suite(name)
        killed = result.returncode == 1 and result.stdout.startswith(diagnostic.encode())
        reports.append({"name": name, "path": relative, "old": old, "new": new,
                        "source_sha256": hashlib.sha256(original).hexdigest(),
                        "mutant_sha256": mutated, "exit_code": result.returncode,
                        "diagnostic": diagnostic, "killed": killed})
        print(json.dumps({"control": name, "killed": killed}), flush=True)
    finally:
        path.write_bytes(original)

build("restored-build")
restored = suite("restored")
passed = all(report["killed"] for report in reports) and restored.returncode == 0
(WORK / "results.json").write_text(json.dumps({"passed": passed, "controls": reports}, indent=2) + "\n")
print(json.dumps({"passed": passed, "killed": sum(report["killed"] for report in reports),
                  "controls": len(reports)}))
raise SystemExit(0 if passed else 1)
