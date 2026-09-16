"""Replay proof, runtime and fixture controls against the heterogeneous left Kan extension suite."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
CONTROLS = [('uniqueness-proof',
  'prelude/cat/heterogeneous-left-kan.mech',
  [('    Base_Base_Target_eqTrans (Base_Base_Target_Hom D d (H.1 y) (G.1 y))\n'
    '      (beta1.1 y) ((s.1).1 y) (beta2.1 y)\n'
    '      (lanUniq J C D j c d K F H G eta alpha s beta1 h1 y)\n'
    '      (Base_targetEqSymm (Base_Base_Target_Hom D d (H.1 y) (G.1 y)) (beta2.1 y) ((s.1).1 y)\n'
    '        (lanUniq J C D j c d K F H G eta alpha s beta2 h2 y))',
    '    categoryRefl',
    1)],
  'PRELUDE-HETEROGENEOUS-LEFT-KAN-FAIL mismatch:'),
 ('factor-projection',
  'prelude/cat/heterogeneous-left-kan.mech',
  [('=> s.2.1', '=> categoryRefl', 1)],
  'PRELUDE-HETEROGENEOUS-LEFT-KAN-FAIL mismatch:'),
 ('solution-sort',
  'prelude/cat/heterogeneous-left-kan.mech',
  [('Sort (max (succ w) (succ q)) :=', 'Sort (succ q) :=', 1)],
  'PRELUDE-HETEROGENEOUS-LEFT-KAN-FAIL universe: the former lives at '),
 ('unit-projection',
  'prelude/cat/heterogeneous-left-kan.mech',
  [('coconeFirst (LanCocone', 'coconeSecond (LanCocone', 1)],
  'PRELUDE-HETEROGENEOUS-LEFT-KAN-FAIL mismatch:'),
 ('functor-map',
  'test/fixtures/prelude/heterogeneous-left-kan-runtime.mech',
  [('=> (f.2, f.2)', '=> ((fun (n : Nat) => n), (fun (n : Nat) => n))', 1)],
  'PRELUDE-HETEROGENEOUS-LEFT-KAN-FAIL wrong computation: lanMapResult'),
 ('mediator-component',
  'test/fixtures/prelude/heterogeneous-left-kan-runtime.mech',
  [('((fun (n : Nat) => x), (fun (n : Nat) => natAdd n x))',
    '((fun (n : Nat) => natAdd x 1), (fun (n : Nat) => natAdd n x))',
    1)],
  'PRELUDE-HETEROGENEOUS-LEFT-KAN-FAIL wrong computation: lanDescFirst'),
 ('cocone-selection',
  'test/fixtures/prelude/heterogeneous-left-kan-runtime.mech',
  [('Constant Constant Beta Forget', 'Constant Constant Alpha Forget', 1)],
  'PRELUDE-HETEROGENEOUS-LEFT-KAN-FAIL wrong computation: lanDescSecond'),
 ('negative-corpus',
  'test/neg/heterogeneous-left-kan/missing-factor.mech',
  [('def bad : (0 J : Type 0) -> (0 C : Type 2) -> (0 D : Type 4) ->\n'
    '    (j : Mixed_Base_Base_Source_Category J) -> (c : Mixed_Base_Base_Middle_Category C) ->\n'
    '    (d : Mixed_Base_Base_Target_Category D) -> (K : Mixed_Base_Base_First_Functor J C j c) '
    '->\n'
    '    (F : Mixed_Base_Base_Composite_Functor J D j d) ->\n'
    '    (H : Mixed_Base_Base_Second_Functor C D c d) -> (G : Mixed_Base_Base_Second_Functor C D c '
    'd) ->\n'
    '    (eta : Mixed_LanCocone J C D j c d K F H) ->\n'
    '    (alpha : Mixed_LanCocone J C D j c d K F G) ->\n'
    '    (s : Mixed_LanSolution J C D j c d K F H G eta alpha) -> Mixed_LanSolution J C D j c d K '
    'F H G eta alpha :=\n'
    '  fun (0 J : Type 0) (0 C : Type 2) (0 D : Type 4)\n'
    '    (j : Mixed_Base_Base_Source_Category J) (c : Mixed_Base_Base_Middle_Category C)\n'
    '    (d : Mixed_Base_Base_Target_Category D) (K : Mixed_Base_Base_First_Functor J C j c)\n'
    '    (F : Mixed_Base_Base_Composite_Functor J D j d)\n'
    '    (H : Mixed_Base_Base_Second_Functor C D c d) (G : Mixed_Base_Base_Second_Functor C D c '
    'd)\n'
    '    (eta : Mixed_LanCocone J C D j c d K F H)\n'
    '    (alpha : Mixed_LanCocone J C D j c d K F G) (s : Mixed_LanSolution J C D j c d K F H G '
    'eta alpha) => (s.1, (0, s.2.2))\n',
    'def bad : Nat := 0\n',
    1)],
  'PRELUDE-HETEROGENEOUS-LEFT-KAN-FAIL missing-factor: expected refusal'),
 ('fixture-solution-pin',
  'test/fixtures/prelude/heterogeneous-left-kan.mech',
  [('Type 3 := Wide_LanSolution', 'Type 4 := Wide_LanSolution', 1)],
  'PRELUDE-HETEROGENEOUS-LEFT-KAN-FAIL mismatch: the term has type '
  '(Ran SPi 0 J Type 6')]


def main():
    if len(sys.argv) != 2:
        print("usage: python3 -I dev/heterogeneous-left-kan-mutations.py OUTPUT.json", file=sys.stderr)
        return 64
    destination = Path(sys.argv[1]).resolve()
    if destination.exists():
        print("refusing to overwrite mutation evidence", file=sys.stderr)
        return 64
    executable = ROOT / "_build/default/test/prelude_heterogeneous_left_kan.exe"
    report = {"version": 1, "passed": False, "controls": [],
              "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest()}
    with tempfile.TemporaryDirectory(prefix="mechanism-left-kan-controls-") as directory:
        work = Path(directory)
        sources = ["prelude/cat/category-core.mech", "prelude/cat/composable-functors.mech",
                   "prelude/cat/heterogeneous-whiskering.mech", "prelude/cat/heterogeneous-left-kan.mech",
                   "test/fixtures/prelude/heterogeneous-left-kan.mech",
                   "test/fixtures/prelude/heterogeneous-left-kan-runtime.mech",
                   "test/prelude_heterogeneous_left_kan.ml",
                   "dev/heterogeneous-left-kan-mutations.py"]
        sources += [str(path.relative_to(ROOT)) for path in
                    sorted((ROOT / "test/neg/heterogeneous-left-kan").iterdir()) if path.is_file()]
        report["sources"] = {}
        for relative in sources:
            target = work / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, target)
            report["sources"][relative] = hashlib.sha256(target.read_bytes()).hexdigest()

        def run(budget=120):
            started = time.monotonic()
            try:
                result = subprocess.run([str(executable), str(work)], cwd=ROOT,
                                        capture_output=True, text=True, timeout=budget)
            except subprocess.TimeoutExpired as error:
                return {"exit_code": None, "timed_out": True,
                        "stdout": (error.stdout or b"").decode(errors="replace"),
                        "stderr": (error.stderr or b"").decode(errors="replace"),
                        "elapsed_ms": round((time.monotonic() - started) * 1000)}
            return {"exit_code": result.returncode, "stdout": result.stdout,
                    "stderr": result.stderr, "elapsed_ms": round((time.monotonic() - started) * 1000)}

        for name, relative, edits, expected in CONTROLS:
            target = work / relative
            original = target.read_text()
            changed = original
            for before, after, count in edits:
                if changed.count(before) != count:
                    raise ValueError(f"expected {count} mutation anchors: {name}")
                changed = changed.replace(before, after)
            target.write_text(changed)
            try:
                result = run()
            finally:
                target.write_text(original)
            result.update(name=name, expected=expected,
                          killed=result["exit_code"] == 1 and not result["stderr"]
                          and result["stdout"].startswith(expected))
            report["controls"].append(result)
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_text(json.dumps(report, indent=2) + "\n")
            print(f'{name}: killed={result["killed"]}', flush=True)
        report["restored"] = run(300)
        restored = report["restored"]
        stdouts = [row["stdout"] for row in report["controls"]]
        report["distinct"] = len(set(stdouts)) == len(stdouts)
        report["sources_restored"] = all(hashlib.sha256((work / path).read_bytes()).hexdigest() == digest
                                         for path, digest in report["sources"].items())
        report["passed"] = all(row["killed"] for row in report["controls"]) and report[
            "distinct"] and report["sources_restored"] and (
            restored["exit_code"] == 0 and not restored["stderr"] and restored["stdout"] ==
            "PRELUDE-HETEROGENEOUS-LEFT-KAN-OK entries=294 instances=3 computations=4 negatives=15\n")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, indent=2) + "\n")
    restored_passed = report["restored"]["exit_code"] == 0
    timeouts = sum(1 for row in report["controls"] + [report["restored"]] if row.get("timed_out"))
    print(f'HETEROGENEOUS-LEFT-KAN-MUTATIONS-TIMEOUTS count={timeouts}')
    print(f'HETEROGENEOUS-LEFT-KAN-MUTATIONS passed={report["passed"]} controls={len(CONTROLS)} restored={int(restored_passed)}')
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.SubprocessError, ValueError) as error:
        print(f"HETEROGENEOUS-LEFT-KAN-MUTATIONS FAIL {error}")
        raise SystemExit(1)
