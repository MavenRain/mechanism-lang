"""Preserve the ten original equality-operation mutation controls in Bend."""
import importlib.util
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit("usage: python3 -I dev/equality-ops-mutations.py NEW_WORK_DIRECTORY")
spec = importlib.util.spec_from_file_location("bend2_mutation", ROOT / "dev/bend2-mutation.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
protocols = {"prelude_equality_ops": ("bend2/tests/surface_prelude_equality_ops.bend", "PRELUDE-EQUALITY-OPS-OK instances=10 computations=11 negatives=8")}
controls = [('C-OPS-M1',
  'bend2/prelude/equality.bend',
  'Term.Var{0n}), Term.Var{0n}, Term.Var{2n}))',
  'Term.Var{0n}), Term.Var{0n}, Term.Var{1n}))',
  'prelude_equality_ops',
  'PRELUDE-EQUALITY-OPS-FAIL quantity: the erased binder y is read in a runtime position'),
 ('C-OPS-M2',
  'bend2/prelude/equality.bend',
  'eq(Term.Var{5n}, Term.Var{1n}, Term.Var{4n})',
  'eq(Term.Var{5n}, Term.Var{4n}, Term.Var{1n})',
  'prelude_equality_ops',
  'PRELUDE-EQUALITY-OPS-FAIL mismatch:'),
 ('C-OPS-M3',
  'bend2/prelude/equality.bend',
  'eq(Term.Var{7n}, Term.Var{6n}, Term.Var{1n}), Term.Var{0n}, Term.Var{1n})',
  'eq(Term.Var{7n}, Term.Var{6n}, Term.Var{1n}), Term.Var{1n}, Term.Var{1n})',
  'prelude_equality_ops',
  'PRELUDE-EQUALITY-OPS-FAIL mismatch:'),
 ('C-OPS-M4',
  'bend2/prelude/equality.bend',
  'D.apply(Term.Var{5n}, Term.Var{3n}, Term.Var{1n}))',
  'D.apply(Term.Var{5n}, Term.Var{3n}, Term.Var{2n}))',
  'prelude_equality_ops',
  'PRELUDE-EQUALITY-OPS-FAIL mismatch:'),
 ('C-OPS-M5',
  'bend2/prelude/equality.bend',
  'type_eq(Term.Var{1n}, Term.Var{4n})',
  'type_eq(Term.Var{4n}, Term.Var{1n})',
  'prelude_equality_ops',
  'PRELUDE-EQUALITY-OPS-FAIL mismatch:'),
 ('C-OPS-M6',
  'bend2/prelude/equality.bend',
  'type_eq(Term.Var{6n}, Term.Var{1n}), Term.Var{0n}, Term.Var{1n})',
  'type_eq(Term.Var{6n}, Term.Var{1n}), Term.Var{0n}, reflexive("MechTypeEq", Term.Var{2n}))',
  'prelude_equality_ops',
  'PRELUDE-EQUALITY-OPS-FAIL mismatch:'),
 ('C-OPS-M7',
  'test/fixtures/prelude/equality-ops.mech',
  '(mechSucc mechZero) mechZero (OpsData_refl MechNat mechZero)',
  'mechZero mechZero (OpsData_refl MechNat mechZero)',
  'prelude_equality_ops',
  'PRELUDE-EQUALITY-OPS-FAIL mismatch: the constructor opsIsOne'),
 ('C-OPS-M8',
  'bend2/tests/surface_foundation_data.bend',
  'Negative{"mixed-instance", "mismatch: the term has type (Lan SMu OpsHigher", "def checkInstance : OpsData '
  'MechNat mechZero mechZero :=\\n    OpsData_symm MechNat mechZero mechZero (OpsData_refl MechNat '
  'mechZero)", "def checkInstance : OpsData MechNat mechZero mechZero :=\\n    OpsData_symm MechNat mechZero '
  'mechZero (OpsHigher_refl MechNat mechZero)"}]',
  'Negative{"mixed-instance", "mismatch: the term has type (Lan SMu OpsHigher", "def checkInstance : OpsData '
  'MechNat mechZero mechZero :=\\n    OpsData_symm MechNat mechZero mechZero (OpsData_refl MechNat '
  'mechZero)", "def checkInstance : OpsData MechNat mechZero mechZero :=\\n    OpsData_symm MechNat mechZero '
  'mechZero (OpsData_refl MechNat mechZero)"}]',
  'prelude_equality_ops',
  'PRELUDE-EQUALITY-OPS-FAIL mixed-instance: expected refusal:'),
 ('C-OPS-M9',
  'bend2/tests/surface_prelude_equality_ops.bend',
  'C.Pair{"opsDependent", T.In',
  'C.Pair{"opsJ", T.In',
  'prelude_equality_ops',
  'PRELUDE-EQUALITY-OPS-FAIL wrong computation: opsJ'),
 ('C-OPS-M10',
  'bend2/prelude/equality.bend',
  'at_motive(Term.Var{7n}, Term.Var{6n}, Term.Var{5n}, Term.Var{1n}, Term.Var{0n})',
  'at_motive(Term.Var{7n}, Term.Var{6n}, Term.Var{5n}, Term.Var{0n}, Term.Var{1n})',
  'prelude_equality_ops',
  'PRELUDE-EQUALITY-OPS-FAIL mismatch: the term has type (Lan SMu MechEq [right]')]
runner.execute(ROOT, sys.argv[1], protocols, controls)
