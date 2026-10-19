"""Preserve ten original transport and family-member mutation controls on the C port (tcc)."""
import importlib.util
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit("usage: python3 -I dev/transport-mutations.py NEW_WORK_DIRECTORY")
spec = importlib.util.spec_from_file_location("c_mutation", ROOT / "dev/c-mutation.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
protocols = {
    'prelude_transport': ('c/tests/surface_prelude_transport.c', 'PRELUDE-TRANSPORT-OK templates=2 instances=7 negatives=5'),
    'family_members': ('c/tests/surface_family_members.c', 'FAMILY-MEMBERS-OK cases=35', []),
}
# Each control names its Bend anchor (HEAD 7a28534) and the rule the C mutation breaks; the diagnostics are
# the original expected prefixes (decision 279 (a), hop 320).
controls = [
  # C-TRANSPORT-M1: surface/family_poly.bend: check_members takes member_entry, which skips the kernel check when the arity is positive (Def entry with the body or Term.Auto); rule: every member of a polymorphic family is checked
 ('C-TRANSPORT-M1',
  'c/surface/family_poly.c',
  'static Comp check_members_k2(Heap *heap, void *ctx, void *value) {\n  Frame *frame = ctx;\n  (void)value;\n  return bind_frame(heap, check_check_decl_comp(heap, frame->globals, frame->arity, frame->decl), check_members_k3, frame);',
  'static Comp unchecked_member(Heap *heap, Decl decl) {\n  DefEntry def;\n  Entry *boxed = heap_alloc(heap, sizeof *boxed);\n  if (boxed == NULL) return comp_oom();\n  memset(&def, 0, sizeof def);\n  def.ty = decl.ty;\n  def.body = decl.body.some ? decl.body.value : term_auto();\n  def.reducible = true;\n  def.partial = false;\n  *boxed = global_entry_def(def);\n  return budget_pure(boxed);\n}\n\nstatic Comp member_entry(Heap *heap, Globals globals, Nat arity, Decl decl) {\n  if (nat_is_zero(arity)) return check_check_decl_comp(heap, globals, arity, decl);\n  return unchecked_member(heap, decl);\n}\n\nstatic Comp check_members_k2(Heap *heap, void *ctx, void *value) {\n  Frame *frame = ctx;\n  (void)value;\n  return bind_frame(heap, member_entry(heap, frame->globals, frame->arity, frame->decl), check_members_k3, frame);',
  'family_members',
  'expected refusal: unbound: de Bruijn index 0',
  {'contains': True, 'arguments': ['definition-must-check']}),
  # C-TRANSPORT-M2: surface/family_poly.bend: rename_instance no longer prefixes the member names; rule: an instance renames its members with the as_name prefix
 ('C-TRANSPORT-M2',
  'c/surface/family_poly.c',
  ' || poly_member(&members, value)',
  '',
  'family_members',
  'the name witness is already declared',
  {'contains': True, 'arguments': ['ordered-members-and-renaming']}),
  # C-TRANSPORT-M3: surface/family_poly.bend: check_members takes member_entry, which skips the kernel check when the arity is zero; rule: the members of a closed family are rechecked
 ('C-TRANSPORT-M3',
  'c/surface/family_poly.c',
  'static Comp check_members_k2(Heap *heap, void *ctx, void *value) {\n  Frame *frame = ctx;\n  (void)value;\n  return bind_frame(heap, check_check_decl_comp(heap, frame->globals, frame->arity, frame->decl), check_members_k3, frame);',
  'static Comp unchecked_member(Heap *heap, Decl decl) {\n  DefEntry def;\n  Entry *boxed = heap_alloc(heap, sizeof *boxed);\n  if (boxed == NULL) return comp_oom();\n  memset(&def, 0, sizeof def);\n  def.ty = decl.ty;\n  def.body = decl.body.some ? decl.body.value : term_auto();\n  def.reducible = true;\n  def.partial = false;\n  *boxed = global_entry_def(def);\n  return budget_pure(boxed);\n}\n\nstatic Comp member_entry(Heap *heap, Globals globals, Nat arity, Decl decl) {\n  if (nat_is_zero(arity)) return unchecked_member(heap, decl);\n  return check_check_decl_comp(heap, globals, arity, decl);\n}\n\nstatic Comp check_members_k2(Heap *heap, void *ctx, void *value) {\n  Frame *frame = ctx;\n  (void)value;\n  return bind_frame(heap, member_entry(heap, frame->globals, frame->arity, frame->decl), check_members_k3, frame);',
  'family_members',
  'expected refusal: mismatch: the term has type',
  {'contains': True, 'arguments': ['closed-rechecking']}),
  # C-TRANSPORT-M4: surface/family_poly.bend: instantiate maps the members with preserve_member_levels(action) (PreserveLevels instead of the instance level action); rule: a specialization substitutes the member levels
 ('C-TRANSPORT-M4',
  'c/surface/family_poly.c',
  '  return bind_frame(heap, family_map_members(heap, &frame->members, frame->action), instantiate_k7, frame);',
  '  frame->action.levels = poly_preserve_levels();\n  return bind_frame(heap, family_map_members(heap, &frame->members, frame->action), instantiate_k7, frame);',
  'family_members',
  'hidden-level-specialization: universe: universe level is outside the global parameter scope',
  {'contains': True, 'arguments': ['hidden-level-specialization']}),
  # C-TRANSPORT-M5: fixture control, unchanged
 ('C-TRANSPORT-M5',
  'test/fixtures/prelude/transport.mech',
  '(CastData_refl MechNat) (mechSucc mechZero)',
  '(CastData_refl MechNat) mechZero',
  'prelude_transport',
  'the constructor transportIsOne of TransportIsOne gives the index',
  {'contains': True}),
  # C-TR-M1: surface/family_poly.bend: map_member drops its Budget.tick(); rule: every member declaration polls the budget
 ('C-TR-M1',
  'c/surface/family_poly.c',
  '  return bind_frame(heap, budget_tick(heap), map_member_k1, frame);',
  '  return map_member_k1(heap, frame, NULL);',
  'family_members',
  'declaration-member-budget: member declaration polls',
  {'contains': True, 'arguments': ['declaration-member-budget']}),
  # C-TR-M2: surface/family_poly.bend: check_members drops its Budget.tick(); rule: every member specialization polls the budget
 ('C-TR-M2',
  'c/surface/family_poly.c',
  '  return bind_frame(heap, budget_tick(heap), check_members_k1, frame);',
  '  return check_members_k1(heap, frame, NULL);',
  'family_members',
  'specialization-member-budget: member specialization polls',
  {'contains': True, 'arguments': ['specialization-member-budget']}),
  # C-TR-M3: surface/family_poly.bend: declared_family skips reserve_members(members, catalog); rule: a member that collides with a template name is refused
 ('C-TR-M3',
  'c/surface/family_poly.c',
  '  return bind_frame(heap, family_reserve_members(heap, frame->items, frame->catalog), declared_family_k2, frame);',
  '  return declared_family_k2(heap, frame, NULL);',
  'family_members',
  'member-template-collision: wrong refusal: not yet: references between family schemas are not supported',
  {'contains': True, 'arguments': ['member-template-collision']}),
  # C-TR-M4: tests/surface_prelude_transport.bend: the mixedInstance negative uses TransportData_refl, so the instance checks; rule: the driver refuses a TransportHigher witness at a TransportData site
 ('C-TR-M4',
  'c/tests/surface_prelude_transport.c',
  '(TransportHigher_refl MechNat mechZero)',
  '(TransportData_refl MechNat mechZero)',
  'prelude_transport',
  'expected refusal: mismatch: the term has type (Lan SMu TransportHigher',
  {'contains': True}),
  # C-TR-M5: tests/surface_prelude_transport.bend: the unavailable negative runs against the installed globals, where CastData_cast is bound; rule: the driver refuses CastData_cast before the prelude is installed
 ('C-TR-M5',
  'c/tests/surface_prelude_transport.c',
  'return refusal(heap, initial,',
  'return refusal(heap, installed,',
  'prelude_transport',
  'expected refusal: unbound: CastData_cast',
  {'contains': True})]
runner.execute(ROOT, sys.argv[1], protocols, controls)
