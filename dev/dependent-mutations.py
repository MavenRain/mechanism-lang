"""Preserve the ten original dependent-prelude mutation controls on the C port (tcc)."""
import importlib.util
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit("usage: python3 -I dev/dependent-mutations.py NEW_WORK_DIRECTORY")
spec = importlib.util.spec_from_file_location("bend2_mutation", ROOT / "dev/c-mutation.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
protocols = {
    "prelude_dependent": ("c/tests/surface_prelude_dependent.c", "PRELUDE-DEPENDENT-OK templates=6 instances=29 computations=12 negatives=6"),
}
# Each control names its Bend anchor (HEAD 7a28534) and the rule the C mutation breaks; the diagnostics are
# the original expected prefixes (decision 279 (a), hop 318).
controls = [
  # C-DEP-M1: bend2/prelude/dependent.bend: MechPi lives in Univ{Level.max(u, v)} instead of Univ{Level.imax(u, v)}; rule: the pi former lives at imax(u, v)
 ('C-DEP-M1',
  'c/prelude/dependent.c',
  'term_univ(heap, level_imax(heap, u, v))',
  'term_univ(heap, level_max(heap, u, v))',
  'prelude_dependent',
  'PRELUDE-DEPENDENT-FAIL universe: the former lives at imax(u0, u1)'),
  # C-DEP-M2: bend2/prelude/dependent.bend: MechSigma lives in Univ{Level.succ(u)} instead of Univ{Level.max(Level.succ(u), Level.succ(v))}; rule: the sigma former lives at max(u + 1, v + 1)
 ('C-DEP-M2',
  'c/prelude/dependent.c',
  'term_univ(heap, level_max(heap, level_succ(heap, u), level_succ(heap, v)))',
  'term_univ(heap, level_succ(heap, u))',
  'prelude_dependent',
  'PRELUDE-DEPENDENT-FAIL universe: the former lives at max((u0 + 1), (u1 + 1)) and the expected universe is (u0 + 1)'),
  # C-DEP-M3: bend2/prelude/dependent.bend: the fst body eliminates with motive Var 3 and proj_branch(1n) instead of first(2n, Var 0); rule: mechSigmaFst projects the first component
 ('C-DEP-M3',
  'c/prelude/dependent.c',
  '&binders, var(heap, 2),\n                              dependent_first(heap, nat(heap, 2), var(heap, 0)));',
  '&binders, var(heap, 2),\n                              dependent_eliminate(heap, nat(heap, 2), var(heap, 0), var(heap, 3), dependent_proj_branch(heap, nat(heap, 1))));',
  'prelude_dependent',
  'PRELUDE-DEPENDENT-FAIL mismatch: the term has type (Out SPi w point A (APt w x) B) and the expected type is A'),
  # C-DEP-M4: bend2/prelude/dependent.bend: the snd branch projects proj_branch(0n) instead of proj_branch(1n); rule: mechSigmaSnd projects the second component
 ('C-DEP-M4',
  'c/prelude/dependent.c',
  'dependent_proj_branch(heap, nat(heap, 1))));',
  'dependent_proj_branch(heap, nat(heap, 0))));',
  'prelude_dependent',
  'PRELUDE-DEPENDENT-FAIL mismatch: the term has type A and the expected type is (Out SPi w point A (APt w x) B)'),
  # C-DEP-M5: bend2/prelude/dependent.bend: the recursor leg applies step to Var 1 twice instead of Var 1 then Var 0; rule: the recursor leg passes both pair components
 ('C-DEP-M5',
  'c/prelude/dependent.c',
  'dependent_apply(heap, var(heap, 6), var(heap, 3), var(heap, 1)), var(heap, 0));',
  'dependent_apply(heap, var(heap, 6), var(heap, 3), var(heap, 1)), var(heap, 1));',
  'prelude_dependent',
  'PRELUDE-DEPENDENT-FAIL mismatch: the term has type A and the expected type is (Out SPi w point A (APt w x) B)'),
  # C-DEP-M6: bend2/prelude/dependent.bend: mechSigmaMk pairs (Var 0, Var 1) instead of (Var 1, Var 0); rule: the constructor keeps the field order
 ('C-DEP-M6',
  'c/prelude/dependent.c',
  'dependent_pair(heap, nat(heap, 3), var(heap, 1), var(heap, 0))',
  'dependent_pair(heap, nat(heap, 3), var(heap, 0), var(heap, 1))',
  'prelude_dependent',
  'PRELUDE-DEPENDENT-FAIL mismatch: the term has type (Out SPi w point A (APt w x) B) and the expected type is A'),
  # C-DEP-M7: fixture control, unchanged
 ('C-DEP-M7',
  'test/fixtures/prelude/dependent.mech',
  'SmallMk MechNat (fun (x : MechNat) => MechNat) mechZero (mechSucc mechZero)',
  'SmallMk MechNat (fun (x : MechNat) => MechNat) mechZero mechZero',
  'prelude_dependent',
  'PRELUDE-DEPENDENT-FAIL wrong computation: pairSecond'),
  # C-DEP-M8: bend2/tests/surface_foundation_data.bend: the wrong-fiber negative repair repeats the bad source (mechSucc mechZero) instead of (unitValue); rule: the negative fixture must be refused
 ('C-DEP-M8',
  'c/tests/surface_foundation_data.c',
  '     "def misuse : SmallPair MechBool DepFiber := SmallMk MechBool DepFiber mechTrue (unitValue)"},',
  '     "def misuse : SmallPair MechBool DepFiber := SmallMk MechBool DepFiber mechTrue (mechSucc mechZero)"},',
  'prelude_dependent',
  'PRELUDE-DEPENDENT-FAIL wrong-fiber: expected refusal:'),
  # C-DEP-M9: bend2/prelude/dependent.bend: the recursor motive applies the sigma to Var 1 instead of Var 0; rule: the recursor motive is applied to the scrutinee
 ('C-DEP-M9',
  'c/prelude/dependent.c',
  'dependent_apply(heap, dependent_sigma(heap, nat(heap, 5), nat(heap, 4)), var(heap, 3), var(heap, 0))',
  'dependent_apply(heap, dependent_sigma(heap, nat(heap, 5), nat(heap, 4)), var(heap, 3), var(heap, 1))',
  'prelude_dependent',
  'PRELUDE-DEPENDENT-FAIL mismatch: the term has type (Out SPi w point (Lan SPi w point A (Out SPi w point A (APt w point) B)) (APt w (In SPi w point A (APt w x) [y])) P)'),
  # C-DEP-M10: bend2/prelude/dependent.bend: catalog_comp runs the group under Budget.unlimited() and lifts the result; rule: the dependent catalog honours the caller's budget
 ('C-DEP-M10',
  'c/prelude/dependent.c',
  'Comp dependent_catalog_comp(Heap *heap, Globals globals) {',
  'static Comp dependent_catalog_without_budget(Heap *heap, Globals globals);\nComp dependent_catalog_comp(Heap *heap, Globals globals) {\n  return budget_lift(budget_run(heap, dependent_catalog_without_budget(heap, globals), budget_unlimited()));\n}\nstatic Comp dependent_catalog_without_budget(Heap *heap, Globals globals) {',
  'prelude_dependent',
  'PRELUDE-DEPENDENT-FAIL budget:')]
runner.execute(ROOT, sys.argv[1], protocols, controls)
