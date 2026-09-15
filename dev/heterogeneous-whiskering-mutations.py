"""Replay proof, runtime and fixture controls against the heterogeneous whiskering suite."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
CONTROLS = [('right-naturality',
  'prelude/cat/heterogeneous-whiskering.mech',
  [('Second_naturality D E d e F G alpha (K.1 x) (K.1 y)\n'
    '         (Base_First_functorMap C D c d K x y f)',
    'categoryRefl',
    1)],
  'PRELUDE-HETEROGENEOUS-WHISKERING-FAIL mismatch: the constructor categoryRefl of Base_Target '
  'gives the index '),
 ('left-naturality',
  'prelude/cat/heterogeneous-whiskering.mech',
  [('(First_naturality C D c d F G alpha x y f)', 'categoryRefl', 1)],
  'PRELUDE-HETEROGENEOUS-WHISKERING-FAIL mismatch: the constructor categoryRefl of Base_Middle '
  'gives the index '),
 ('left-composition-law',
  'prelude/cat/heterogeneous-whiskering.mech',
  [('(Base_Second_functorMapComp D E d e H (F.1 x) (F.1 y) (G.1 y)\n'
    '             (Base_First_functorMap C D c d F x y f) (alpha.1 y))',
    'categoryRefl',
    1)],
  'PRELUDE-HETEROGENEOUS-WHISKERING-FAIL mismatch: the constructor categoryRefl of Base_Target '
  'gives the index '),
 ('object-map',
  'test/fixtures/prelude/heterogeneous-whiskering-runtime.mech',
  [('natAdd x 5', 'natAdd x 6', 3),
   ('natAdd y 5', 'natAdd y 6', 1),
   ('natAdd right 5', 'natAdd right 6', 1)],
  'PRELUDE-HETEROGENEOUS-WHISKERING-FAIL wrong computation: whiskerRightFirst'),
 ('arrow-map',
  'test/fixtures/prelude/heterogeneous-whiskering-runtime.mech',
  [('(f.2, f.2)', '((fun (n : Nat) => n), (fun (n : Nat) => n))', 1)],
  'PRELUDE-HETEROGENEOUS-WHISKERING-FAIL wrong computation: whiskerLeftFirst'),
 ('negative-corpus',
  'test/neg/heterogeneous-whiskering/missing-law.mech',
  [('def bad : (0 C : Type 0) -> (0 D : Type 2) ->\n'
    '    (c : Mixed_Base_Source_Category C) -> (d : Mixed_Base_Middle_Category D) ->\n'
    '    (F : Mixed_Base_First_Functor C D c d) -> (G : Mixed_Base_First_Functor C D c d) ->\n'
    '    ((x : C) -> Mixed_Base_Middle_Hom D d (F.1 x) (G.1 x)) -> Mixed_First_NatTrans C D c d F '
    'G :=\n'
    '  fun (0 C : Type 0) (0 D : Type 2) (c : Mixed_Base_Source_Category C)\n'
    '      (d : Mixed_Base_Middle_Category D) (F : Mixed_Base_First_Functor C D c d)\n'
    '      (G : Mixed_Base_First_Functor C D c d)\n'
    '      (app : (x : C) -> Mixed_Base_Middle_Hom D d (F.1 x) (G.1 x)) =>\n'
    '    (app, (fun (0 x : C) (0 y : C) (f : Mixed_Base_Source_Hom C c x y) => 0))\n',
    'def bad : Nat := 0\n',
    1)],
  'PRELUDE-HETEROGENEOUS-WHISKERING-FAIL missing-law: expected refusal'),
 ('canonical-mirror',
  'prelude/cat/heterogeneous-nattrans.mech',
  [('def natApp :', 'def changedNatApp :', 1)],
  'PRELUDE-HETEROGENEOUS-WHISKERING-FAIL natural transformation mirror changed: First'),
 ('second-mirror',
  'prelude/cat/heterogeneous-whiskering.mech',
  [('def Second_natApp :', 'def changedSecondNatApp :', 1)],
  'PRELUDE-HETEROGENEOUS-WHISKERING-FAIL natural transformation mirror changed: Second'),
 ('composite-mirror',
  'prelude/cat/heterogeneous-whiskering.mech',
  [('def Composite_natApp :', 'def changedCompositeNatApp :', 1)],
  'PRELUDE-HETEROGENEOUS-WHISKERING-FAIL natural transformation mirror changed: Composite')]


def main():
    if len(sys.argv) != 2:
        print("usage: python3 -I dev/heterogeneous-whiskering-mutations.py OUTPUT.json", file=sys.stderr)
        return 64
    destination = Path(sys.argv[1]).resolve()
    if destination.exists():
        print("refusing to overwrite mutation evidence", file=sys.stderr)
        return 64
    executable = ROOT / "_build/default/test/prelude_heterogeneous_whiskering.exe"
    report = {"version": 1, "passed": False, "controls": [],
              "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest()}
    with tempfile.TemporaryDirectory(prefix="mechanism-whiskering-controls-") as directory:
        work = Path(directory)
        sources = ["prelude/cat/category-core.mech", "prelude/cat/composable-functors.mech",
                   "prelude/cat/heterogeneous-nattrans.mech", "prelude/cat/heterogeneous-whiskering.mech",
                   "test/fixtures/prelude/heterogeneous-whiskering.mech",
                   "test/fixtures/prelude/heterogeneous-whiskering-runtime.mech",
                   "test/prelude_heterogeneous_whiskering.ml",
                   "dev/heterogeneous-whiskering-mutations.py"]
        sources += [str(path.relative_to(ROOT)) for path in
                    sorted((ROOT / "test/neg/heterogeneous-whiskering").iterdir()) if path.is_file()]
        report["sources"] = {}
        for relative in sources:
            target = work / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, target)
            report["sources"][relative] = hashlib.sha256(target.read_bytes()).hexdigest()

        def run():
            started = time.monotonic()
            try:
                result = subprocess.run([str(executable), str(work)], cwd=ROOT,
                                        capture_output=True, text=True, timeout=120)
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
        report["restored"] = run()
        restored = report["restored"]
        stdouts = [row["stdout"] for row in report["controls"]]
        report["distinct"] = len(set(stdouts)) == len(stdouts)
        report["sources_restored"] = all(hashlib.sha256((work / path).read_bytes()).hexdigest() == digest
                                         for path, digest in report["sources"].items())
        report["passed"] = all(row["killed"] for row in report["controls"]) and report[
            "distinct"] and report["sources_restored"] and (
            restored["exit_code"] == 0 and not restored["stderr"] and restored["stdout"] ==
            "PRELUDE-HETEROGENEOUS-WHISKERING-OK entries=274 instances=4 computations=4 negatives=16\n")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, indent=2) + "\n")
    restored_passed = report["restored"]["exit_code"] == 0
    timeouts = sum(1 for row in report["controls"] + [report["restored"]] if row.get("timed_out"))
    print(f'HETEROGENEOUS-WHISKERING-MUTATIONS-TIMEOUTS count={timeouts}')
    print(f'HETEROGENEOUS-WHISKERING-MUTATIONS passed={report["passed"]} controls={len(CONTROLS)} restored={int(restored_passed)}')
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.SubprocessError, ValueError) as error:
        print(f"HETEROGENEOUS-WHISKERING-MUTATIONS FAIL {error}")
        raise SystemExit(1)
