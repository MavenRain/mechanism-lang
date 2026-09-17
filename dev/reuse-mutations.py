"""Compile isolated reuse mutants and require named suite verdicts."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) not in (2, 3) or (len(sys.argv) == 3 and sys.argv[2] != "--symbolic"):
    raise SystemExit("usage: python3 -I dev/reuse-mutations.py NEW_WORK_DIRECTORY [--symbolic]")
SYMBOLIC = len(sys.argv) == 3
SUITE = "template_symbolic_reuse" if SYMBOLIC else "template_reuse"
OK = b"TEMPLATE-SYMBOLIC-REUSE-OK negatives=12 parser=6 raw=5\n" if SYMBOLIC \
    else b"TEMPLATE-REUSE-OK negatives=17 parser=6 raw=6\n"
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
CAPTURES = {}


def run(name, command, limit):
    result = subprocess.run(command, cwd=COPY, env=ENV, capture_output=True, timeout=limit)
    out = result.stdout.replace(str(COPY).encode(), b"<copy>")
    err = result.stderr.replace(str(COPY).encode(), b"<copy>")
    (WORK / f"{name}.stdout").write_bytes(out)
    (WORK / f"{name}.stderr").write_bytes(err)
    CAPTURES[name] = {"stdout": out.decode(errors="replace"),
                      "stderr": err.decode(errors="replace")}
    return result


def build(name):
    result = run(name, ["zsh", str(COPY / "dev/dunecho.sh"), "build"], 1800)
    if result.returncode or b"0 errors, 0 warnings" not in result.stdout:
        raise SystemExit(f"{name}: build failed; this is not a killed mutation")


def suite(name):
    return run(name, [str(COPY / f"_build/default/test/{SUITE}.exe"), str(COPY)], 900)


surface = "surface/family_poly.ml"
controls = [
    # SC-L2-4, review round 2:  this mutant defeats the whole structural
    # comparison, so the suite stops at its first scenario that needs the
    # comparison, which is wrong-constructor and not wrong-level.  The
    # expectation names the verdict the suite really prints.
    ("C-REUSE-M1", surface, "| () when level_ok && actual = leveled -> Ok globals",
     "| () when level_ok && actual = actual -> Ok globals", 1,
     "TEMPLATE-REUSE-FAIL wrong-constructor: wrong refusal: missing branch: "
     "the elimination of Other has no branch at other"),
    ("C-REUSE-M2", surface,
     "(fun (c : Positivity.ctor) -> { c with Positivity.c_self_rec = false })",
     "(fun (c : Positivity.ctor) -> c)", 1,
     "TEMPLATE-REUSE-FAIL mutual-group-member: wrong refusal: mismatch: "
     "the reused family B does not match the template"),
    ("C-REUSE-M3", surface, """  let* level_ok = Level.equal_budget budget
    actual.Positivity.f_level expected.Positivity.f_level in""",
     "  let* level_ok = Ok (actual.Positivity.f_level = expected.Positivity.f_level) in", 1,
     "TEMPLATE-REUSE-FAIL mismatch: the reused family E does not match the template"),
    ("C-REUSE-M4", "surface/elab.ml", """          let* () = match () with
            | () when reuse = [] -> Ok ()
            | () when Option.is_none (Poly.arity catalog name) -> Ok ()
            | () -> Error (Error.Not_yet "family reuse requires a family template") in""",
     """          let* () = if reuse = [] then Ok ()
            else Error (Error.Not_yet "family reuse requires a family template") in""", 1,
     "TEMPLATE-REUSE-FAIL unknown-template-name: wrong refusal: not yet: "
     "family reuse requires a family template"),
    ("C-REUSE-M5", "test/template_reuse.ml", "    { original with Positivity.f_ctors = [] }] in",
     "    ] in", 0, "TEMPLATE-REUSE-OK negatives=17 parser=6 raw=5"),
]

if SYMBOLIC:
    controls = [
        ("C-SREUSE-M1", surface,
         "if reused then check_reuse ~arity budget symbolic family ctors",
         "if reused then Ok symbolic", 1,
         "TEMPLATE-SYMBOLIC-REUSE-FAIL independent-universes:"),
        ("C-SREUSE-M2", surface,
         "if reused then check_reuse ~arity budget symbolic family ctors",
         "if reused then check_reuse budget symbolic family ctors", 1,
         "TEMPLATE-SYMBOLIC-REUSE-FAIL universe:"),
        ("C-SREUSE-M3", "surface/parser.ml",
         "dependencies rest ((source, levels, as_name, reuse) :: acc)",
         "let _ignored = reuse in dependencies rest ((source, levels, as_name, []) :: acc)", 1,
         "TEMPLATE-SYMBOLIC-REUSE-FAIL mismatch:"),
        ("C-SREUSE-M4", surface,
         "| () when List.mem local bound ->",
         "| () when List.mem local [] ->", 1,
         "TEMPLATE-SYMBOLIC-REUSE-FAIL duplicate-binding:"),
    ]

build("baseline-build")
baseline = suite("baseline")
if baseline.returncode or baseline.stdout != OK or baseline.stderr:
    raise SystemExit("baseline suite failed")
reports = []
for name, relative, old, new, code, diagnostic in controls:
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
        killed = result.returncode == code and not result.stderr \
            and result.stdout.startswith(diagnostic.encode())
        reports.append({"name": name, "path": relative, "old": old, "new": new,
                        "source_sha256": hashlib.sha256(original).hexdigest(),
                        "mutant_sha256": mutated, "exit_code": result.returncode,
                        "expected_exit_code": code, "diagnostic": diagnostic,
                        "killed": killed, name: CAPTURES[name],
                        name + "-build": CAPTURES[name + "-build"]})
        print(json.dumps({"control": name, "killed": killed}), flush=True)
    finally:
        path.write_bytes(original)
build("restored-build")
restored = suite("restored")
passed = all(report["killed"] for report in reports) and restored.returncode == 0
passed = passed and restored.stdout == OK and not restored.stderr
(WORK / "results.json").write_text(json.dumps({"passed": passed, "controls": reports,
    "baseline_stdout": CAPTURES["baseline"]["stdout"],
    "restored_stdout": CAPTURES["restored"]["stdout"],
    "suite_sha256": hashlib.sha256((COPY / f"test/{SUITE}.ml").read_bytes()).hexdigest()},
    indent=2) + "\n")
print(json.dumps({"passed": passed, "killed": sum(report["killed"] for report in reports),
                  "controls": len(reports)}))
raise SystemExit(0 if passed else 1)
