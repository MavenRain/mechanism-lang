"""Compile isolated family-group and congruence mutants and require specific refusals."""

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit("usage: python3 -I dev/congruence-mutations.py NEW_WORK_DIRECTORY")
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


def suite(name, executable):
    return run(name, [str(COPY / f"_build/default/test/{executable}.exe"), str(COPY)])


build("baseline-build")
for executable in ["family_groups", "prelude_congruence"]:
    if suite("baseline-" + executable, executable).returncode != 0:
        raise SystemExit("baseline failed: " + executable)

library = "prelude/congruence.ml"
groups = "surface/family_poly.ml"
controls = [
    ("C-CONG-M1", library, '"B", Term.Univ v', '"B", Term.Univ u',
     "prelude_congruence", "PRELUDE-CONGRUENCE-FAIL mismatch: the term has type Type u0"
     " and the expected type is Type u1"),
    ("C-CONG-M2", library,
     '(apply (Term.Var 5) (Term.Var 3) (Term.Var 1)) in',
     '(apply (Term.Var 5) (Term.Var 3) (Term.Var 2)) in',
     "prelude_congruence", "PRELUDE-CONGRUENCE-FAIL mismatch: the term has type"
     " (Lan SMu Result [(Out SPi w point A (APt w y) f)]"),
    ("C-CONG-M3", groups,
     'scheme.companions -> as_name ^ "_" ^ n', 'scheme.companions -> n',
     "family_groups", "FAMILY-GROUPS-FAIL ordered-families-members-and-renaming:"
     " mismatch: the name Second is already declared"),
    ("C-CONG-M4", groups,
     'let* _checked = check_members budget catalog ~arity symbolic members in\n'
     '      Ok ((family.fam_name, { arity; family; ctors; companions; members }) :: catalog)',
     'let* _checked = Ok symbolic in\n'
     '      Ok ((family.fam_name, { arity; family; ctors; companions; members }) :: catalog)',
     "family_groups", "FAMILY-GROUPS-FAIL member-family-collision: expected refusal:"
     " mismatch: the name Second is already declared"),
    ("C-CONG-M5", library,
     'Family_poly.declare_group ~budget ~members:[congr]',
     'Family_poly.declare_group ~budget:Budget.unlimited ~members:[congr]',
     "prelude_congruence", "PRELUDE-CONGRUENCE-FAIL budget: expected refusal"),
    ("C-CONG-M6", "test/fixtures/prelude/congruence.mech",
     'case upProof as self in Up_Result right return MechNat with\n'
     '  | mechReflCtor => mechSucc mechZero',
     'case upProof as self in Up_Result right return MechNat with\n'
     '  | mechReflCtor => mechZero',
     "prelude_congruence", "PRELUDE-CONGRUENCE-FAIL wrong computation: upValue"),
    ("C-CONG-M7", "test/prelude_congruence.ml",
     '"Same_Result", "Again_Result",', '"Same_Result", "Same_Result",',
     "prelude_congruence", "PRELUDE-CONGRUENCE-FAIL wrong-instance: expected refusal:"),
    ("C-CONG-M8", groups,
     '      let name n = if List.mem_assoc n catalog then\n'
     '          Error (Error.Not_yet "references between family schemas are not supported")\n'
     '        else Ok n in',
     '      let name n = Ok n in',
     "family_groups", "FAMILY-GROUPS-FAIL member-template-reference: wrong refusal:"
     " unbound: the family Other is not declared"),
    ("C-CONG-M9", groups,
     '        if occupied globals catalog family.Check.fam_name then'
     ' Error (collision family.fam_name)\n'
     '        else\n'
     '          let* provisional = Check.declare_family ~budget globals family in',
     '          let* provisional = Check.declare_family ~budget globals family in',
     "family_groups", "FAMILY-GROUPS-FAIL target-companion-collision-atomic: expected refusal:"
     " mismatch: the name One_Second is already declared"),
]

reports = []
for name, relative, old, new, executable, diagnostic in controls:
    path = COPY / relative
    original = path.read_bytes()
    source = original.decode()
    if source.count(old) != 1:
        raise SystemExit(f"{name}: mutation anchor is not unique")
    try:
        path.write_text(source.replace(old, new))
        mutated = hashlib.sha256(path.read_bytes()).hexdigest()
        build(name + "-build")
        result = suite(name, executable)
        killed = result.returncode == 1 and result.stdout.startswith(diagnostic.encode())
        reports.append({"name": name, "path": relative, "old": old, "new": new,
                        "source_sha256": hashlib.sha256(original).hexdigest(),
                        "mutant_sha256": mutated, "exit_code": result.returncode,
                        "diagnostic": diagnostic, "killed": killed})
        print(json.dumps({"control": name, "killed": killed}), flush=True)
    finally:
        path.write_bytes(original)

build("restored-build")
restored = [suite("restored-" + executable, executable).returncode
            for executable in ["family_groups", "prelude_congruence"]]
passed = all(report["killed"] for report in reports) and restored == [0, 0]
(WORK / "results.json").write_text(json.dumps({"passed": passed, "controls": reports}, indent=2) + "\n")
print(json.dumps({"passed": passed, "killed": sum(report["killed"] for report in reports),
                  "controls": len(reports)}))
raise SystemExit(0 if passed else 1)
