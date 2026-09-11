"""Replay isolated category and dependent-record regression controls."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
VERIFY = len(sys.argv) == 3 and sys.argv[1] == "--verify"
if len(sys.argv) != 2 and not VERIFY:
    raise SystemExit("usage: python3 -I dev/category-mutations.py [--verify] WORK_DIRECTORY")
WORK = Path(sys.argv[-1]).resolve()
if not VERIFY and (WORK.exists() or WORK == ROOT or ROOT in WORK.parents):
    raise SystemExit("the work directory must be new and outside the source repository")
COPY = WORK / "copy"
if not VERIFY:
    WORK.mkdir(parents=True)
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
    if result.returncode or b"0 errors, 0 warnings" not in result.stdout:
        raise SystemExit(f"{name}: build failed; this is not a killed mutation")


def suite(name):
    return run(name, [str(COPY / "_build/default/test/prelude_category.exe"), str(COPY)])


controls = [
    ("C-CAT-M1", "surface/elab.ml",
     "vs Quantity.One first_motive (Rules.proj_branch q 0)",
     "vs Quantity.One (Option.bind first_motive (fun _ -> None)) (Rules.proj_branch q 0)",
     "PRELUDE-CATEGORY-FAIL cannot infer: an elimination without a motive needs an expected type"),
    ("C-CAT-M2", "lib/rules.ml",
     "let* eq = ops.o_conv ctx ~ty got want in",
     "let* eq = ops.o_conv_type ctx got want in\n      let _ = ty in",
     "PRELUDE-CATEGORY-FAIL mismatch: the constructor categoryRefl of Small gives the index"),
    ("C-CAT-M3", "lib/eval.ml", "e_motive = motive;",
     "e_motive = Option.bind motive (fun _ -> se.Value.s_motive);",
     "PRELUDE-CATEGORY-FAIL universe:"),
    ("C-CAT-M4", "lib/eval.ml", "e_branches = branches;",
     "e_branches = (let _ = branches in se.Value.s_branches);",
     "PRELUDE-CATEGORY-FAIL unbound: de Bruijn index 2 is outside the context"),
    ("C-CAT-M5", "lib/rules.ml", "if eq then Ok (want :: index_env)",
     "if eq then Ok index_env",
     "PRELUDE-CATEGORY-FAIL unbound: de Bruijn index 0 is outside the environment"),
    ("C-CAT-M6", "lib/rules.ml", "if eq then Ok (want :: index_env)",
     "if eq || true then Ok (want :: index_env)",
     "PRELUDE-CATEGORY-FAIL unequal-endpoint: expected refusal"),
    ("C-CAT-M7", "prelude/cat/category.mech",
     "=> category.2.2.2.2.1", "=> category.2.2.2.1",
     "PRELUDE-CATEGORY-FAIL mismatch:"),
    ("C-CAT-M8", "test/fixtures/prelude/category.mech",
     "(fun (n : Tiny) => next n) (fun (n : Tiny) => zero) (next zero)",
     "(fun (n : Tiny) => zero) (fun (n : Tiny) => next n) (next zero)",
     "PRELUDE-CATEGORY-FAIL wrong computation: compositionValue ="),
    ("C-CAT-M9", "lib/rules.ml",
     "s Quantity.One first_motive (proj_branch q 0)",
     "s Quantity.One (Option.bind first_motive (fun _ -> None)) (proj_branch q 0)",
     "PRELUDE-CATEGORY-FAIL mismatch: the constructor categoryRefl of Up gives the index c"),
    ("C-CAT-M10", "lib/eval.ml", "fun i -> Value.var (size + i)",
     "fun i -> Value.var (size + arity - 1 - i)",
     "PRELUDE-CATEGORY-FAIL mismatch: the term has type "
     "(Lan SMu IndexedValue [Ty; value]"),
]

if VERIFY:
    report = json.loads((WORK / "results.json").read_text())
    rows = {row["name"]: row for row in report["controls"]}
    checks = []
    for name, relative, old, new, diagnostic in controls:
        row = rows.get(name, {})
        passed = (report["passed"] and row.get("killed") and row.get("exit_code") == 1
                  and row.get("path") == relative and row.get("old") == old and row.get("new") == new
                  and row.get("source_sha256") == hashlib.sha256((ROOT / relative).read_bytes()).hexdigest()
                  and (WORK / f"{name}.stdout").read_bytes().startswith(diagnostic.encode())
                  and (WORK / f"{name}.stderr").read_bytes() == b"")
        checks.append({"name": name, "verified": bool(passed), "diagnostic": diagnostic})
    passed = len(rows) == len(controls) and all(row["verified"] for row in checks)
    result = {"passed": passed, "controls": checks}
    (WORK / "verification.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"passed": passed, "verified": sum(row["verified"] for row in checks)}))
    raise SystemExit(0 if passed else 1)

build("baseline-build")
if suite("baseline").returncode:
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
        mutant_hash = hashlib.sha256(path.read_bytes()).hexdigest()
        build(name + "-build")
        result = suite(name)
        killed = result.returncode == 1 and result.stdout.startswith(diagnostic.encode())
        reports.append({"name": name, "path": relative, "old": old, "new": new,
                        "source_sha256": hashlib.sha256(original).hexdigest(),
                        "mutant_sha256": mutant_hash, "exit_code": result.returncode,
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
