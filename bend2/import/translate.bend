import Base
import ./ndjson.bend as Json
import ./table.bend as Table
import ./export.bend as Export
import ./exprs.bend as Exprs
import ./levels.bend as Levels
import ./decls.bend as Decls
import ../kernel/bignum.bend as Big

# Source identities remain decimal strings, including values beyond UInt32.
type IdSet is Data:
  IdSet{values: Table.T<Unit>}

type Level is Data:
  LZero{}
  LSucc{child: String}
  LMax{left: String, right: String}
  LImax{left: String, right: String}
  LParam{source_name: String, name: String}

type Binder is Data:
  Binder{name: String, typ: String, body: String, info: Exprs.BinderInfo}

type Node is Data:
  Bound{index: String}
  Sort{level: String}
  Constant{source_name: String, name: String, universes: List<&2, String>}
  Apply{fn: String, arg: String}
  Lambda{binder: Binder}
  Pi{binder: Binder}
  Let{name: String, typ: String, value: String, body: String, nondep: Bool}
  Projection{source_type_name: String, type_name: String, index: String, structure: String}
  NatLiteral{value: String}
  StringLiteral{value: String}
  Metadata{expr: String, data: List<&2, Json.Field<Json.T>>}

type Parameter is Data:
  Parameter{source_name: String, name: String}

type Row is Data:
  Row{declaration: Decls.T, name: String, parameters: List<&2, Parameter>, root: String, dependencies: List<&2, String>}

type Summary is Data:
  Summary{binders: Big.T, params: IdSet, constants: IdSet}

type T is Data:
  Translated{type_nodes: Table.T<Node>, level_nodes: Table.T<Level>, summaries: Table.T<Summary>, level_params: Table.T<IdSet>, decl_rows: List<&2, Row>}

type LevelVisit is Data:
  LevelVisit{table: T, params: IdSet}

type ExprVisit is Data:
  ExprVisit{table: T, summary: Summary}

type ExprBuild is Data:
  ExprBuild{table: T, node: Node, summary: Summary}

def empty_set() -> IdSet:
  IdSet{Table.empty(Unit)}

def set_add(set: IdSet, key: String) -> IdSet:
  match set:
    case IdSet{values}:
      IdSet{Table.put(Unit, values, key, Unit{})}

def set_has(set: IdSet, key: String) -> Bool:
  match set:
    case IdSet{values}:
      Table.has(Unit, values, key)

def set_keys(set: IdSet) -> List<&2, String>:
  match set:
    case IdSet{values}:
      Table.keys(Unit, values)

def set_insert_all(keys: List<&2, String>, set: IdSet) -> IdSet:
  match keys:
    case Nil{}:
      set
    case Con{key, rest}:
      set_insert_all(rest, set_add(set, key))

def union(a: IdSet, b: IdSet) -> IdSet:
  set_insert_all(set_keys(a), b)

def empty_summary() -> Summary:
  Summary{Big.zero(), empty_set(), empty_set()}

def max_big(order: Cmp, a: Big.T, b: Big.T) -> Big.T:
  match order:
    case GT{}:
      a
    case other:
      b

def merge(a: Summary, b: Summary) -> Summary:
  match a:
    case Summary{+n, p, c}:
      match b:
        case Summary{+m, q, d}:
          Summary{max_big(Big.compare(n, m), n, m), union(p, q), union(c, d)}

def decrement(order: Cmp, number: Big.T) -> Big.T:
  match order:
    case GT{}:
      Big.sub(number, Big.one())
    case other:
      Big.zero()

def under_binder(summary: Summary) -> Summary:
  match summary:
    case Summary{+n, params, constants}:
      Summary{decrement(Big.compare(n, Big.zero()), n), params, constants}

def empty() -> T:
  Translated{Table.empty(Node), Table.empty(Level), Table.empty(Summary), Table.empty(IdSet), Nil{}}

def require(-A: Data, value: Maybe<&2, A>, line: U32, message: String) -> Result<&2, &2, Export.Error, A>:
  match value:
    case None{}:
      Fail{Export.ImportError{line, message}}
    case Some{value}:
      Done{value}

