#!/usr/bin/env python3
"""Replay dependent closure controls in a new workspace copy."""
import hashlib
import json
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent
bounded_run = runpy.run_path(str(ROOT / "dev/bend2-process.py"))["run"]
SOURCES = ["bend2/wasm/link.bend", "bend2/wasm/emit.bend",
           "bend2/tests/cli_core.bend", "dev/bend2-mutation-build.py",
           "test/prenex_runtime.py",
           "test/fixtures/prelude/dependent-closure-runtime.mech"]

# This control reinstates static call_ref dispatch from the former backend.
# The production path dispatches using the closure's stored runtime arity.
STATIC_HELPER = """@unsafe def mutation_static(+c:Ctx,+env:List<&2,Binding>,+tail:Bool,head:List<&2,G.Instr>,r:E.Repr,+args:List<&2,E.Ktm>,+m:Nat) -> S.Comp(List<&2,G.Instr>):
  do S.Comp<List<&2,G.Instr>>:
    cast : List<&2,G.Instr> <- S.lift(List<&2,G.Instr>,coerce(layout(c),r,E.RFunc{E.Tid{L.fn_key(m)}}))
    +ci : U32 <- S.lift(U32,L.type_index(layout(c),"clos"))
    +fti : U32 <- S.lift(U32,L.type_index(layout(c),L.fn_key(m)))
    +idx : U32 <- S.alloc(G.Ref{G.HType{ci}})
    ia : List<&2,G.Instr> <- each(c,env,List.take(&2,E.Ktm,args,m))
    steps(c,env,tail,cats([head,cast,[G.Local_set{idx},G.Local_get{idx},G.Struct_get{ci,2}],ia,[G.Local_get{idx},G.Struct_get{ci,1},G.Ref_cast{G.HType{fti}},G.Call_ref{fti}]]),L.any_repr(),List.drop(&2,E.Ktm,args,m))

@unsafe def mutation_dispatch(use_static:Bool,c:Ctx,env:List<&2,Binding>,tail:Bool,head:List<&2,G.Instr>,r:E.Repr,args:List<&2,E.Ktm>,m:Nat) -> S.Comp(List<&2,G.Instr>):
  match use_static:
    case True{}: mutation_static(c,env,tail,head,r,args,m)
    case False{}: steps_apply(c,env,tail,head,r,args)

"""
EMPTY_HELPER = """@unsafe def mutation_empty(c:Ctx,env:List<&2,Binding>,tail:Bool,head:List<&2,G.Instr>,r:E.Repr,args:List<&2,E.Ktm>) -> S.Comp(List<&2,G.Instr>):
  match args:
    case Nil{}: S.pure(List<&2,G.Instr>,head)
    case Con{x,xs}: steps(c,env,tail,head,r,x <> xs)

"""
CONTROLS = [
    ("C-CLOS-M1", "bend2/wasm/emit.bend",
     """        m : Nat <- S.lift(Nat,L.arity_of_fn(t))
        steps_apply(c,env,tail,head,r,args)""",
     """        +m : Nat <- S.lift(Nat,L.arity_of_fn(t))
        mutation_dispatch(Nat.is_lt(0n,m) && Nat.is_le(m,List.length(&2,E.Ktm,args)),c,env,tail,head,r,args,m)""",
     "partialPayload"),
    ("C-CLOS-M2", "bend2/wasm/link.bend",
     "case E.RFunc{E.Tid{t}}: String.eq(t,fn_key(0n))",
     "case E.RFunc{E.Tid{t}}: False{}", "nullaryPayload"),
    ("C-CLOS-M3", "bend2/wasm/link.bend",
     "case E.RFunc{E.Tid{+t}} 0n: Done{choose(E.Repr,String.eq(t,fn_key(0n)),any_repr(),E.RFunc{E.Tid{t}})}",
     "case E.RFunc{E.Tid{+t}} 0n: Done{E.RFunc{E.Tid{t}}}", "nonTailPayload"),
    ("C-CLOS-M4", "bend2/wasm/emit.bend",
     "        steps(c,env,tail,ih,hr,args)\n",
     "        mutation_empty(c,env,tail,ih,hr,args)\n", "nullaryPayload"),
]

BUILD_TIMEOUT = 300
SUITE_TIMEOUT = 120


def text_of(blob):
    if isinstance(blob, bytes):
        return blob.decode(errors="replace")
    return blob or ""


