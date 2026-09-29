import Base
import ./ndjson.bend as Json
import ./names.bend as Names
import ./levels.bend as Levels
import ./exprs.bend as Exprs
import ./decls.bend as Decls
import ./table.bend as Table

type Error is Data:
  ImportError{line: U32, message: String}

type Metadata is Data:
  Metadata{exporter_name: String, exporter_version: String, lean_githash: String, lean_version: String, format_version: String}

type Counts is Data:
  Counts{lines: U32, names: Nat, levels: Nat, exprs: Nat, declarations: Nat}

type NameRow is Data:
  NameRow{node: Names.T, key: String, text: String, canonical: String}

type ExprRow is Data:
  ExprRow{node: Exprs.T, line: U32}

type State is Data:
  State{names: Table.T<NameRow>, interned: Table.T<String>, levels: Table.T<Levels.T>, exprs: Table.T<ExprRow>, decls: Table.T<Decls.T>, expr_order: List<&2, Table.Indexed<Exprs.T>>, decl_order: List<&2, Decls.T>, groups: List<&2, Decls.Group>}

type T is Data:
  Export{meta: Metadata, totals: Counts, state: State}

def state_names(state: State) -> Table.T<NameRow>:
  match state:
    case State{names, interned, levels, exprs, decls, expr_order, decl_order, groups}:
      names

def with_names(state: State, replacement: Table.T<NameRow>) -> State:
  match state:
    case State{names, interned, levels, exprs, decls, expr_order, decl_order, groups}:
      State{replacement, interned, levels, exprs, decls, expr_order, decl_order, groups}

def state_interned(state: State) -> Table.T<String>:
  match state:
    case State{names, interned, levels, exprs, decls, expr_order, decl_order, groups}:
      interned

def with_interned(state: State, replacement: Table.T<String>) -> State:
  match state:
    case State{names, interned, levels, exprs, decls, expr_order, decl_order, groups}:
      State{names, replacement, levels, exprs, decls, expr_order, decl_order, groups}

def state_levels(state: State) -> Table.T<Levels.T>:
  match state:
    case State{names, interned, levels, exprs, decls, expr_order, decl_order, groups}:
      levels

def with_levels(state: State, replacement: Table.T<Levels.T>) -> State:
  match state:
    case State{names, interned, levels, exprs, decls, expr_order, decl_order, groups}:
      State{names, interned, replacement, exprs, decls, expr_order, decl_order, groups}

def state_exprs(state: State) -> Table.T<ExprRow>:
  match state:
    case State{names, interned, levels, exprs, decls, expr_order, decl_order, groups}:
      exprs

def with_exprs(state: State, replacement: Table.T<ExprRow>) -> State:
  match state:
    case State{names, interned, levels, exprs, decls, expr_order, decl_order, groups}:
      State{names, interned, levels, replacement, decls, expr_order, decl_order, groups}

def state_decls(state: State) -> Table.T<Decls.T>:
  match state:
    case State{names, interned, levels, exprs, decls, expr_order, decl_order, groups}:
      decls

def with_decls(state: State, replacement: Table.T<Decls.T>) -> State:
  match state:
    case State{names, interned, levels, exprs, decls, expr_order, decl_order, groups}:
      State{names, interned, levels, exprs, replacement, expr_order, decl_order, groups}

def state_expr_order(state: State) -> List<&2, Table.Indexed<Exprs.T>>:
  match state:
    case State{names, interned, levels, exprs, decls, expr_order, decl_order, groups}:
      expr_order

def with_expr_order(state: State, replacement: List<&2, Table.Indexed<Exprs.T>>) -> State:
  match state:
    case State{names, interned, levels, exprs, decls, expr_order, decl_order, groups}:
      State{names, interned, levels, exprs, decls, replacement, decl_order, groups}

def state_decl_order(state: State) -> List<&2, Decls.T>:
  match state:
    case State{names, interned, levels, exprs, decls, expr_order, decl_order, groups}:
      decl_order

def with_decl_order(state: State, replacement: List<&2, Decls.T>) -> State:
  match state:
    case State{names, interned, levels, exprs, decls, expr_order, decl_order, groups}:
      State{names, interned, levels, exprs, decls, expr_order, replacement, groups}

