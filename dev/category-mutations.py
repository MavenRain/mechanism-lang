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
bounded_run = runpy.run_path(str(ROOT / "dev/bend2-process.py"))["run"]
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
        ".git", "_build", "_bend2", ".gatework", ".kanon-exec", ".kanon-wait", ".kanon-replies", "build", "logs", "__pycache__"))
ENV = dict(os.environ)
ENV.pop("OPAM_SWITCH_PREFIX", None)
ENV.pop("CAML_LD_LIBRARY_PATH", None)


def run(name, command):
    result = bounded_run(command, cwd=COPY, env=ENV, capture_output=True, timeout=300)
    (WORK / f"{name}.stdout").write_bytes(result.stdout)
    (WORK / f"{name}.stderr").write_bytes(result.stderr)
    return result


def build(name):
    result = run(name, [sys.executable, "-I", str(COPY / "dev/bend2-mutation-build.py"), "category"])
    if result.returncode or b"BEND2 MUTATION BUILD PASS category" not in result.stdout:
        raise SystemExit(f"{name}: build failed; this is not a killed mutation")


def suite(name):
    return run(name, [str(COPY / "_bend2/test/prelude_category.exe"), str(COPY)])


controls = [('C-CAT-M1',
  'bend2/surface/elab_term.bend',
  'Quantity.QOne{}, Some{Term.Motive{None{}, [], "self", domain}}, proj_branch(point_q(point), 0n)',
  'Quantity.QOne{}, None{}, proj_branch(point_q(point), 0n)',
  'PRELUDE-CATEGORY-FAIL cannot infer: an elimination without a motive needs an expected type'),
 ('C-CAT-M2',
  'bend2/kernel/check_engine.bend',
  'eq : Bool <- conv(ctx, tyv, got, want)',
  'eq : Bool <- conv_type(ctx, got, want)',
  'PRELUDE-CATEGORY-FAIL mismatch: the constructor categoryRefl of Small gives the index'),
 ('C-CAT-M3',
  'bend2/kernel/eval.bend',
  '    case Con{Value.SElim{Value.StuckElim{shape, q, motive, branches, +env}}, rest}:\n'
  '      do Result<&2, &2, Error.T, Term.T>:\n'
  '        s : Shape.T<Term.T> <- quote_shape(shape, globals, size)\n'
  '        mo : Maybe<&2, Term.Motive<Term.T>> <- quote_motive(motive, globals, env, size)\n'
  '        bs : List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>> <- '
  'quote_branches(branches, globals, env, size)\n'
  '        quote_frames(rest, globals, size, Term.Elim{Term.ElimData{s, head, q, mo, bs}})',
  '    case Con{Value.SElim{Value.StuckElim{shape, q, +motive, branches, +env}}, rest}:\n'
  '      do Result<&2, &2, Error.T, Term.T>:\n'
  '        s : Shape.T<Term.T> <- quote_shape(shape, globals, size)\n'
  '        mo : Maybe<&2, Term.Motive<Term.T>> <- quote_motive(motive, globals, env, size)\n'
  '        bs : List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>> <- '
  'quote_branches(branches, globals, env, size)\n'
  '        quote_frames(rest, globals, size, Term.Elim{Term.ElimData{s, head, q, motive, bs}})',
  'PRELUDE-CATEGORY-FAIL universe:'),
 ('C-CAT-M4',
  'bend2/kernel/eval.bend',
  '    case Con{Value.SElim{Value.StuckElim{shape, q, motive, branches, +env}}, rest}:\n'
  '      do Result<&2, &2, Error.T, Term.T>:\n'
  '        s : Shape.T<Term.T> <- quote_shape(shape, globals, size)\n'
  '        mo : Maybe<&2, Term.Motive<Term.T>> <- quote_motive(motive, globals, env, size)\n'
  '        bs : List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>> <- '
  'quote_branches(branches, globals, env, size)\n'
  '        quote_frames(rest, globals, size, Term.Elim{Term.ElimData{s, head, q, mo, bs}})',
  '    case Con{Value.SElim{Value.StuckElim{shape, q, motive, +branches, +env}}, rest}:\n'
  '      do Result<&2, &2, Error.T, Term.T>:\n'
  '        s : Shape.T<Term.T> <- quote_shape(shape, globals, size)\n'
  '        mo : Maybe<&2, Term.Motive<Term.T>> <- quote_motive(motive, globals, env, size)\n'
  '        bs : List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>> <- '
  'quote_branches(branches, globals, env, size)\n'
  '        quote_frames(rest, globals, size, Term.Elim{Term.ElimData{s, head, q, mo, branches}})',
  'PRELUDE-CATEGORY-FAIL unbound: de Bruijn index 2 is outside the context'),
 ('C-CAT-M5',
  'bend2/kernel/check_engine.bend',
  'index_loop(ts, rs, ws, ctx, name, ctor, env, want <> index_env)',
  'index_loop(ts, rs, ws, ctx, name, ctor, env, index_env)',
  'PRELUDE-CATEGORY-FAIL unbound: de Bruijn index 0 is outside the environment'),
 ('C-CAT-M6',
  'bend2/kernel/check_engine.bend',
  'ok : Unit <- index_equal_result(eq,ctx,ctor,name,got,want)',
  'ok : Unit <- index_equal_result(Bool.or(eq, True{}),ctx,ctor,name,got,want)',
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
  'bend2/kernel/check_engine.bend',
  'conv_project(ctx,s,q,Q.QOne{},Some{Term.Motive{None{},[],"self",domain}},0n,Value.var(C.size_of(ctx)))',
  'conv_project(ctx,s,q,Q.QOne{},None{},0n,Value.var(C.size_of(ctx)))',
  'PRELUDE-CATEGORY-FAIL mismatch: the constructor categoryRefl of Up gives the index c'),
 ('C-CAT-M10',
  'bend2/kernel/eval.bend',
  'open_closure(globals, Value.close(env, body), fresh_vars(arity, size))',
  'open_closure(globals, Value.close(env, body), List.reverse(&2, Value.T, fresh_vars(arity, '
  'size)))',
  'PRELUDE-CATEGORY-FAIL mismatch: the term has type (Lan SMu IndexedValue [Ty; value]')]

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
        build(name + "-build")
        result = suite(name)
        artifacts = list((COPY / "_bend2/mutation").glob("category*.build.json"))
        if len(artifacts) != 1:
            raise SystemExit("missing unique native build evidence")
        native_build = json.loads(artifacts[0].read_text())
        killed = result.returncode == 1 and not result.stderr and result.stdout.startswith(diagnostic.encode())
        reports.append({"name": name, "path": relative, "old": old, "new": new,
                        "source_sha256": hashlib.sha256(original).hexdigest(),
                        "mutant_sha256": mutant_hash, "exit_code": result.returncode,
                        "diagnostic": diagnostic, "killed": killed, "bend_build": native_build})
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