def main():
    if len(sys.argv) != 2:
        print("usage: python3 -I dev/closure-mutations.py NEW_DIRECTORY")
        return 64
    work = Path(sys.argv[1]).resolve()
    if work == ROOT or ROOT in work.parents:
        print("mutation workspace must be outside the repository")
        return 64
    work.mkdir(parents=True, exist_ok=False)
    copy = work / "copy"
    shutil.copytree(ROOT, copy, ignore=shutil.ignore_patterns(
        ".git", "_build", "_bend2", ".gatework", ".kanon-exec", ".kanon-wait", ".kanon-replies", "build", "logs", "__pycache__"))
    environment = dict(os.environ)
    backend = environment.get("BEND_MUTATION_BACKEND",
                              "javascript" if environment.get("BEND_MUTATION_PACKAGED_CLI")
                              else environment.get("BEND_TEST_BACKEND", "javascript"))
    originals = {path: (copy / path).read_text() for path in SOURCES}
    report = {
        "backend": backend,
        "sources": {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest()
                    for path in SOURCES},
        "controls": [],
        "passed": False,
    }

    def capture(label, out, err):
        (work / f"{label}.stdout").write_text(text_of(out))
        (work / f"{label}.stderr").write_text(text_of(err))

    def run(label, command, limit=SUITE_TIMEOUT):
        try:
            result = bounded_run(command, cwd=copy, env=environment,
                                    capture_output=True, text=True,
                                    timeout=limit)
        except subprocess.TimeoutExpired as expiry:
            capture(label, expiry.stdout, expiry.stderr)
            raise
        capture(label, result.stdout, result.stderr)
        return result

    def build(label):
        result = run(label, [sys.executable, "-P", "dev/bend2-mutation-build.py", "cli"], BUILD_TIMEOUT)
        return (result.returncode == 0
                and "BEND2 MUTATION BUILD PASS cli\n" in result.stdout)

    def suite(label):
        return run(label, [sys.executable, "-P", "test/prenex_runtime.py",
                           "--closures"])

    def replay():
        expected = "DEPENDENT-CLOSURE-RUNTIME OK cases=9 hosts=3 mutation=1\n"
        if not build("baseline-build"):
            print("CLOSURE-MUTATIONS FAIL baseline build")
            return 1
        baseline = suite("baseline")
        report["baseline"] = (baseline.returncode == 0
                              and baseline.stdout == expected and baseline.stderr == "")
        if not report["baseline"]:
            print("CLOSURE-MUTATIONS FAIL baseline suite")
            return 1
        for name, path, before, after, export in CONTROLS:
            if originals[path].count(before) != 1:
                print(f"CLOSURE-MUTATIONS FAIL {name} anchor count")
                return 1
            target = copy / path
            mutant = originals[path].replace(before, after)
            if name == "C-CLOS-M1":
                mutant = mutant.replace("@unsafe def steps_type(", STATIC_HELPER + "@unsafe def steps_type(", 1)
                mutant = mutant.replace("+r:E.Repr,args:List<&2,E.Ktm>) -> S.Comp(List<&2,G.Instr>):\n  match r:",
                                        "+r:E.Repr,+args:List<&2,E.Ktm>) -> S.Comp(List<&2,G.Instr>):\n  match r:", 1)
            if name == "C-CLOS-M4":
                mutant = mutant.replace("@unsafe def apply_kind(", EMPTY_HELPER + "@unsafe def apply_kind(", 1)
            target.write_text(mutant)
            try:
                built = build(f"{name}-build")
                result = suite(name) if built else None
                killed = (result is not None and result.returncode == 1
                          and result.stderr == "" and "kernel_checks=" not in result.stdout
                          and all(f"FAIL original/{export}/{host}: exit=1" in result.stdout
                                  for host in ("node", "wasmtime"))
                          and "FAIL host_checks=" in result.stdout)
                report["controls"].append({"id": name, "built": built,
                                           "export": export, "killed": killed})
                # A control that does not build is a distinct outcome from a
                # control that builds and does not fail its export.
                verdict = ("KILLED" if killed
                           else "FAILED build" if not built else "FAILED suite")
                print(f"{name} {verdict}", flush=True)
            finally:
                target.write_text(originals[path])
        restored_build = build("restored-build")
        restored = suite("restored") if restored_build else None
        report["restored"] = (restored is not None and restored.returncode == 0
                              and restored.stdout == expected and restored.stderr == "")
        report["passed"] = (report["restored"] and len(report["controls"]) == len(CONTROLS)
                            and all(row["killed"] for row in report["controls"]))
        print(f"CLOSURE-MUTATIONS {'OK' if report['passed'] else 'FAIL'} "
              f"controls={len(report['controls'])} restored={int(report['restored'])}")
        return 0 if report["passed"] else 1

    # A timeout or an OSError still leaves a report on disk, because
    # the report names the step that did not finish.
    try:
        return replay()
    finally:
        (work / "results.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"CLOSURE-MUTATIONS FAIL {error}", file=sys.stderr)
        sys.exit(2)
