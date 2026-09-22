#!/usr/bin/env python3
"""Build isolated mutants and require the dependency-export gate to reject each.

Usage: python3 -P dev/validation/prenex-dependency-exports/mutations.py NEW_WORK_DIRECTORY

The work directory must be new and outside the repository. The runner copies
the tree there, builds the baseline, every mutant and the restored tree, and
copies each <name>-test.log next to this script before it exits, so the
repository keeps the per-attempt test logs and the caller owns the copy.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[3]
RECORD = Path(__file__).resolve().parent
USAGE = "usage: python3 -P dev/validation/prenex-dependency-exports/mutations.py NEW_WORK_DIRECTORY"
MUTANTS = [
    ("mapping-validation", "surface/family_poly.ml",
     "if List.length sources = List.length names then Ok bindings",
     "if List.length sources <= List.length names then Ok bindings", "missing: expected refusal"),
    ("reference-renaming", "surface/family_poly.ml",
     "~default:(rename_export scheme ~name:source ~as_name exports n)",
     "~default:(rename_export scheme ~name:source ~as_name (List.filter (fun _ -> false) exports) n)",
     "unbound"),
    ("alias-reservation", "surface/family_poly.ml",
     "if List.mem generated aliases then Error (collision generated)",
     "if List.mem generated aliases && false then Error (collision generated)",
     "earlier-composed-alias: expected refusal"),
    ("name-planning", "surface/elab.ml",
     "let generated = Family_poly.instance_names ~reuse ?exports catalog ~name:source ~as_name",
     "let generated = Family_poly.instance_names ~reuse ?exports:(Option.bind exports (fun _ -> None)) catalog ~name:source ~as_name",
     "poly-name: expected refusal"),
    ("printing", "surface/syntax.ml",
     "(reuse_text reuse) (exports_text exports)) dependencies)",
     "(reuse_text reuse) (exports_text (Option.bind exports (fun _ -> None)))) dependencies)",
     "parse/print changed a dependency export mapping"),
    ("planning-budget", "surface/elab.ml",
     "Family_poly.validate_exports ~budget ~reuse catalog",
     "Family_poly.validate_exports ~budget:Budget.unlimited ~reuse catalog",
     "collision-budget: mismatch: the name w is already declared"),
]


def keep_test_logs(work):
    logs = sorted(work.glob("*-test.log"))
    for log in logs:
        shutil.copyfile(log, RECORD / log.name)
    print(f"MUTATION-LOGS {RECORD.relative_to(ROOT)} copied={len(logs)}", flush=True)


def main():
    if len(sys.argv) != 2:
        raise SystemExit(USAGE)
    work = Path(sys.argv[1]).resolve()
    if work.exists() or work == ROOT or ROOT in work.parents:
        raise SystemExit("the work directory must be new and outside the source repository")
    work.mkdir(parents=True)
    copy = work / "copy"
    shutil.copytree(ROOT, copy, ignore=shutil.ignore_patterns(
        ".git", "_build", ".gatework", ".kanon-*", ".kanonx", ".venv", "__pycache__"))
    print(f"MUTATION-WORK {work}", flush=True)
    env = {key: value for key, value in os.environ.items()
           if key not in ("OPAM_SWITCH_PREFIX", "CAML_LD_LIBRARY_PATH")}
    try:
        check(work, copy, env)
    finally:
        keep_test_logs(work)


def check(work, copy, env):
    def run(label, command):
        result = subprocess.run(command, cwd=copy, env=env, text=True, capture_output=True,
                                timeout=120)
        output = result.stdout + result.stderr
        (work / f"{label}.log").write_text(output)
        return result.returncode, output

    def build(label):
        status, output = run(label, ["zsh", "dev/dunecho.sh", "build"])
        if status or "OK build: 0 errors, 0 warnings" not in output:
            raise RuntimeError(f"{label}: mutant did not compile cleanly: {output}")

    def test(label):
        return run(label, [str(copy / "_build/default/test/prenex_dependency_exports.exe"), str(copy)])

    build("baseline-build")
    status, output = test("baseline-test")
    if status or not output.startswith("PRENEX-DEPENDENCY-EXPORTS-OK "):
        raise RuntimeError(f"baseline failed: {output}")
    for name, relative, before, after, expected in MUTANTS:
        path = copy / relative
        original = path.read_text()
        if original.count(before) != 1:
            raise RuntimeError(f"{name}: expected exactly one mutation site")
        try:
            path.write_text(original.replace(before, after))
            build(f"{name}-build")
            status, output = test(f"{name}-test")
            if status == 0 or "PRENEX-DEPENDENCY-EXPORTS FAIL " not in output or expected not in output:
                raise RuntimeError(f"{name}: wrong mutation outcome: {output}")
            print(f"KILLED {name}: {output.strip()}", flush=True)
        finally:
            path.write_text(original)
    build("restored-build")
    status, output = test("restored-test")
    if status or not output.startswith("PRENEX-DEPENDENCY-EXPORTS-OK "):
        raise RuntimeError(f"restored test failed: {output}")
    print(f"DEPENDENCY-EXPORT-MUTATIONS OK killed={len(MUTANTS)} restored=1", flush=True)


if __name__ == "__main__":
    main()