def state_groups(state: State) -> List<&2, Decls.Group>:
  match state:
    case State{names, interned, levels, exprs, decls, expr_order, decl_order, groups}:
      groups

def with_groups(state: State, replacement: List<&2, Decls.Group>) -> State:
  match state:
    case State{names, interned, levels, exprs, decls, expr_order, decl_order, groups}:
      State{names, interned, levels, exprs, decls, expr_order, decl_order, replacement}

def initial() -> State:
  State{
    Table.put(NameRow, Table.empty(NameRow), "0", NameRow{Names.Anonymous{}, "", "", "0"}),
    Table.put(String, Table.empty(String), "", "0"),
    Table.put(Levels.T, Table.empty(Levels.T), "0", Levels.LZero{}),
    Table.empty(ExprRow), Table.empty(Decls.T), Nil{}, Nil{}, Nil{}
  }

def at_line(-A: Data, result: Result<&2, &2, String, A>, line: U32) -> Result<&2, &2, Error, A>:
  match result:
    case Done{value}:
      Done{value}
    case Fail{message}:
      Fail{ImportError{line, message}}

def metadata_version(valid: Bool, version: String, exporter: String, exporter_version: String, lean_githash: String, lean_version: String) -> Result<&2, &2, String, Metadata>:
  match valid:
    case True{}:
      Done{Metadata{exporter, exporter_version, lean_githash, lean_version, version}}
    case False{}:
      Fail{"unsupported export format " ++ version ++ ", expected 3.1.0"}

def parse_metadata(value: Json.T) -> Result<&2, &2, String, Metadata>:
  do Result<&2, &2, String, Metadata>:
    outer : List<&2, Json.Field<Json.T>> <- Json.exact_object(value, ["meta"])
    meta : Json.T <- Json.field(outer, "meta")
    +fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(meta, ["exporter", "lean", "format"])
    exporter : Json.T <- Json.field(fields, "exporter")
    +exporter_fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(exporter, ["name", "version"])
    exporter_name : String <- Json.string_field(exporter_fields, "name")
    exporter_version : String <- Json.string_field(exporter_fields, "version")
    lean : Json.T <- Json.field(fields, "lean")
    +lean_fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(lean, ["githash", "version"])
    lean_githash : String <- Json.string_field(lean_fields, "githash")
    lean_version : String <- Json.string_field(lean_fields, "version")
    format : Json.T <- Json.field(fields, "format")
    format_fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(format, ["version"])
    +version : String <- Json.string_field(format_fields, "version")
    metadata_version(Json.str_eq(version, "3.1.0"), version, exporter_name,
      exporter_version, lean_githash, lean_version)

def byte_length(text: String) -> Nat:
  match text:
    case SNil{}:
      0n
    case SCon{ch, rest}:
      Nat.add(U32.to_nat(Json.byte_width(ch)), byte_length(rest))

def ordinary_char(ch: Char) -> Bool:
  match ch:
    case Chr{+code}:
      Bool.or(Bool.or(Bool.or(
        Bool.and(U32.is_ge(code, 97), U32.is_le(code, 122)),
        Bool.and(U32.is_ge(code, 65), U32.is_le(code, 90))),
        Bool.and(U32.is_ge(code, 48), U32.is_le(code, 57))),
        Bool.or(U32.is_eq(code, 95), U32.is_eq(code, 39)))

def ordinary_text(text: String) -> Bool:
  match text:
    case SNil{}:
      True{}
    case SCon{ch, rest}:
      Bool.and(ordinary_char(ch), ordinary_text(rest))

def display_pick(ordinary: Bool, text: String) -> String:
  match ordinary:
    case True{}:
      text
    case False{}:
      "«" ++ Json.quoted(text) ++ "»"

def display_component(text: String) -> String:
  match text:
    case SNil{}:
      "«\"\"»"
    case SCon{+ch, +rest}:
      display_pick(Bool.and(ordinary_text(SCon{ch, rest}),
        Bool.not(Json.decimal_digits(SCon{ch, rest}))), SCon{ch, rest})

def display_append(prefix: String, component: String) -> String:
  match prefix:
    case SNil{}:
      component
    case SCon{ch, rest}:
      SCon{ch, rest} ++ "." ++ component

type NameParts is Data:
  NameParts{key: String, text: String}

