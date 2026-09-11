#!/usr/bin/env python3
"""Replay dependent closure controls in a new workspace copy."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent
SOURCES = ["wasm/link.ml", "wasm/emit.ml", "wasm/dune",
           "test/prenex_runtime.py",
           "test/fixtures/prelude/dependent-closure-runtime.mech"]
CONTROLS = [
    ("C-CLOS-M1", "wasm/link.ml",
     """        let* _m = arity_of_fn t in
        (* A dependent result can expose more runtime parameters after
           specialization.  Dispatch on the closure's stored arity. *)
        Ok [ SApply k ]""",
     """        let* m = arity_of_fn t in
        if m > 0 && m <= k then Ok [ SCallRef m ]
        else Ok [ SApply k ]""", "partialPayload"),
    ("C-CLOS-M2", "wasm/link.ml",
     "| E.RFunc (E.Tid t) -> String.equal t (fn_key 0)",
     "| E.RFunc (E.Tid _t) -> false", "nullaryPayload"),
    ("C-CLOS-M3", "wasm/link.ml",
     "if Int.equal k 0 then Ok (if nullary_closure r then any_repr else r)",
     "if Int.equal k 0 then Ok r", "nonTailPayload"),
    ("C-CLOS-M4", "wasm/emit.ml",
     "steps c s1 env tail ih hr args\n\nand direct",
     """if List.is_empty args then Ok (ih, s1)
      else steps c s1 env tail ih hr args

and direct""", "nullaryPayload"),
]


# A copy of the tree carries no _build, so every build of a control is a
# cold build.  A measured cold build of this tree took 261 s under load,
# so the build limit is the 300 s the sibling replay uses.  The suite
# keeps the shorter limit.
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
        ".git", "_build", ".gatework", ".kanon-exec", ".kanon-wait"))
    environment = dict(os.environ)
    environment.pop("OPAM_SWITCH_PREFIX", None)
    environment.pop("CAML_LD_LIBRARY_PATH", None)
    originals = {path: (copy / path).read_text() for path in SOURCES}
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
            result = subprocess.run(command, cwd=copy, env=environment,
                                    capture_output=True, text=True,
                                    timeout=limit)
        except subprocess.TimeoutExpired as expiry:
            capture(label, expiry.stdout, expiry.stderr)
            raise
        capture(label, result.stdout, result.stderr)
        return result

    def build(label):
        result = run(label, ["zsh", "dev/dunecho.sh", "build"], BUILD_TIMEOUT)
        return (result.returncode == 0 and result.stderr == ""
                and result.stdout == "OK build: 0 errors, 0 warnings\n")

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
            target.write_text(originals[path].replace(before, after))
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
