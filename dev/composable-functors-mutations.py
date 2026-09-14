"""Replay bounded source controls against the composable functor suite."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
PRELUDE = "prelude/cat/composable-functors.mech"
FIXTURE = "test/fixtures/prelude/composable-functors-runtime.mech"
GENERIC = "test/fixtures/prelude/composable-functors.mech"
NEGATIVE = "test/neg/composable-functors/missing-law.mech"
PREFIX = "PRELUDE-COMPOSABLE-FUNCTORS-FAIL "
CONTROLS = [
    ("first-identity-proof", PRELUDE, "(First_functorMapId C D c d F x)", "categoryRefl",
     PREFIX + "mismatch:"),
    ("second-composition-proof", PRELUDE,
     "(Second_functorMapComp D E d e G (F.1 x) (F.1 y) (F.1 z)\n"
     "              (First_functorMap C D c d F x y f) (First_functorMap C D c d F y z g))",
     "categoryRefl", PREFIX + "mismatch:"),
    ("second-object-map", FIXTURE, "higher (natMul n 2)", "higher (natAdd n 2)",
     PREFIX + "wrong computation: objectValue"),
    ("constant-arrow", FIXTURE, "=> f Nat)", "=> (fun (n : Nat) => n))",
     PREFIX + "wrong computation: mapValue"),
    ("generic-target-sort", GENERIC, "-> Type 5 := Mixed_Target_Hom", "-> Type 4 := Mixed_Target_Hom",
     PREFIX + "mismatch:"),
    ("negative-corpus", NEGATIVE,
     "def bad : Run_Composite_Functor Nat Higher SourceCategory TargetCategory :=\n"
     "  (Composed.1, Composed.2.1)", "def bad : Nat := 0",
     PREFIX + "missing-law: expected refusal"),
    ("canonical-mirror", "prelude/cat/heterogeneous-functor.mech",
     "=> F.2.2.1", "=> F.2.2.2", PREFIX + "functor mirror changed: First"),
    ("composite-mirror", PRELUDE,
     "(F : Composite_Functor C D c d) => F.2.2.1",
     "(F : Composite_Functor C D c d) => F.2.2.2",
     PREFIX + "functor mirror changed: Composite"),
]


def main():
    if len(sys.argv) != 2:
        print("usage: python3 -I dev/composable-functors-mutations.py OUTPUT.json", file=sys.stderr)
        return 64
    destination = Path(sys.argv[1]).resolve()
    if destination.exists():
        print("refusing to overwrite mutation evidence", file=sys.stderr)
        return 64
    executable = ROOT / "_build/default/test/prelude_composable_functors.exe"
    report = {"version": 1, "passed": False, "controls": [],
              "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest()}
    with tempfile.TemporaryDirectory(prefix="mechanism-heterogeneous-controls-") as directory:
        work = Path(directory)
        sources = ["prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech",
                   PRELUDE, GENERIC, FIXTURE]
        sources += [str(path.relative_to(ROOT)) for path in
                    sorted((ROOT / "test/neg/composable-functors").iterdir()) if path.is_file()]
        report["sources"] = {}
        for relative in sources:
            target = work / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, target)
            report["sources"][relative] = hashlib.sha256(target.read_bytes()).hexdigest()

        def run():
            started = time.monotonic()
            result = subprocess.run([str(executable), str(work)], cwd=ROOT,
                                    capture_output=True, text=True, timeout=120)
            return {"exit_code": result.returncode, "stdout": result.stdout,
                    "stderr": result.stderr, "elapsed_ms": round((time.monotonic() - started) * 1000)}

        for name, relative, before, after, expected in CONTROLS:
            target = work / relative
            original = target.read_text()
            if original.count(before) != 1:
                raise ValueError(f"expected one mutation anchor: {name}")
            target.write_text(original.replace(before, after, 1))
            try:
                result = run()
            finally:
                target.write_text(original)
            result.update(name=name, expected=expected,
                          killed=result["exit_code"] == 1 and not result["stderr"]
                          and result["stdout"].startswith(expected))
            report["controls"].append(result)
            print(f'{name}: killed={result["killed"]}', flush=True)
        report["restored"] = run()
        restored = report["restored"]
        stdouts = [row["stdout"] for row in report["controls"]]
        report["distinct"] = len(set(stdouts)) == len(stdouts)
        print(f'controls distinct={report["distinct"]}', flush=True)
        report["passed"] = all(row["killed"] for row in report["controls"]) and report[
            "distinct"] and (
            restored["exit_code"] == 0 and not restored["stderr"] and restored["stdout"] ==
            "PRELUDE-COMPOSABLE-FUNCTORS-OK entries=158 instances=3 computations=4 negatives=12\n")
    destination.write_text(json.dumps(report, indent=2) + "\n")
    print(f'COMPOSABLE-FUNCTORS-MUTATIONS passed={report["passed"]} controls={len(CONTROLS)} restored=1')
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.SubprocessError, ValueError) as error:
        print(f"COMPOSABLE-FUNCTORS-MUTATIONS FAIL {error}", file=sys.stderr)
        raise SystemExit(1)
