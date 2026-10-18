"""Replay isolated category and dependent-record regression controls."""
import hashlib
import json
import runpy
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
bounded_run = runpy.run_path(str(ROOT / "dev/c-process.py"))["run"]
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
        ".git", "_build", "_bend2", "_tcc", ".gatework", ".kanon-exec", ".kanon-wait", ".kanon-replies", "build", "logs",
        "__pycache__"))
ENV = dict(os.environ)
ENV.pop("OPAM_SWITCH_PREFIX", None)
ENV.pop("CAML_LD_LIBRARY_PATH", None)
TARGET = "_tcc/prelude_category"


def run(name, command):
    result = bounded_run(command, cwd=COPY, env=ENV, capture_output=True, timeout=300)
    (WORK / f"{name}.stdout").write_bytes(result.stdout)
    (WORK / f"{name}.stderr").write_bytes(result.stderr)
    return result


def build(name):
    binary = COPY / TARGET
    binary.unlink(missing_ok=True)
    result = run(name, ["make", "-C", str(COPY), TARGET])
    if result.returncode or not os.access(binary, os.X_OK):
        raise SystemExit(f"{name}: build failed; this is not a killed mutation")
    return {"target": TARGET, "exit_code": result.returncode,
            "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest()}


def suite(name):
    return run(name, [str(COPY / TARGET), str(COPY)])


controls = [('C-CAT-M1',
  'c/surface/elab_term.c',
  '  MaybeMotive motive = term_motive_some(term_motive(str_none(), empty(heap), str_lit("self"), value));\n'
  '  StuckElim data = value_stuck_elim(f->shape, QUANTITY_ONE, motive,',
  '  MaybeMotive motive = term_motive_none();\n'
  '  (void)value;\n'
  '  StuckElim data = value_stuck_elim(f->shape, QUANTITY_ONE, motive,',
  'PRELUDE-CATEGORY-FAIL cannot infer: an elimination without a motive needs an expected type'),
 ('C-CAT-M2',
  'c/kernel/check_engine.c',
  'check_engine_conv_entry(heap, f->ctx, f->value3, got, f->value)',
  'check_engine_conv_type(heap, f->ctx, got, f->value)',
  'PRELUDE-CATEGORY-FAIL mismatch: the constructor categoryRefl of Small gives the index'),
 ('C-CAT-M3',
  'c/kernel/eval.c',
  'current = term_elim(heap, term_elim_data(s.value, current, data.scrut_q, mo.value, bs.value));',
  'current = term_elim(heap, term_elim_data(s.value, current, data.scrut_q, data.motive, bs.value));',
  'PRELUDE-CATEGORY-FAIL universe:'),
 # C-CAT-M4: the Bend anchor put the raw Value branches into the quoted Term.Elim (bend2/kernel/eval.bend
 # quote_frames, `+branches` ownership flip); the typed C port cannot express that confusion, so the C
 # control drops the read-back branches of the stuck eliminator instead (same rule: the quoted eliminator
 # must carry its read-back branches); the diagnostic follows the C port (hop 312, decision 279).
 ('C-CAT-M4',
  'c/kernel/eval.c',
  '        VecResult bs = eval_quote_branches(heap, mode, &data.branches, globals, data.env, size);',
  '        Vec none = vec_new(heap->arena);\n'
  '        VecResult bs = eval_quote_branches(heap, mode, &none, globals, data.env, size);',
  'PRELUDE-CATEGORY-FAIL missing branch: an elimination at the point shape has one branch'),
 ('C-CAT-M5',
  'c/kernel/check_engine.c',
  'f->vec4, cons(heap, (void *)f->value, &f->vec5));',
  'f->vec4, f->vec5);',
  'PRELUDE-CATEGORY-FAIL unbound: de Bruijn index 0 is outside the environment'),
 ('C-CAT-M6',
  'c/kernel/check_engine.c',
  'check_engine_index_equal_result(heap, kernel_bool_unbox(value), f->ctx, f->name2, f->name, f->value2,',
  'check_engine_index_equal_result(heap, kernel_bool_unbox(value) || true, f->ctx, f->name2, f->name, f->value2,',
  'PRELUDE-CATEGORY-FAIL unequal-endpoint: expected refusal'),
 ('C-CAT-M7',
  'prelude/cat/category.mech',
  '=> category.2.2.2.2.1',
  '=> category.2.2.2.1',
  'PRELUDE-CATEGORY-FAIL mismatch:'),
 ('C-CAT-M8',
  'test/fixtures/prelude/category.mech',
  '(fun (n : Tiny) => next n) (fun (n : Tiny) => zero) (next zero)',
  '(fun (n : Tiny) => zero) (fun (n : Tiny) => next n) (next zero)',
  'PRELUDE-CATEGORY-FAIL wrong computation: compositionValue ='),
 ('C-CAT-M9',
  'c/kernel/check_engine.c',
  'check_engine_conv_project(heap, f->ctx, f->shape, f->q, QUANTITY_ONE, conv_self_motive(heap, value),',
  'check_engine_conv_project(heap, f->ctx, f->shape, f->q, QUANTITY_ONE, term_motive_none(),',
  'PRELUDE-CATEGORY-FAIL mismatch: the constructor categoryRefl of Up gives the index c'),
 # C-CAT-M10: the Bend anchor reversed the fresh variables of quote_motive (bend2/kernel/eval.bend); the
 # motive quoted by this driver has arity 1 on C, where the reverse is the identity, so the C control
 # reverses the fresh variables at the leg site (eval_quote_leg; same rule: fresh variables are opened in
 # binder order during read-back); the diagnostic follows the C port (hop 312, decision 279).
 ('C-CAT-M10',
  'c/kernel/eval.c',
  '  Result value = eval_open_closure(heap, globals, leg.clo, eval_fresh_vars(heap, arity, size));',
  '  Vec fresh = eval_fresh_vars(heap, arity, size);\n'
  '  Result value = eval_open_closure(heap, globals, leg.clo, vec_reverse(heap->arena, &fresh));',
  'PRELUDE-CATEGORY-FAIL mismatch: the term has type (Lan SPi w identity')]

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
baseline = suite("baseline")
expected_ok = b"PRELUDE-CATEGORY-OK entries=181 computations=7 negatives=9 quotation=3\n"
if baseline.returncode or baseline.stderr or baseline.stdout != expected_ok:
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
        c_build = build(name + "-build")
        result = suite(name)
        killed = result.returncode == 1 and not result.stderr and result.stdout.startswith(diagnostic.encode())
        reports.append({"name": name, "path": relative, "old": old, "new": new,
                        "source_sha256": hashlib.sha256(original).hexdigest(),
                        "mutant_sha256": mutant_hash, "exit_code": result.returncode,
                        "diagnostic": diagnostic, "killed": killed, "c_build": c_build})
        print(json.dumps({"control": name, "killed": killed}), flush=True)
    finally:
        path.write_bytes(original)
build("restored-build")
restored = suite("restored")
passed = (all(report["killed"] for report in reports) and restored.returncode == 0
          and not restored.stderr and restored.stdout == expected_ok)
(WORK / "results.json").write_text(json.dumps({"passed": passed, "controls": reports}, indent=2) + "\n")
print(json.dumps({"passed": passed, "killed": sum(report["killed"] for report in reports),
                  "controls": len(reports)}))
raise SystemExit(0 if passed else 1)
