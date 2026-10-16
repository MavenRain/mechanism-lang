"""Preserve the ten original equality-operation mutation controls on the C port (tcc)."""
import importlib.util
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit("usage: python3 -I dev/equality-ops-mutations.py NEW_WORK_DIRECTORY")
spec = importlib.util.spec_from_file_location("bend2_mutation", ROOT / "dev/bend2-mutation.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
protocols = {
    'prelude_equality_ops': ('c/tests/surface_prelude_equality_ops.c', 'PRELUDE-EQUALITY-OPS-OK instances=10 computations=11 negatives=8'),
}
# Each control names its Bend anchor (HEAD 7a28534) and the rule the C mutation breaks; the diagnostics are
# the original expected prefixes (decision 279 (a), hop 318).
controls = [
  # C-OPS-M1: bend2/prelude/equality.bend: the j eliminator branch returns Var 1 (the erased y) instead of Var 2 (reflCase); rule: the j branch returns the runtime reflCase
 ('C-OPS-M1',
  'c/prelude/equality.c',
  'equality_at_motive(heap, var(heap, 7), var(heap, 6), var(heap, 5), var(heap, 1), var(heap, 0)), var(heap, 0),\n                            var(heap, 2)));',
  'equality_at_motive(heap, var(heap, 7), var(heap, 6), var(heap, 5), var(heap, 1), var(heap, 0)), var(heap, 0),\n                            var(heap, 1)));',
  'prelude_equality_ops',
  'PRELUDE-EQUALITY-OPS-FAIL quantity: the erased binder y is read in a runtime position'),
  # C-OPS-M2: bend2/prelude/equality.bend: the symm motive states eq(A, y, x) as eq(A, x, y) (Var 4 and Var 1 swapped); rule: symm flips the sides
 ('C-OPS-M2',
  'c/prelude/equality.c',
  'equality_eq(heap, var(heap, 5), var(heap, 1), var(heap, 4))',
  'equality_eq(heap, var(heap, 5), var(heap, 4), var(heap, 1))',
  'prelude_equality_ops',
  'PRELUDE-EQUALITY-OPS-FAIL mismatch:'),
  # C-OPS-M3: bend2/prelude/equality.bend: the trans eliminator branch returns Var 1 for the scrutinee slot instead of Var 0; rule: trans eliminates the second proof
 ('C-OPS-M3',
  'c/prelude/equality.c',
  'equality_eq(heap, var(heap, 7), var(heap, 6), var(heap, 1)),\n                                                    var(heap, 0), var(heap, 1)));',
  'equality_eq(heap, var(heap, 7), var(heap, 6), var(heap, 1)),\n                                                    var(heap, 1), var(heap, 1)));',
  'prelude_equality_ops',
  'PRELUDE-EQUALITY-OPS-FAIL mismatch:'),
  # C-OPS-M4: bend2/prelude/equality.bend: the congr result relates f x to f x (Var 2) instead of f y (Var 1); rule: congr relates f x to f y
 ('C-OPS-M4',
  'c/prelude/equality.c',
  'dependent_apply(heap, var(heap, 5), var(heap, 3), var(heap, 1)))',
  'dependent_apply(heap, var(heap, 5), var(heap, 3), var(heap, 2)))',
  'prelude_equality_ops',
  'PRELUDE-EQUALITY-OPS-FAIL mismatch:'),
  # C-OPS-M5: bend2/prelude/equality.bend: the type_symm motive states type_eq(B, A) as type_eq(A, B) (Var 4 and Var 1 swapped); rule: type symm flips the sides
 ('C-OPS-M5',
  'c/prelude/equality.c',
  'equality_type_eq(heap, var(heap, 1), var(heap, 4))',
  'equality_type_eq(heap, var(heap, 4), var(heap, 1))',
  'prelude_equality_ops',
  'PRELUDE-EQUALITY-OPS-FAIL mismatch:'),
  # C-OPS-M6: bend2/prelude/equality.bend: the type_trans branch returns reflexive(MechTypeEq, Var 2) instead of Var 1 (first); rule: type trans returns the first proof under the eliminated second
 ('C-OPS-M6',
  'c/prelude/equality.c',
  'equality_type_eq(heap, var(heap, 6), var(heap, 1)),\n                                                    var(heap, 0), var(heap, 1)));',
  'equality_type_eq(heap, var(heap, 6), var(heap, 1)),\n                                                    var(heap, 0), equality_reflexive(heap, mech_type_eq(), var(heap, 2))));',
  'prelude_equality_ops',
  'PRELUDE-EQUALITY-OPS-FAIL mismatch:'),
  # C-OPS-M7: fixture control, unchanged
 ('C-OPS-M7',
  'test/fixtures/prelude/equality-ops.mech',
  '(mechSucc mechZero) mechZero (OpsData_refl MechNat mechZero)',
  'mechZero mechZero (OpsData_refl MechNat mechZero)',
  'prelude_equality_ops',
  'PRELUDE-EQUALITY-OPS-FAIL mismatch: the constructor opsIsOne'),
  # C-OPS-M8: bend2/tests/surface_foundation_data.bend: the mixed-instance negative repair repeats the bad source (OpsData_refl ...) instead of (OpsHigher_refl ...); rule: the negative fixture must be refused
 ('C-OPS-M8',
  'c/tests/surface_foundation_data.c',
  '(OpsHigher_refl MechNat mechZero)"},',
  '(OpsData_refl MechNat mechZero)"},',
  'prelude_equality_ops',
  'PRELUDE-EQUALITY-OPS-FAIL mixed-instance: expected refusal:'),
  # C-OPS-M9: bend2/tests/surface_prelude_equality_ops.bend: the dependent computation row is named opsJ instead of opsDependent; rule: every computation row names its own definition
 ('C-OPS-M9',
  'c/tests/surface_prelude_equality_ops.c',
  '  row->name = str_lit("opsDependent");',
  '  row->name = str_lit("opsJ");',
  'prelude_equality_ops',
  'PRELUDE-EQUALITY-OPS-FAIL wrong computation: opsJ'),
  # C-OPS-M10: bend2/prelude/equality.bend: the j eliminator motive swaps the right point and the proof (at_motive(..., Var 0, Var 1)); rule: the j motive is applied to right then proof
 ('C-OPS-M10',
  'c/prelude/equality.c',
  'equality_at_motive(heap, var(heap, 7), var(heap, 6), var(heap, 5), var(heap, 1), var(heap, 0))',
  'equality_at_motive(heap, var(heap, 7), var(heap, 6), var(heap, 5), var(heap, 0), var(heap, 1))',
  'prelude_equality_ops',
  'PRELUDE-EQUALITY-OPS-FAIL mismatch: the term has type (Lan SMu MechEq [right]')]
runner.execute(ROOT, sys.argv[1], protocols, controls)