def str_parts(prefix: NameRow, +component: String) -> NameParts:
  match prefix:
    case NameRow{node, key, text, canonical}:
      NameParts{key ++ "s" ++ Nat.show(byte_length(component)) ++ ":" ++ component,
        display_append(text, display_component(component))}

def num_parts(prefix: NameRow, +number: String) -> NameParts:
  match prefix:
    case NameRow{node, key, text, canonical}:
      NameParts{key ++ "n" ++ number ++ ":", display_append(text, number)}

def name_parts(node: Names.T, names: Table.T<NameRow>) -> Result<&2, &2, String, NameParts>:
  match node:
    case Names.Anonymous{}:
      Done{NameParts{"", ""}}
    case Names.Str{pre, component}:
      do Result<&2, &2, String, NameParts>:
        prefix : NameRow <- Table.lookup(NameRow, names, "name", pre)
        return str_parts(prefix, component)
    case Names.Num{pre, number}:
      do Result<&2, &2, String, NameParts>:
        prefix : NameRow <- Table.lookup(NameRow, names, "name", pre)
        return num_parts(prefix, number)

def name_install(parts: NameParts, +state: State, +index: String, node: Names.T) -> State:
  match parts:
    case NameParts{+key, text}:
      +canonical = Table.default(String, Table.get(String, state_interned(state), key), index)
      updated = with_interned(state, Table.put(String, state_interned(state), key, canonical))
      with_names(updated, Table.put(NameRow, state_names(state), index, NameRow{node, key, text, canonical}))

def name_unique(duplicate: Bool, +state: State, +index: String, tag: String, value: Json.T) -> Result<&2, &2, String, State>:
  match duplicate:
    case True{}:
      Fail{"duplicate name index " ++ index}
    case False{}:
      do Result<&2, &2, String, State>:
        +node : Names.T <- Names.parse(tag, value)
        checked : Unit <- Table.references(NameRow, Names.references(node), state_names(state), "name")
        parts : NameParts <- name_parts(node, state_names(state))
        return name_install(parts, state, index, node)

def add_name(+state: State, +index: String, tag: String, value: Json.T) -> Result<&2, &2, String, State>:
  name_unique(Table.has(NameRow, state_names(state), index), state, index, tag, value)

def level_unique(duplicate: Bool, +state: State, index: String, tag: String, value: Json.T) -> Result<&2, &2, String, State>:
  match duplicate:
    case True{}:
      Fail{"duplicate level index " ++ index}
    case False{}:
      do Result<&2, &2, String, State>:
        +node : Levels.T <- Levels.parse(tag, value)
        names_checked : Unit <- Table.references(NameRow, Levels.name_references(node), state_names(state), "name")
        levels_checked : Unit <- Table.references(Levels.T, Levels.level_references(node), state_levels(state), "level")
        return with_levels(state, Table.put(Levels.T, state_levels(state), index, node))

def add_level(+state: State, +index: String, tag: String, value: Json.T) -> Result<&2, &2, String, State>:
  level_unique(Table.has(Levels.T, state_levels(state), index), state, index, tag, value)

def expr_unique(duplicate: Bool, +state: State, +index: String, tag: String, value: Json.T, line: U32) -> Result<&2, &2, String, State>:
  match duplicate:
    case True{}:
      Fail{"duplicate expression index " ++ index}
    case False{}:
      do Result<&2, &2, String, State>:
        +node : Exprs.T <- Exprs.parse(tag, value)
        names_checked : Unit <- Table.references(NameRow, Exprs.name_references(node), state_names(state), "name")
        levels_checked : Unit <- Table.references(Levels.T, Exprs.level_references(node), state_levels(state), "level")
        exprs_checked : Unit <- Table.references(ExprRow, Exprs.expr_references(node), state_exprs(state), "expression")
        updated : State = with_exprs(state, Table.put(ExprRow, state_exprs(state), index, ExprRow{node, line}))
        return with_expr_order(updated, Con{Table.Indexed{index, node}, state_expr_order(state)})

def add_expr(+state: State, +index: String, tag: String, value: Json.T, line: U32) -> Result<&2, &2, String, State>:
  expr_unique(Table.has(ExprRow, state_exprs(state), index), state, index, tag, value, line)

def decl_name(declaration: Decls.T) -> String:
  match declaration:
    case Decls.Decl{name, levels, typ, kind, line}:
      name

