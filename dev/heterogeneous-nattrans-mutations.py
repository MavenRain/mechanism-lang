"""Replay bounded source controls against the heterogeneous natural transformation suite."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
PRELUDE = "prelude/cat/heterogeneous-nattrans.mech"
FIXTURE = "test/fixtures/prelude/heterogeneous-nattrans-runtime.mech"
GENERIC = "test/fixtures/prelude/heterogeneous-nattrans.mech"
NEGATIVE = "test/neg/heterogeneous-nattrans/missing-law.mech"
PREFIX = "PRELUDE-HETEROGENEOUS-NATTRANS-FAIL "

EXPECT_IDENTITY_PROOF = ('mismatch: the constructor categoryRefl of Base_Target gives '
    'the index (Out SPi w _ (Out SPi 0 y D (APt 0 (Out SPi w _ C '
    '(APt w y) (Elim SPi w obj (Ran SPi w _ C D) F as self return '
    '(Ran SPi w _ C D) with | (ALeg 0) x y => x)')
EXPECT_FIRST_NATURALITY = ('mismatch: the constructor categoryRefl of Base_Target gives '
    'the index (Out SPi w _ (Out SPi 0 y D (APt 0 (Out SPi w _ C '
    '(APt w y) (Elim SPi w obj (Ran SPi w _ C D) G as self return '
    '(Ran SPi w _ C D) with | (ALeg 0) x y => x)')
EXPECT_SECOND_NATURALITY = ('mismatch: the constructor categoryRefl of Base_Target gives '
    'the index (Out SPi w _ (Out SPi 0 y D (APt 0 (Out SPi w _ C '
    '(APt w y) (Elim SPi w obj (Ran SPi w _ C D) H as self return '
    '(Ran SPi w _ C D) with | (ALeg 0) x y => x)')
EXPECT_COMPONENT_ORDER = ('mismatch: the term has type (Out SPi 0 y D (APt 0 (Out SPi w _'
    ' C (APt w x) (Elim SPi w obj (Ran SPi w _ C D) H as self '
    'return (Ran SPi w _ C D) with | (ALeg 0) x y => x))) (Out SPi '
    '0 x D (APt 0 (Out SPi w _ C (APt w x) (Elim')
EXPECT_SOURCE_HOM_UNIVERSE = ('mismatch: the term has type (Ran SPi 0 Obj Type 5 (Ran SPi w '
    'category (Lan SPi w hom (Ran SPi 0 x Obj (Ran SPi 0 y Obj Type'
    ' 4)) (Lan SPi w identity (Ran SPi 0 x Obj (Out SPi 0 y Obj '
    '(APt 0 x) (Out SPi 0 x Obj (APt 0 x) hom))')
CONTROLS = [
    ("identity-proof", PRELUDE,
     "(Base_Target_compId D d (F.1 x) (F.1 y) (Base_functorMap C D c d F x y f))",
     "categoryRefl", PREFIX + EXPECT_IDENTITY_PROOF),
    ("first-naturality", PRELUDE,
     "(naturality C D c d F G alpha x y f)",
     "categoryRefl", PREFIX + EXPECT_FIRST_NATURALITY),
    ("second-naturality", PRELUDE,
     "(naturality C D c d G H beta x y f)",
     "categoryRefl", PREFIX + EXPECT_SECOND_NATURALITY),
    ("component-order", PRELUDE,
     "Base_Target_comp D d (F.1 x) (G.1 x) (H.1 x) (alpha.1 x) (beta.1 x)",
     "Base_Target_comp D d (F.1 x) (G.1 x) (H.1 x) (beta.1 x) (alpha.1 x)",
     PREFIX + EXPECT_COMPONENT_ORDER),
    ("component-object", FIXTURE,
     "(fun (n : Nat) => x), (fun (n : Nat) => n)",
     "(fun (n : Nat) => 0), (fun (n : Nat) => n)",
     PREFIX + "wrong computation: componentValue"),
    ("beta-action", FIXTURE, "natAdd n x", "natMul n x",
     PREFIX + "wrong computation: verticalForward"),
    ("source-hom-universe", GENERIC,
     "Type 3 := Wide_Base_Source_Hom", "Type 2 := Wide_Base_Source_Hom",
     PREFIX + EXPECT_SOURCE_HOM_UNIVERSE),
    ("negative-corpus", NEGATIVE,
     'def bad : (0 C : Type 0) -> (0 D : Type 2) ->\n    (c : Mixed_Base_Source_Category C) -> (d : Mixed_Base_Target_Category D) ->\n    (F : Mixed_Base_Functor C D c d) -> (G : Mixed_Base_Functor C D c d) ->\n    ((x : C) -> Mixed_Base_Target_Hom D d (F.1 x) (G.1 x)) -> Mixed_NatTrans C D c d F G :=\n  fun (0 C : Type 0) (0 D : Type 2) (c : Mixed_Base_Source_Category C)\n      (d : Mixed_Base_Target_Category D) (F : Mixed_Base_Functor C D c d)\n      (G : Mixed_Base_Functor C D c d)\n      (app : (x : C) -> Mixed_Base_Target_Hom D d (F.1 x) (G.1 x)) =>\n    (app, (fun (0 x : C) (0 y : C) (f : Mixed_Base_Source_Hom C c x y) => 0))\n', "def bad : Nat := 0\n",
     PREFIX + "missing-law: expected refusal"),
]


def main():
    if len(sys.argv) != 2:
        print("usage: python3 -I dev/heterogeneous-nattrans-mutations.py OUTPUT.json", file=sys.stderr)
        return 64
    destination = Path(sys.argv[1]).resolve()
    if destination.exists():
        print("refusing to overwrite mutation evidence", file=sys.stderr)
        return 64
    executable = ROOT / "_build/default/test/prelude_heterogeneous_nattrans.exe"
    report = {"version": 1, "passed": False, "controls": [],
              "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest()}
    with tempfile.TemporaryDirectory(prefix="mechanism-heterogeneous-controls-") as directory:
        work = Path(directory)
        sources = ["prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech",
                   PRELUDE, GENERIC, FIXTURE]
        sources += [str(path.relative_to(ROOT)) for path in
                    sorted((ROOT / "test/neg/heterogeneous-nattrans").iterdir()) if path.is_file()]
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
            "PRELUDE-HETEROGENEOUS-NATTRANS-OK entries=150 instances=4 computations=5 negatives=13\n")
    destination.write_text(json.dumps(report, indent=2) + "\n")
    print(f'HETEROGENEOUS-NATTRANS-MUTATIONS passed={report["passed"]} controls={len(CONTROLS)} restored=1')
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.SubprocessError, ValueError) as error:
        print(f"HETEROGENEOUS-NATTRANS-MUTATIONS FAIL {error}", file=sys.stderr)
        raise SystemExit(1)
