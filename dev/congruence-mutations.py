"""Preserve the original congruence and family-group mutation controls on the C port (tcc)."""
import importlib.util
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit("usage: python3 -I dev/congruence-mutations.py NEW_WORK_DIRECTORY")
spec = importlib.util.spec_from_file_location("c_mutation", ROOT / "dev/c-mutation.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
protocols = {
    "prelude_congruence": ("c/tests/surface_prelude_congruence.c", "PRELUDE-CONGRUENCE-OK instances=8 computations=4 negatives=5"),
    "family_groups": ("c/tests/surface_family_groups.c", "FAMILY-GROUPS-OK cases=31", []),
}
# Each control names its Bend anchor (HEAD 7a28534) and the rule the C mutation breaks; the diagnostics are
# the original expected prefixes (decision 279 (a), hop 316).
controls = [
  # C-CONG-M1: prelude/congruence.bend: the B binder lives in Term.Univ{u} instead of Term.Univ{v}; rule: the congr member keeps its two universe parameters apart
 ('C-CONG-M1',
  'c/prelude/congruence.c',
  '  push(heap, &binders, binder(heap, QUANTITY_ZERO, "B", term_univ(heap, v)));',
  '  push(heap, &binders, binder(heap, QUANTITY_ZERO, "B", term_univ(heap, u)));',
  'prelude_congruence',
  'PRELUDE-CONGRUENCE-FAIL mismatch: the term has type Type u0 and the expected type is Type u1'),
  # C-CONG-M2: prelude/congruence.bend: the Result right side applies f to x (Var 2) instead of y (Var 1); rule: the congr result relates f x to f y
 ('C-CONG-M2',
  'c/prelude/congruence.c',
  '                    dependent_apply(heap, var(heap, 5), var(heap, 3), var(heap, 1))),',
  '                    dependent_apply(heap, var(heap, 5), var(heap, 3), var(heap, 2))),',
  'prelude_congruence',
  'PRELUDE-CONGRUENCE-FAIL mismatch: the term has type (Lan SMu Result [(Out SPi w point A (APt w y) f)]'),
  # C-CONG-M3: surface/family_poly.bend: the rename test dropped Poly.member(companion_names(companions), value); rule: a supplied member named like a companion is renamed under the alias
 ('C-CONG-M3',
  'c/surface/family_poly.c',
  '  if (str_eq(value, family_family_name(scheme.family)) || poly_member(&companions, value) || poly_member(&members, value))',
  '  if (str_eq(value, family_family_name(scheme.family)) || poly_member(&members, value))',
  'family_groups',
  'FAMILY-GROUPS-FAIL ordered-families-members-and-renaming: mismatch: the name Second is already declared'),
  # C-CONG-M4: surface/family_poly.bend: check_members([member], catalog, arity, globals) became Budget.pure(globals); rule: a group member callback result is checked against the catalog
 ('C-CONG-M4',
  'c/surface/family_poly.c',
  '  return bind_frame(heap, family_check_members(heap, &frame->members, frame->catalog, frame->arity, frame->globals),\n                    elaborate_members_k4, frame);',
  '  return bind_frame(heap, pure_globals(heap, frame->globals), elaborate_members_k4, frame);',
  'family_groups',
  'FAMILY-GROUPS-FAIL member-family-collision: expected refusal: mismatch: the name Second is already declared'),
  # C-CONG-M5: prelude/congruence.bend: catalog_comp runs the group under Budget.unlimited() and lifts the result; rule: the congruence catalog honours the caller's budget
 ('C-CONG-M5',
  'c/prelude/congruence.c',
  'Comp congruence_catalog_comp(Heap *heap, Globals globals) {',
  'static Comp congruence_catalog_without_budget(Heap *heap, Globals globals);\nComp congruence_catalog_comp(Heap *heap, Globals globals) {\n  return budget_lift(budget_run(heap, congruence_catalog_without_budget(heap, globals), budget_unlimited()));\n}\nstatic Comp congruence_catalog_without_budget(Heap *heap, Globals globals) {',
  'prelude_congruence',
  'PRELUDE-CONGRUENCE-FAIL budget: expected refusal'),
  # C-CONG-M6: fixture control, unchanged
 ('C-CONG-M6',
  'test/fixtures/prelude/congruence.mech',
  'case upProof as self in Up_Result right return MechNat with\n  | mechReflCtor => mechSucc mechZero',
  'case upProof as self in Up_Result right return MechNat with\n  | mechReflCtor => mechZero',
  'prelude_congruence',
  'PRELUDE-CONGRUENCE-FAIL wrong computation: upValue'),
  # C-CONG-M7: tests/surface_foundation_data.bend: the wrong-instance negative names Same_Result instead of Again_Result; rule: the negative fixture must be refused
 ('C-CONG-M7',
  'c/tests/surface_foundation_data.c',
  '     "mismatch: the term has type (Lan SMu Same_Result [y] (Sec SColl 2 [ => (Lan SMu MechNat [] (Sec SColl 0 []));  => x])) and the expected type is (Lan SMu Again_Result [y]",\n     "def badInstance : (0 x : MechNat) -> (0 y : MechNat) ->\\n      Same MechNat x y -> Same_Result MechNat x y :=\\n    fun (0 x : MechNat) (0 y : MechNat) (e : Same MechNat x y) =>\\n      Same_congr MechNat MechNat (fun (n : MechNat) => n) x y e",\n     "def badInstance : (0 x : MechNat) -> (0 y : MechNat) ->\\n      Same MechNat x y -> Again_Result MechNat x y :=\\n    fun (0 x : MechNat) (0 y : MechNat) (e : Same MechNat x y) =>\\n      Same_congr MechNat MechNat (fun (n : MechNat) => n) x y e"},\n',
  '     "mismatch: the term has type (Lan SMu Same_Result [y] (Sec SColl 2 [ => (Lan SMu MechNat [] (Sec SColl 0 []));  => x])) and the expected type is (Lan SMu Same_Result [y]",\n     "def badInstance : (0 x : MechNat) -> (0 y : MechNat) ->\\n      Same MechNat x y -> Same_Result MechNat x y :=\\n    fun (0 x : MechNat) (0 y : MechNat) (e : Same MechNat x y) =>\\n      Same_congr MechNat MechNat (fun (n : MechNat) => n) x y e",\n     "def badInstance : (0 x : MechNat) -> (0 y : MechNat) ->\\n      Same MechNat x y -> Same_Result MechNat x y :=\\n    fun (0 x : MechNat) (0 y : MechNat) (e : Same MechNat x y) =>\\n      Same_congr MechNat MechNat (fun (n : MechNat) => n) x y e"},\n',
  'prelude_congruence',
  'PRELUDE-CONGRUENCE-FAIL wrong-instance: expected refusal:'),
  # C-CONG-M8: surface/family_poly.bend: map_member received Poly.Action{ScopeLevels{arity}, [], []} instead of catalog_names(catalog); rule: a member template may reference the catalog families
 ('C-CONG-M8',
  'c/surface/family_poly.c',
  '  return poly_action(poly_scope_levels(arity), names_new(heap), family_catalog_names(heap, catalog));',
  '  return poly_action(poly_scope_levels(arity), names_new(heap), names_new(heap));',
  'family_groups',
  'FAMILY-GROUPS-FAIL member-template-reference: wrong refusal: unbound: the family Other is not declared'),
  # C-CONG-M9: surface/family_poly.bend: install_import dropped require_fresh(occupied(globals, catalog, family_name(family)), ...); rule: an imported family may not collide with a declared name
 ('C-CONG-M9',
  'c/surface/family_poly.c',
  '  return bind_frame(heap, family_require_fresh(heap, family_occupied(globals, catalog, name), name),\n                    install_import_closed_k1, frame);',
  '  return bind_frame(heap, family_require_fresh(heap, false, name),\n                    install_import_closed_k1, frame);',
  'family_groups',
  'FAMILY-GROUPS-FAIL target-companion-collision-atomic: expected refusal: mismatch: the name One_Second is already declared'),
  # C-CONG-M10: surface/family_poly.bend: check_members takes the entry from unchecked_member(decl) instead of Check.check_decl_comp; rule: every group member body is checked
 ('C-CONG-M10',
  'c/surface/family_poly.c',
  'static Comp check_members_k2(Heap *heap, void *ctx, void *value) {\n  Frame *frame = ctx;\n  (void)value;\n  return bind_frame(heap, check_check_decl_comp(heap, frame->globals, frame->arity, frame->decl), check_members_k3, frame);',
  'static Comp unchecked_member(Heap *heap, Decl decl) {\n  DefEntry def;\n  Entry *boxed = heap_alloc(heap, sizeof *boxed);\n  if (boxed == NULL) return comp_oom();\n  if (!decl.body.some) {\n    return budget_failed(error_make(\n        ERROR_CANNOT_INFER, SURFACE_CAT(heap, str_lit("the definition "), decl.name, str_lit(" has no body"))));\n  }\n  memset(&def, 0, sizeof def);\n  def.ty = decl.ty;\n  def.body = decl.body.value;\n  def.reducible = true;\n  def.partial = false;\n  *boxed = global_entry_def(def);\n  return budget_pure(boxed);\n}\n\nstatic Comp check_members_k2(Heap *heap, void *ctx, void *value) {\n  Frame *frame = ctx;\n  (void)value;\n  return bind_frame(heap, unchecked_member(heap, frame->decl), check_members_k3, frame);',
  'family_groups',
  'FAMILY-GROUPS-FAIL member-body: expected refusal: mismatch: the term has type (Lan SMu First'),
  # C-CONG-M11: surface/family_poly.bend: the group member callback ran under Global.empty() instead of globals; rule: a callback sees the complete family
 ('C-CONG-M11',
  'c/surface/family_poly.c',
  '  return bind_frame(heap, frame->elaborator.fn(heap, frame->elaborator.ctx, frame->globals), elaborate_members_k2, frame);',
  '  return bind_frame(heap, frame->elaborator.fn(heap, frame->elaborator.ctx, global_empty()), elaborate_members_k2, frame);',
  'family_groups',
  'FAMILY-GROUPS-FAIL callback-order-and-parity: mismatch: callback saw an incomplete family'),
  # C-CONG-M12: surface/family_poly.bend: the Budget.tick() before elaborate(globals) was dropped; rule: every group member callback costs one tick
 ('C-CONG-M12',
  'c/surface/family_poly.c',
  '  return bind_frame(heap, budget_tick(heap), elaborate_members_k1, frame);',
  '  return elaborate_members_k1(heap, frame, NULL);',
  'family_groups',
  'FAMILY-GROUPS-FAIL callback-budget-stops-at-entry: wrong refusal: mismatch: a callback ran after an invalid predecessor')]
runner.execute(ROOT, sys.argv[1], protocols, controls, suite_timeout=180, build_timeout=180)