def ensure(ok: Bool, line: U32, message: String) -> Result<&2, &2, Export.Error, Unit>:
  match ok:
    case True{}:
      Done{Unit{}}
    case False{}:
      Fail{Export.ImportError{line, message}}

def named(source: Export.T, line: U32, +id: String) -> Result<&2, &2, Export.Error, String>:
  require(String, Export.name_string(source, id), line, String.append("missing source name ", id))

def canonical(source: Export.T, line: U32, +id: String) -> Result<&2, &2, Export.Error, String>:
  require(String, Export.canonical_name(source, id), line, String.append("missing source name identity ", id))

def join(items: List<&2, String>, +separator: String) -> String:
  match items:
    case Nil{}:
      ""
    case Con{one, Nil{}}:
      one
    case Con{one, Con{two, rest}}:
      String.append(one, String.append(separator, join(Con{two, rest}, separator)))

def named_all(ids: List<&2, String>, +source: Export.T, +line: U32) -> Result<&2, &2, Export.Error, List<&2, String>>:
  match ids:
    case Nil{}:
      Done{Nil{}}
    case Con{id, rest}:
      do Result<&2, &2, Export.Error, List<&2, String>>:
        name : String <- named(source, line, id)
        names : List<&2, String> <- named_all(rest, source, line)
        Done{Con{name, names}}

def type_nodes(value: T) -> Table.T<Node>:
  match value:
    case Translated{type_nodes, level_nodes, summaries, level_params, decl_rows}:
      type_nodes

def with_type_nodes(value: T, replacement: Table.T<Node>) -> T:
  match value:
    case Translated{type_nodes, level_nodes, summaries, level_params, decl_rows}:
      Translated{replacement, level_nodes, summaries, level_params, decl_rows}

def level_nodes(value: T) -> Table.T<Level>:
  match value:
    case Translated{type_nodes, level_nodes, summaries, level_params, decl_rows}:
      level_nodes

def with_level_nodes(value: T, replacement: Table.T<Level>) -> T:
  match value:
    case Translated{type_nodes, level_nodes, summaries, level_params, decl_rows}:
      Translated{type_nodes, replacement, summaries, level_params, decl_rows}

def summaries(value: T) -> Table.T<Summary>:
  match value:
    case Translated{type_nodes, level_nodes, summaries, level_params, decl_rows}:
      summaries

def with_summaries(value: T, replacement: Table.T<Summary>) -> T:
  match value:
    case Translated{type_nodes, level_nodes, summaries, level_params, decl_rows}:
      Translated{type_nodes, level_nodes, replacement, level_params, decl_rows}

def level_params(value: T) -> Table.T<IdSet>:
  match value:
    case Translated{type_nodes, level_nodes, summaries, level_params, decl_rows}:
      level_params

def with_level_params(value: T, replacement: Table.T<IdSet>) -> T:
  match value:
    case Translated{type_nodes, level_nodes, summaries, level_params, decl_rows}:
      Translated{type_nodes, level_nodes, summaries, replacement, decl_rows}

def decl_rows(value: T) -> List<&2, Row>:
  match value:
    case Translated{type_nodes, level_nodes, summaries, level_params, decl_rows}:
      decl_rows

def with_decl_rows(value: T, replacement: List<&2, Row>) -> T:
  match value:
    case Translated{type_nodes, level_nodes, summaries, level_params, decl_rows}:
      Translated{type_nodes, level_nodes, summaries, level_params, replacement}

def levelvisit_table(value: LevelVisit) -> T:
  match value:
    case LevelVisit{table, params}:
      table

def levelvisit_params(value: LevelVisit) -> IdSet:
  match value:
    case LevelVisit{table, params}:
      params

def exprvisit_table(value: ExprVisit) -> T:
  match value:
    case ExprVisit{table, summary}:
      table

def exprvisit_summary(value: ExprVisit) -> Summary:
  match value:
    case ExprVisit{table, summary}:
      summary

def exprbuild_table(value: ExprBuild) -> T:
  match value:
    case ExprBuild{table, node, summary}:
      table

