"""Preserve the six original dependency-export mutation controls on the C port (tcc)."""
import importlib.util
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[3]
if len(sys.argv) != 2:
    raise SystemExit("usage: python3 -I dev/validation/prenex-dependency-exports/mutations.py NEW_WORK_DIRECTORY")
spec = importlib.util.spec_from_file_location("bend2_mutation", ROOT / "dev/bend2-mutation.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
protocols = {"deps": ("c/tests/surface_prenex_dependency_exports.c", "PRENEX-DEPENDENCY-EXPORTS-OK positives=7 negatives=23 parser=6 budget=2")}
# Each control names its Bend anchor (bend2/surface at HEAD) and the rule the C mutation breaks; the diagnostics
# are the original expected texts. Every C anchor is unique in its file (rg -c -F == 1).
controls = [
 # mapping-validation: family_poly.bend export_loop, Nil case: Nat.is_eq(length(sources), length(names)) ->
 # Bool.not(Nat.is_lt(length(names), length(sources))); rule: every family member needs an export name
 ('mapping-validation',
  'c/surface/family_poly.c',
  '    if (frame->sources.len == frame->names.len) return budget_lift(done_unit());',
  '    if (frame->sources.len <= frame->names.len) return budget_lift(done_unit());',
  'deps',
  'missing: expected refusal',
  {'contains': True, 'stream': 'stderr'}),
 # reference-renaming: family_poly.bend rename_bindings: rename_export(scheme, source, alias, exports, name) ->
 # exports = []; rule: a member reference inside a binding is renamed through the export mapping
 ('reference-renaming',
  'c/surface/family_poly.c',
  '      PolyRename rename;\n'
  '      rename.source = name;\n'
  '      rename.target = family_mapped_name(family_mapping_find(reuse, name),\n'
  '                                         family_rename_export(heap, scheme, source, alias, exports, name));\n',
  '      PolyRename rename;\n'
  '      Vec empty = names_new(heap);\n'
  '      rename.source = name;\n'
  '      rename.target = family_mapped_name(family_mapping_find(reuse, name),\n'
  '                                         family_rename_export(heap, scheme, source, alias, &empty, name));\n',
  'deps',
  'unbound',
  {'contains': True, 'stream': 'stderr'}),
 # alias-reservation: family_poly.bend collision_list: require_fresh(Poly.member(occupied, name), name) ->
 # Poly.member(occupied, name) && False{}; rule: an export alias may not reuse an earlier composed alias
 ('alias-reservation',
  'c/surface/family_poly.c',
  'family_require_fresh(heap, poly_member(&frame->names, name), name)',
  'family_require_fresh(heap, poly_member(&frame->names, name) && false, name)',
  'deps',
  'earlier-composed-alias: expected refusal',
  {'contains': True, 'stream': 'stderr'}),
 # name-planning: elab.bend imported_names: planned_names(FamilyPoly.instance_names(catalog, source, alias, reuse,
 # exports)) -> exports = None{}; rule: the planned export names take part in the collision check
 ('name-planning',
  'c/surface/elab.c',
  '      Vec planned = elab_planned_names(\n'
  '          heap, family_instance_names(heap, catalog, dep->source, dep->alias, &dep->reuse, dep->exports));\n',
  '      FamilyMaybeVec dropped = dep->exports;\n'
  '      Vec planned;\n'
  '      dropped.some = false;\n'
  '      planned = elab_planned_names(\n'
  '          heap, family_instance_names(heap, catalog, dep->source, dep->alias, &dep->reuse, dropped));\n',
  'deps',
  'poly-name: expected refusal',
  {'contains': True, 'stream': 'stderr'}),
 # printing: syntax.bend specializations_text prints every specialization through drop_dependency_exports
 # (helpers = None{}); rule: print keeps the dependency export mapping
 ('printing',
  'c/surface/syntax.c',
  'Str syntax_specializations_text(Heap *heap, const Vec *specializations) {\n'
  '  Text text = text_new(heap);\n'
  '  for (size_t i = 0; i < specializations->len; i++) {\n'
  '    const SSpecialization *specialization = item_at(specializations, i);\n'
  '    if (specialization) text_add(&text, syntax_specialization_text(heap, *specialization));\n'
  '  }\n'
  '  return text_done(heap, &text);\n'
  '}\n',
  'static Str drop_dependency_exports(Heap *heap, SSpecialization specialization) {\n'
  '  SSpecialization dropped = specialization;\n'
  '  dropped.helpers.some = false;\n'
  '  return syntax_specialization_text(heap, dropped);\n'
  '}\n'
  '\n'
  'Str syntax_specializations_text(Heap *heap, const Vec *specializations) {\n'
  '  Text text = text_new(heap);\n'
  '  for (size_t i = 0; i < specializations->len; i++) {\n'
  '    const SSpecialization *specialization = item_at(specializations, i);\n'
  '    if (specialization) text_add(&text, drop_dependency_exports(heap, *specialization));\n'
  '  }\n'
  '  return text_done(heap, &text);\n'
  '}\n',
  'deps',
  'parse/print changed a dependency export mapping',
  {'contains': True, 'stream': 'stderr'}),
 # planning-budget: elab.bend validate_dependency_exports, Some case: FamilyPoly.validate_exports_comp(...) wrapped in
 # Budget.lift(Unit, Budget.run(Unit, ..., Budget.unlimited())); rule: export validation ticks the caller's budget
 ('planning-budget',
  'c/surface/elab.c',
  '  return family_validate_exports_comp(heap, globals, available, catalog, source, alias, reuse, boxed);',
  '  return budget_lift(budget_run(heap, family_validate_exports_comp(heap, globals, available, catalog, source, alias, reuse, boxed),\n'
  '                                budget_unlimited()));',
  'deps',
  'collision-budget: wrong refusal: mismatch: the name w is already declared',
  {'contains': True, 'stream': 'stderr'})]
runner.execute(ROOT, sys.argv[1], protocols, controls, suite_timeout=120, build_timeout=120)
