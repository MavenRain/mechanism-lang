#!/usr/bin/env python3
"""Replay dependent closure controls in a new workspace copy."""
import hashlib
import json
import os
import re
from pathlib import Path
import runpy
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent
bounded_run = runpy.run_path(str(ROOT / "dev/c-process.py"))["run"]
SOURCES = ["c/wasm/link.c", "c/wasm/emit.c",
           "test/prenex_runtime.py",
           "test/fixtures/prelude/dependent-closure-runtime.mech"]
BUILD_TARGET = "_tcc/bin/mech"

# This control reinstates static call_ref dispatch from the former backend.
# The production path dispatches using the closure's stored runtime arity.
# The helper is the C form of the Bend 2 mutation_static (bend2/wasm/emit.bend
# steps_type); it is spliced in front of emit_steps_type in c/wasm/emit.c.
STATIC_HELPER = """/* mutation_static(c, env, tail, head, r, args, m): the former backend's static call_ref dispatch */
static Result emit_steps_static(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail,
                                const Vec *head, Repr r, const Vec *args, Nat m) {
  Result cast = emit_coerce(heap, emit_layout(c), r, eterm_rfunc(eterm_tid(link_fn_key(heap, m))));
  if (cast.kind == RESULT_FAIL) return cast;
  Result ci = link_type_index(heap, emit_layout(c), str_lit("clos"));
  if (ci.kind == RESULT_FAIL) return ci;
  Result fti = link_type_index(heap, emit_layout(c), link_fn_key(heap, m));
  if (fti.kind == RESULT_FAIL) return fti;
  Result idx = alloc_local(heap, st, gc_valtype_ref(gc_heaptype(GC_HEAP_TYPE, index_of(ci))));
  if (idx.kind == RESULT_FAIL) return idx;
  Vec taken = take_vec(heap, args, (size_t)u32_of(m));
  Result ia = emit_each(heap, st, c, env, &taken);
  if (ia.kind == RESULT_FAIL) return ia;
  Vec rest = drop_vec(heap, args, (size_t)u32_of(m));
  Vec save = seq(heap, at(heap, GC_LOCAL_SET, index_of(idx)), at(heap, GC_LOCAL_GET, index_of(idx)),
                 gc_instr_struct_get(heap, index_of(ci), 2), &stop);
  Vec call = seq(heap, at(heap, GC_LOCAL_GET, index_of(idx)), gc_instr_struct_get(heap, index_of(ci), 1),
                 gc_instr_ref_cast(heap, gc_heaptype(GC_HEAP_TYPE, index_of(fti))),
                 at(heap, GC_CALL_REF, index_of(fti)), &stop);
  Vec front = emit_cat(heap, head, (const Vec *)cast.value);
  Vec saved = emit_cat(heap, &front, &save);
  Vec loaded = emit_cat(heap, &saved, (const Vec *)ia.value);
  Vec called = emit_cat(heap, &loaded, &call);
  return emit_steps(heap, st, c, env, tail, &called, link_any_repr(), &rest);
}

"""
STATIC_ANCHOR = "/* steps_type(c, env, tail, head, r, args): only a function or union head applies */"
# Each control is the C form of the Bend 2 control of the same name:
# M1 bend2/wasm/emit.bend steps_type (static dispatch when 0 < m <= |args|),
# M2 bend2/wasm/link.bend nullary_closure (always False),
# M3 bend2/wasm/link.bend steps_result at 0n (the nullary closure keeps its
# function repr), M4 bend2/wasm/emit.bend apply_kind HValue (mutation_empty:
# no args returns the head without the steps call).
CONTROLS = [
    ("C-CLOS-M1", "c/wasm/emit.c",
     """      m = link_arity_of_fn(heap, r.tid.name);
      if (m.kind == RESULT_FAIL) return m;
      return emit_steps_apply(heap, st, c, env, tail, head, r, args);
""",
     """      m = link_arity_of_fn(heap, r.tid.name);
      if (m.kind == RESULT_FAIL) return m;
      if (!nat_is_zero(*(const Nat *)m.value)
          && nat_cmp(*(const Nat *)m.value, nat_of_u64(heap->arena, args->len)) <= 0)
        return emit_steps_static(heap, st, c, env, tail, head, r, args, *(const Nat *)m.value);
      return emit_steps_apply(heap, st, c, env, tail, head, r, args);
""",
     "partialPayload"),
    ("C-CLOS-M2", "c/wasm/link.c",
     "  return r.kind == REPR_FUNC && str_eq(r.tid.name, link_fn_key(heap, nat_zero()));\n",
     "  (void)heap;\n  (void)r;\n  return false;\n", "nullaryPayload"),
    ("C-CLOS-M3", "c/wasm/link.c",
     "    if (link_nullary_closure(heap, r)) return link_done(eterm_repr_new(heap, link_any_repr()));\n"
     "    return link_done(eterm_repr_new(heap, r));\n",
     "    return link_done(eterm_repr_new(heap, r));\n", "nonTailPayload"),
    ("C-CLOS-M4", "c/wasm/emit.c",
     "      return emit_steps(heap, st, c, env, tail, (const Vec *)ih.value, *(const Repr *)hr.value, args);\n",
     "      if (args->len == 0) return ih;\n"
     "      return emit_steps(heap, st, c, env, tail, (const Vec *)ih.value, *(const Repr *)hr.value, args);\n",
     "nullaryPayload"),
]


def mutant_text(name, original, before, after):
    """The mutated source of one control; M1 also splices its static helper."""
    mutant = original.replace(before, after)
    if name == "C-CLOS-M1":
        mutant = mutant.replace(STATIC_ANCHOR, STATIC_HELPER + STATIC_ANCHOR, 1)
    return mutant

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
        ".git", "_build", "_bend2", "_tcc", ".gatework", ".kanon-exec", ".kanon-wait", ".kanon-replies", "build", "logs", "__pycache__"))
    environment = dict(os.environ)
    originals = {path: (copy / path).read_text() for path in SOURCES}
    executable = copy / BUILD_TARGET
    report = {
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
        if environment.get("MEMCAP"):
            # Raw logs retain telemetry. Only complete cap records are excluded from diagnostics.
            record = (r"memcap cmd=.+ exit=-?\d+ signal=\d+ peakMB=\d+ footMB=\d+ capMB=\d+ "
                      r"killed_mem=[01] killed_time=[01] killed_orphan=[01] secs=\d+\.\d+ "
                      r"polls_ok=\d+ polls_failed=\d+ foot_ok=\d+\n?")
            result.stderr = "".join(line for line in result.stderr.splitlines(keepends=True)
                                    if re.fullmatch(record, line) is None)
        return result

    def build(label):
        # The C build of the copy: a stale binary never stands in for a failed build.
        if executable.exists():
            executable.unlink()
        result = run(label, ["make", "-C", str(copy), BUILD_TARGET], BUILD_TIMEOUT)
        built = result.returncode == 0 and os.access(executable, os.X_OK)
        if label == "baseline-build":
            report["c_build"] = {
                "target": BUILD_TARGET,
                "exit_code": result.returncode,
                "binary_sha256": (hashlib.sha256(executable.read_bytes()).hexdigest()
                                  if built else None),
            }
        return built

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
            target.write_text(mutant_text(name, originals[path], before, after))
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
