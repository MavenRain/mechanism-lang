"""Preserve all four original prenex mutation-control groups in Bend."""
import importlib.util
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[1]
flags = {(): "base", ("--families",): "families", ("--groups",): "groups", ("--exports",): "exports"}
if len(sys.argv) < 2 or tuple(sys.argv[2:]) not in flags:
    raise SystemExit("usage: python3 -I dev/prenex-mutations.py NEW_WORK_DIRECTORY [--families|--groups|--exports]")
spec = importlib.util.spec_from_file_location("bend2_mutation", ROOT / "dev/bend2-mutation.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
protocols = {'base': ('bend2/tests/surface_prenex.bend', 'PRENEX-OK entries=16 computations=4 negatives=25'),
 'exports': ('bend2/tests/surface_prenex_exports.bend',
             'PRENEX-EXPORTS-OK entries=12 positives=8 negatives=21 parser=10 budget=3'),
 'families': ('bend2/tests/surface_prenex_families.bend',
              'PRENEX-FAMILIES-OK families=13 entries=18 computations=5 negatives=30'),
 'groups': ('bend2/tests/surface_prenex_groups.bend',
            'PRENEX-GROUPS-OK families=13 entries=23 computations=7 negatives=39')}
controls = {'base': [('C-PRENEX-M1',
           'bend2/surface/parser.bend',
           'Done{Universe.VarLevel{n}}',
           'Done{Universe.VarLevel{0n}}',
           'base',
           'PRENEX-FAIL universe: the former lives at 0 and the expected universe is 1'),
          ('C-PRENEX-M2',
           'bend2/surface/universe.bend',
           'return Level.imax(left, right)',
           'return Level.max(left, right)',
           'base',
           'PRENEX-FAIL universe: the former lives at imax(u0, u1) and the expected universe is max(u0, u1)'),
          ('C-PRENEX-M3',
           'bend2/surface/elab.bend',
           'def elab_declaration(decl: Syntax.Decl, +globals: Global.T, +definitions: Poly.T, +families: '
           'FamilyPoly.T, rows: Rows) -> Budget.Comp(Program):\n'
           '  match decl:\n'
           '    case Syntax.DPoly{+arity, name, ty, body}:\n'
           '      do Budget.Comp<Program>:\n'
           '        raw : Check.Decl <- E.elab_decl(Syntax.DDef{name, ty, body}, context(globals, arity))\n'
           '        definitions : Poly.T <- Poly.declare_comp(globals, definitions, arity, raw)\n'
           '        return Program{globals, definitions, families, rows}\n'
           '    case Syntax.DPolyMu{arity, family}:\n'
           '      do Budget.Comp<Program>:\n'
           '        families : FamilyPoly.T <- elab_family_template(globals, families, arity, family)\n'
           '        return Program{globals, definitions, families, rows}\n'
           '    case Syntax.DPolyGroup{arity, family, companions, members}:\n'
           '      do Budget.Comp<Program>:\n'
           '        families : FamilyPoly.T <- elab_family_group(globals, families, definitions, arity, '
           'family <> companions, members)\n'
           '        return Program{globals, definitions, families, rows}\n'
           '    case Syntax.DPolyCompose{arity, name, dependencies, members}:\n'
           '      do Budget.Comp<Program>:\n'
           '        families : FamilyPoly.T <- elab_composition(globals, families, definitions, arity, name, '
           'dependencies, members)\n'
           '        return Program{globals, definitions, families, rows}\n'
           '    case Syntax.DSpecialize{Syntax.Specialization{+source, levels, alias, reuse, exports}}:\n'
           '      do Budget.Comp<Program>:\n'
           '        levels : List<&2, Level.T> <- lower_levels(levels)\n'
           '        specialize_kind(Poly.is_some(Nat, FamilyPoly.arity(families, source)), globals, '
           'definitions, families, rows, source, levels, alias, reuse, exports)\n'
           '    case Syntax.DMu{group}:\n'
           '      do Budget.Comp<Program>:\n'
           '        globals : Global.T <- elab_mu_group_comp(globals, group)\n'
           '        return Program{globals, definitions, families, rows}\n'
           '    case Syntax.DRec{members}:\n'
           '      do Budget.Comp<Program>:\n'
           '        result : Checked <- elab_rec_group_comp(globals, members)\n'
           '        return append_checked(result, definitions, families, rows)\n'
           '    case other:\n'
           '      do Budget.Comp<Program>:\n'
           '        raw : Check.Decl <- E.elab_decl(other, context(globals, 0n))\n'
           '        checked : Rows <- Check.check_decls_comp(globals, [raw])\n'
           '        append_one(checked, globals, definitions, families, rows)\n',
           'def elab_declaration(decl: Syntax.Decl, +globals: Global.T, +definitions: Poly.T, +families: '
           'FamilyPoly.T, rows: Rows) -> Budget.Comp(Program):\n'
           '  match decl:\n'
           '    case Syntax.DPoly{+arity, name, ty, body}:\n'
           '      do Budget.Comp<Program>:\n'
           '        raw : Check.Decl <- E.elab_decl(Syntax.DDef{name, ty, body}, context(globals, 0n))\n'
           '        definitions : Poly.T <- Poly.declare_comp(globals, definitions, arity, raw)\n'
           '        return Program{globals, definitions, families, rows}\n'
           '    case Syntax.DPolyMu{arity, family}:\n'
           '      do Budget.Comp<Program>:\n'
           '        families : FamilyPoly.T <- elab_family_template(globals, families, arity, family)\n'
           '        return Program{globals, definitions, families, rows}\n'
           '    case Syntax.DPolyGroup{arity, family, companions, members}:\n'
           '      do Budget.Comp<Program>:\n'
           '        families : FamilyPoly.T <- elab_family_group(globals, families, definitions, arity, '
           'family <> companions, members)\n'
           '        return Program{globals, definitions, families, rows}\n'
           '    case Syntax.DPolyCompose{arity, name, dependencies, members}:\n'
           '      do Budget.Comp<Program>:\n'
           '        families : FamilyPoly.T <- elab_composition(globals, families, definitions, arity, name, '
           'dependencies, members)\n'
           '        return Program{globals, definitions, families, rows}\n'
           '    case Syntax.DSpecialize{Syntax.Specialization{+source, levels, alias, reuse, exports}}:\n'
           '      do Budget.Comp<Program>:\n'
           '        levels : List<&2, Level.T> <- lower_levels(levels)\n'
           '        specialize_kind(Poly.is_some(Nat, FamilyPoly.arity(families, source)), globals, '
           'definitions, families, rows, source, levels, alias, reuse, exports)\n'
           '    case Syntax.DMu{group}:\n'
           '      do Budget.Comp<Program>:\n'
           '        globals : Global.T <- elab_mu_group_comp(globals, group)\n'
           '        return Program{globals, definitions, families, rows}\n'
           '    case Syntax.DRec{members}:\n'
           '      do Budget.Comp<Program>:\n'
           '        result : Checked <- elab_rec_group_comp(globals, members)\n'
           '        return append_checked(result, definitions, families, rows)\n'
           '    case other:\n'
           '      do Budget.Comp<Program>:\n'
           '        raw : Check.Decl <- E.elab_decl(other, context(globals, 0n))\n'
           '        checked : Rows <- Check.check_decls_comp(globals, [raw])\n'
           '        append_one(checked, globals, definitions, families, rows)\n',
           'base',
           'PRENEX-FAIL universe: universe level is outside the global parameter scope'),
          ('C-PRENEX-M4',
           'bend2/surface/elab.bend',
           'def elab_program_comp(globals: Global.T, decls: List<&2, Syntax.Decl>) -> Budget.Comp(Checked):\n'
           '  do Budget.Comp<Checked>:\n'
           '    state : Program <- program_loop(decls, Program{globals, Poly.empty(), FamilyPoly.empty(), '
           '[]})\n'
           '    return finish_program(state)\n',
           'def elab_program_comp(globals: Global.T, decls: List<&2, Syntax.Decl>) -> Budget.Comp(Checked):\n'
           '  do Budget.Comp<Checked>:\n'
           '    state : Program <- Budget.lift(Program, Budget.run(Program, program_loop(decls, '
           'Program{globals, Poly.empty(), FamilyPoly.empty(), []}), Budget.unlimited()))\n'
           '    return finish_program(state)\n',
           'base',
           'PRENEX-FAIL budget: expected refusal'),
          ('C-PRENEX-M5',
           'bend2/surface/elab.bend',
           'def reserve_catalogs(names: List<&2, String>, +definitions: Poly.T, +families: FamilyPoly.T) -> '
           'Budget.Comp(Unit):\n'
           '  match names:\n'
           '    case Nil{}:\n'
           '      Budget.pure(Unit, Unit{})\n'
           '    case Con{+name, tail}:\n'
           '      do Budget.Comp<Unit>:\n'
           '        checked : Unit <- FamilyPoly.require_fresh(Poly.is_some(Nat, Poly.arity(definitions, '
           'name)) || Poly.is_some(Nat, FamilyPoly.arity(families, name)), name)\n'
           '        reserve_catalogs(tail, definitions, families)\n',
           'def reserve_catalogs(names: List<&2, String>, +definitions: Poly.T, +families: FamilyPoly.T) -> '
           'Budget.Comp(Unit):\n'
           '  match names:\n'
           '    case Nil{}:\n'
           '      Budget.pure(Unit, Unit{})\n'
           '    case Con{+name, tail}:\n'
           '      do Budget.Comp<Unit>:\n'
           '        checked : Unit <- FamilyPoly.require_fresh(False{} || Poly.is_some(Nat, '
           'FamilyPoly.arity(families, name)), name)\n'
           '        reserve_catalogs(tail, definitions, families)\n',
           'base',
           'PRENEX-FAIL template-collision: expected refusal'),
          ('C-PRENEX-M6',
           'bend2/surface/elab.bend',
           'Poly.instantiate_comp(globals, definitions, source, levels, alias)',
           'Poly.instantiate_comp(globals, definitions, source, List.reverse(&2, Level.T, levels), alias)',
           'base',
           'PRENEX-FAIL universe: the former lives at 1 and the expected universe is 0'),
          ('C-PRENEX-M7',
           'test/fixtures/prelude/prenex.mech',
           'dataIdentity Tiny (tinySucc tinyZero)',
           'dataIdentity Tiny tinyZero',
           'base',
           'PRENEX-FAIL wrong computation: dataValue = (In SMu Tiny [] (ACtor tinyZero) [])')],
 'exports': [('C-PRENEX-EXPORT-M1',
              'bend2/surface/parser.bend',
              'def export_bindings(tokens: List<&2, Token.T>) -> Result<&2, &2, Error.T, Parsed<Maybe<&2, '
              'List<&2, Common.Pair<String, String>>>>>:\n'
              '  match tokens:\n'
              "    case Con{Token.Tok{Token.Ident{SCon{'e', SCon{'x', SCon{'p', SCon{'o', SCon{'r', "
              "SCon{'t', SNil{}}}}}}}}, l1}, Con{Token.Tok{Token.UnitTok{}, l2}, tail}}:\n"
              '      Done{Parsed{Some{[]}, tail}}\n',
              'def export_bindings(tokens: List<&2, Token.T>) -> Result<&2, &2, Error.T, Parsed<Maybe<&2, '
              'List<&2, Common.Pair<String, String>>>>>:\n'
              '  match tokens:\n'
              "    case Con{Token.Tok{Token.Ident{SCon{'e', SCon{'x', SCon{'p', SCon{'o', SCon{'r', "
              "SCon{'t', SNil{}}}}}}}}, l1}, Con{Token.Tok{Token.UnitTok{}, l2}, tail}}:\n"
              '      Done{Parsed{None{}, tail}}\n',
              'exports',
              'parse/print changed the export mapping',
              {'contains': True, 'stream': 'stderr'}),
             ('C-PRENEX-EXPORT-M2',
              'bend2/surface/syntax.bend',
              'def exports_text(exports: Maybe<&2, List<&2, Common.Pair<String, String>>>) -> String:\n'
              '  match exports:\n'
              '    case None{}:\n'
              '      ""\n'
              '    case Some{bindings}:\n'
              '      " export (" ++ bindings_text(bindings) ++ ")"\n',
              'def exports_text(exports: Maybe<&2, List<&2, Common.Pair<String, String>>>) -> String:\n'
              '  match exports:\n'
              '    case None{}:\n'
              '      ""\n'
              '    case Some{bindings}:\n'
              '      " export (" ++ bindings_text([]) ++ ")"\n',
              'exports',
              'parse/print changed the export mapping',
              {'contains': True, 'stream': 'stderr'}),
             ('C-PRENEX-EXPORT-M3',
              'bend2/surface/elab.bend',
              'FamilyPoly.instantiate_comp(globals, families, source, levels, alias, reuse, exports)',
              'FamilyPoly.instantiate_comp(globals, families, source, levels, alias, reuse, None{})',
              'exports',
              'specialization returned no member',
              {'contains': True, 'stream': 'stderr'}),
             ('C-PRENEX-EXPORT-M4',
              'bend2/surface/elab.bend',
              'def template_reservations(names: List<&2, String>, +globals: Global.T, +definitions: Poly.T, '
              '+catalog: FamilyPoly.T) -> Budget.Comp(Unit):\n'
              '  match names:\n'
              '    case Nil{}:\n'
              '      Budget.pure(Unit, Unit{})\n'
              '    case Con{+name, tail}:\n'
              '      do Budget.Comp<Unit>:\n'
              '        checked : Unit <- FamilyPoly.require_fresh(Poly.is_some(Nat, Poly.arity(definitions, '
              'name)) || Poly.is_some(Nat, FamilyPoly.arity(catalog, name)) || '
              'FamilyPoly.ctor_declared(globals, name), name)\n'
              '        template_reservations(tail, globals, definitions, catalog)\n',
              'def template_reservations(names: List<&2, String>, +globals: Global.T, +definitions: Poly.T, '
              '+catalog: FamilyPoly.T) -> Budget.Comp(Unit):\n'
              '  match names:\n'
              '    case Nil{}:\n'
              '      Budget.pure(Unit, Unit{})\n'
              '    case Con{+name, tail}:\n'
              '      do Budget.Comp<Unit>:\n'
              '        checked : Unit <- FamilyPoly.require_fresh(Poly.is_some(Nat, Poly.arity(definitions, '
              'name ++ ":missing")) || Poly.is_some(Nat, FamilyPoly.arity(catalog, name)) || '
              'FamilyPoly.ctor_declared(globals, name), name)\n'
              '        template_reservations(tail, globals, definitions, catalog)\n',
              'exports',
              'definition-catalog: expected refusal',
              {'contains': True, 'stream': 'stderr'}),
             ('C-PRENEX-EXPORT-M5',
              'bend2/surface/elab.bend',
              'FamilyPoly.instance_names(families, source, alias, reuse, exports)',
              'FamilyPoly.instance_names(families, source, alias, [], exports)',
              'exports',
              'specialization returned no family',
              {'contains': True, 'stream': 'stderr'}),
             ('C-PRENEX-EXPORT-M6',
              'bend2/surface/elab.bend',
              'FamilyPoly.instantiate_comp(globals, families, source, levels, alias, reuse, exports)',
              'Budget.lift(Global.T, Budget.run(Global.T, FamilyPoly.instantiate_comp(globals, families, '
              'source, levels, alias, reuse, exports), Budget.unlimited()))',
              'exports',
              'instance-budget: expected refusal',
              {'contains': True, 'stream': 'stderr'}),
             ('C-PRENEX-EXPORT-M7',
              'bend2/surface/elab.bend',
              'Budget.failed(Unit, Error.Not_yet{"member exports require a family template"})',
              'Budget.pure(Unit, Unit{})',
              'exports',
              'definition-template: expected refusal',
              {'contains': True, 'stream': 'stderr'})],
 'families': [('C-PRENEX-FAM-M1',
               'bend2/surface/elab.bend',
               'def elab_family_parts(syntax: Syntax.Fam<Syntax.T>, +globals: Global.T, +arity: Nat) -> '
               'Budget.Comp(FamilyParts):\n'
               '  match syntax:\n'
               '    case Syntax.Fam{+name, +params, ty, +ctors}:\n'
               '      do Budget.Comp<FamilyParts>:\n'
               '        checked : Unit <- ctor_distinct(ctors, name)\n'
               '        +family : Check.FamilyDecl <- E.elab_fam_decl(Syntax.Fam{name, params, ty, ctors}, '
               'context(globals, arity))\n'
               '        +provisional : Global.T <- Check.declare_family_comp(globals, arity, family)\n'
               '        telescope : E.TelescopeState <- E.elab_telescope(params, context(provisional, '
               'arity), [])\n'
               '        ctors : List<&2, Check.CtorDecl> <- E.elab_ctor_decls(ctors, '
               'E.telescope_context(telescope), name)\n'
               '        return FamilyParts{family, ctors, provisional}\n',
               'def elab_family_parts(syntax: Syntax.Fam<Syntax.T>, +globals: Global.T, +arity: Nat) -> '
               'Budget.Comp(FamilyParts):\n'
               '  match syntax:\n'
               '    case Syntax.Fam{+name, +params, ty, +ctors}:\n'
               '      do Budget.Comp<FamilyParts>:\n'
               '        checked : Unit <- ctor_distinct(ctors, name)\n'
               '        +family : Check.FamilyDecl <- E.elab_fam_decl(Syntax.Fam{name, params, ty, ctors}, '
               'context(globals, 0n))\n'
               '        +provisional : Global.T <- Check.declare_family_comp(globals, arity, family)\n'
               '        telescope : E.TelescopeState <- E.elab_telescope(params, context(provisional, '
               'arity), [])\n'
               '        ctors : List<&2, Check.CtorDecl> <- E.elab_ctor_decls(ctors, '
               'E.telescope_context(telescope), name)\n'
               '        return FamilyParts{family, ctors, provisional}\n',
               'families',
               'PRENEX-FAMILIES-FAIL universe: universe level is outside the global parameter scope'),
              ('C-PRENEX-FAM-M2',
               'bend2/surface/elab.bend',
               'FamilyPoly.instantiate_comp(globals, families, source, levels, alias, reuse, exports)',
               'FamilyPoly.instantiate_comp(globals, families, source, List.reverse(&2, Level.T, levels), '
               'alias, reuse, exports)',
               'families',
               'PRENEX-FAMILIES-FAIL universe: the former lives at 1 and the expected universe is 2'),
              ('C-PRENEX-FAM-M3',
               'bend2/surface/elab.bend',
               'def reserve_catalogs(names: List<&2, String>, +definitions: Poly.T, +families: FamilyPoly.T) '
               '-> Budget.Comp(Unit):\n'
               '  match names:\n'
               '    case Nil{}:\n'
               '      Budget.pure(Unit, Unit{})\n'
               '    case Con{+name, tail}:\n'
               '      do Budget.Comp<Unit>:\n'
               '        checked : Unit <- FamilyPoly.require_fresh(Poly.is_some(Nat, Poly.arity(definitions, '
               'name)) || Poly.is_some(Nat, FamilyPoly.arity(families, name)), name)\n'
               '        reserve_catalogs(tail, definitions, families)\n',
               'def reserve_catalogs(names: List<&2, String>, +definitions: Poly.T, +families: FamilyPoly.T) '
               '-> Budget.Comp(Unit):\n'
               '  match names:\n'
               '    case Nil{}:\n'
               '      Budget.pure(Unit, Unit{})\n'
               '    case Con{+name, tail}:\n'
               '      do Budget.Comp<Unit>:\n'
               '        checked : Unit <- FamilyPoly.require_fresh(Poly.is_some(Nat, Poly.arity(definitions, '
               'name)) || False{}, name)\n'
               '        reserve_catalogs(tail, definitions, families)\n',
               'families',
               'PRENEX-FAMILIES-FAIL family-template-collision: expected refusal'),
              ('C-PRENEX-FAM-M4',
               'bend2/surface/elab.bend',
               'def specialize_family_names(names: Maybe<&2, Common.Pair<List<&2, String>, List<&2, '
               'String>>>, installed: Global.T, +globals: Global.T, +definitions: Poly.T, +families: '
               'FamilyPoly.T, rows: Rows, source: String, +alias: String) -> Budget.Comp(Program):\n'
               '  match names:\n'
               '    case None{}:\n'
               '      Budget.failed(Program, Error.Unbound{"unknown family universe schema " ++ source})\n'
               '    case Some{Common.Pair{+family_names, +members}}:\n'
               '      do Budget.Comp<Program>:\n'
               '        +installed : Global.T = installed\n'
               '        +generated : List<&2, String> = List.append(&2, String, family_names, members)\n'
               '        checked : Unit <- template_reservations(alias <> generated, globals, definitions, '
               '[])\n'
               '        labels : List<&2, String> <- instance_labels(family_names, installed)\n'
               '        checked : Unit <- label_reservations(labels, generated, alias, globals, definitions, '
               'families)\n'
               '        member_rows : Rows <- instance_members(members, installed)\n'
               '        return Program{installed, definitions, families, List.append(&2, Common.Pair<String, '
               'Global.Entry>, List.reverse(&2, Common.Pair<String, Global.Entry>, member_rows), rows)}\n',
               'def specialize_family_names(names: Maybe<&2, Common.Pair<List<&2, String>, List<&2, '
               'String>>>, installed: Global.T, +globals: Global.T, +definitions: Poly.T, +families: '
               'FamilyPoly.T, rows: Rows, source: String, +alias: String) -> Budget.Comp(Program):\n'
               '  match names:\n'
               '    case None{}:\n'
               '      Budget.failed(Program, Error.Unbound{"unknown family universe schema " ++ source})\n'
               '    case Some{Common.Pair{+family_names, +members}}:\n'
               '      do Budget.Comp<Program>:\n'
               '        +installed : Global.T = installed\n'
               '        +generated : List<&2, String> = List.append(&2, String, family_names, members)\n'
               '        checked : Unit <- template_reservations(alias <> generated, globals, definitions, '
               '[])\n'
               '        labels : List<&2, String> <- instance_labels(family_names, installed)\n'
               '        checked : Unit <- label_reservations(labels, generated, alias, globals, definitions, '
               'families)\n'
               '        member_rows : Rows <- instance_members(members, installed)\n'
               '        return Program{globals, definitions, families, List.append(&2, Common.Pair<String, '
               'Global.Entry>, List.reverse(&2, Common.Pair<String, Global.Entry>, member_rows), rows)}\n',
               'families',
               'PRENEX-FAMILIES-FAIL unbound: DataBox'),
              ('C-PRENEX-FAM-M5',
               'bend2/surface/elab.bend',
               'def label_reservations(labels: List<&2, String>, +names: List<&2, String>, +root: String, '
               '+globals: Global.T, +definitions: Poly.T, +catalog: FamilyPoly.T) -> Budget.Comp(Unit):\n'
               '  match labels:\n'
               '    case Nil{}:\n'
               '      Budget.pure(Unit, Unit{})\n'
               '    case Con{+label, tail}:\n'
               '      do Budget.Comp<Unit>:\n'
               '        checked : Unit <- FamilyPoly.require_fresh(String.eq(label, root) || '
               'Poly.member(names, label) ||\n'
               '          Poly.is_some(Nat, Poly.arity(definitions, label)) || Poly.is_some(Nat, '
               'FamilyPoly.arity(catalog, label)) ||\n'
               '          Poly.is_some(Positivity.Family, Global.find_family(label, globals)) ||\n'
               '          (Poly.is_some(Global.Entry, Global.find(label, globals)) && '
               'Bool.not(FamilyPoly.ctor_declared(globals, label))), label)\n'
               '        label_reservations(tail, names, root, globals, definitions, catalog)\n',
               'def label_reservations(labels: List<&2, String>, +names: List<&2, String>, +root: String, '
               '+globals: Global.T, +definitions: Poly.T, +catalog: FamilyPoly.T) -> Budget.Comp(Unit):\n'
               '  match labels:\n'
               '    case Nil{}:\n'
               '      Budget.pure(Unit, Unit{})\n'
               '    case Con{+label, tail}:\n'
               '      do Budget.Comp<Unit>:\n'
               '        checked : Unit <- FamilyPoly.require_fresh(String.eq(label, root) || '
               'Poly.member(names, label) ||\n'
               '          Poly.is_some(Nat, Poly.arity(definitions, label ++ ":missing")) || '
               'Poly.is_some(Nat, FamilyPoly.arity(catalog, label)) ||\n'
               '          Poly.is_some(Positivity.Family, Global.find_family(label, globals)) ||\n'
               '          (Poly.is_some(Global.Entry, Global.find(label, globals)) && '
               'Bool.not(FamilyPoly.ctor_declared(globals, label))), label)\n'
               '        label_reservations(tail, names, root, globals, definitions, catalog)\n',
               'families',
               'PRENEX-FAMILIES-FAIL late-constructor-collision: expected refusal'),
              ('C-PRENEX-FAM-M6',
               'bend2/surface/elab.bend',
               'families : FamilyPoly.T <- elab_family_template(globals, families, arity, family)',
               'families : FamilyPoly.T <- Budget.lift(FamilyPoly.T, Budget.run(FamilyPoly.T, '
               'elab_family_template(globals, families, arity, family), Budget.unlimited()))',
               'families',
               'PRENEX-FAMILIES-FAIL budget: expected refusal'),
              ('C-PRENEX-FAM-M7',
               'test/fixtures/prelude/prenex-families.mech',
               'def dataBox : DataBox Tiny := box (tinySucc tinyZero)',
               'def dataBox : DataBox Tiny := box tinyZero',
               'families',
               'PRENEX-FAMILIES-FAIL wrong computation: dataValue = (In SMu Tiny [] (ACtor tinyZero) [])'),
              ('C-PRENEX-FAM-M8',
               'bend2/surface/elab.bend',
               'def label_reservations(labels: List<&2, String>, +names: List<&2, String>, +root: String, '
               '+globals: Global.T, +definitions: Poly.T, +catalog: FamilyPoly.T) -> Budget.Comp(Unit):\n'
               '  match labels:\n'
               '    case Nil{}:\n'
               '      Budget.pure(Unit, Unit{})\n'
               '    case Con{+label, tail}:\n'
               '      do Budget.Comp<Unit>:\n'
               '        checked : Unit <- FamilyPoly.require_fresh(String.eq(label, root) || '
               'Poly.member(names, label) ||\n'
               '          Poly.is_some(Nat, Poly.arity(definitions, label)) || Poly.is_some(Nat, '
               'FamilyPoly.arity(catalog, label)) ||\n'
               '          Poly.is_some(Positivity.Family, Global.find_family(label, globals)) ||\n'
               '          (Poly.is_some(Global.Entry, Global.find(label, globals)) && '
               'Bool.not(FamilyPoly.ctor_declared(globals, label))), label)\n'
               '        label_reservations(tail, names, root, globals, definitions, catalog)\n',
               'def label_reservations(labels: List<&2, String>, +names: List<&2, String>, +root: String, '
               '+globals: Global.T, +definitions: Poly.T, +catalog: FamilyPoly.T) -> Budget.Comp(Unit):\n'
               '  match labels:\n'
               '    case Nil{}:\n'
               '      Budget.pure(Unit, Unit{})\n'
               '    case Con{+label, tail}:\n'
               '      do Budget.Comp<Unit>:\n'
               '        checked : Unit <- FamilyPoly.require_fresh(String.eq(label, root) || '
               'Poly.member(names, label) ||\n'
               '          Poly.is_some(Nat, Poly.arity(definitions, label)) || Poly.is_some(Nat, '
               'FamilyPoly.arity(catalog, label ++ ":missing")) ||\n'
               '          Poly.is_some(Positivity.Family, Global.find_family(label, globals)) ||\n'
               '          (Poly.is_some(Global.Entry, Global.find(label, globals)) && '
               'Bool.not(FamilyPoly.ctor_declared(globals, label))), label)\n'
               '        label_reservations(tail, names, root, globals, definitions, catalog)\n',
               'families',
               'PRENEX-FAMILIES-FAIL late-family-template-collision: expected refusal'),
              ('C-PRENEX-FAM-M9',
               'bend2/surface/elab.bend',
               'FamilyPoly.instantiate_comp(globals, families, source, levels, alias, reuse, exports)',
               'Budget.lift(Global.T, Budget.run(Global.T, FamilyPoly.instantiate_comp(globals, families, '
               'source, levels, alias, reuse, exports), Budget.unlimited()))',
               'families',
               'PRENEX-FAMILIES-FAIL specialize-entry-budget: expected refusal'),
              ('C-PRENEX-FAM-M10',
               'bend2/surface/elab.bend',
               'String.eq(name, family),\n          Fail{Error.Mismatch',
               'String.eq(name, family ++ ":missing"),\n          Fail{Error.Mismatch',
               'families',
               'PRENEX-FAMILIES-FAIL self-named-constructor: expected refusal'),
              ('C-PRENEX-FAM-M11',
               'bend2/surface/elab.bend',
               'def label_reservations(labels: List<&2, String>, +names: List<&2, String>, +root: String, '
               '+globals: Global.T, +definitions: Poly.T, +catalog: FamilyPoly.T) -> Budget.Comp(Unit):\n'
               '  match labels:\n'
               '    case Nil{}:\n'
               '      Budget.pure(Unit, Unit{})\n'
               '    case Con{+label, tail}:\n'
               '      do Budget.Comp<Unit>:\n'
               '        checked : Unit <- FamilyPoly.require_fresh(String.eq(label, root) || '
               'Poly.member(names, label) ||\n'
               '          Poly.is_some(Nat, Poly.arity(definitions, label)) || Poly.is_some(Nat, '
               'FamilyPoly.arity(catalog, label)) ||\n'
               '          Poly.is_some(Positivity.Family, Global.find_family(label, globals)) ||\n'
               '          (Poly.is_some(Global.Entry, Global.find(label, globals)) && '
               'Bool.not(FamilyPoly.ctor_declared(globals, label))), label)\n'
               '        label_reservations(tail, names, root, globals, definitions, catalog)\n',
               'def label_reservations(labels: List<&2, String>, +names: List<&2, String>, +root: String, '
               '+globals: Global.T, +definitions: Poly.T, +catalog: FamilyPoly.T) -> Budget.Comp(Unit):\n'
               '  match labels:\n'
               '    case Nil{}:\n'
               '      Budget.pure(Unit, Unit{})\n'
               '    case Con{+label, tail}:\n'
               '      do Budget.Comp<Unit>:\n'
               '        checked : Unit <- FamilyPoly.require_fresh(String.eq(label, root ++ ":missing") || '
               'Poly.member(names, label) ||\n'
               '          Poly.is_some(Nat, Poly.arity(definitions, label)) || Poly.is_some(Nat, '
               'FamilyPoly.arity(catalog, label)) ||\n'
               '          Poly.is_some(Positivity.Family, Global.find_family(label, globals)) ||\n'
               '          (Poly.is_some(Global.Entry, Global.find(label, globals)) && '
               'Bool.not(FamilyPoly.ctor_declared(globals, label))), label)\n'
               '        label_reservations(tail, names, root, globals, definitions, catalog)\n',
               'families',
               'PRENEX-FAMILIES-FAIL instance-own-constructor: expected refusal')],
 'groups': [('C-PRENEX-GROUP-M1',
             'bend2/surface/elab.bend',
             'FamilyPoly.declare_group_elaborated_comp(globals, catalog, arity, families, '
             'member_callbacks(members, arity))',
             'FamilyPoly.declare_group_elaborated_comp(globals, catalog, arity, List.reverse(&2, '
             'FamilyPoly.RawFamily, families), member_callbacks(members, arity))',
             'groups',
             'PRENEX-GROUPS-FAIL unbound: the family Box is not declared'),
            ('C-PRENEX-GROUP-M2',
             'bend2/surface/elab.bend',
             'FamilyPoly.declare_group_elaborated_comp(globals, catalog, arity, families, '
             'member_callbacks(members, arity))',
             'FamilyPoly.declare_group_elaborated_comp(globals, catalog, arity, families, '
             'member_callbacks(List.reverse(&2, Syntax.RecDef<Syntax.T>, members), arity))',
             'groups',
             'PRENEX-GROUPS-FAIL unbound: unbox'),
            ('C-PRENEX-GROUP-M3',
             'bend2/surface/elab.bend',
             'def specialize_family_names(names: Maybe<&2, Common.Pair<List<&2, String>, List<&2, String>>>, '
             'installed: Global.T, +globals: Global.T, +definitions: Poly.T, +families: FamilyPoly.T, rows: '
             'Rows, source: String, +alias: String) -> Budget.Comp(Program):\n'
             '  match names:\n'
             '    case None{}:\n'
             '      Budget.failed(Program, Error.Unbound{"unknown family universe schema " ++ source})\n'
             '    case Some{Common.Pair{+family_names, +members}}:\n'
             '      do Budget.Comp<Program>:\n'
             '        +installed : Global.T = installed\n'
             '        +generated : List<&2, String> = List.append(&2, String, family_names, members)\n'
             '        checked : Unit <- template_reservations(alias <> generated, globals, definitions, [])\n'
             '        labels : List<&2, String> <- instance_labels(family_names, installed)\n'
             '        checked : Unit <- label_reservations(labels, generated, alias, globals, definitions, '
             'families)\n'
             '        member_rows : Rows <- instance_members(members, installed)\n'
             '        return Program{installed, definitions, families, List.append(&2, Common.Pair<String, '
             'Global.Entry>, List.reverse(&2, Common.Pair<String, Global.Entry>, member_rows), rows)}\n',
             'def specialize_family_names(names: Maybe<&2, Common.Pair<List<&2, String>, List<&2, String>>>, '
             'installed: Global.T, +globals: Global.T, +definitions: Poly.T, +families: FamilyPoly.T, rows: '
             'Rows, source: String, +alias: String) -> Budget.Comp(Program):\n'
             '  match names:\n'
             '    case None{}:\n'
             '      Budget.failed(Program, Error.Unbound{"unknown family universe schema " ++ source})\n'
             '    case Some{Common.Pair{+family_names, +members}}:\n'
             '      do Budget.Comp<Program>:\n'
             '        +installed : Global.T = installed\n'
             '        +generated : List<&2, String> = List.append(&2, String, family_names, members)\n'
             '        checked : Unit <- template_reservations(alias <> generated, globals, definitions, [])\n'
             '        labels : List<&2, String> <- instance_labels(family_names, installed)\n'
             '        checked : Unit <- label_reservations(labels, generated, alias, globals, definitions, '
             'families)\n'
             '        member_rows : Rows <- instance_members(members, installed)\n'
             '        return Program{installed, definitions, families, List.append(&2, Common.Pair<String, '
             'Global.Entry>, member_rows, rows)}\n',
             'groups',
             'PRENEX-GROUPS-FAIL member inventory or row order changed'),
            ('C-PRENEX-GROUP-M4',
             'bend2/surface/elab.bend',
             'def template_reservations(names: List<&2, String>, +globals: Global.T, +definitions: Poly.T, '
             '+catalog: FamilyPoly.T) -> Budget.Comp(Unit):\n'
             '  match names:\n'
             '    case Nil{}:\n'
             '      Budget.pure(Unit, Unit{})\n'
             '    case Con{+name, tail}:\n'
             '      do Budget.Comp<Unit>:\n'
             '        checked : Unit <- FamilyPoly.require_fresh(Poly.is_some(Nat, Poly.arity(definitions, '
             'name)) || Poly.is_some(Nat, FamilyPoly.arity(catalog, name)) || '
             'FamilyPoly.ctor_declared(globals, name), name)\n'
             '        template_reservations(tail, globals, definitions, catalog)\n',
             'def template_reservations(names: List<&2, String>, +globals: Global.T, +definitions: Poly.T, '
             '+catalog: FamilyPoly.T) -> Budget.Comp(Unit):\n'
             '  match names:\n'
             '    case Nil{}:\n'
             '      Budget.pure(Unit, Unit{})\n'
             '    case Con{+name, tail}:\n'
             '      do Budget.Comp<Unit>:\n'
             '        checked : Unit <- FamilyPoly.require_fresh(Poly.is_some(Nat, Poly.arity(definitions, '
             'name ++ ":missing")) || Poly.is_some(Nat, FamilyPoly.arity(catalog, name)) || '
             'FamilyPoly.ctor_declared(globals, name), name)\n'
             '        template_reservations(tail, globals, definitions, catalog)\n',
             'groups',
             'PRENEX-GROUPS-FAIL companion-definition-template: expected refusal'),
            ('C-PRENEX-GROUP-M5',
             'bend2/surface/elab.bend',
             'def template_reservations(names: List<&2, String>, +globals: Global.T, +definitions: Poly.T, '
             '+catalog: FamilyPoly.T) -> Budget.Comp(Unit):\n'
             '  match names:\n'
             '    case Nil{}:\n'
             '      Budget.pure(Unit, Unit{})\n'
             '    case Con{+name, tail}:\n'
             '      do Budget.Comp<Unit>:\n'
             '        checked : Unit <- FamilyPoly.require_fresh(Poly.is_some(Nat, Poly.arity(definitions, '
             'name)) || Poly.is_some(Nat, FamilyPoly.arity(catalog, name)) || '
             'FamilyPoly.ctor_declared(globals, name), name)\n'
             '        template_reservations(tail, globals, definitions, catalog)\n',
             'def template_reservations(names: List<&2, String>, +globals: Global.T, +definitions: Poly.T, '
             '+catalog: FamilyPoly.T) -> Budget.Comp(Unit):\n'
             '  match names:\n'
             '    case Nil{}:\n'
             '      Budget.pure(Unit, Unit{})\n'
             '    case Con{+name, tail}:\n'
             '      do Budget.Comp<Unit>:\n'
             '        checked : Unit <- FamilyPoly.require_fresh(Poly.is_some(Nat, Poly.arity(definitions, '
             'name)) || Poly.is_some(Nat, FamilyPoly.arity(catalog, name)) || '
             'FamilyPoly.ctor_declared(globals, name ++ ":missing"), name)\n'
             '        template_reservations(tail, globals, definitions, catalog)\n',
             'groups',
             'PRENEX-GROUPS-FAIL companion-constructor-target: expected refusal'),
            ('C-PRENEX-GROUP-M6',
             'bend2/surface/elab.bend',
             'def label_reservations(labels: List<&2, String>, +names: List<&2, String>, +root: String, '
             '+globals: Global.T, +definitions: Poly.T, +catalog: FamilyPoly.T) -> Budget.Comp(Unit):\n'
             '  match labels:\n'
             '    case Nil{}:\n'
             '      Budget.pure(Unit, Unit{})\n'
             '    case Con{+label, tail}:\n'
             '      do Budget.Comp<Unit>:\n'
             '        checked : Unit <- FamilyPoly.require_fresh(String.eq(label, root) || '
             'Poly.member(names, label) ||\n'
             '          Poly.is_some(Nat, Poly.arity(definitions, label)) || Poly.is_some(Nat, '
             'FamilyPoly.arity(catalog, label)) ||\n'
             '          Poly.is_some(Positivity.Family, Global.find_family(label, globals)) ||\n'
             '          (Poly.is_some(Global.Entry, Global.find(label, globals)) && '
             'Bool.not(FamilyPoly.ctor_declared(globals, label))), label)\n'
             '        label_reservations(tail, names, root, globals, definitions, catalog)\n',
             'def label_reservations(labels: List<&2, String>, +names: List<&2, String>, +root: String, '
             '+globals: Global.T, +definitions: Poly.T, +catalog: FamilyPoly.T) -> Budget.Comp(Unit):\n'
             '  match labels:\n'
             '    case Nil{}:\n'
             '      Budget.pure(Unit, Unit{})\n'
             '    case Con{+label, tail}:\n'
             '      do Budget.Comp<Unit>:\n'
             '        checked : Unit <- FamilyPoly.require_fresh(String.eq(label, root) || Poly.member([], '
             'label) ||\n'
             '          Poly.is_some(Nat, Poly.arity(definitions, label)) || Poly.is_some(Nat, '
             'FamilyPoly.arity(catalog, label)) ||\n'
             '          Poly.is_some(Positivity.Family, Global.find_family(label, globals)) ||\n'
             '          (Poly.is_some(Global.Entry, Global.find(label, globals)) && '
             'Bool.not(FamilyPoly.ctor_declared(globals, label))), label)\n'
             '        label_reservations(tail, names, root, globals, definitions, catalog)\n',
             'groups',
             'PRENEX-GROUPS-FAIL generated-companion-label: expected refusal'),
            ('C-PRENEX-GROUP-M7',
             'bend2/surface/elab.bend',
             'def specialize_family_names(names: Maybe<&2, Common.Pair<List<&2, String>, List<&2, String>>>, '
             'installed: Global.T, +globals: Global.T, +definitions: Poly.T, +families: FamilyPoly.T, rows: '
             'Rows, source: String, +alias: String) -> Budget.Comp(Program):\n'
             '  match names:\n'
             '    case None{}:\n'
             '      Budget.failed(Program, Error.Unbound{"unknown family universe schema " ++ source})\n'
             '    case Some{Common.Pair{+family_names, +members}}:\n'
             '      do Budget.Comp<Program>:\n'
             '        +installed : Global.T = installed\n'
             '        +generated : List<&2, String> = List.append(&2, String, family_names, members)\n'
             '        checked : Unit <- template_reservations(alias <> generated, globals, definitions, [])\n'
             '        labels : List<&2, String> <- instance_labels(family_names, installed)\n'
             '        checked : Unit <- label_reservations(labels, generated, alias, globals, definitions, '
             'families)\n'
             '        member_rows : Rows <- instance_members(members, installed)\n'
             '        return Program{installed, definitions, families, List.append(&2, Common.Pair<String, '
             'Global.Entry>, List.reverse(&2, Common.Pair<String, Global.Entry>, member_rows), rows)}\n',
             'def specialize_family_names(names: Maybe<&2, Common.Pair<List<&2, String>, List<&2, String>>>, '
             'installed: Global.T, +globals: Global.T, +definitions: Poly.T, +families: FamilyPoly.T, rows: '
             'Rows, source: String, +alias: String) -> Budget.Comp(Program):\n'
             '  match names:\n'
             '    case None{}:\n'
             '      Budget.failed(Program, Error.Unbound{"unknown family universe schema " ++ source})\n'
             '    case Some{Common.Pair{+family_names, +members}}:\n'
             '      do Budget.Comp<Program>:\n'
             '        +installed : Global.T = installed\n'
             '        +generated : List<&2, String> = List.append(&2, String, family_names, members)\n'
             '        checked : Unit <- template_reservations(alias <> generated, globals, definitions, [])\n'
             '        labels : List<&2, String> <- instance_labels([alias], installed)\n'
             '        checked : Unit <- label_reservations(labels, generated, alias, globals, definitions, '
             'families)\n'
             '        member_rows : Rows <- instance_members(members, installed)\n'
             '        return Program{installed, definitions, families, List.append(&2, Common.Pair<String, '
             'Global.Entry>, List.reverse(&2, Common.Pair<String, Global.Entry>, member_rows), rows)}\n',
             'groups',
             'PRENEX-GROUPS-FAIL companion-label-definition-template: expected refusal'),
            ('C-PRENEX-GROUP-M8',
             'bend2/surface/elab.bend',
             'families : FamilyPoly.T <- elab_family_group(globals, families, definitions, arity, family <> '
             'companions, members)',
             'families : FamilyPoly.T <- Budget.lift(FamilyPoly.T, Budget.run(FamilyPoly.T, '
             'elab_family_group(globals, families, definitions, arity, family <> companions, members), '
             'Budget.unlimited()))',
             'groups',
             'PRENEX-GROUPS-FAIL declaration-budget: expected refusal'),
            ('C-PRENEX-GROUP-M9',
             'bend2/surface/elab.bend',
             'FamilyPoly.instantiate_comp(globals, families, source, levels, alias, reuse, exports)',
             'Budget.lift(Global.T, Budget.run(Global.T, FamilyPoly.instantiate_comp(globals, families, '
             'source, levels, alias, reuse, exports), Budget.unlimited()))',
             'groups',
             'PRENEX-GROUPS-FAIL instance-budget: expected refusal'),
            ('C-PRENEX-GROUP-M10',
             'test/fixtures/prelude/prenex-groups.mech',
             'Data_unpack Tiny (Data_pack Tiny (next zero))',
             'Data_unpack Tiny (Data_pack Tiny zero)',
             'groups',
             'PRENEX-GROUPS-FAIL wrong computation: dataValue = (In SMu Tiny [] (ACtor zero) [])'),
            ('C-PRENEX-GROUP-M11',
             'bend2/surface/elab.bend',
             'def template_reservations(names: List<&2, String>, +globals: Global.T, +definitions: Poly.T, '
             '+catalog: FamilyPoly.T) -> Budget.Comp(Unit):\n'
             '  match names:\n'
             '    case Nil{}:\n'
             '      Budget.pure(Unit, Unit{})\n'
             '    case Con{+name, tail}:\n'
             '      do Budget.Comp<Unit>:\n'
             '        checked : Unit <- FamilyPoly.require_fresh(Poly.is_some(Nat, Poly.arity(definitions, '
             'name)) || Poly.is_some(Nat, FamilyPoly.arity(catalog, name)) || '
             'FamilyPoly.ctor_declared(globals, name), name)\n'
             '        template_reservations(tail, globals, definitions, catalog)\n',
             'def template_reservations(names: List<&2, String>, +globals: Global.T, +definitions: Poly.T, '
             '+catalog: FamilyPoly.T) -> Budget.Comp(Unit):\n'
             '  match names:\n'
             '    case Nil{}:\n'
             '      Budget.pure(Unit, Unit{})\n'
             '    case Con{+name, tail}:\n'
             '      do Budget.Comp<Unit>:\n'
             '        checked : Unit <- FamilyPoly.require_fresh(Poly.is_some(Nat, Poly.arity(definitions, '
             'name ++ ":missing")) || Poly.is_some(Nat, FamilyPoly.arity(catalog, name)) || '
             'FamilyPoly.ctor_declared(globals, name), name)\n'
             '        template_reservations(tail, globals, definitions, catalog)\n',
             'groups',
             'PRENEX-GROUPS-FAIL companion-definition-template: expected refusal'),
            ('C-PRENEX-GROUP-M12',
             'bend2/surface/elab.bend',
             'def label_reservations(labels: List<&2, String>, +names: List<&2, String>, +root: String, '
             '+globals: Global.T, +definitions: Poly.T, +catalog: FamilyPoly.T) -> Budget.Comp(Unit):\n'
             '  match labels:\n'
             '    case Nil{}:\n'
             '      Budget.pure(Unit, Unit{})\n'
             '    case Con{+label, tail}:\n'
             '      do Budget.Comp<Unit>:\n'
             '        checked : Unit <- FamilyPoly.require_fresh(String.eq(label, root) || '
             'Poly.member(names, label) ||\n'
             '          Poly.is_some(Nat, Poly.arity(definitions, label)) || Poly.is_some(Nat, '
             'FamilyPoly.arity(catalog, label)) ||\n'
             '          Poly.is_some(Positivity.Family, Global.find_family(label, globals)) ||\n'
             '          (Poly.is_some(Global.Entry, Global.find(label, globals)) && '
             'Bool.not(FamilyPoly.ctor_declared(globals, label))), label)\n'
             '        label_reservations(tail, names, root, globals, definitions, catalog)\n',
             'def label_reservations(labels: List<&2, String>, +names: List<&2, String>, +root: String, '
             '+globals: Global.T, +definitions: Poly.T, +catalog: FamilyPoly.T) -> Budget.Comp(Unit):\n'
             '  match labels:\n'
             '    case Nil{}:\n'
             '      Budget.pure(Unit, Unit{})\n'
             '    case Con{+label, tail}:\n'
             '      do Budget.Comp<Unit>:\n'
             '        checked : Unit <- FamilyPoly.require_fresh(String.eq(label, root) || '
             'Poly.member(names, label) ||\n'
             '          Poly.is_some(Nat, Poly.arity(definitions, label)) || Poly.is_some(Nat, '
             'FamilyPoly.arity(catalog, label)) ||\n'
             '          Poly.is_some(Positivity.Family, Global.find_family(label ++ ":missing", globals)) '
             '||\n'
             '          (Poly.is_some(Global.Entry, Global.find(label, globals)) && '
             'Bool.not(FamilyPoly.ctor_declared(globals, label))), label)\n'
             '        label_reservations(tail, names, root, globals, definitions, catalog)\n',
             'groups',
             'PRENEX-GROUPS-FAIL late-label-instance-name: expected refusal'),
            ('C-PRENEX-GROUP-M13',
             'bend2/surface/elab.bend',
             'def template_reservations(names: List<&2, String>, +globals: Global.T, +definitions: Poly.T, '
             '+catalog: FamilyPoly.T) -> Budget.Comp(Unit):\n'
             '  match names:\n'
             '    case Nil{}:\n'
             '      Budget.pure(Unit, Unit{})\n'
             '    case Con{+name, tail}:\n'
             '      do Budget.Comp<Unit>:\n'
             '        checked : Unit <- FamilyPoly.require_fresh(Poly.is_some(Nat, Poly.arity(definitions, '
             'name)) || Poly.is_some(Nat, FamilyPoly.arity(catalog, name)) || '
             'FamilyPoly.ctor_declared(globals, name), name)\n'
             '        template_reservations(tail, globals, definitions, catalog)\n',
             'def template_reservations(names: List<&2, String>, +globals: Global.T, +definitions: Poly.T, '
             '+catalog: FamilyPoly.T) -> Budget.Comp(Unit):\n'
             '  match names:\n'
             '    case Nil{}:\n'
             '      Budget.pure(Unit, Unit{})\n'
             '    case Con{+name, tail}:\n'
             '      do Budget.Comp<Unit>:\n'
             '        checked : Unit <- FamilyPoly.require_fresh(Poly.is_some(Nat, Poly.arity(definitions, '
             'name)) || Poly.is_some(Nat, FamilyPoly.arity(catalog, name)) || '
             'FamilyPoly.ctor_declared(globals, name ++ ":missing"), name)\n'
             '        template_reservations(tail, globals, definitions, catalog)\n',
             'groups',
             'PRENEX-GROUPS-FAIL companion-constructor-name: expected refusal')]}
# Final original sources contain redundant constructor reservations. These
# controls remove the same reservation at both layers so the intended defect
# remains observable; the old single-site survivors are recorded separately.
reservation_without_constructor = (
    'Poly.is_some(Global.Entry, Global.find(family_name(family), globals)) || '
    'Poly.is_some(Positivity.Family, Global.find_family(family_name(family), globals)) || '
    'Poly.is_some(Scheme, lookup(catalog, family_name(family)))')
install_reservation = (
    '    case Imported{False{}, +family, ctors}:\n'
    '      do Budget.Comp<Global.T>:\n'
    '        checked : Unit <- require_fresh(occupied(globals, catalog, family_name(family)), family_name(family))')
declare_reservation = (
    '  do Budget.Comp<T>:\n'
    '    checked : Unit <- require_fresh(occupied(globals, catalog, family_name(family)), family_name(family))')
for group, cases in controls.items():
    for index, control in enumerate(cases):
        name, path, old, new, protocol, diagnostic, *options = control
        if name == 'C-PRENEX-FAM-M11':
            new = new.replace('Poly.member(names, label)',
                              '(Poly.member(names, label) && Bool.not(String.eq(label, root)))')
            cases[index] = (name, path, old, new, protocol, diagnostic)
        if name in {'C-PRENEX-GROUP-M5', 'C-PRENEX-GROUP-M6', 'C-PRENEX-GROUP-M13'}:
            anchor = declare_reservation if name == 'C-PRENEX-GROUP-M13' else install_reservation
            replacement = anchor.replace('occupied(globals, catalog, family_name(family))',
                                         reservation_without_constructor)
            cases[index] = (name, path, old, new, protocol, diagnostic,
                            {'extra_edits': [('bend2/surface/family_poly.bend', anchor, replacement)]})
mode = flags[tuple(sys.argv[2:])]
runner.execute(ROOT, sys.argv[1], {mode: protocols[mode]}, controls[mode])
