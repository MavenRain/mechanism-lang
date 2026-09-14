"""Replay bounded source controls against the heterogeneous functor suite."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
PRELUDE = "prelude/cat/heterogeneous-functor.mech"
FIXTURE = "test/fixtures/prelude/heterogeneous-functor-runtime.mech"
GENERIC = "test/fixtures/prelude/heterogeneous-functor.mech"
NEGATIVE = "test/neg/heterogeneous-functor/wrong-object.mech"
PREFIX = "PRELUDE-HETEROGENEOUS-FUNCTOR-FAIL "
CONTROLS = [
    ("target-object-level", PRELUDE, "(0 D : Sort (succ w))", "(0 D : Sort (succ u))",
     PREFIX + "mismatch:"),
    ("source-hom", PRELUDE, "Source_Hom C c x y", "Source_Hom C c y x",
     PREFIX + "mismatch:"),
    ("wrong-equality", PRELUDE, "Target (Target_Hom", "Source (Target_Hom",
     PREFIX + "mismatch:"),
    ("law-accessor", PRELUDE, "=> F.2.2.1", "=> F.2.2.2",
     PREFIX + "mismatch:"),
    ("constant-object", FIXTURE, "high (natAdd x 2)", "high 0",
     PREFIX + "wrong computation: objectValue"),
    ("constant-arrow", FIXTURE, "=> f Nat)", "=> (fun (n : Nat) => n))",
     PREFIX + "wrong computation: mapValue"),
    ("generic-sort", GENERIC, "-> Type 3 := Mixed_Target_Hom", "-> Type 4 := Mixed_Target_Hom",
     PREFIX + "mismatch:"),
    ("negative-corpus", NEGATIVE,
     "def bad : Run_Functor Nat High SourceCategory TargetCategory :=\n"
     "  ((fun (x : Nat) => x), Lift.2)", "def bad : Nat := 0",
     PREFIX + "wrong-object: expected refusal"),
]


def main():
    if len(sys.argv) != 2:
        print("usage: python3 -I dev/heterogeneous-functor-mutations.py OUTPUT.json", file=sys.stderr)
        return 64
    destination = Path(sys.argv[1]).resolve()
    if destination.exists():
        print("refusing to overwrite mutation evidence", file=sys.stderr)
        return 64
    executable = ROOT / "_build/default/test/prelude_heterogeneous_functor.exe"
    report = {"version": 1, "passed": False, "controls": [],
              "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest()}
    with tempfile.TemporaryDirectory(prefix="mechanism-heterogeneous-controls-") as directory:
        work = Path(directory)
        sources = ["prelude/cat/category-core.mech", "prelude/cat/category.mech",
                   PRELUDE, GENERIC, FIXTURE]
        sources += [str(path.relative_to(ROOT)) for path in
                    sorted((ROOT / "test/neg/heterogeneous-functor").iterdir()) if path.is_file()]
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
            if before not in original:
                raise ValueError(f"missing mutation anchor: {name}")
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
            "PRELUDE-HETEROGENEOUS-FUNCTOR-OK entries=90 instances=3 computations=3 negatives=12\n")
    destination.write_text(json.dumps(report, indent=2) + "\n")
    print(f'HETEROGENEOUS-FUNCTOR-MUTATIONS passed={report["passed"]} controls={len(CONTROLS)} restored=1')
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.SubprocessError, ValueError) as error:
        print(f"HETEROGENEOUS-FUNCTOR-MUTATIONS FAIL {error}", file=sys.stderr)
        raise SystemExit(1)
