"""Preserve the ten original dependent-prelude mutation controls in Bend."""
import importlib.util
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit("usage: python3 -I dev/dependent-mutations.py NEW_WORK_DIRECTORY")
spec = importlib.util.spec_from_file_location("bend2_mutation", ROOT / "dev/bend2-mutation.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
protocols = {"prelude_dependent": ("bend2/tests/surface_prelude_dependent.bend", "PRELUDE-DEPENDENT-OK templates=6 instances=29 computations=12 negatives=6")}
controls = [('C-DEP-M1',
  'bend2/prelude/dependent.bend',
  'Level.imax(u, v)',
  'Level.max(u, v)',
  'prelude_dependent',
  'PRELUDE-DEPENDENT-FAIL universe: the former lives at imax(u0, u1)'),
 ('C-DEP-M2',
  'bend2/prelude/dependent.bend',
  'Level.max(Level.succ(u), Level.succ(v))',
  'Level.succ(u)',
  'prelude_dependent',
  'PRELUDE-DEPENDENT-FAIL universe: the former lives at max((u0 + 1), (u1 + 1)) and the expected universe is (u0 + 1)'),
 ('C-DEP-M3',
  'bend2/prelude/dependent.bend',
  'Term.Var{2n}, first(2n, Term.Var{0n}))',
  'Term.Var{2n}, eliminate(2n, Term.Var{0n}, Term.Var{3n}, proj_branch(1n)))',
  'prelude_dependent',
  'PRELUDE-DEPENDENT-FAIL mismatch: the term has type (Out SPi w point A (APt w x) B) and the expected type '
  'is A'),
 ('C-DEP-M4',
  'bend2/prelude/dependent.bend',
  'proj_branch(1n)))',
  'proj_branch(0n)))',
  'prelude_dependent',
  'PRELUDE-DEPENDENT-FAIL mismatch: the term has type A and the expected type is (Out SPi w point A (APt w '
  'x) B)'),
 ('C-DEP-M5',
  'bend2/prelude/dependent.bend',
  'apply(Term.Var{6n}, Term.Var{3n}, Term.Var{1n}), Term.Var{0n})',
  'apply(Term.Var{6n}, Term.Var{3n}, Term.Var{1n}), Term.Var{1n})',
  'prelude_dependent',
  'PRELUDE-DEPENDENT-FAIL mismatch: the term has type A and the expected type is (Out SPi w point A (APt w '
  'x) B)'),
 ('C-DEP-M6',
  'bend2/prelude/dependent.bend',
  'pair(3n, Term.Var{1n}, Term.Var{0n})',
  'pair(3n, Term.Var{0n}, Term.Var{1n})',
  'prelude_dependent',
  'PRELUDE-DEPENDENT-FAIL mismatch: the term has type (Out SPi w point A (APt w x) B) and the expected type '
  'is A'),
 ('C-DEP-M7',
  'test/fixtures/prelude/dependent.mech',
  'SmallMk MechNat (fun (x : MechNat) => MechNat) mechZero (mechSucc mechZero)',
  'SmallMk MechNat (fun (x : MechNat) => MechNat) mechZero mechZero',
  'prelude_dependent',
  'PRELUDE-DEPENDENT-FAIL wrong computation: pairSecond'),
 ('C-DEP-M8',
  'bend2/tests/surface_foundation_data.bend',
  '[Negative{"wrong-fiber", "mismatch: the term has type (Lan SMu MechUnit", "def misuse : SmallPair '
  'MechBool DepFiber := SmallMk MechBool DepFiber mechTrue (mechSucc mechZero)", "def misuse : SmallPair '
  'MechBool DepFiber := SmallMk MechBool DepFiber mechTrue (unitValue)"},',
  '[Negative{"wrong-fiber", "mismatch: the term has type (Lan SMu MechUnit", "def misuse : SmallPair '
  'MechBool DepFiber := SmallMk MechBool DepFiber mechTrue (mechSucc mechZero)", "def misuse : SmallPair '
  'MechBool DepFiber := SmallMk MechBool DepFiber mechTrue (mechSucc mechZero)"},',
  'prelude_dependent',
  'PRELUDE-DEPENDENT-FAIL wrong-fiber: expected refusal:'),
 ('C-DEP-M9',
  'bend2/prelude/dependent.bend',
  'apply(sigma(5n, 4n), Term.Var{3n}, Term.Var{0n})',
  'apply(sigma(5n, 4n), Term.Var{3n}, Term.Var{1n})',
  'prelude_dependent',
  'PRELUDE-DEPENDENT-FAIL mismatch: the term has type (Out SPi w point (Lan SPi w point A (Out SPi w point A '
  '(APt w point) B)) (APt w (In SPi w point A (APt w x) [y])) P)'),
 ('C-DEP-M10',
  'bend2/prelude/dependent.bend',
  'def catalog_comp(+globals: Global.T) -> Budget.Comp(Poly.T):\n'
  '  +u = Level.var(0n)\n'
  '  +v = Level.var(1n)\n'
  '  w = Level.var(2n)\n'
  '  do Budget.Comp<Poly.T>:\n'
  '    catalog : Poly.T <- Poly.declare_comp(globals, [], 2n, pi(u, v))\n'
  '    catalog : Poly.T <- Poly.declare_comp(globals, catalog, 2n, sigma_type(u, v))\n'
  '    catalog : Poly.T <- Poly.declare_comp(globals, catalog, 2n, make(u, v))\n'
  '    catalog : Poly.T <- Poly.declare_comp(globals, catalog, 2n, fst(u, v))\n'
  '    catalog : Poly.T <- Poly.declare_comp(globals, catalog, 2n, snd(u, v))\n'
  '    Poly.declare_comp(globals, catalog, 3n, recursor(u, v, w))\n'
  '\n',
  'def catalog_without_budget(+globals: Global.T) -> Budget.Comp(Poly.T):\n'
  '  +u = Level.var(0n)\n'
  '  +v = Level.var(1n)\n'
  '  w = Level.var(2n)\n'
  '  do Budget.Comp<Poly.T>:\n'
  '    catalog : Poly.T <- Poly.declare_comp(globals, [], 2n, pi(u, v))\n'
  '    catalog : Poly.T <- Poly.declare_comp(globals, catalog, 2n, sigma_type(u, v))\n'
  '    catalog : Poly.T <- Poly.declare_comp(globals, catalog, 2n, make(u, v))\n'
  '    catalog : Poly.T <- Poly.declare_comp(globals, catalog, 2n, fst(u, v))\n'
  '    catalog : Poly.T <- Poly.declare_comp(globals, catalog, 2n, snd(u, v))\n'
  '    Poly.declare_comp(globals, catalog, 3n, recursor(u, v, w))\n'
  '\n'
  'def catalog_comp(globals: Global.T) -> Budget.Comp(Poly.T):\n'
  '  Budget.lift(Poly.T, Budget.run(Poly.T, catalog_without_budget(globals), Budget.unlimited()))\n'
  '\n',
  'prelude_dependent',
  'PRELUDE-DEPENDENT-FAIL budget:')]
runner.execute(ROOT, sys.argv[1], protocols, controls)