def declaration_unique(duplicate: Bool, +state: State, key: String, text: String, +declaration: Decls.T) -> Result<&2, &2, String, State>:
  match duplicate:
    case True{}:
      Fail{"duplicate declaration " ++ text}
    case False{}:
      updated = with_decls(state, Table.put(Decls.T, state_decls(state), key, declaration))
      Done{with_decl_order(updated, Con{declaration, state_decl_order(state)})}

def declaration_named(name: NameRow, +state: State, declaration: Decls.T) -> Result<&2, &2, String, State>:
  match name:
    case NameRow{node, +key, text, canonical}:
      declaration_unique(Table.has(Decls.T, state_decls(state), key), state, key, text, declaration)

def add_declaration(+state: State, +declaration: Decls.T) -> Result<&2, &2, String, State>:
  do Result<&2, &2, String, State>:
    names_checked : Unit <- Table.references(NameRow, Decls.name_references(declaration), state_names(state), "name")
    exprs_checked : Unit <- Table.references(ExprRow, Decls.expr_references(declaration), state_exprs(state), "expression")
    name : NameRow <- Table.lookup(NameRow, state_names(state), "declaration name", decl_name(declaration))
    declaration_named(name, state, declaration)

def add_rows(rows: List<&2, Decls.T>, state: State) -> Result<&2, &2, String, State>:
  match rows:
    case Nil{}:
      Done{state}
    case Con{declaration, rest}:
      do Result<&2, &2, String, State>:
        next : State <- add_declaration(state, declaration)
        add_rows(rest, next)

def add_group(group: Maybe<&2, Decls.Group>, +state: State) -> State:
  match group:
    case None{}:
      state
    case Some{value}:
      with_groups(state, Con{value, state_groups(state)})

def add_parsed(parsed: Decls.Parsed, state: State) -> Result<&2, &2, String, State>:
  match parsed:
    case Decls.Parsed{rows, group}:
      do Result<&2, &2, String, State>:
        next : State <- add_rows(rows, state)
        return add_group(group, next)

def add_declarations(state: State, tag: String, value: Json.T, line: U32) -> Result<&2, &2, String, State>:
  do Result<&2, &2, String, State>:
    parsed : Decls.Parsed <- Decls.parse(tag, value, line)
    add_parsed(parsed, state)

def indexed_kind(key: String, index: String, tag: String, value: Json.T, state: State, line: U32) -> Result<&2, &2, String, State>:
  match key:
    case "in":
      add_name(state, index, tag, value)
    case "il":
      add_level(state, index, tag, value)
    case "ie":
      add_expr(state, index, tag, value, line)
    case other:
      Fail{"expected name, level, or expression index"}

def indexed(first: Json.Field<Json.T>, second: Json.Field<Json.T>, state: State, line: U32) -> Result<&2, &2, String, State>:
  match first:
    case Json.Field{key, raw_index}:
      match second:
        case Json.Field{tag, value}:
          do Result<&2, &2, String, State>:
            index : String <- Json.nat(raw_index)
            indexed_kind(key, index, tag, value, state, line)

def single(tag: String, value: Json.T, state: State, line: U32) -> Result<&2, &2, String, State>:
  match tag:
    case "meta":
      Fail{"duplicate metadata"}
    case other:
      add_declarations(state, other, value, line)

def index_order(key: String, first: Json.Field<Json.T>, second: Json.Field<Json.T>, state: State, line: U32) -> Result<&2, &2, String, State>:
  match key:
    case "in":
      indexed(first, second, state, line)
    case "il":
      indexed(first, second, state, line)
    case "ie":
      indexed(first, second, state, line)
    case other:
      indexed(second, first, state, line)

def item_fields(fields: List<&2, Json.Field<Json.T>>, state: State, line: U32) -> Result<&2, &2, String, State>:
  match fields:
    case Con{Json.Field{tag, value}, Nil{}}:
      single(tag, value, state, line)
    case Con{Json.Field{+key, raw_index}, Con{second, Nil{}}}:
      index_order(key, Json.Field{key, raw_index}, second, state, line)
    case other:
      Fail{"invalid export object shape"}

