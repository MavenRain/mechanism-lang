"""Preserve all four original prenex mutation-control groups on the C port (tcc)."""
import importlib.util
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[1]
flags = {(): 'base', ('--families',): 'families', ('--groups',): 'groups', ('--exports',): 'exports'}
if len(sys.argv) < 2 or tuple(sys.argv[2:]) not in flags:
    raise SystemExit("usage: python3 -I dev/prenex-mutations.py NEW_WORK_DIRECTORY [--families|--groups|--exports]")
spec = importlib.util.spec_from_file_location("bend2_mutation", ROOT / "dev/bend2-mutation.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
protocols = {
    'base': ('c/tests/surface_prenex.c', 'PRENEX-OK entries=16 computations=4 negatives=25'),
    'exports': ('c/tests/surface_prenex_exports.c', 'PRENEX-EXPORTS-OK entries=12 positives=8 negatives=21 parser=10 budget=3'),
    'families': ('c/tests/surface_prenex_families.c', 'PRENEX-FAMILIES-OK families=13 entries=18 computations=5 negatives=30'),
    'groups': ('c/tests/surface_prenex_groups.c', 'PRENEX-GROUPS-OK families=13 entries=23 computations=7 negatives=39'),
}
# Each control names its Bend anchor (HEAD 7a28534) and the rule the C mutation breaks; the diagnostics are
# the original expected prefixes (decision 279 (a), hop 318). The FAM-M11 replacement and the GROUP-M5/M6/M13
# extra_edits of the Bend harness are folded into the tuples.
controls = {
 'base': [
  # C-PRENEX-M1: bend2/surface/parser.bend: every universe variable lowers to VarLevel 0 (Done{Universe.VarLevel{0n}}); rule: a universe name lowers to its own index
  ('C-PRENEX-M1',
   'c/surface/parser.c',
   '  if (index.some) return done_level(universe_var_level(heap, index.value));',
   '  if (index.some) return done_level(universe_var_level(heap, nat_zero()));',
   'base',
   'PRENEX-FAIL universe: the former lives at 0 and the expected universe is 1'),
  # C-PRENEX-M2: bend2/surface/universe.bend: an imax universe lowers to Level.max; rule: imax lowers to the impredicative max
  ('C-PRENEX-M2',
   'c/surface/universe.c',
   '      return budget_result_done((void *)level_imax(heap, left.value, right.value));',
   '      return budget_result_done((void *)level_max(heap, left.value, right.value));',
   'base',
   'PRENEX-FAIL universe: the former lives at imax(u0, u1) and the expected universe is max(u0, u1)'),
  # C-PRENEX-M3: bend2/surface/elab.bend: elab_declaration checks a DPoly body in context(globals, 0n) instead of the declared arity; rule: a polymorphic definition is checked under its universe parameters
  ('C-PRENEX-M3',
   'c/surface/elab.c',
   'elab_context(heap, globals, decl->universes)',
   'elab_context(heap, globals, nat_zero())',
   'base',
   'PRENEX-FAIL universe: universe level is outside the global parameter scope'),
  # C-PRENEX-M4: bend2/surface/elab.bend: elab_program_comp runs program_loop under Budget.unlimited() and lifts the result; rule: the program loop polls the caller's budget
  ('C-PRENEX-M4',
   'c/surface/elab.c',
   '  return bind_ctx(heap,\n                  elab_program_loop(heap, decls,\n                                    program_make(globals, poly_empty(), family_empty(), vec_new(heap->arena))),\n                  program_comp_after_loop, f);',
   '  return bind_ctx(heap,\n                  budget_lift(budget_run(heap,\n                                         elab_program_loop(heap, decls,\n                                                           program_make(globals, poly_empty(), family_empty(),\n                                                                        vec_new(heap->arena))),\n                                         budget_unlimited())),\n                  program_comp_after_loop, f);',
   'base',
   'PRENEX-FAIL budget: expected refusal'),
  # C-PRENEX-M5: bend2/surface/elab.bend: reserve_catalogs ignores the definition catalog (False{} || families); rule: a name that collides with a definition template is refused
  ('C-PRENEX-M5',
   'c/surface/elab.c',
   '  used = poly_arity(f->definitions, name).some || family_arity(f->families, name).some;',
   '  used = false || family_arity(f->families, name).some;',
   'base',
   'PRENEX-FAIL template-collision: expected refusal'),
  # C-PRENEX-M6: bend2/surface/elab.bend: specialize_kind instantiates the definition with List.reverse(levels); rule: the specialization levels are substituted in declaration order
  ('C-PRENEX-M6',
   'c/surface/elab.c',
   'poly_instantiate_comp(heap, f->globals, f->definitions, f->source, f->levels, f->alias)',
   'poly_instantiate_comp(heap, f->globals, f->definitions, f->source, heap_vec_reverse(heap, &f->levels), f->alias)',
   'base',
   'PRENEX-FAIL universe: the former lives at 1 and the expected universe is 0'),
  # C-PRENEX-M7: fixture control, unchanged
  ('C-PRENEX-M7',
   'test/fixtures/prelude/prenex.mech',
   'dataIdentity Tiny (tinySucc tinyZero)',
   'dataIdentity Tiny tinyZero',
   'base',
   'PRENEX-FAIL wrong computation: dataValue = (In SMu Tiny [] (ACtor tinyZero) [])')],
 'exports': [
  # C-PRENEX-EXPORT-M1: bend2/surface/parser.bend: export_bindings lowers the bare `export ()` unit token to None{} instead of Some{[]}; rule: an empty export list is a present, empty mapping
  ('C-PRENEX-EXPORT-M1',
   'c/surface/parser.c',
   '  if (export_word && head_is(h2, TOKEN_UNIT_TOK)) return exports_done(some_vec(empty_vec(heap)), h2.tail);',
   '  if (export_word && head_is(h2, TOKEN_UNIT_TOK)) return exports_done(none_vec(), h2.tail);',
   'exports',
   'parse/print changed the export mapping',
   {'contains': True, 'stream': 'stderr'}),
  # C-PRENEX-EXPORT-M2: bend2/surface/syntax.bend: exports_text prints bindings_text([]) for every Some; rule: the export mapping round-trips through print
  ('C-PRENEX-EXPORT-M2',
   'c/surface/syntax.c',
   '  return SURFACE_CAT(heap, str_lit(" export ("), syntax_bindings_text(heap, &exports.value), str_lit(")"));',
   '  return SURFACE_CAT(heap, str_lit(" export ("), syntax_bindings_text(heap, &(Vec){heap->arena, NULL, 0, 0}), str_lit(")"));',
   'exports',
   'parse/print changed the export mapping',
   {'contains': True, 'stream': 'stderr'}),
  # C-PRENEX-EXPORT-M3: bend2/surface/elab.bend: specialize_kind instantiates a family with exports None{}; rule: the member exports reach the family instantiation (both C routes: the instantiate split head and elab_specialize_kind)
  ('C-PRENEX-EXPORT-M3',
   'c/surface/elab.c',
   '&spec->names, specialization_exports(f->decl));',
   '&spec->names, (FamilyMaybeVec){false, {heap->arena, NULL, 0, 0}});',
   'exports',
   'specialization returned no member',
   {'contains': True, 'stream': 'stderr', 'extra_edits': [('c/surface/elab.c', 'family_instantiate_comp(heap, globals, families, source, levels, alias, reuse, exports)', 'family_instantiate_comp(heap, globals, families, source, levels, alias, reuse, (FamilyMaybeVec){false, {heap->arena, NULL, 0, 0}})')]}),
  # C-PRENEX-EXPORT-M4: bend2/surface/elab.bend: template_reservations looks the definition catalog up under name ++ ":missing"; rule: a reserved name that collides with a definition template is refused
  ('C-PRENEX-EXPORT-M4',
   'c/surface/elab.c',
   '  used = poly_arity(f->definitions, name).some || family_arity(f->catalog, name).some ||',
   '  used = poly_arity(f->definitions, str_cat(heap->arena, name, str_lit(":missing"))).some ||\n         family_arity(f->catalog, name).some ||',
   'exports',
   'definition-catalog: expected refusal',
   {'contains': True, 'stream': 'stderr'}),
  # C-PRENEX-EXPORT-M5: bend2/surface/elab.bend: specialize_kind names the family instance with reuse []; rule: the reuse list reaches instance_names (both C routes: the instantiate split close and specialize_kind_after_installed)
  ('C-PRENEX-EXPORT-M5',
   'c/surface/elab.c',
   'family_instance_names(heap, state.families, spec->template, spec->name, &spec->names,',
   'family_instance_names(heap, state.families, spec->template, spec->name, &(Vec){heap->arena, NULL, 0, 0},',
   'exports',
   'specialization returned no family',
   {'contains': True, 'stream': 'stderr', 'extra_edits': [('c/surface/elab.c', 'family_instance_names(heap, f->families, f->source, f->alias, f->reuse, f->exports)', 'family_instance_names(heap, f->families, f->source, f->alias, &(Vec){heap->arena, NULL, 0, 0}, f->exports)')]}),
  # C-PRENEX-EXPORT-M6: bend2/surface/elab.bend: specialize_kind runs the family instantiation under Budget.unlimited() and lifts the result; rule: the family instantiation polls the caller's budget
  ('C-PRENEX-EXPORT-M6',
   'c/surface/elab.c',
   '    return bind_ctx(heap, family_instantiate_comp(heap, globals, families, source, levels, alias, reuse, exports),\n                    specialize_kind_after_installed, f);',
   '    return bind_ctx(heap,\n                    budget_lift(budget_run(heap,\n                                           family_instantiate_comp(heap, globals, families, source, levels,\n                                                                   alias, reuse, exports),\n                                           budget_unlimited())),\n                    specialize_kind_after_installed, f);',
   'exports',
   'instance-budget: expected refusal',
   {'contains': True, 'stream': 'stderr'}),
  # C-PRENEX-EXPORT-M7: bend2/surface/elab.bend: definition_clauses accepts member exports on a definition template (Budget.pure); rule: member exports require a family template
  ('C-PRENEX-EXPORT-M7',
   'c/surface/elab.c',
   '  return budget_failed(error_make(ERROR_NOT_YET, str_lit("member exports require a family template")));',
   '  return pure_unit();',
   'exports',
   'definition-template: expected refusal',
   {'contains': True, 'stream': 'stderr'})],
 'families': [
  # C-PRENEX-FAM-M1: bend2/surface/elab.bend: elab_family_parts checks the family declaration in context(globals, 0n) instead of the declared arity; rule: a family template is checked under its universe parameters
  ('C-PRENEX-FAM-M1',
   'c/surface/elab.c',
   'elab_term_elab_fam_decl(heap, f->fam, elab_context(heap, f->globals, f->arity))',
   'elab_term_elab_fam_decl(heap, f->fam, elab_context(heap, f->globals, nat_zero()))',
   'families',
   'PRENEX-FAMILIES-FAIL universe: universe level is outside the global parameter scope'),
  # C-PRENEX-FAM-M2: bend2/surface/elab.bend: specialize_kind instantiates a family with List.reverse(levels); rule: the family specialization levels are substituted in declaration order (both C routes: the instantiate split head and elab_specialize_kind)
  ('C-PRENEX-FAM-M2',
   'c/surface/elab.c',
   'family_instantiate_open_head(heap, f->globals, f->families, spec->template, levels, spec->name,',
   'family_instantiate_open_head(heap, f->globals, f->families, spec->template, vec_box(heap, heap_vec_reverse(heap, levels)), spec->name,',
   'families',
   'PRENEX-FAMILIES-FAIL universe: the former lives at 1 and the expected universe is 2',
   {'extra_edits': [('c/surface/elab.c', 'family_instantiate_comp(heap, globals, families, source, levels, alias, reuse, exports)', 'family_instantiate_comp(heap, globals, families, source, vec_box(heap, heap_vec_reverse(heap, levels)), alias, reuse, exports)')]}),
  # C-PRENEX-FAM-M3: bend2/surface/elab.bend: reserve_catalogs ignores the family catalog (definitions || False{}); rule: a name that collides with a family template is refused
  ('C-PRENEX-FAM-M3',
   'c/surface/elab.c',
   '|| family_arity(f->families, name).some;',
   '|| false;',
   'families',
   'PRENEX-FAMILIES-FAIL family-template-collision: expected refusal'),
  # C-PRENEX-FAM-M4: bend2/surface/elab.bend: specialize_family_names returns Program{globals, ...} instead of the installed globals; rule: the specialized family members are installed in the program globals
  ('C-PRENEX-FAM-M4',
   'c/surface/elab.c',
   'program_make(f->installed, f->definitions, f->families,',
   'program_make(f->globals, f->definitions, f->families,',
   'families',
   'PRENEX-FAMILIES-FAIL unbound: DataBox'),
  # C-PRENEX-FAM-M5: bend2/surface/elab.bend: label_reservations looks the definition catalog up under label ++ ":missing"; rule: a member label that collides with a definition template is refused
  ('C-PRENEX-FAM-M5',
   'c/surface/elab.c',
   'poly_arity(f->definitions, label).some ||',
   'poly_arity(f->definitions, str_cat(heap->arena, label, str_lit(":missing"))).some ||',
   'families',
   'PRENEX-FAMILIES-FAIL late-constructor-collision: expected refusal'),
  # C-PRENEX-FAM-M6: bend2/surface/elab.bend: elab_declaration runs elab_family_template under Budget.unlimited() and lifts the result; rule: the family template elaboration polls the caller's budget
  ('C-PRENEX-FAM-M6',
   'c/surface/elab.c',
   '    return bind_ctx(heap, elab_elab_family_template(heap, globals, families, decl->universes, decl->family),\n                    declaration_after_families, f);',
   '    return bind_ctx(heap,\n                    budget_lift(budget_run(heap,\n                                           elab_elab_family_template(heap, globals, families, decl->universes,\n                                                                     decl->family),\n                                           budget_unlimited())),\n                    declaration_after_families, f);',
   'families',
   'PRENEX-FAMILIES-FAIL budget: expected refusal'),
  # C-PRENEX-FAM-M7: fixture control, unchanged
  ('C-PRENEX-FAM-M7',
   'test/fixtures/prelude/prenex-families.mech',
   'def dataBox : DataBox Tiny := box (tinySucc tinyZero)',
   'def dataBox : DataBox Tiny := box tinyZero',
   'families',
   'PRENEX-FAMILIES-FAIL wrong computation: dataValue = (In SMu Tiny [] (ACtor tinyZero) [])'),
  # C-PRENEX-FAM-M8: bend2/surface/elab.bend: label_reservations looks the family catalog up under label ++ ":missing"; rule: a member label that collides with a family template is refused
  ('C-PRENEX-FAM-M8',
   'c/surface/elab.c',
   'family_arity(f->catalog, label).some ||',
   'family_arity(f->catalog, str_cat(heap->arena, label, str_lit(":missing"))).some ||',
   'families',
   'PRENEX-FAMILIES-FAIL late-family-template-collision: expected refusal'),
  # C-PRENEX-FAM-M9: bend2/surface/elab.bend: specialize_kind runs the family instantiation under Budget.unlimited() and lifts the result; rule: the family specialization entry polls the caller's budget
  ('C-PRENEX-FAM-M9',
   'c/surface/elab.c',
   '    return bind_ctx(heap, family_instantiate_comp(heap, globals, families, source, levels, alias, reuse, exports),\n                    specialize_kind_after_installed, f);',
   '    return bind_ctx(heap,\n                    budget_lift(budget_run(heap,\n                                           family_instantiate_comp(heap, globals, families, source, levels,\n                                                                   alias, reuse, exports),\n                                           budget_unlimited())),\n                    specialize_kind_after_installed, f);',
   'families',
   'PRENEX-FAMILIES-FAIL specialize-entry-budget: expected refusal'),
  # C-PRENEX-FAM-M10: bend2/surface/elab.bend: ctor_distinct compares the constructor name with family ++ ":missing"; rule: a constructor named after its own family is refused
  ('C-PRENEX-FAM-M10',
   'c/surface/elab.c',
   'if (str_eq(ctor->name, family)) {',
   'if (str_eq(ctor->name, str_cat(heap->arena, family, str_lit(":missing")))) {',
   'families',
   'PRENEX-FAMILIES-FAIL self-named-constructor: expected refusal'),
  # C-PRENEX-FAM-M11: bend2/surface/elab.bend: label_reservations compares the label with root ++ ":missing" (the Bend harness also guards Poly.member against the root); rule: an instance label equal to the instance root is refused
  ('C-PRENEX-FAM-M11',
   'c/surface/elab.c',
   'used = str_eq(label, f->root) || poly_member(f->names, label) ||',
   'used = str_eq(label, str_cat(heap->arena, f->root, str_lit(":missing"))) || (poly_member(f->names, label) && !str_eq(label, f->root)) ||',
   'families',
   'PRENEX-FAMILIES-FAIL instance-own-constructor: expected refusal')],
 'groups': [
  # C-PRENEX-GROUP-M1: bend2/surface/elab.bend: elab_family_group declares the group with List.reverse(families); rule: the group families are declared in source order (both C routes: elab_family_group and the compact group_open_after_families)
  ('C-PRENEX-GROUP-M1',
   'c/surface/elab.c',
   'return family_declare_group_elaborated_comp(heap, f->globals, f->catalog, f->arity, value, callbacks);',
   'return family_declare_group_elaborated_comp(heap, f->globals, f->catalog, f->arity, vec_box(heap, heap_vec_reverse(heap, value)), callbacks);',
   'groups',
   'PRENEX-GROUPS-FAIL unbound: the family Box is not declared',
   {'extra_edits': [('c/surface/elab.c', 'return family_group_open(heap, f->globals, f->catalog, f->arity, value);', 'return family_group_open(heap, f->globals, f->catalog, f->arity, vec_box(heap, heap_vec_reverse(heap, value)));')]}),
  # C-PRENEX-GROUP-M2: bend2/surface/elab.bend: elab_family_group builds the member callbacks over List.reverse(members); rule: the group members are elaborated in source order (both C routes: elab_family_group and the compact_group_entry member loop)
  ('C-PRENEX-GROUP-M2',
   'c/surface/elab.c',
   '  Vec *callbacks = vec_box(heap, elab_member_callbacks(heap, f->members, f->arity));',
   '  Vec *callbacks = vec_box(heap, elab_member_callbacks(heap, vec_box(heap, heap_vec_reverse(heap, f->members)), f->arity));',
   'groups',
   'PRENEX-GROUPS-FAIL unbound: unbox',
   {'extra_edits': [('c/surface/elab.c', '    VecGet got = vec_get(&decl->helpers, i);\n    FamilyMemberElaborator callback;', '    VecGet got = vec_get(&decl->helpers, decl->helpers.len - 1 - i);\n    FamilyMemberElaborator callback;')]}),
  # C-PRENEX-GROUP-M3: bend2/surface/elab.bend: specialize_family_names appends member_rows without the List.reverse (the C rows are forward, so the C mutant reverses them); rule: the member rows keep their order
  ('C-PRENEX-GROUP-M3',
   'c/surface/elab.c',
   'names_append(heap, &f->rows, member_rows)',
   'names_append(heap, &f->rows, vec_box(heap, heap_vec_reverse(heap, member_rows)))',
   'groups',
   'PRENEX-GROUPS-FAIL member inventory or row order changed'),
  # C-PRENEX-GROUP-M4: bend2/surface/elab.bend: template_reservations looks the definition catalog up under name ++ ":missing"; rule: a reserved name that collides with a definition template is refused
  ('C-PRENEX-GROUP-M4',
   'c/surface/elab.c',
   '  used = poly_arity(f->definitions, name).some || family_arity(f->catalog, name).some ||',
   '  used = poly_arity(f->definitions, str_cat(heap->arena, name, str_lit(":missing"))).some ||\n         family_arity(f->catalog, name).some ||',
   'groups',
   'PRENEX-GROUPS-FAIL companion-definition-template: expected refusal'),
  # C-PRENEX-GROUP-M5: bend2/surface/elab.bend: template_reservations checks ctor_declared under name ++ ":missing" (the install_import reservation drops its constructor clause); rule: a reserved name that collides with a declared constructor is refused
  ('C-PRENEX-GROUP-M5',
   'c/surface/elab.c',
   'family_ctor_declared(f->globals, name);',
   'family_ctor_declared(f->globals, str_cat(heap->arena, name, str_lit(":missing")));',
   'groups',
   'PRENEX-GROUPS-FAIL companion-constructor-target: expected refusal',
   {'extra_edits': [('c/surface/family_poly.c', '  return bind_frame(heap, family_require_fresh(heap, family_occupied(globals, catalog, name), name),\n                    install_import_closed_k1, frame);', '  return bind_frame(heap,\n                    family_require_fresh(heap,\n                                         global_find(name, globals).some || global_find_family(name, globals).some ||\n                                             family_lookup(catalog, name).some,\n                                         name),\n                    install_import_closed_k1, frame);')]}),
  # C-PRENEX-GROUP-M6: bend2/surface/elab.bend: label_reservations checks Poly.member([], label) (the install_import reservation drops its constructor clause); rule: a generated companion label is refused
  ('C-PRENEX-GROUP-M6',
   'c/surface/elab.c',
   'poly_member(f->names, label)',
   'poly_member(&(Vec){heap->arena, NULL, 0, 0}, label)',
   'groups',
   'PRENEX-GROUPS-FAIL generated-companion-label: expected refusal',
   {'extra_edits': [('c/surface/family_poly.c', '  return bind_frame(heap, family_require_fresh(heap, family_occupied(globals, catalog, name), name),\n                    install_import_closed_k1, frame);', '  return bind_frame(heap,\n                    family_require_fresh(heap,\n                                         global_find(name, globals).some || global_find_family(name, globals).some ||\n                                             family_lookup(catalog, name).some,\n                                         name),\n                    install_import_closed_k1, frame);')]}),
  # C-PRENEX-GROUP-M7: bend2/surface/elab.bend: specialize_family_names reserves instance_labels([alias]) instead of the family names; rule: every companion label is reserved against the installed globals
  ('C-PRENEX-GROUP-M7',
   'c/surface/elab.c',
   'elab_instance_labels(heap, &f->family_names, f->installed)',
   'elab_instance_labels(heap, vec_box(heap, names_one(heap, f->alias)), f->installed)',
   'groups',
   'PRENEX-GROUPS-FAIL companion-label-definition-template: expected refusal'),
  # C-PRENEX-GROUP-M8: bend2/surface/elab.bend: elab_declaration runs elab_family_group under Budget.unlimited() and lifts the result; rule: the group declaration polls the caller's budget
  ('C-PRENEX-GROUP-M8',
   'c/surface/elab.c',
   '    return bind_ctx(heap,\n                    elab_elab_family_group(heap, globals, families, definitions, decl->universes, group_box,\n                                           &decl->helpers),\n                    declaration_after_families, f);',
   '    return bind_ctx(heap,\n                    budget_lift(budget_run(heap,\n                                           elab_elab_family_group(heap, globals, families, definitions,\n                                                                  decl->universes, group_box, &decl->helpers),\n                                           budget_unlimited())),\n                    declaration_after_families, f);',
   'groups',
   'PRENEX-GROUPS-FAIL declaration-budget: expected refusal'),
  # C-PRENEX-GROUP-M9: bend2/surface/elab.bend: specialize_kind runs the family instantiation under Budget.unlimited() and lifts the result; rule: the family instantiation polls the caller's budget
  ('C-PRENEX-GROUP-M9',
   'c/surface/elab.c',
   '    return bind_ctx(heap, family_instantiate_comp(heap, globals, families, source, levels, alias, reuse, exports),\n                    specialize_kind_after_installed, f);',
   '    return bind_ctx(heap,\n                    budget_lift(budget_run(heap,\n                                           family_instantiate_comp(heap, globals, families, source, levels,\n                                                                   alias, reuse, exports),\n                                           budget_unlimited())),\n                    specialize_kind_after_installed, f);',
   'groups',
   'PRENEX-GROUPS-FAIL instance-budget: expected refusal'),
  # C-PRENEX-GROUP-M10: fixture control, unchanged
  ('C-PRENEX-GROUP-M10',
   'test/fixtures/prelude/prenex-groups.mech',
   'Data_unpack Tiny (Data_pack Tiny (next zero))',
   'Data_unpack Tiny (Data_pack Tiny zero)',
   'groups',
   'PRENEX-GROUPS-FAIL wrong computation: dataValue = (In SMu Tiny [] (ACtor zero) [])'),
  # C-PRENEX-GROUP-M11: bend2/surface/elab.bend: template_reservations looks the definition catalog up under name ++ ":missing"; rule: a reserved name that collides with a definition template is refused
  ('C-PRENEX-GROUP-M11',
   'c/surface/elab.c',
   '  used = poly_arity(f->definitions, name).some || family_arity(f->catalog, name).some ||',
   '  used = poly_arity(f->definitions, str_cat(heap->arena, name, str_lit(":missing"))).some ||\n         family_arity(f->catalog, name).some ||',
   'groups',
   'PRENEX-GROUPS-FAIL companion-definition-template: expected refusal'),
  # C-PRENEX-GROUP-M12: bend2/surface/elab.bend: label_reservations looks the installed families up under label ++ ":missing"; rule: a member label that collides with an instance name is refused
  ('C-PRENEX-GROUP-M12',
   'c/surface/elab.c',
   'global_find_family(label, f->globals).some ||',
   'global_find_family(str_cat(heap->arena, label, str_lit(":missing")), f->globals).some ||',
   'groups',
   'PRENEX-GROUPS-FAIL late-label-instance-name: expected refusal'),
  # C-PRENEX-GROUP-M13: bend2/surface/elab.bend: template_reservations checks ctor_declared under name ++ ":missing" (the declare_comp reservation drops its constructor clause); rule: a companion constructor name that collides with a reserved name is refused
  ('C-PRENEX-GROUP-M13',
   'c/surface/elab.c',
   'family_ctor_declared(f->globals, name);',
   'family_ctor_declared(f->globals, str_cat(heap->arena, name, str_lit(":missing")));',
   'groups',
   'PRENEX-GROUPS-FAIL companion-constructor-name: expected refusal',
   {'extra_edits': [('c/surface/family_poly.c', '  return bind_frame(heap, family_require_fresh(heap, family_occupied(globals, catalog, name), name), declare_comp_k1,\n                    frame);', '  return bind_frame(heap,\n                    family_require_fresh(heap,\n                                         global_find(name, globals).some || global_find_family(name, globals).some ||\n                                             family_lookup(catalog, name).some,\n                                         name),\n                    declare_comp_k1, frame);')]})]}
mode = flags[tuple(sys.argv[2:])]
runner.execute(ROOT, sys.argv[1], {mode: protocols[mode]}, controls[mode])
