"""Compile isolated composition mutants and require named suite failures."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit("usage: python3 -I dev/composition-mutations.py NEW_WORK_DIRECTORY")
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
    result = subprocess.run(command, cwd=COPY, env=ENV, capture_output=True, timeout=180)
    (WORK / f"{name}.stdout").write_bytes(result.stdout.replace(str(COPY).encode(), b"<copy>"))
    (WORK / f"{name}.stderr").write_bytes(result.stderr.replace(str(COPY).encode(), b"<copy>"))
    return result


def build(name):
    result = run(name, ["zsh", str(COPY / "dev/dunecho.sh"), "build"])
    if result.returncode or b"0 errors, 0 warnings" not in result.stdout:
        raise SystemExit(f"{name}: build failed; this is not a killed mutation")


def suite(name):
    return run(name, [str(COPY / "_build/default/test/template_composition.exe"), str(COPY)])


relative = "surface/family_poly.ml"
controls = [
    ("C-COMP-M1", 'String.equal n scheme.family.Check.fam_name\n      || List.exists',
     'List.exists', "TEMPLATE-COMPOSITION-FAIL mismatch: the name Left is already declared"),
    ("C-COMP-M2", 'let definitions = List.concat_map snd groups in',
     'let definitions = [] in', "TEMPLATE-COMPOSITION-FAIL unbound: Left_unbox"),
    ("C-COMP-M3", 'String.equal as_name name || List.mem as_name aliases',
     'String.equal as_name name',
     "TEMPLATE-COMPOSITION-FAIL duplicate-disjoint-prefix: expected refusal: mismatch: the name Shared is already declared"),
    ("C-COMP-M4", 'if String.equal declaration.Check.d_name name then Error (collision name)',
     'if false then Error (collision name)',
     "TEMPLATE-COMPOSITION-FAIL raw-group-name: expected refusal: mismatch: the name Composed is already declared"),
    ("C-COMP-M5", 'let members = List.map (fun member _globals -> Ok member) definitions @ members in',
     'let members = List.map (fun member _globals -> Ok member) definitions in',
     "TEMPLATE-COMPOSITION-FAIL unbound: P_transfer"),
    ("C-COMP-M6", '          let level l = Level.subst levels l |> Option.to_result ~none:scope_error in',
     '          let level l = Level.subst (List.rev levels) l |> Option.to_result ~none:scope_error in',
     "TEMPLATE-COMPOSITION-FAIL mismatch: the term has type"),
    ("C-COMP-M7", 'Ok (as_name :: generated @ aliases, (families, definitions) :: groups))',
     'Ok (as_name :: List.filter (fun _name -> false) generated @ aliases, (families, definitions) :: groups))',
     "TEMPLATE-COMPOSITION-FAIL prefix-generated-family: expected refusal: mismatch: the name X_One is already declared"),
]

build("baseline-build")
baseline = suite("baseline")
if baseline.returncode or not baseline.stdout.startswith(b"TEMPLATE-COMPOSITION-OK "):
    raise SystemExit("baseline suite failed")
reports = []
for name, old, new, diagnostic in controls:
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
        killed = result.returncode == 1 and not result.stderr and result.stdout.startswith(diagnostic.encode())
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
passed = passed and restored.stdout.startswith(b"TEMPLATE-COMPOSITION-OK ") and not restored.stderr
(WORK / "results.json").write_text(json.dumps({"passed": passed, "controls": reports,
    "suite_sha256": hashlib.sha256((COPY / "test/template_composition.ml").read_bytes()).hexdigest()},
    indent=2) + "\n")
print(json.dumps({"passed": passed, "killed": sum(report["killed"] for report in reports),
                  "controls": len(reports)}))
raise SystemExit(0 if passed else 1)