def exprbuild_node(value: ExprBuild) -> Node:
  match value:
    case ExprBuild{table, node, summary}:
      node

def exprbuild_summary(value: ExprBuild) -> Summary:
  match value:
    case ExprBuild{table, node, summary}:
      summary

def summary_binders(value: Summary) -> Big.T:
  match value:
    case Summary{binders, params, constants}:
      binders

def summary_params(value: Summary) -> IdSet:
  match value:
    case Summary{binders, params, constants}:
      params

def summary_constants(value: Summary) -> IdSet:
  match value:
    case Summary{binders, params, constants}:
      constants

def commit_level(+table: T, +id: String, node: Level, +params: IdSet) -> LevelVisit:
  +table = with_level_nodes(table, Table.put(Level, level_nodes(table), id, node))
  LevelVisit{with_level_params(table, Table.put(IdSet, level_params(table), id, params)), params}

def commit_expr(result: ExprBuild, +id: String) -> ExprVisit:
  match result:
    case ExprBuild{+table, node, +summary}:
      +table = with_type_nodes(table, Table.put(Node, type_nodes(table), id, node))
      ExprVisit{with_summaries(table, Table.put(Summary, summaries(table), id, summary)), summary}

law visit_level:
  for depth: U32
  for source: Export.T
  for line: U32
  for +table: T
  for +id: String
  Result<&2, &2, Export.Error, LevelVisit>

law level_raw:
  for raw: Levels.T
  for +depth: U32
  for +source: Export.T
  for +line: U32
  for table: T
  for id: String
  Result<&2, &2, Export.Error, LevelVisit>

@unsafe def level_cached(cached: Maybe<&2, IdSet>, +depth: U32, +source: Export.T, +line: U32, +table: T, +id: String) -> Result<&2, &2, Export.Error, LevelVisit>:
  match cached:
    case Some{params}:
      Done{LevelVisit{table, params}}
    case None{}:
      do Result<&2, &2, Export.Error, LevelVisit>:
        Unit <- ensure(U32.is_le(depth, 1024), line, "type level DAG exceeds the depth limit of 1024")
        raw : Levels.T <- require(Levels.T, Export.level(source, id), line, String.append("missing source level ", id))
        level_raw(raw, depth, source, line, table, id)

@unsafe def level_pair(+depth: U32, +source: Export.T, +line: U32, table: T, id: String, left: String, right: String, node: Level) -> Result<&2, &2, Export.Error, LevelVisit>:
  do Result<&2, &2, Export.Error, LevelVisit>:
    +a : LevelVisit <- visit_level(U32.add(depth, 1), source, line, table, left)
    +b : LevelVisit <- visit_level(U32.add(depth, 1), source, line, levelvisit_table(a), right)
    Done{commit_level(levelvisit_table(b), id, node, union(levelvisit_params(a), levelvisit_params(b)))}

@unsafe def level_raw(raw, depth, source, line, table, id):
  match raw:
    case Levels.LZero{}:
      Done{commit_level(table, id, LZero{}, empty_set())}
    case Levels.LParam{+source_name}:
      do Result<&2, &2, Export.Error, LevelVisit>:
        name : String <- named(source, line, source_name)
        +identity : String <- canonical(source, line, source_name)
        Done{commit_level(table, id, LParam{identity, name}, set_add(empty_set(), identity))}
    case Levels.LSucc{+child}:
      do Result<&2, &2, Export.Error, LevelVisit>:
        +a : LevelVisit <- visit_level(U32.add(depth, 1), source, line, table, child)
        Done{commit_level(levelvisit_table(a), id, LSucc{child}, levelvisit_params(a))}
    case Levels.LMax{+left, +right}:
      level_pair(depth, source, line, table, id, left, right, LMax{left, right})
    case Levels.LImax{+left, +right}:
      level_pair(depth, source, line, table, id, left, right, LImax{left, right})

@unsafe def visit_level(depth, source, line, table, id):
  level_cached(Table.get(IdSet, level_params(table), id), depth, source, line, table, id)

