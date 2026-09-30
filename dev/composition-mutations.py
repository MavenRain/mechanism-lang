"""Preserve the seven original composition mutation controls in Bend."""
import importlib.util
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit("usage: python3 -I dev/composition-mutations.py NEW_WORK_DIRECTORY")
spec = importlib.util.spec_from_file_location("bend2_mutation", ROOT / "dev/bend2-mutation.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
protocols = {"composition": ("bend2/tests/surface_template_composition.bend", "TEMPLATE-COMPOSITION-OK negatives=24 parser=9 raw=9 type-excluded=negative-arity")}
controls = [('C-COMP-M1',
  'bend2/surface/family_poly.bend',
  'String.eq(value, family_name(family)) || Poly.member(companion_names(companions), value)',
  'Poly.member(companion_names(companions), value)',
  'composition',
  'TEMPLATE-COMPOSITION-FAIL mismatch: the name Left is already declared'),
 ('C-COMP-M2',
  'bend2/surface/family_poly.bend',
  'List.append(&1, MemberElaborator, raw_callbacks(members), supplied)',
  'List.append(&1, MemberElaborator, raw_callbacks([]), supplied)',
  'composition',
  'TEMPLATE-COMPOSITION-FAIL unbound: Left_unbox'),
 ('C-COMP-M3',
  'bend2/surface/family_poly.bend',
  'String.eq(alias, root) || Poly.member(aliases, alias) || occupied(globals, catalog, alias)',
  'String.eq(alias, root) || occupied(globals, catalog, alias)',
  'composition',
  'TEMPLATE-COMPOSITION-FAIL duplicate-disjoint-prefix: expected refusal: mismatch: the name Shared is '
  'already declared'),
 ('C-COMP-M4',
  'bend2/surface/family_poly.bend',
  'require_fresh(String.eq(Poly.decl_name(decl), root), root)',
  'require_fresh(False{}, root)',
  'composition',
  'TEMPLATE-COMPOSITION-FAIL raw-group-name: expected refusal: mismatch: the name Composed is already '
  'declared'),
 ('C-COMP-M5',
  'bend2/surface/family_poly.bend',
  'List.append(&1, MemberElaborator, raw_callbacks(members), supplied)',
  'raw_callbacks(members)',
  'composition',
  'TEMPLATE-COMPOSITION-FAIL unbound: P_transfer'),
 ('C-COMP-M6',
  'bend2/surface/family_poly.bend',
  'instance_action(scheme, source, alias, levels, reuse, exports)',
  'instance_action(scheme, source, alias, List.reverse(&2, Level.T, levels), reuse, exports)',
  'composition',
  'TEMPLATE-COMPOSITION-FAIL mismatch: the term has type'),
 ('C-COMP-M7',
  'bend2/surface/family_poly.bend',
  'Composed{alias <> List.append(&2, String, generated, aliases),',
  'Composed{alias <> aliases,',
  'composition',
  'TEMPLATE-COMPOSITION-FAIL prefix-generated-family: expected refusal: mismatch: the name X_One is already '
  'declared')]
runner.execute(ROOT, sys.argv[1], protocols, controls, suite_timeout=180, build_timeout=180)