def item(value: Json.T, state: State, line: U32) -> Result<&2, &2, String, State>:
  do Result<&2, &2, String, State>:
    fields : List<&2, Json.Field<Json.T>> <- Json.object_fields(value)
    item_fields(fields, state, line)

def finish(+state: State, meta: Metadata, lines: U32) -> T:
  Export{meta, Counts{lines,
    Nat.sub(Table.size(NameRow, state_names(state)), 1n),
    Nat.sub(Table.size(Levels.T, state_levels(state)), 1n),
    Table.size(ExprRow, state_exprs(state)), Table.size(Decls.T, state_decls(state))}, state}

def read_remaining(lines: List<&2, String>, +meta: Metadata, state: State, +previous: U32) -> Result<&2, &2, Error, T>:
  match lines:
    case Nil{}:
      Done{finish(state, meta, previous)}
    case Con{text, rest}:
      do Result<&2, &2, Error, T>:
        +line : U32 = (previous + 1 : U32)
        value : Json.T <- at_line(Json.T, Json.parse(text), line)
        next : State <- at_line(State, item(value, state, line), line)
        read_remaining(rest, meta, next, line)

def read_lines(lines: List<&2, String>) -> Result<&2, &2, Error, T>:
  match lines:
    case Nil{}:
      Fail{ImportError{1, "missing initial metadata"}}
    case Con{first, rest}:
      do Result<&2, &2, Error, T>:
        value : Json.T <- at_line(Json.T, Json.parse(first), 1)
        meta : Metadata <- at_line(Metadata, parse_metadata(value), 1)
        read_remaining(rest, meta, initial(), 1)

def split_lines(source: String, reversed: String, lines: List<&2, String>) -> List<&2, String>:
  match source:
    case SNil{}:
      Con{Json.reverse(reversed, ""), lines}
    case SCon{'\n', rest}:
      split_lines(rest, "", Con{Json.reverse(reversed, ""), lines})
    case SCon{ch, rest}:
      split_lines(rest, SCon{ch, reversed}, lines)

def trailing_line(lines: List<&2, String>) -> List<&2, String>:
  match lines:
    case Con{SNil{}, rest}:
      List.reverse(&2, String, rest)
    case other:
      List.reverse(&2, String, other)

def read_string(source: String) -> Result<&2, &2, Error, T>:
  read_lines(trailing_line(split_lines(source, "", Nil{})))

def metadata(source: T) -> Metadata:
  match source:
    case Export{meta, totals, state}:
      meta

def counts(source: T) -> Counts:
  match source:
    case Export{meta, totals, state}:
      totals

def source_state(source: T) -> State:
  match source:
    case Export{meta, totals, state}:
      state

def name_node(row: Maybe<&2, NameRow>) -> Maybe<&2, Names.T>:
  match row:
    case None{}:
      None{}
    case Some{NameRow{node, key, text, canonical}}:
      Some{node}

def name(source: T, index: String) -> Maybe<&2, Names.T>:
  name_node(Table.get(NameRow, state_names(source_state(source)), index))

def name_display(row: Maybe<&2, NameRow>) -> Maybe<&2, String>:
  match row:
    case None{}:
      None{}
    case Some{NameRow{node, key, text, canonical}}:
      Some{text}

def name_string(source: T, index: String) -> Maybe<&2, String>:
  name_display(Table.get(NameRow, state_names(source_state(source)), index))

def name_canonical(row: Maybe<&2, NameRow>) -> Maybe<&2, String>:
  match row:
    case None{}:
      None{}
    case Some{NameRow{node, key, text, canonical}}:
      Some{canonical}

def canonical_name(source: T, index: String) -> Maybe<&2, String>:
  name_canonical(Table.get(NameRow, state_names(source_state(source)), index))

def level(source: T, index: String) -> Maybe<&2, Levels.T>:
  Table.get(Levels.T, state_levels(source_state(source)), index)

def expr_node(row: Maybe<&2, ExprRow>) -> Maybe<&2, Exprs.T>:
  match row:
    case None{}:
      None{}
    case Some{ExprRow{node, line}}:
      Some{node}

def expr(source: T, index: String) -> Maybe<&2, Exprs.T>:
  expr_node(Table.get(ExprRow, state_exprs(source_state(source)), index))