@unsafe def visit_levels(ids: List<&2, String>, +source: Export.T, +line: U32, table: T, params: IdSet) -> Result<&2, &2, Export.Error, LevelVisit>:
  match ids:
    case Nil{}:
      Done{LevelVisit{table, params}}
    case Con{id, rest}:
      do Result<&2, &2, Export.Error, LevelVisit>:
        +next : LevelVisit <- visit_level(0, source, line, table, id)
        visit_levels(rest, source, line, levelvisit_table(next), union(params, levelvisit_params(next)))

law visit_expr:
  for depth: U32
  for source: Export.T
  for +table: T
  for +id: String
  Result<&2, &2, Export.Error, ExprVisit>

law expr_raw:
  for raw: Exprs.T
  for +depth: U32
  for +source: Export.T
  for +line: U32
  for table: T
  Result<&2, &2, Export.Error, ExprBuild>

def natural_summary(value: Maybe<&2, Big.T>, line: U32) -> Result<&2, &2, Export.Error, Summary>:
  match value:
    case None{}:
      Fail{Export.ImportError{line, "a type has an invalid de Bruijn index"}}
    case Some{value}:
      Done{Summary{Big.add(value, Big.one()), empty_set(), empty_set()}}

def binder_node(pi: Bool, binder: Binder) -> Node:
  match pi:
    case True{}:
      Pi{binder}
    case False{}:
      Lambda{binder}

@unsafe def expr_binder(binder: Exprs.Binder, +depth: U32, +source: Export.T, line: U32, table: T, pi: Bool) -> Result<&2, &2, Export.Error, ExprBuild>:
  match binder:
    case Exprs.Binder{name, +typ, +body, info}:
      do Result<&2, &2, Export.Error, ExprBuild>:
        name : String <- named(source, line, name)
        +a : ExprVisit <- visit_expr(U32.add(depth, 1), source, table, typ)
        +b : ExprVisit <- visit_expr(U32.add(depth, 1), source, exprvisit_table(a), body)
        Done{ExprBuild{exprvisit_table(b), binder_node(pi, Binder{name, typ, body, info}),
          merge(exprvisit_summary(a), under_binder(exprvisit_summary(b)))}}

def decl_arity(declaration: Decls.T) -> Nat:
  match declaration:
    case Decls.Decl{name, levels, typ, kind, line}:
      List.length(&2, String, levels)

def add_constant(summary: Summary, identity: String) -> Summary:
  match summary:
    case Summary{binders, params, constants}:
      Summary{binders, params, set_add(constants, identity)}

