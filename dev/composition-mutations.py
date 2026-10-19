"""Preserve the seven original composition mutation controls on the C port (tcc)."""
import importlib.util
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit("usage: python3 -I dev/composition-mutations.py NEW_WORK_DIRECTORY")
spec = importlib.util.spec_from_file_location("c_mutation", ROOT / "dev/c-mutation.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
protocols = {"composition": ("c/tests/surface_template_composition.c", "TEMPLATE-COMPOSITION-OK negatives=24 parser=9 raw=9 type-excluded=negative-arity")}
# Each control names its Bend anchor (surface/family_poly.bend at HEAD 7a28534) and the rule the C
# mutation breaks; the diagnostics are the original expected prefixes (decision 279 (a), hop 314).
controls = [
  # C-COMP-M1: surface/family_poly.bend line 187: the rename test dropped String.eq(value, family_name(family)); rule: a supplied member named like the family itself is renamed under the alias
 ('C-COMP-M1',
  'c/surface/family_poly.c',
  '  if (str_eq(value, family_family_name(scheme.family)) || poly_member(&companions, value) || poly_member(&members, value))',
  '  if (poly_member(&companions, value) || poly_member(&members, value))',
  'composition',
  'TEMPLATE-COMPOSITION-FAIL mismatch: the name Left is already declared'),
  # C-COMP-M2: surface/family_poly.bend line 754: raw_callbacks(members) became raw_callbacks([]); rule: the composed group elaborates the raw member callbacks before the supplied definitions; C site: the elab.c compose chain skips the pending raw member loop (composed_group_k2 is off the driver path)
 ('C-COMP-M2',
  'c/surface/elab.c',
  '    VecGet got = vec_get(&cc.compose.pending, i);',
  '    VecGet got = vec_get(&cc.compose.pending, cc.compose.pending.len);',
  'composition',
  'TEMPLATE-COMPOSITION-FAIL unbound: Inner_Right_unbox'),
  # C-COMP-M3: surface/family_poly.bend line 673: the alias freshness test dropped Poly.member(aliases, alias); rule: a dependency alias may not repeat an alias of the composed state
 ('C-COMP-M3',
  'c/surface/family_poly.c',
  '  used = str_eq(alias, frame->root) || poly_member(&frame->state.aliases, alias) ||\n         family_occupied(frame->globals, frame->catalog, alias);',
  '  used = str_eq(alias, frame->root) ||\n         family_occupied(frame->globals, frame->catalog, alias);',
  'composition',
  'TEMPLATE-COMPOSITION-FAIL duplicate-disjoint-prefix: expected refusal: mismatch: the name Shared is already declared'),
  # C-COMP-M4: surface/family_poly.bend line 701: require_fresh(String.eq(Poly.decl_name(decl), root), root) became require_fresh(False{}, root); rule: a raw group member may not carry the group name
 ('C-COMP-M4',
  'c/surface/family_poly.c',
  'family_require_fresh(heap, str_eq(poly_decl_name(decl), root), root)',
  'family_require_fresh(heap, false, root)',
  'composition',
  'TEMPLATE-COMPOSITION-FAIL raw-group-name: expected refusal: mismatch: the name Composed is already declared'),
  # C-COMP-M5: surface/family_poly.bend line 754: List.append(raw_callbacks(members), supplied) became raw_callbacks(members); rule: the supplied definitions follow the raw member callbacks; C site: the elab.c compose chain skips the supplied helper loop (composed_group_k2 is off the driver path)
 ('C-COMP-M5',
  'c/surface/elab.c',
  '    VecGet got = vec_get(&decl->helpers, i);\n    if (got.status != VEC_GET_OK) break;\n    work = compact_heap_new(live);\n    run = compact_compose_member(heap, live, &cc, elab_member_callback(&work, got.value, decl->universes), &work);',
  '    VecGet got = vec_get(&decl->helpers, decl->helpers.len);\n    if (got.status != VEC_GET_OK) break;\n    work = compact_heap_new(live);\n    run = compact_compose_member(heap, live, &cc, elab_member_callback(&work, got.value, decl->universes), &work);',
  'composition',
  'TEMPLATE-COMPOSITION-FAIL unbound: P_transfer'),
  # C-COMP-M6: surface/family_poly.bend line 677: instance_action received List.reverse(levels); rule: the dependency levels reach the instance action in source order
 ('C-COMP-M6',
  'c/surface/family_poly.c',
  '  frame->action = family_instance_action(heap, frame->scheme, frame->dependency.source, frame->dependency.alias,\n                                         frame->dependency.levels, &frame->dependency.reuse, &frame->exports);',
  '  frame->action = family_instance_action(heap, frame->scheme, frame->dependency.source, frame->dependency.alias,\n                                         vec_reverse(heap->arena, &frame->dependency.levels), &frame->dependency.reuse,\n                                         &frame->exports);',
  'composition',
  'TEMPLATE-COMPOSITION-FAIL mismatch: the term has type'),
  # C-COMP-M7: surface/family_poly.bend line 685: Composed{alias <> List.append(generated, aliases)} became Composed{alias <> aliases}; rule: the generated companion names join the composed aliases
 ('C-COMP-M7',
  'c/surface/family_poly.c',
  '  Vec generated_aliases = names_append(heap, &frame->generated, &frame->state.aliases);',
  '  Vec generated_aliases = frame->state.aliases;',
  'composition',
  'TEMPLATE-COMPOSITION-FAIL prefix-generated-family: expected refusal: mismatch: the name X_One is already declared')]
runner.execute(ROOT, sys.argv[1], protocols, controls, suite_timeout=180, build_timeout=180)
