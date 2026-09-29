import Base
import ./ndjson.bend as Json
import ./table.bend as Table
import ./translate.bend as Translate
import ./exprs.bend as Exprs
import ./decls.bend as Decls
import ./names.bend as Names

def number(value: String) -> Json.T:
  Json.Number{value}

def number_items(values: List<&2, String>) -> List<&2, Json.T>:
  match values:
    case Nil{}:
      Nil{}
    case Con{head, rest}:
      Con{number(head), number_items(rest)}

def numbers(values: List<&2, String>) -> Json.T:
  Json.Items{number_items(values)}

def string(value: String) -> Json.T:
  Json.Text{value}

def string_items(values: List<&2, String>) -> List<&2, Json.T>:
  match values:
    case Nil{}:
      Nil{}
    case Con{head, rest}:
      Con{string(head), string_items(rest)}

def strings(values: List<&2, String>) -> Json.T:
  Json.Items{string_items(values)}

def binder_info(info: Exprs.BinderInfo) -> String:
  match info:
    case Exprs.Default{}:
      "default"
    case Exprs.Implicit{}:
      "implicit"
    case Exprs.StrictImplicit{}:
      "strictImplicit"
    case Exprs.InstImplicit{}:
      "instImplicit"

def binder(value: Translate.Binder) -> Json.T:
  match value:
    case Translate.Binder{name, typ, body, info}:
      Json.Object{[Json.Field{"name", Json.Text{name}}, Json.Field{"type", Json.Number{typ}},
        Json.Field{"body", Json.Number{body}}, Json.Field{"binderInfo", Json.Text{binder_info(info)}}]}

def node_field(value: Translate.Node) -> Json.Field<Json.T>:
  match value:
    case Translate.Bound{index}:
      Json.Field{"bvar", Json.Number{index}}
    case Translate.Sort{level}:
      Json.Field{"sort", Json.Number{level}}
    case Translate.Constant{source_name, name, universes}:
      Json.Field{"const", Json.Object{[Json.Field{"source_name", Json.Number{source_name}},
        Json.Field{"name", Json.Text{name}}, Json.Field{"universes", numbers(universes)}]}}
    case Translate.Apply{fn, arg}:
      Json.Field{"app", Json.Object{[Json.Field{"fn", Json.Number{fn}}, Json.Field{"arg", Json.Number{arg}}]}}
    case Translate.Lambda{value}:
      Json.Field{"lam", binder(value)}
    case Translate.Pi{value}:
      Json.Field{"forallE", binder(value)}
    case Translate.Let{name, typ, value, body, nondep}:
      Json.Field{"letE", Json.Object{[Json.Field{"name", Json.Text{name}}, Json.Field{"type", Json.Number{typ}},
        Json.Field{"value", Json.Number{value}}, Json.Field{"body", Json.Number{body}},
        Json.Field{"nondep", Json.Boolean{nondep}}]}}
    case Translate.Projection{source_type_name, type_name, index, structure}:
      Json.Field{"proj", Json.Object{[Json.Field{"source_type_name", Json.Number{source_type_name}},
        Json.Field{"typeName", Json.Text{type_name}}, Json.Field{"idx", Json.Number{index}},
        Json.Field{"struct", Json.Number{structure}}]}}
    case Translate.NatLiteral{value}:
      Json.Field{"natVal", Json.Text{value}}
    case Translate.StringLiteral{value}:
      Json.Field{"strVal", Json.Text{value}}
    case Translate.Metadata{expr, data}:
      Json.Field{"mdata", Json.Object{[Json.Field{"expr", Json.Number{expr}}, Json.Field{"data", Json.Object{data}}]}}

def node_record(row: Table.Indexed<Translate.Node>) -> Json.T:
  match row:
    case Table.Indexed{id, node}:
      Json.Object{[Json.Field{"expr", Json.Number{id}}, node_field(node)]}

def node_records(rows: List<&2, Table.Indexed<Translate.Node>>) -> List<&2, Json.T>:
  match rows:
    case Nil{}:
      Nil{}
    case Con{head, rest}:
      Con{node_record(head), node_records(rest)}

def level_field(level: Translate.Level) -> Json.Field<Json.T>:
  match level:
    case Translate.LZero{}:
      Json.Field{"zero", Json.Boolean{True{}}}
    case Translate.LSucc{child}:
      Json.Field{"succ", Json.Number{child}}
    case Translate.LMax{left, right}:
      Json.Field{"max", numbers([left, right])}
    case Translate.LImax{left, right}:
      Json.Field{"imax", numbers([left, right])}
    case Translate.LParam{source_name, name}:
      Json.Field{"param", Json.Object{[Json.Field{"source_name", Json.Number{source_name}}, Json.Field{"name", Json.Text{name}}]}}

def level_record(row: Table.Indexed<Translate.Level>) -> Json.T:
  match row:
    case Table.Indexed{id, level}:
      Json.Object{[Json.Field{"level", Json.Number{id}}, level_field(level)]}

def level_records(rows: List<&2, Table.Indexed<Translate.Level>>) -> List<&2, Json.T>:
  match rows:
    case Nil{}:
      Nil{}
    case Con{head, rest}:
      Con{level_record(head), level_records(rest)}

def name_field(value: Names.T) -> Json.Field<Json.T>:
  match value:
    case Names.Anonymous{}:
      Json.Field{"anonymous", Json.Boolean{True{}}}
    case Names.Str{prefix, text}:
      Json.Field{"str", Json.Object{[Json.Field{"pre", Json.Number{prefix}}, Json.Field{"str", Json.Text{text}}]}}
    case Names.Num{prefix, index}:
      Json.Field{"num", Json.Object{[Json.Field{"pre", Json.Number{prefix}}, Json.Field{"i", Json.Number{index}}]}}

def name_record(row: Table.Indexed<Names.T>) -> Json.T:
  match row:
    case Table.Indexed{id, value}:
      Json.Object{[Json.Field{"name", Json.Number{id}}, name_field(value)]}

def parameter(value: Translate.Parameter) -> Json.T:
  match value:
    case Translate.Parameter{id, name}:
      Json.Object{[Json.Field{"source_name", Json.Number{id}}, Json.Field{"name", Json.Text{name}}]}

def parameters(values: List<&2, Translate.Parameter>) -> List<&2, Json.T>:
  match values:
    case Nil{}:
      Nil{}
    case Con{head, rest}:
      Con{parameter(head), parameters(rest)}

def declaration_id(value: Decls.T) -> String:
  match value:
    case Decls.Decl{id, levels, typ, kind, line}:
      id

def row_summary(value: Translate.Row) -> Json.T:
  match value:
    case Translate.Row{declaration, name, params, root, dependencies}:
      Json.Items{[Json.Number{declaration_id(declaration)}, Json.Text{name}, Json.Items{parameters(params)},
        Json.Number{root}, strings(dependencies)]}

def row_summaries(values: List<&2, Translate.Row>) -> List<&2, Json.T>:
  match values:
    case Nil{}:
      Nil{}
    case Con{head, rest}:
      Con{row_summary(head), row_summaries(rest)}

def scoped_summary(+table: Translate.T) -> Json.T:
  Json.Items{[Json.Items{row_summaries(Translate.rows(table))},
    Json.Items{level_records(Translate.levels(table))}, Json.Items{node_records(Translate.nodes(table))}]}
