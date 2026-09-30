"""Preserve twelve original congruence and family-group mutation controls in Bend."""
import importlib.util
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit("usage: python3 -I dev/congruence-mutations.py NEW_WORK_DIRECTORY")
spec = importlib.util.spec_from_file_location("bend2_mutation", ROOT / "dev/bend2-mutation.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
protocols = {
    "prelude_congruence": ("bend2/tests/surface_prelude_congruence.bend", "PRELUDE-CONGRUENCE-OK instances=8 computations=4 negatives=5"),
    "family_groups": ("bend2/tests/surface_family_groups.bend", "FAMILY-GROUPS-OK cases=31"),
}
controls = [('C-CONG-M1',
  'bend2/prelude/congruence.bend',
  'def congr(u: Level.T, v: Level.T) -> Check.Decl:\n'
  '  D.definition("congr",\n'
  '    [P.Binder{Q.QZero{}, "A", Term.Univ{u}}, P.Binder{Q.QZero{}, "B", Term.Univ{v}},\n'
  '     P.Binder{Q.QZero{}, "f", D.arrow(P.Binder{Q.QMany{}, "point", Term.Var{1n}}, Term.Var{1n})},\n'
  '     P.Binder{Q.QZero{}, "x", Term.Var{2n}}, P.Binder{Q.QZero{}, "y", Term.Var{3n}},\n'
  '     P.Binder{Q.QMany{}, "proof", eq("MechCongr", Term.Var{4n}, Term.Var{1n}, Term.Var{0n})}],\n'
  '    eq("Result", Term.Var{4n}, D.apply(Term.Var{5n}, Term.Var{3n}, Term.Var{2n}), D.apply(Term.Var{5n}, '
  'Term.Var{3n}, Term.Var{1n})),\n'
  '    Term.Elim{Term.ElimData{Shape.SMu{"MechCongr", [Term.Var{1n}]}, Term.Var{0n}, Q.QOne{},\n'
  '      Some{Term.Motive{Some{"MechCongr"}, ["right"], "self",\n'
  '        eq("Result", Term.Var{6n}, D.apply(Term.Var{7n}, Term.Var{5n}, Term.Var{4n}), '
  'D.apply(Term.Var{7n}, Term.Var{5n}, Term.Var{1n}))}},\n'
  '      [Common.Pair{Term.ACtor{"mechReflCtor"}, Term.Leg{[],\n'
  '        Term.In{Shape.SMu{"Result", [D.apply(Term.Var{5n}, Term.Var{3n}, Term.Var{2n})]}, '
  'Term.ACtor{"mechReflCtor"}, []}}}]}})\n'
  '\n',
  'def congr(+u: Level.T, v: Level.T) -> Check.Decl:\n'
  '  D.definition("congr",\n'
  '    [P.Binder{Q.QZero{}, "A", Term.Univ{u}}, P.Binder{Q.QZero{}, "B", Term.Univ{u}},\n'
  '     P.Binder{Q.QZero{}, "f", D.arrow(P.Binder{Q.QMany{}, "point", Term.Var{1n}}, Term.Var{1n})},\n'
  '     P.Binder{Q.QZero{}, "x", Term.Var{2n}}, P.Binder{Q.QZero{}, "y", Term.Var{3n}},\n'
  '     P.Binder{Q.QMany{}, "proof", eq("MechCongr", Term.Var{4n}, Term.Var{1n}, Term.Var{0n})}],\n'
  '    eq("Result", Term.Var{4n}, D.apply(Term.Var{5n}, Term.Var{3n}, Term.Var{2n}), D.apply(Term.Var{5n}, '
  'Term.Var{3n}, Term.Var{1n})),\n'
  '    Term.Elim{Term.ElimData{Shape.SMu{"MechCongr", [Term.Var{1n}]}, Term.Var{0n}, Q.QOne{},\n'
  '      Some{Term.Motive{Some{"MechCongr"}, ["right"], "self",\n'
  '        eq("Result", Term.Var{6n}, D.apply(Term.Var{7n}, Term.Var{5n}, Term.Var{4n}), '
  'D.apply(Term.Var{7n}, Term.Var{5n}, Term.Var{1n}))}},\n'
  '      [Common.Pair{Term.ACtor{"mechReflCtor"}, Term.Leg{[],\n'
  '        Term.In{Shape.SMu{"Result", [D.apply(Term.Var{5n}, Term.Var{3n}, Term.Var{2n})]}, '
  'Term.ACtor{"mechReflCtor"}, []}}}]}})\n'
  '\n',
  'prelude_congruence',
  'PRELUDE-CONGRUENCE-FAIL mismatch: the term has type Type u0 and the expected type is Type u1'),
 ('C-CONG-M2',
  'bend2/prelude/congruence.bend',
  'D.apply(Term.Var{5n}, Term.Var{3n}, Term.Var{1n})),',
  'D.apply(Term.Var{5n}, Term.Var{3n}, Term.Var{2n})),',
  'prelude_congruence',
  'PRELUDE-CONGRUENCE-FAIL mismatch: the term has type (Lan SMu Result [(Out SPi w point A (APt w y) f)]'),
 ('C-CONG-M3',
  'bend2/surface/family_poly.bend',
  ' || Poly.member(companion_names(companions), value)',
  '',
  'family_groups',
  'FAMILY-GROUPS-FAIL ordered-families-members-and-renaming: mismatch: the name Second is already declared'),
 ('C-CONG-M4',
  'bend2/surface/family_poly.bend',
  'globals : Global.T <- check_members([member], catalog, arity, globals)',
  'globals : Global.T <- Budget.pure(Global.T, globals)',
  'family_groups',
  'FAMILY-GROUPS-FAIL member-family-collision: expected refusal: mismatch: the name Second is already '
  'declared'),
 ('C-CONG-M5',
  'bend2/prelude/congruence.bend',
  'def catalog_comp(globals: Global.T) -> Budget.Comp(Family.T):\n'
  '  +u = Level.var(0n)\n'
  '  +v = Level.var(1n)\n'
  '  Family.declare_group_elaborated_comp(globals, [], 2n, [equality("MechCongr", u), equality("Result", '
  'v)], Family.raw_callbacks([congr(u, v)]))\n'
  '\n',
  'def catalog_without_budget(globals: Global.T) -> Budget.Comp(Family.T):\n'
  '  +u = Level.var(0n)\n'
  '  +v = Level.var(1n)\n'
  '  Family.declare_group_elaborated_comp(globals, [], 2n, [equality("MechCongr", u), equality("Result", '
  'v)], Family.raw_callbacks([congr(u, v)]))\n'
  '\n'
  'def catalog_comp(globals: Global.T) -> Budget.Comp(Family.T):\n'
  '  Budget.lift(Family.T, Budget.run(Family.T, catalog_without_budget(globals), Budget.unlimited()))\n'
  '\n',
  'prelude_congruence',
  'PRELUDE-CONGRUENCE-FAIL budget: expected refusal'),
 ('C-CONG-M6',
  'test/fixtures/prelude/congruence.mech',
  'case upProof as self in Up_Result right return MechNat with\n  | mechReflCtor => mechSucc mechZero',
  'case upProof as self in Up_Result right return MechNat with\n  | mechReflCtor => mechZero',
  'prelude_congruence',
  'PRELUDE-CONGRUENCE-FAIL wrong computation: upValue'),
 ('C-CONG-M7',
  'bend2/tests/surface_foundation_data.bend',
  'Negative{"wrong-instance", "mismatch: the term has type (Lan SMu Same_Result [y] (Sec SColl 2 [ => (Lan '
  'SMu MechNat [] (Sec SColl 0 []));  => x])) and the expected type is (Lan SMu Again_Result [y]", "def '
  'badInstance : (0 x : MechNat) -> (0 y : MechNat) ->\\n      Same MechNat x y -> Same_Result MechNat x y '
  ':=\\n    fun (0 x : MechNat) (0 y : MechNat) (e : Same MechNat x y) =>\\n      Same_congr MechNat MechNat '
  '(fun (n : MechNat) => n) x y e", "def badInstance : (0 x : MechNat) -> (0 y : MechNat) ->\\n      Same '
  'MechNat x y -> Again_Result MechNat x y :=\\n    fun (0 x : MechNat) (0 y : MechNat) (e : Same MechNat x '
  'y) =>\\n      Same_congr MechNat MechNat (fun (n : MechNat) => n) x y e"},',
  'Negative{"wrong-instance", "mismatch: the term has type (Lan SMu Same_Result [y] (Sec SColl 2 [ => (Lan '
  'SMu MechNat [] (Sec SColl 0 []));  => x])) and the expected type is (Lan SMu Same_Result [y]", "def '
  'badInstance : (0 x : MechNat) -> (0 y : MechNat) ->\\n      Same MechNat x y -> Same_Result MechNat x y '
  ':=\\n    fun (0 x : MechNat) (0 y : MechNat) (e : Same MechNat x y) =>\\n      Same_congr MechNat MechNat '
  '(fun (n : MechNat) => n) x y e", "def badInstance : (0 x : MechNat) -> (0 y : MechNat) ->\\n      Same '
  'MechNat x y -> Same_Result MechNat x y :=\\n    fun (0 x : MechNat) (0 y : MechNat) (e : Same MechNat x '
  'y) =>\\n      Same_congr MechNat MechNat (fun (n : MechNat) => n) x y e"},',
  'prelude_congruence',
  'PRELUDE-CONGRUENCE-FAIL wrong-instance: expected refusal:'),
 ('C-CONG-M8',
  'bend2/surface/family_poly.bend',
  'map_member(member, Poly.Action{Poly.ScopeLevels{arity}, [], catalog_names(catalog)})',
  'map_member(member, Poly.Action{Poly.ScopeLevels{arity}, [], []})',
  'family_groups',
  'FAMILY-GROUPS-FAIL member-template-reference: wrong refusal: unbound: the family Other is not declared'),
 ('C-CONG-M9',
  'bend2/surface/family_poly.bend',
  'def install_import(imported: Imported, +globals: Global.T, +catalog: T) -> Budget.Comp(Global.T):\n'
  '  match imported:\n'
  '    case Imported{True{}, family, ctors}:\n'
  '      check_reuse(globals, 0n, family, ctors)\n'
  '    case Imported{False{}, +family, ctors}:\n'
  '      do Budget.Comp<Global.T>:\n'
  '        checked : Unit <- require_fresh(occupied(globals, catalog, family_name(family)), '
  'family_name(family))\n'
  '        provisional : Global.T <- Check.declare_family_comp(globals, 0n, family)\n'
  '        Check.define_ctors_comp(provisional, 0n, [family_name(family)], family_name(family), ctors)\n'
  '\n',
  'def install_import(imported: Imported, +globals: Global.T, +catalog: T) -> Budget.Comp(Global.T):\n'
  '  match imported:\n'
  '    case Imported{True{}, family, ctors}:\n'
  '      check_reuse(globals, 0n, family, ctors)\n'
  '    case Imported{False{}, +family, ctors}:\n'
  '      do Budget.Comp<Global.T>:\n'
  '        provisional : Global.T <- Check.declare_family_comp(globals, 0n, family)\n'
  '        Check.define_ctors_comp(provisional, 0n, [family_name(family)], family_name(family), ctors)\n'
  '\n',
  'family_groups',
  'FAMILY-GROUPS-FAIL target-companion-collision-atomic: expected refusal: mismatch: the name One_Second is '
  'already declared'),
 ('C-CONG-M10',
  'bend2/surface/family_poly.bend',
  'def check_members(members: List<&2, Check.Decl>, +catalog: T, +arity: Nat, +globals: Global.T) -> '
  'Budget.Comp(Global.T):\n'
  '  match members:\n'
  '    case Nil{}:\n'
  '      Budget.pure(Global.T, globals)\n'
  '    case Con{+decl, tail}:\n'
  '      do Budget.Comp<Global.T>:\n'
  '        tick : Unit <- Budget.tick()\n'
  '        +name : String = Poly.decl_name(decl)\n'
  '        checked : Unit <- require_fresh(occupied(globals, catalog, name), name)\n'
  '        entry : Global.Entry <- Check.check_decl_comp(globals, arity, decl)\n'
  '        check_members(tail, catalog, arity, Global.add(name, entry, globals))\n'
  '\n',
  'def unchecked_member(decl: Check.Decl) -> Budget.Comp(Global.Entry):\n'
  '  match decl:\n'
  '    case Check.Decl{name, kind, ty, None{}}:\n'
  '      Budget.failed(Global.Entry, Error.Cannot_infer{"the definition " ++ name ++ " has no body"})\n'
  '    case Check.Decl{name, kind, ty, Some{body}}:\n'
  '      Budget.pure(Global.Entry, Global.Def{Global.DefEntry{ty, body, True{}, None{}, False{}}})\n'
  '\n'
  'def check_members(members: List<&2, Check.Decl>, +catalog: T, +arity: Nat, +globals: Global.T) -> '
  'Budget.Comp(Global.T):\n'
  '  match members:\n'
  '    case Nil{}:\n'
  '      Budget.pure(Global.T, globals)\n'
  '    case Con{+decl, tail}:\n'
  '      do Budget.Comp<Global.T>:\n'
  '        tick : Unit <- Budget.tick()\n'
  '        +name : String = Poly.decl_name(decl)\n'
  '        checked : Unit <- require_fresh(occupied(globals, catalog, name), name)\n'
  '        entry : Global.Entry <- unchecked_member(decl)\n'
  '        check_members(tail, catalog, arity, Global.add(name, entry, globals))\n'
  '\n',
  'family_groups',
  'FAMILY-GROUPS-FAIL member-body: expected refusal: mismatch: the term has type (Lan SMu First'),
 ('C-CONG-M11',
  'bend2/surface/family_poly.bend',
  'member : Check.Decl <- elaborate(globals)',
  'member : Check.Decl <- elaborate(Global.empty())',
  'family_groups',
  'FAMILY-GROUPS-FAIL callback-order-and-parity: mismatch: callback saw an incomplete family'),
 ('C-CONG-M12',
  'bend2/surface/family_poly.bend',
  '        tick : Unit <- Budget.tick()\n        member : Check.Decl <- elaborate(globals)',
  '        member : Check.Decl <- elaborate(globals)',
  'family_groups',
  'FAMILY-GROUPS-FAIL callback-budget-stops-at-entry: wrong refusal: mismatch: a callback ran after an invalid predecessor')]
runner.execute(ROOT, sys.argv[1], protocols, controls)