@unsafe def expr_raw(raw, depth, source, line, table):
  match raw:
    case Exprs.Bvar{+index}:
      do Result<&2, &2, Export.Error, ExprBuild>:
        Unit <- ensure(Bool.not(Json.str_eq(index, "4611686018427387903")), line, "a type has an invalid de Bruijn index")
        summary : Summary <- natural_summary(Big.of_decimal(index), line)
        Done{ExprBuild{table, Bound{index}, summary}}
    case Exprs.Sort{+index}:
      do Result<&2, &2, Export.Error, ExprBuild>:
        +a : LevelVisit <- visit_level(0, source, line, table, index)
        Done{ExprBuild{levelvisit_table(a), Sort{index}, Summary{Big.zero(), levelvisit_params(a), empty_set()}}}
    case Exprs.Const{+name, +universes}:
      do Result<&2, &2, Export.Error, ExprBuild>:
        +display : String <- named(source, line, name)
        declaration : Decls.T <- require(Decls.T, Export.declaration(source, name), line, String.append("type references undeclared constant ", display))
        +expected : Nat = decl_arity(declaration)
        +given : Nat = List.length(&2, String, universes)
        Unit <- ensure(Nat.is_eq(expected, given), line, String.append("constant ", String.append(display,
          String.append(" expects ", String.append(Nat.show(expected), String.append(" universe arguments but has ", Nat.show(given)))))))
        +a : LevelVisit <- visit_levels(universes, source, line, table, empty_set())
        identity : String <- canonical(source, line, name)
        Done{ExprBuild{levelvisit_table(a), Constant{name, display, universes},
          Summary{Big.zero(), levelvisit_params(a), set_add(empty_set(), identity)}}}
    case Exprs.Apply{+fn, +arg}:
      do Result<&2, &2, Export.Error, ExprBuild>:
        +a : ExprVisit <- visit_expr(U32.add(depth, 1), source, table, fn)
        +b : ExprVisit <- visit_expr(U32.add(depth, 1), source, exprvisit_table(a), arg)
        Done{ExprBuild{exprvisit_table(b), Apply{fn, arg}, merge(exprvisit_summary(a), exprvisit_summary(b))}}
    case Exprs.Lam{binder}:
      expr_binder(binder, depth, source, line, table, False{})
    case Exprs.Forall{binder}:
      expr_binder(binder, depth, source, line, table, True{})
    case Exprs.Let{name, +typ, +value, +body, nondep}:
      do Result<&2, &2, Export.Error, ExprBuild>:
        name : String <- named(source, line, name)
        +a : ExprVisit <- visit_expr(U32.add(depth, 1), source, table, typ)
        +b : ExprVisit <- visit_expr(U32.add(depth, 1), source, exprvisit_table(a), value)
        +c : ExprVisit <- visit_expr(U32.add(depth, 1), source, exprvisit_table(b), body)
        Done{ExprBuild{exprvisit_table(c), Let{name, typ, value, body, nondep},
          merge(merge(exprvisit_summary(a), exprvisit_summary(b)), under_binder(exprvisit_summary(c)))}}
    case Exprs.Proj{+type_name, index, +structure}:
      do Result<&2, &2, Export.Error, ExprBuild>:
        name : String <- named(source, line, type_name)
        identity : String <- canonical(source, line, type_name)
        +a : ExprVisit <- visit_expr(U32.add(depth, 1), source, table, structure)
        Done{ExprBuild{exprvisit_table(a), Projection{type_name, name, index, structure},
          add_constant(exprvisit_summary(a), identity)}}
    case Exprs.NatLiteral{text}:
      Done{ExprBuild{table, NatLiteral{text}, empty_summary()}}
    case Exprs.StringLiteral{text}:
      Done{ExprBuild{table, StringLiteral{text}, empty_summary()}}
    case Exprs.Mdata{+expr, data}:
      do Result<&2, &2, Export.Error, ExprBuild>:
        +a : ExprVisit <- visit_expr(U32.add(depth, 1), source, table, expr)
        Done{ExprBuild{exprvisit_table(a), Metadata{expr, data}, exprvisit_summary(a)}}

@unsafe def expr_cached(cached: Maybe<&2, Summary>, +depth: U32, +source: Export.T, table: T, +id: String) -> Result<&2, &2, Export.Error, ExprVisit>:
  match cached:
    case Some{summary}:
      Done{ExprVisit{table, summary}}
    case None{}:
      do Result<&2, &2, Export.Error, ExprVisit>:
        +line : U32 = Table.default(U32, Export.expr_line(source, id), 0)
        Unit <- ensure(U32.is_le(depth, 1024), line, "type expression DAG exceeds the depth limit of 1024")
        raw : Exprs.T <- require(Exprs.T, Export.expr(source, id), line, String.append("missing source expression ", id))
        build : ExprBuild <- expr_raw(raw, depth, source, line, table)
        Done{commit_expr(build, id)}

@unsafe def visit_expr(depth, source, table, id):
  expr_cached(Table.get(Summary, summaries(table), id), depth, source, table, id)

def parameters(ids: List<&2, String>, +source: Export.T, +line: U32) -> Result<&2, &2, Export.Error, List<&2, Parameter>>:
  match ids:
    case Nil{}:
      Done{Nil{}}
    case Con{+id, rest}:
      do Result<&2, &2, Export.Error, List<&2, Parameter>>:
        name : String <- named(source, line, id)
        identity : String <- canonical(source, line, id)
        rest : List<&2, Parameter> <- parameters(rest, source, line)
        Done{Con{Parameter{identity, name}, rest}}

