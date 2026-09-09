"""Run isolated, build-checked controls for polymorphic family members."""

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit("usage: python3 -I dev/transport-mutations.py NEW_WORK_DIRECTORY")
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


def suite(name, case=None):
    if case is None:
        command = [str(COPY / "_build/default/test/prelude_transport.exe"), str(COPY)]
    else:
        command = [str(COPY / "_build/default/test/family_members.exe"), case]
    return run(name, command)


build("baseline-build")
baseline = [suite("baseline-prelude"), run("baseline-members",
    [str(COPY / "_build/default/test/family_members.exe")])]
if any(result.returncode != 0 for result in baseline):
    raise SystemExit("baseline failed")

check = "let* entry = Check.check_decl_at ~arity globals budget decl in"
unchecked = "Ok (Global.Def { Global.ty = decl.d_ty; " \
    "def = Option.value ~default:Term.Auto decl.d_body; " \
    "reducible = true; rec_arg = None; partial = false })"
surface = "surface/family_poly.ml"
transport = "test/prelude_transport.ml"
member_poll = "let map_member budget level name (decl : Check.decl) =\n  let* () = poll budget in\n"
members_poll = "    let* globals = acc in\n    let* () = poll budget in\n"
name_guard = "    let* () = List.find_opt (fun (d : Check.decl) -> " \
    "List.mem_assoc d.d_name catalog) members\n      |> Option.fold ~none:(Ok ()) " \
    "~some:(fun (d : Check.decl) -> Error (collision d.d_name)) in\n"
controls = [
    ("C-TRANSPORT-M1", surface, check,
     "let* entry = if arity > 0 then " + unchecked +
     " else Check.check_decl_at ~arity globals budget decl in",
     "definition-must-check", "expected refusal: unbound: de Bruijn index 0"),
    ("C-TRANSPORT-M2", surface, 'then as_name ^ "_" ^ n else n', 'then n else n',
     "ordered-members-and-renaming", "the name witness is already declared"),
    ("C-TRANSPORT-M3", surface, check,
     "let* entry = if Int.equal arity 0 then " + unchecked +
     " else Check.check_decl_at ~arity globals budget decl in",
     "closed-rechecking", "expected refusal: mismatch: the term has type"),
    ("C-TRANSPORT-M4", surface,
     "map_list (map_member budget level rename) scheme.members",
     "map_list (map_member budget (fun l -> Ok l) rename) scheme.members",
     "hidden-level-specialization",
     "hidden-level-specialization: universe: universe level is outside the global parameter scope"),
    ("C-TRANSPORT-M5", "test/fixtures/prelude/transport.mech",
     "(CastData_refl MechNat) (mechSucc mechZero)",
     "(CastData_refl MechNat) mechZero", None,
     "the constructor transportIsOne of TransportIsOne gives the index"),
    ("C-TR-M1", surface, member_poll,
     "let map_member budget level name (decl : Check.decl) =\n",
     "declaration-member-budget", "declaration-member-budget: member declaration polls"),
    ("C-TR-M2", surface, members_poll, "    let* globals = acc in\n",
     "specialization-member-budget",
     "specialization-member-budget: member specialization polls"),
    ("C-TR-M3", surface, name_guard, "", "member-template-collision",
     "member-template-collision: wrong refusal: not yet: "
     "references between family schemas are not supported"),
    ("C-TR-M4", transport, "(TransportHigher_refl MechNat mechZero)",
     "(TransportData_refl MechNat mechZero)", None,
     "expected refusal: mismatch: the term has type (Lan SMu TransportHigher"),
    ("C-TR-M5", transport, '(globals, "unbound: CastData_cast",',
     '(installed, "unbound: CastData_cast",', None,
     "expected refusal: unbound: CastData_cast"),
]

reports = []
for name, relative, old, new, case, diagnostic in controls:
    path = COPY / relative
    original = path.read_bytes()
    text = original.decode()
    if text.count(old) != 1:
        raise SystemExit(f"{name}: mutation anchor is not unique")
    try:
        path.write_text(text.replace(old, new))
        mutated = hashlib.sha256(path.read_bytes()).hexdigest()
        build(name + "-build")
        result = suite(name, case)
        killed = result.returncode == 1 and diagnostic.encode() in result.stdout
        report = {"name": name, "path": relative, "case": case, "old": old, "new": new,
                  "source_sha256": hashlib.sha256(original).hexdigest(),
                  "mutant_sha256": mutated, "exit_code": result.returncode,
                  "diagnostic": diagnostic, "killed": killed}
        reports.append(report)
        print(json.dumps({"control": name, "killed": killed}), flush=True)
    finally:
        path.write_bytes(original)

build("restored-build")
restored = [suite("restored-prelude"), run("restored-members",
    [str(COPY / "_build/default/test/family_members.exe")])]
passed = all(report["killed"] for report in reports) and all(
    result.returncode == 0 for result in restored)
(WORK / "results.json").write_text(json.dumps({"passed": passed, "controls": reports}, indent=2) + "\n")
print(json.dumps({"passed": passed, "killed": sum(report["killed"] for report in reports),
                  "controls": len(reports)}))
raise SystemExit(0 if passed else 1)
