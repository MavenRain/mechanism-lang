"""Preserve ten original transport and family-member mutation controls in Bend."""
import importlib.util
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit("usage: python3 -I dev/transport-mutations.py NEW_WORK_DIRECTORY")
spec = importlib.util.spec_from_file_location("bend2_mutation", ROOT / "dev/bend2-mutation.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
protocols = {
    "prelude_transport": ("bend2/tests/surface_prelude_transport.bend", "PRELUDE-TRANSPORT-OK templates=2 instances=7 negatives=5"),
    "family_members": ("bend2/tests/surface_family_members.bend", "FAMILY-MEMBERS-OK cases=35", []),
}
controls = [('C-TRANSPORT-M1',
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
  '      Budget.pure(Global.Entry, Global.Def{Global.DefEntry{ty, Term.Auto{}, True{}, None{}, False{}}})\n'
  '    case Check.Decl{name, kind, ty, Some{body}}:\n'
  '      Budget.pure(Global.Entry, Global.Def{Global.DefEntry{ty, body, True{}, None{}, False{}}})\n'
  '\n'
  'def member_entry(globals: Global.T, arity: Nat, decl: Check.Decl) -> Budget.Comp(Global.Entry):\n'
  '  match arity:\n'
  '    case 0n: Check.check_decl_comp(globals, 0n, decl)\n'
  '    case 1n+rest: unchecked_member(decl)\n'
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
  '        entry : Global.Entry <- member_entry(globals, arity, decl)\n'
  '        check_members(tail, catalog, arity, Global.add(name, entry, globals))\n'
  '\n',
  'family_members',
  'expected refusal: unbound: de Bruijn index 0',
  {'contains': True, 'arguments': ['definition-must-check']}),
 ('C-TRANSPORT-M2',
  'bend2/surface/family_poly.bend',
  ' || Poly.member(member_names(members), value)',
  '',
  'family_members',
  'the name witness is already declared',
  {'contains': True, 'arguments': ['ordered-members-and-renaming']}),
 ('C-TRANSPORT-M3',
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
  '      Budget.pure(Global.Entry, Global.Def{Global.DefEntry{ty, Term.Auto{}, True{}, None{}, False{}}})\n'
  '    case Check.Decl{name, kind, ty, Some{body}}:\n'
  '      Budget.pure(Global.Entry, Global.Def{Global.DefEntry{ty, body, True{}, None{}, False{}}})\n'
  '\n'
  'def member_entry(globals: Global.T, arity: Nat, decl: Check.Decl) -> Budget.Comp(Global.Entry):\n'
  '  match arity:\n'
  '    case 0n: unchecked_member(decl)\n'
  '    case 1n+rest: Check.check_decl_comp(globals, 1n+rest, decl)\n'
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
  '        entry : Global.Entry <- member_entry(globals, arity, decl)\n'
  '        check_members(tail, catalog, arity, Global.add(name, entry, globals))\n'
  '\n',
  'family_members',
  'expected refusal: mismatch: the term has type',
  {'contains': True, 'arguments': ['closed-rechecking']}),
 ('C-TRANSPORT-M4',
  'bend2/surface/family_poly.bend',
  'def instantiate_comp(+globals: Global.T, +catalog: T, +name: String, +levels: List<&2, Level.T>, '
  '+as_name: String, +reuse: Bindings, exports: Maybe<&2, Bindings>) -> Budget.Comp(Global.T):\n'
  '  do Budget.Comp<Global.T>:\n'
  '    +scheme : Scheme <- require_schema(lookup(catalog, name), name)\n'
  '    checked : Unit <- require_fresh(occupied(globals, catalog, as_name), as_name)\n'
  '    checked : Unit <- check_levels(levels, schema_arity(scheme), name, 0n, True{})\n'
  '    tick : Unit <- Budget.tick()\n'
  '    checked : Unit <- check_bindings(reuse, companion_names(raw_families(scheme)), [], globals, [])\n'
  '    exports : Bindings <- check_exports(scheme, name, as_name, reuse, exports)\n'
  '    +action : Poly.Action = instance_action(scheme, name, as_name, normal_levels(levels), reuse, '
  'exports)\n'
  '    families : List<&2, Imported> <- map_imports(raw_families(scheme), action, reuse)\n'
  '    members : List<&2, Check.Decl> <- map_members(schema_members(scheme), action)\n'
  '    installed : Global.T <- install_imports(families, globals, catalog)\n'
  '    check_members(members, catalog, 0n, installed)',
  'def preserve_member_levels(action: Poly.Action) -> Poly.Action:\n'
  '  match action:\n'
  '    case Poly.Action{levels, names, forbidden}: Poly.Action{Poly.PreserveLevels{}, names, forbidden}\n'
  '\n'
  'def instantiate_comp(+globals: Global.T, +catalog: T, +name: String, +levels: List<&2, Level.T>, '
  '+as_name: String, +reuse: Bindings, exports: Maybe<&2, Bindings>) -> Budget.Comp(Global.T):\n'
  '  do Budget.Comp<Global.T>:\n'
  '    +scheme : Scheme <- require_schema(lookup(catalog, name), name)\n'
  '    checked : Unit <- require_fresh(occupied(globals, catalog, as_name), as_name)\n'
  '    checked : Unit <- check_levels(levels, schema_arity(scheme), name, 0n, True{})\n'
  '    tick : Unit <- Budget.tick()\n'
  '    checked : Unit <- check_bindings(reuse, companion_names(raw_families(scheme)), [], globals, [])\n'
  '    exports : Bindings <- check_exports(scheme, name, as_name, reuse, exports)\n'
  '    +action : Poly.Action = instance_action(scheme, name, as_name, normal_levels(levels), reuse, '
  'exports)\n'
  '    families : List<&2, Imported> <- map_imports(raw_families(scheme), action, reuse)\n'
  '    members : List<&2, Check.Decl> <- map_members(schema_members(scheme), '
  'preserve_member_levels(action))\n'
  '    installed : Global.T <- install_imports(families, globals, catalog)\n'
  '    check_members(members, catalog, 0n, installed)',
  'family_members',
  'hidden-level-specialization: universe: universe level is outside the global parameter scope',
  {'contains': True, 'arguments': ['hidden-level-specialization']}),
 ('C-TRANSPORT-M5',
  'test/fixtures/prelude/transport.mech',
  '(CastData_refl MechNat) (mechSucc mechZero)',
  '(CastData_refl MechNat) mechZero',
  'prelude_transport',
  'the constructor transportIsOne of TransportIsOne gives the index',
  {'contains': True}),
 ('C-TR-M1',
  'bend2/surface/family_poly.bend',
  'def map_member(decl: Check.Decl, +action: Poly.Action) -> Budget.Comp(Check.Decl):\n'
  '  match decl:\n'
  '    case Check.Decl{+name, +kind, ty, body}:\n'
  '      do Budget.Comp<Check.Decl>:\n'
  '        tick : Unit <- Budget.tick()\n'
  '        checked : Unit <- Budget.lift(Unit, member_kind(kind))\n'
  '        mapped_name : String <- Budget.lift(String, Poly.map_name(action, name))\n'
  '        ty : Term.T <- Poly.map_term(action, ty)\n'
  '        body : Term.T <- Budget.lift(Term.T, member_body(body, name))\n'
  '        body : Term.T <- Poly.map_term(action, body)\n'
  '        return Check.Decl{mapped_name, kind, ty, Some{body}}',
  'def map_member(decl: Check.Decl, +action: Poly.Action) -> Budget.Comp(Check.Decl):\n'
  '  match decl:\n'
  '    case Check.Decl{+name, +kind, ty, body}:\n'
  '      do Budget.Comp<Check.Decl>:\n'
  '        checked : Unit <- Budget.lift(Unit, member_kind(kind))\n'
  '        mapped_name : String <- Budget.lift(String, Poly.map_name(action, name))\n'
  '        ty : Term.T <- Poly.map_term(action, ty)\n'
  '        body : Term.T <- Budget.lift(Term.T, member_body(body, name))\n'
  '        body : Term.T <- Poly.map_term(action, body)\n'
  '        return Check.Decl{mapped_name, kind, ty, Some{body}}',
  'family_members',
  'declaration-member-budget: member declaration polls',
  {'contains': True, 'arguments': ['declaration-member-budget']}),
 ('C-TR-M2',
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
  'def check_members(members: List<&2, Check.Decl>, +catalog: T, +arity: Nat, +globals: Global.T) -> '
  'Budget.Comp(Global.T):\n'
  '  match members:\n'
  '    case Nil{}:\n'
  '      Budget.pure(Global.T, globals)\n'
  '    case Con{+decl, tail}:\n'
  '      do Budget.Comp<Global.T>:\n'
  '        +name : String = Poly.decl_name(decl)\n'
  '        checked : Unit <- require_fresh(occupied(globals, catalog, name), name)\n'
  '        entry : Global.Entry <- Check.check_decl_comp(globals, arity, decl)\n'
  '        check_members(tail, catalog, arity, Global.add(name, entry, globals))\n'
  '\n',
  'family_members',
  'specialization-member-budget: member specialization polls',
  {'contains': True, 'arguments': ['specialization-member-budget']}),
 ('C-TR-M3',
  'bend2/surface/family_poly.bend',
  '        reserved : Unit <- reserve_members(members, catalog)\n',
  '',
  'family_members',
  'member-template-collision: wrong refusal: not yet: references between family schemas are not supported',
  {'contains': True, 'arguments': ['member-template-collision']}),
 ('C-TR-M4',
  'bend2/tests/surface_prelude_transport.bend',
  '(TransportHigher_refl MechNat mechZero)',
  '(TransportData_refl MechNat mechZero)',
  'prelude_transport',
  'expected refusal: mismatch: the term has type (Lan SMu TransportHigher',
  {'contains': True}),
 ('C-TR-M5',
  'bend2/tests/surface_prelude_transport.bend',
  '    refusal(initial,\n      "def unavailable',
  '    refusal(installed,\n      "def unavailable',
  'prelude_transport',
  'expected refusal: unbound: CastData_cast',
  {'contains': True})]
runner.execute(ROOT, sys.argv[1], protocols, controls)