def expr_source_line(row: Maybe<&2, ExprRow>) -> Maybe<&2, U32>:
  match row:
    case None{}:
      None{}
    case Some{ExprRow{node, line}}:
      Some{line}

def expr_line(source: T, index: String) -> Maybe<&2, U32>:
  expr_source_line(Table.get(ExprRow, state_exprs(source_state(source)), index))

def expressions(source: T) -> List<&2, Table.Indexed<Exprs.T>>:
  List.reverse(&2, Table.Indexed<Exprs.T>, state_expr_order(source_state(source)))

def declaration_named_lookup(row: Maybe<&2, NameRow>, decls: Table.T<Decls.T>) -> Maybe<&2, Decls.T>:
  match row:
    case None{}:
      None{}
    case Some{NameRow{node, key, text, canonical}}:
      Table.get(Decls.T, decls, key)

def declaration(+source: T, index: String) -> Maybe<&2, Decls.T>:
  declaration_named_lookup(Table.get(NameRow, state_names(source_state(source)), index),
    state_decls(source_state(source)))

def declarations(source: T) -> List<&2, Decls.T>:
  List.reverse(&2, Decls.T, state_decl_order(source_state(source)))

def groups(source: T) -> List<&2, Decls.Group>:
  List.reverse(&2, Decls.Group, state_groups(source_state(source)))

def index_order_length(order: Cmp, a: String, b: String) -> Cmp:
  match order:
    case EQ{}:
      Json.lexical(a, b)
    case other:
      other

def compare_index(+a: String, +b: String) -> Cmp:
  index_order_length(Nat.cmp(Json.string_length(a), Json.string_length(b)), a, b)

type KeySplit is Data:
  KeySplit{left: List<&2, String>, right: List<&2, String>}

def split_prepend(a: String, b: String, rest: KeySplit) -> KeySplit:
  match rest:
    case KeySplit{left, right}:
      KeySplit{Con{a, left}, Con{b, right}}

def split_keys(keys: List<&2, String>) -> KeySplit:
  match keys:
    case Nil{}:
      KeySplit{Nil{}, Nil{}}
    case Con{one, Nil{}}:
      KeySplit{[one], Nil{}}
    case Con{one, Con{two, rest}}:
      split_prepend(one, two, split_keys(rest))

law merge_keys:
  for left: List<&2, String>
  for right: List<&2, String>
  List<&2, String>

@unsafe def merge_choice(order: Cmp, left: String, left_rest: List<&2, String>, right: String, right_rest: List<&2, String>) -> List<&2, String>:
  match order:
    case GT{}:
      Con{right, merge_keys(Con{left, left_rest}, right_rest)}
    case other:
      Con{left, merge_keys(left_rest, Con{right, right_rest})}

@unsafe def merge_keys(left, right):
  match left:
    case Nil{}:
      right
    case Con{+a, xs}:
      match right:
        case Nil{}:
          Con{a, xs}
        case Con{+b, ys}:
          merge_choice(compare_index(a, b), a, xs, b, ys)

law sort_keys:
  for keys: List<&2, String>
  List<&2, String>

@unsafe def sort_split(split: KeySplit) -> List<&2, String>:
  match split:
    case KeySplit{left, right}:
      merge_keys(sort_keys(left), sort_keys(right))

@unsafe def sort_keys(keys):
  match keys:
    case Nil{}:
      Nil{}
    case Con{one, Nil{}}:
      [one]
    case Con{one, Con{two, rest}}:
      sort_split(split_keys(Con{one, Con{two, rest}}))

def name_indexed(row: Maybe<&2, NameRow>, index: String, rest: List<&2, Table.Indexed<Names.T>>) -> List<&2, Table.Indexed<Names.T>>:
  match row:
    case None{}:
      rest
    case Some{NameRow{node, key, text, canonical}}:
      Con{Table.Indexed{index, node}, rest}

def ordered_names(keys: List<&2, String>, +table: Table.T<NameRow>) -> List<&2, Table.Indexed<Names.T>>:
  match keys:
    case Nil{}:
      Nil{}
    case Con{+index, rest}:
      name_indexed(Table.get(NameRow, table, index), index, ordered_names(rest, table))

def names(source: T) -> List<&2, Table.Indexed<Names.T>>:
  +table = state_names(source_state(source))
  ordered_names(sort_keys(Table.keys(NameRow, table)), table)
