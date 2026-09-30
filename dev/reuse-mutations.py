"""Preserve the original reuse and symbolic-reuse mutation controls in Bend."""
import importlib.util
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) not in (2, 3) or (len(sys.argv) == 3 and sys.argv[2] != "--symbolic"):
    raise SystemExit("usage: python3 -I dev/reuse-mutations.py NEW_WORK_DIRECTORY [--symbolic]")
spec = importlib.util.spec_from_file_location("bend2_mutation", ROOT / "dev/bend2-mutation.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
ordinary = [('C-REUSE-M1',
  'bend2/surface/family_poly.bend',
  'level_ok && Positivity.same_family(actual, expected), Done{globals}',
  'level_ok && Positivity.same_family(actual, actual), Done{globals}',
  'reuse',
  'TEMPLATE-REUSE-FAIL wrong-constructor: wrong refusal: missing branch: the elimination of Other has no '
  'branch at other'),
 ('C-REUSE-M2',
  'bend2/surface/family_poly.bend',
  'Positivity.Ctor{name, args, indices, arity, False{}} <> erase_rec(tail)',
  'Positivity.Ctor{name, args, indices, arity, self_rec} <> erase_rec(tail)',
  'reuse',
  'TEMPLATE-REUSE-FAIL mutual-group-member: wrong refusal: mismatch: the reused family B does not match the '
  'template'),
 ('C-REUSE-M3',
  'bend2/surface/family_poly.bend',
  'agrees : Bool <- LevelEq.decide_m(kernel_level(actual), kernel_level(expected), True{})',
  'agrees : Bool <- Budget.pure(Bool, Term.equal(Term.Univ{kernel_level(actual)}, Term.Univ{kernel_level(expected)}))',
  'reuse',
  'TEMPLATE-REUSE-FAIL mismatch: the reused family E does not match the template'),
 ('C-REUSE-M4',
  'bend2/surface/elab.bend',
  '    case reuse exports False{}:\n      Budget.pure(Unit, Unit{})',
  '    case Con{head, tail} exports False{}:\n'
  '      Budget.failed(Unit, Error.Not_yet{"family reuse requires a family template"})\n'
  '    case Nil{} exports False{}:\n'
  '      Budget.pure(Unit, Unit{})',
  'reuse',
  'TEMPLATE-REUSE-FAIL unknown-template-name: wrong refusal: not yet: family reuse requires a family '
  'template'),
 ('C-REUSE-M5',
  'bend2/tests/surface_template_reuse.bend',
  '      Raw.Case{"a weakened certificate refuses the reuse", unit => weakened(globals, catalog, original, '
  '2n)},\n',
  '',
  'reuse',
  'TEMPLATE-REUSE-OK negatives=17 parser=6 raw=5',
  {'exit_code': 0})]
symbolic = [('C-SREUSE-M1',
  'bend2/surface/family_poly.bend',
  '      check_reuse(globals, arity, family, ctors)',
  '      Budget.pure(Global.T, globals)',
  'symbolic',
  'TEMPLATE-SYMBOLIC-REUSE-FAIL independent-universes:'),
 ('C-SREUSE-M2',
  'bend2/surface/family_poly.bend',
  '      check_reuse(globals, arity, family, ctors)',
  '      check_reuse(globals, 0n, family, ctors)',
  'symbolic',
  'TEMPLATE-SYMBOLIC-REUSE-FAIL universe:'),
 ('C-SREUSE-M3',
  'bend2/surface/parser.bend',
  'return Parsed{Syntax.Specialization{name, value(List<&2, Universe.T>, levels), value(String, alias), '
  'value(List<&2, Common.Pair<String, String>>, reuse), value(Maybe<&2, List<&2, Common.Pair<String, '
  'String>>>, exports)}',
  'return Parsed{Syntax.Specialization{name, value(List<&2, Universe.T>, levels), value(String, alias), [], '
  'value(Maybe<&2, List<&2, Common.Pair<String, String>>>, exports)}',
  'symbolic',
  'TEMPLATE-SYMBOLIC-REUSE-FAIL mismatch:'),
 ('C-SREUSE-M4',
  'bend2/surface/family_poly.bend',
  'Poly.member(bound, local), Fail{Error.Mismatch{"the family "',
  'Poly.member([], local), Fail{Error.Mismatch{"the family "',
  'symbolic',
  'TEMPLATE-SYMBOLIC-REUSE-FAIL duplicate-binding:')]
if len(sys.argv) == 3:
    protocols = {"symbolic": ("bend2/tests/surface_template_symbolic_reuse.bend", "TEMPLATE-SYMBOLIC-REUSE-OK negatives=12 parser=6 raw=5")}
    controls = symbolic
else:
    protocols = {"reuse": ("bend2/tests/surface_template_reuse.bend", "TEMPLATE-REUSE-OK negatives=17 parser=6 raw=6")}
    controls = ordinary
runner.execute(ROOT, sys.argv[1], protocols, controls, suite_timeout=900, build_timeout=1800)