def bound_parameters(parameters: List<&2, Parameter>, set: IdSet) -> IdSet:
  match parameters:
    case Nil{}:
      set
    case Con{Parameter{id, name}, rest}:
      bound_parameters(rest, set_add(set, id))

def not_bound(found: Bool, key: String, rest: List<&2, String>) -> List<&2, String>:
  match found:
    case True{}:
      rest
    case False{}:
      Con{key, rest}

def unbound_keys(keys: List<&2, String>, +bound: IdSet) -> List<&2, String>:
  match keys:
    case Nil{}:
      Nil{}
    case Con{+key, rest}:
      not_bound(set_has(bound, key), key, unbound_keys(rest, bound))

@unsafe def add_declaration(declaration: Decls.T, +source: Export.T, table: T) -> Result<&2, &2, Export.Error, T>:
  match declaration:
    case Decls.Decl{+id, +levels, +typ, +kind, +line}:
      do Result<&2, &2, Export.Error, T>:
        +name : String <- named(source, line, id)
        +params : List<&2, Parameter> <- parameters(levels, source, line)
        +bound : IdSet = bound_parameters(params, empty_set())
        Unit <- ensure(Nat.is_eq(List.length(&2, String, set_keys(bound)), List.length(&2, Parameter, params)), line,
          String.append("duplicate universe parameters in ", name))
        +a : ExprVisit <- visit_expr(0, source, table, typ)
        +summary : Summary = exprvisit_summary(a)
        Unit <- ensure(Big.equal(summary_binders(summary), Big.zero()), line,
          String.append("type of ", String.append(name, String.append(" has ", String.append(Big.to_string(summary_binders(summary)), " unbound term binders")))))
        +unbound : List<&2, String> <- named_all(Export.sort_keys(unbound_keys(set_keys(summary_params(summary)), bound)), source, line)
        Unit <- ensure(Nat.is_eq(List.length(&2, String, unbound), 0n), line,
          String.append("type of ", String.append(name, String.append(" has unbound universe parameters: ", join(unbound, ", ")))))
        dependencies : List<&2, String> <- named_all(Export.sort_keys(set_keys(summary_constants(summary))), source, line)
        +table : T = exprvisit_table(a)
        row : Row = Row{Decls.Decl{id, levels, typ, kind, line}, name, params, typ, dependencies}
        Done{with_decl_rows(table, Con{row, decl_rows(table)})}

@unsafe def translate_declarations(declarations: List<&2, Decls.T>, +source: Export.T, table: T) -> Result<&2, &2, Export.Error, T>:
  match declarations:
    case Nil{}:
      +table = table
      Done{with_decl_rows(table, List.reverse(&2, Row, decl_rows(table)))}
    case Con{declaration, rest}:
      do Result<&2, &2, Export.Error, T>:
        table : T <- add_declaration(declaration, source, table)
        translate_declarations(rest, source, table)

@unsafe def translate(+source: Export.T) -> Result<&2, &2, Export.Error, T>:
  translate_declarations(Export.declarations(source), source, empty())

def rows(table: T) -> List<&2, Row>:
  decl_rows(table)

def indexed_value(-A: Data, item: Maybe<&2, A>, id: String, rest: List<&2, Table.Indexed<A>>) -> List<&2, Table.Indexed<A>>:
  match item:
    case None{}:
      rest
    case Some{value}:
      Con{Table.Indexed{id, value}, rest}

def indexed(-A: Data, ids: List<&2, String>, +table: Table.T<A>) -> List<&2, Table.Indexed<A>>:
  match ids:
    case Nil{}:
      Nil{}
    case Con{+id, rest}:
      indexed_value(A, Table.get(A, table, id), id, indexed(A, rest, table))

@unsafe def nodes(table: T) -> List<&2, Table.Indexed<Node>>:
  +table = type_nodes(table)
  indexed(Node, Export.sort_keys(Table.keys(Node, table)), table)

@unsafe def levels(table: T) -> List<&2, Table.Indexed<Level>>:
  +table = level_nodes(table)
  indexed(Level, Export.sort_keys(Table.keys(Level, table)), table)
