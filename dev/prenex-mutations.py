"""Build isolated prenex mutants and require the designated test refusal."""

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) not in (2, 3) or (len(sys.argv) == 3 and sys.argv[2] != "--families"):
    raise SystemExit("usage: python3 -I dev/prenex-mutations.py NEW_WORK_DIRECTORY [--families]")
FAMILIES = len(sys.argv) == 3
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
    executable = "prenex_families.exe" if FAMILIES else "prenex.exe"
    return run(name, [str(COPY / "_build/default/test" / executable), str(COPY)])


controls = [
    ("C-PRENEX-M1", "surface/parser.ml",
     "Ok (Universe.Var index, rest)", "Ok (Universe.Var (Int.min index 0), rest)",
     "PRENEX-FAIL universe: the former lives at 0 and the expected universe is 1"),
    ("C-PRENEX-M2", "surface/universe.ml",
     "Ok (Level.imax u v)", "Ok (Level.max u v)",
     "PRENEX-FAIL universe: the former lives at imax(u0, u1) and the expected universe is max(u0, u1)"),
    ("C-PRENEX-M3", "surface/elab.ml",
     "Check.make ~level_arity:arity g budget", "Check.make ~level_arity:0 g budget",
     "PRENEX-FAIL universe: universe level is outside the global parameter scope"),
    ("C-PRENEX-M4", "surface/elab.ml",
     "  elab_program_in ~budget globals ds",
     "  elab_program_in ~budget:(if Budget.exhausted budget then Budget.unlimited else budget) globals ds",
     "PRENEX-FAIL budget: expected refusal"),
    ("C-PRENEX-M5", "surface/elab.ml",
     "if Option.is_some (Poly.arity catalog name) then",
     "if Option.is_some (Poly.arity catalog name) && String.equal name \"\" then",
     "PRENEX-FAIL template-collision: expected refusal"),
    ("C-PRENEX-M6", "surface/elab.ml",
     "Poly.instantiate ~budget g catalog ~name ~levels ~as_name",
     "Poly.instantiate ~budget g catalog ~name ~levels:(List.rev levels) ~as_name",
     "PRENEX-FAIL universe: the former lives at 1 and the expected universe is 0"),
    ("C-PRENEX-M7", "test/fixtures/prelude/prenex.mech",
     "dataIdentity Tiny (tinySucc tinyZero)", "dataIdentity Tiny tinyZero",
     "PRENEX-FAIL wrong computation: dataValue = (In SMu Tiny [] (ACtor tinyZero) [])"),
]

if FAMILIES:
    controls = [
        ("C-PRENEX-FAM-M1", "surface/elab.ml",
         "Check.make ~level_arity:arity globals budget", "Check.make ~level_arity:0 globals budget",
         "PRENEX-FAMILIES-FAIL universe: universe level is outside the global parameter scope"),
        ("C-PRENEX-FAM-M2", "surface/elab.ml",
         "Family_poly.instantiate ~budget g families ~name ~levels ~as_name",
         "Family_poly.instantiate ~budget g families ~name ~levels:(List.rev levels) ~as_name",
         "PRENEX-FAMILIES-FAIL universe: the former lives at 1 and the expected universe is 2"),
        ("C-PRENEX-FAM-M3", "surface/elab.ml",
         "else if Option.is_some (Family_poly.arity families name) then",
         "else if Option.is_some (Family_poly.arity families name) && String.equal name \"\" then",
         "PRENEX-FAMILIES-FAIL family-template-collision: expected refusal"),
        ("C-PRENEX-FAM-M4", "surface/elab.ml",
         "Ok (installed, catalog, families, rows)", "Ok (g, catalog, families, rows)",
         "PRENEX-FAMILIES-FAIL unbound: DataBox"),
        ("C-PRENEX-FAM-M5", "surface/elab.ml",
         "Poly.arity catalog ctor.Positivity.c_name",
         "Poly.arity catalog (ctor.Positivity.c_name ^ \":missing\")",
         "PRENEX-FAMILIES-FAIL late-constructor-collision: expected refusal"),
        ("C-PRENEX-FAM-M6", "surface/elab.ml",
         "elab_family_template ~budget g families ~arity fm",
         "elab_family_template ~budget:Budget.unlimited g families ~arity fm",
         "PRENEX-FAMILIES-FAIL budget: expected refusal"),
        ("C-PRENEX-FAM-M7", "test/fixtures/prelude/prenex-families.mech",
         "def dataBox : DataBox Tiny := box (tinySucc tinyZero)",
         "def dataBox : DataBox Tiny := box tinyZero",
         "PRENEX-FAMILIES-FAIL wrong computation: dataValue = (In SMu Tiny [] (ACtor tinyZero) [])"),
        ("C-PRENEX-FAM-M8", "surface/elab.ml",
         "Option.is_some (Family_poly.arity families ctor.Positivity.c_name)",
         "Option.is_some (Family_poly.arity families (ctor.Positivity.c_name ^ \":missing\"))",
         "PRENEX-FAMILIES-FAIL late-family-template-collision: expected refusal"),
        ("C-PRENEX-FAM-M9", "surface/elab.ml",
         "Family_poly.instantiate ~budget g families ~name ~levels ~as_name",
         "Family_poly.instantiate ~budget:Budget.unlimited g families ~name ~levels ~as_name",
         "PRENEX-FAMILIES-FAIL specialize-budget: expected refusal"),
        ("C-PRENEX-FAM-M10", "surface/elab.ml",
         "String.equal fc.Syntax.fc_name fm.Syntax.fm_name",
         "String.equal fc.Syntax.fc_name (fm.Syntax.fm_name ^ \":missing\")",
         "PRENEX-FAMILIES-FAIL self-named-constructor: expected refusal"),
        ("C-PRENEX-FAM-M11", "surface/elab.ml",
         "String.equal ctor.Positivity.c_name as_name",
         "String.equal ctor.Positivity.c_name (as_name ^ \":missing\")",
         "PRENEX-FAMILIES-FAIL instance-own-constructor: expected refusal"),
    ]

build("baseline-build")
if suite("baseline").returncode != 0:
    raise SystemExit("baseline failed")

reports = []
for name, relative, old, new, diagnostic in controls:
    path = COPY / relative
    original = path.read_bytes()
    source = original.decode()
    if source.count(old) != 1:
        raise SystemExit(f"{name}: mutation anchor is not unique")
    try:
        path.write_text(source.replace(old, new))
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
passed = all(report["killed"] for report in reports) and suite("restored").returncode == 0
(WORK / "results.json").write_text(json.dumps({"passed": passed, "controls": reports}, indent=2) + "\n")
print(json.dumps({"passed": passed, "killed": sum(report["killed"] for report in reports),
                  "controls": len(reports)}))
raise SystemExit(0 if passed else 1)
