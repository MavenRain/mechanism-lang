"""Preserve the six original dependency-export mutation controls in Bend."""
import importlib.util
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[3]
if len(sys.argv) != 2:
    raise SystemExit("usage: python3 -I dev/validation/prenex-dependency-exports/mutations.py NEW_WORK_DIRECTORY")
spec = importlib.util.spec_from_file_location("bend2_mutation", ROOT / "dev/bend2-mutation.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
protocols = {"deps": ("bend2/tests/surface_prenex_dependency_exports.bend", "PRENEX-DEPENDENCY-EXPORTS-OK positives=7 negatives=23 parser=6 budget=2")}
controls = [('mapping-validation',
  'bend2/surface/family_poly.bend',
  'Nat.is_eq(List.length(&2, String, sources), List.length(&2, String, names))',
  'Bool.not(Nat.is_lt(List.length(&2, String, names), List.length(&2, String, sources)))',
  'deps',
  'missing: expected refusal',
  {'contains': True, 'stream': 'stderr'}),
 ('reference-renaming',
  'bend2/surface/family_poly.bend',
  'rename_export(scheme, source, alias, exports, name)',
  'rename_export(scheme, source, alias, [], name)',
  'deps',
  'unbound',
  {'contains': True, 'stream': 'stderr'}),
 ('alias-reservation',
  'bend2/surface/family_poly.bend',
  'require_fresh(Poly.member(occupied, name), name)',
  'require_fresh(Poly.member(occupied, name) && False{}, name)',
  'deps',
  'earlier-composed-alias: expected refusal',
  {'contains': True, 'stream': 'stderr'}),
 ('name-planning',
  'bend2/surface/elab.bend',
  'planned_names(FamilyPoly.instance_names(catalog, source, alias, reuse, exports))',
  'planned_names(FamilyPoly.instance_names(catalog, source, alias, reuse, None{}))',
  'deps',
  'poly-name: expected refusal',
  {'contains': True, 'stream': 'stderr'}),
 ('printing',
  'bend2/surface/syntax.bend',
  'def specializations_text(specializations: List<&2, Specialization>) -> String:\n'
  '  match specializations:\n'
  '    case Nil{}:\n'
  '      ""\n'
  '    case Con{specialization, tail}:\n'
  '      specialization_text(specialization) ++ specializations_text(tail)\n',
  'def drop_dependency_exports(specialization: Specialization) -> String:\n'
  '  match specialization:\n'
  '    case Specialization{template, levels, name, names, helpers}:\n'
  '      specialization_text(Specialization{template, levels, name, names, None{}})\n'
  '\n'
  'def specializations_text(specializations: List<&2, Specialization>) -> String:\n'
  '  match specializations:\n'
  '    case Nil{}:\n'
  '      ""\n'
  '    case Con{specialization, tail}:\n'
  '      drop_dependency_exports(specialization) ++ specializations_text(tail)\n',
  'deps',
  'parse/print changed a dependency export mapping',
  {'contains': True, 'stream': 'stderr'}),
 ('planning-budget',
  'bend2/surface/elab.bend',
  'FamilyPoly.validate_exports_comp(globals, available, catalog, source, alias, reuse, exports)',
  'Budget.lift(Unit, Budget.run(Unit, FamilyPoly.validate_exports_comp(globals, available, catalog, source, '
  'alias, reuse, exports), Budget.unlimited()))',
  'deps',
  'collision-budget: wrong refusal: mismatch: the name w is already declared',
  {'contains': True, 'stream': 'stderr'})]
runner.execute(ROOT, sys.argv[1], protocols, controls, suite_timeout=120, build_timeout=120)
