"""Preserve the original reuse and symbolic-reuse mutation controls on the C port (tcc)."""
import importlib.util
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) not in (2, 3) or (len(sys.argv) == 3 and sys.argv[2] != "--symbolic"):
    raise SystemExit("usage: python3 -I dev/reuse-mutations.py NEW_WORK_DIRECTORY [--symbolic]")
spec = importlib.util.spec_from_file_location("c_mutation", ROOT / "dev/c-mutation.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
# Each control names its Bend anchor (HEAD 7a28534) and the rule the C mutation breaks; the diagnostics are
# the original expected prefixes (decision 279 (a), hop 319).
ordinary = [
  # C-REUSE-M1: surface/family_poly.bend: reuse_agrees compares the actual family with itself (same_family(actual, actual)); rule: a reused family must match the template
 ('C-REUSE-M1',
  'c/surface/family_poly.c',
  '  if (level_ok && positivity_same_family(actual, expected)) return pure_globals(heap, globals);',
  '  if (level_ok && positivity_same_family(actual, actual)) return pure_globals(heap, globals);',
  'reuse',
  'TEMPLATE-REUSE-FAIL wrong-constructor: wrong refusal: missing branch: the elimination of Other has no branch at other'),
  # C-REUSE-M2: surface/family_poly.bend: erase_rec keeps self_rec instead of clearing it; rule: the mutual-group repair erases the self-recursion flag
 ('C-REUSE-M2',
  'c/surface/family_poly.c',
  '      ctor.self_rec = false;\n',
  '',
  'reuse',
  'TEMPLATE-REUSE-FAIL mutual-group-member: wrong refusal: mismatch: the reused family B does not match the template'),
  # C-REUSE-M3: surface/family_poly.bend: the universe agreement uses Term.equal on Univ instead of LevelEq.decide_m; rule: level agreement is decided by the level solver
 ('C-REUSE-M3',
  'c/surface/family_poly.c',
  'level_eq_decide_m(heap, family_kernel_level(frame->actual), family_kernel_level(frame->expected), true),',
  'budget_pure(kernel_bool_box(term_equal(term_univ(heap, family_kernel_level(frame->actual)),\n                                                           term_univ(heap, family_kernel_level(frame->expected))))),',
  'reuse',
  'TEMPLATE-REUSE-FAIL mismatch: the reused family E does not match the template'),
  # C-REUSE-M4: surface/elab.bend: an unknown source with reuse clauses fails Not_yet instead of passing to the template path; rule: reuse on an unknown name reports the unknown template
 ('C-REUSE-M4',
  'c/surface/elab.c',
  '  if (!known) return pure_unit();\n  if (reuse->len > 0) {',
  '  if (!known && reuse->len == 0) return pure_unit();\n  if (reuse->len > 0) {',
  'reuse',
  'TEMPLATE-REUSE-FAIL unknown-template-name: wrong refusal: not yet: family reuse requires a family template'),
  # C-REUSE-M5: tests/surface_template_reuse.bend: the weakened-ctors raw case is dropped; rule: the driver counts raw=6
 ('C-REUSE-M5',
  'c/tests/surface_template_reuse.c',
  '      {str_lit("a weakened certificate refuses the reuse"), case_weakened_ctors, ctx},\n',
  '',
  'reuse',
  'TEMPLATE-REUSE-OK negatives=17 parser=6 raw=5',
  {'exit_code': 0})]
symbolic = [
  # C-SREUSE-M1: surface/family_poly.bend: an imported reused family skips check_reuse (pure globals); rule: every reused family is checked
 ('C-SREUSE-M1',
  'c/surface/family_poly.c',
  '  if (imported.reused) return family_check_reuse(heap, globals, arity, imported.family, imported.ctors);',
  '  if (imported.reused) return pure_globals(heap, globals);',
  'symbolic',
  'TEMPLATE-SYMBOLIC-REUSE-FAIL independent-universes:'),
  # C-SREUSE-M2: surface/family_poly.bend: check_reuse runs with arity 0 instead of the template arity; rule: the reuse check sees the template arity
 ('C-SREUSE-M2',
  'c/surface/family_poly.c',
  '  if (imported.reused) return family_check_reuse(heap, globals, arity, imported.family, imported.ctors);',
  '  if (imported.reused) return family_check_reuse(heap, globals, nat_zero(), imported.family, imported.ctors);',
  'symbolic',
  'TEMPLATE-SYMBOLIC-REUSE-FAIL universe:'),
  # C-SREUSE-M3: surface/parser.bend: the parsed specialization drops its reuse bindings ([]); rule: the parser keeps the with-clause
 ('C-SREUSE-M3',
  'c/surface/parser.c',
  'return spec_done(sspecialization(h2.token->kind.name, levels.value, alias.value, reuse.value, exports.value),',
  'return spec_done(sspecialization(h2.token->kind.name, levels.value, alias.value, empty_vec(heap), exports.value),',
  'symbolic',
  'TEMPLATE-SYMBOLIC-REUSE-FAIL mismatch:'),
  # C-SREUSE-M4: surface/family_poly.bend: the duplicate-binding check consults an empty bound list; rule: a family bound twice is refused
 ('C-SREUSE-M4',
  'c/surface/family_poly.c',
  '  if (poly_member(bound, local))\n',
  '  if (false && poly_member(bound, local))\n',
  'symbolic',
  'TEMPLATE-SYMBOLIC-REUSE-FAIL duplicate-binding:')]
if len(sys.argv) == 3:
    protocols = {"symbolic": ('c/tests/surface_template_symbolic_reuse.c', 'TEMPLATE-SYMBOLIC-REUSE-OK negatives=12 parser=6 raw=5')}
    controls = symbolic
else:
    protocols = {"reuse": ('c/tests/surface_template_reuse.c', 'TEMPLATE-REUSE-OK negatives=17 parser=6 raw=6')}
    controls = ordinary
runner.execute(ROOT, sys.argv[1], protocols, controls, suite_timeout=900, build_timeout=1800)
