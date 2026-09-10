"""Build and reject isolated dependent-prelude mutants, then restore the source."""

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit("usage: python3 -I dev/dependent-mutations.py NEW_WORK_DIRECTORY")
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
    return run(name, [str(COPY / "_build/default/test/prelude_dependent.exe"), str(COPY)])


build("baseline-build")
if suite("baseline").returncode != 0:
    raise SystemExit("baseline failed")

library = "prelude/dependent.ml"
projection = ("PRELUDE-DEPENDENT-FAIL mismatch: the term has type"
              " (Out SPi w point A (APt w x) B) and the expected type is A")
carrier = ("PRELUDE-DEPENDENT-FAIL mismatch: the term has type A"
           " and the expected type is (Out SPi w point A (APt w x) B)")
motive = ("PRELUDE-DEPENDENT-FAIL mismatch: the term has type"
          " (Out SPi w point (Lan SPi w point A (Out SPi w point A (APt w point) B))"
          " (APt w (In SPi w point A (APt w x) [y])) P)")
controls = [
    ("C-DEP-M1", library, "Level.imax u v", "Level.max u v",
     "PRELUDE-DEPENDENT-FAIL universe: the former lives at imax(u0, u1)"),
    ("C-DEP-M2", library,
     "Level.max (Level.succ u) (Level.succ v)", "Level.succ u",
     "PRELUDE-DEPENDENT-FAIL universe: the former lives at max(max(0, (u0 + 1)), (u1 + 1))"),
    ("C-DEP-M3", library,
     "(first 2 (Term.Var 0)) in",
     "(eliminate 2 (Term.Var 0) (Term.Var 3) (Rules.proj_branch Quantity.Many 1)) in",
     projection),
    ("C-DEP-M4", library,
     "(Rules.proj_branch Quantity.Many 1)) in",
     "(Rules.proj_branch Quantity.Many 0)) in", carrier),
    ("C-DEP-M5", library,
     "(apply (Term.Var 6) (Term.Var 3) (Term.Var 1)) (Term.Var 0)",
     "(apply (Term.Var 6) (Term.Var 3) (Term.Var 1)) (Term.Var 1)", carrier),
    ("C-DEP-M6", library,
     "(pair 3 (Term.Var 1) (Term.Var 0))",
     "(pair 3 (Term.Var 0) (Term.Var 1))", projection),
    ("C-DEP-M7", "test/fixtures/prelude/dependent.mech",
     "SmallMk MechNat (fun (x : MechNat) => MechNat) mechZero (mechSucc mechZero)",
     "SmallMk MechNat (fun (x : MechNat) => MechNat) mechZero mechZero",
     "PRELUDE-DEPENDENT-FAIL wrong computation: pairSecond"),
    ("C-DEP-M8", "test/prelude_dependent.ml",
     '"mechSucc mechZero", "unitValue",',
     '"mechSucc mechZero", "mechSucc mechZero",',
     "PRELUDE-DEPENDENT-FAIL wrong-fiber: expected refusal:"),
    ("C-DEP-M9", library,
     "(apply (sigma 5 4) (Term.Var 3) (Term.Var 0))",
     "(apply (sigma 5 4) (Term.Var 3) (Term.Var 1))", motive),
    ("C-DEP-M10", library,
     "Mechanism_surface.Poly.declare ~budget globals",
     "Mechanism_surface.Poly.declare ~budget:Budget.unlimited globals",
     "PRELUDE-DEPENDENT-FAIL budget:"),
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
