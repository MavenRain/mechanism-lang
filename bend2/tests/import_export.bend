import Base
import ../import/ndjson.bend as Json
import ../import/names.bend as Names
import ../import/exprs.bend as Exprs
import ../import/decls.bend as Decls
import ../import/table.bend as Table
import ../import/export.bend as Export

def maybe_text(value: Maybe<&2, String>) -> Json.T:
  match value:
    case None{}:
      Json.Null{}
    case Some{text}:
      Json.Text{text}

def number_items(values: List<&2, String>) -> List<&2, Json.T>:
  match values:
    case Nil{}:
      Nil{}
    case Con{head, tail}:
      Con{Json.Number{head}, number_items(tail)}

def numbers(values: List<&2, String>) -> Json.T:
  Json.Items{number_items(values)}

def counts(value: Export.Counts) -> Json.T:
  match value:
    case Export.Counts{lines, names, levels, exprs, decls}:
      Json.Items{[Json.Number{U32.show(lines)}, Json.Number{Nat.show(names)},
        Json.Number{Nat.show(levels)}, Json.Number{Nat.show(exprs)}, Json.Number{Nat.show(decls)}]}

def names(rows: List<&2, Table.Indexed<Names.T>>, +source: Export.T) -> List<&2, Json.T>:
  match rows:
    case Nil{}:
      Nil{}
    case Con{Table.Indexed{+index, node}, rest}:
      Con{Json.Items{[Json.Number{index}, maybe_text(Export.name_string(source, index)),
        maybe_text(Export.canonical_name(source, index))]}, names(rest, source)}

def decl(declaration: Decls.T) -> Json.T:
  match declaration:
    case Decls.Decl{+name, +levels, +typ, +kind, +line}:
      +original : Decls.T = Decls.Decl{name, levels, typ, kind, line}
      Json.Items{[Json.Number{name}, Json.Text{Decls.kind_name(kind)}, Json.Number{typ},
        numbers(levels), numbers(Decls.name_references(original)),
        numbers(Decls.expr_references(original)), Json.Number{U32.show(line)}]}

def decls(rows: List<&2, Decls.T>) -> List<&2, Json.T>:
  match rows:
    case Nil{}:
      Nil{}
    case Con{head, rest}:
      Con{decl(head), decls(rest)}

def expr_kind(value: Exprs.T) -> String:
  match value:
    case Exprs.Bvar{index}:
      "bvar"
    case Exprs.Sort{index}:
      "sort"
    case Exprs.Const{name, universes}:
      "const"
    case Exprs.Apply{fn, arg}:
      "app"
    case Exprs.Lam{binder}:
      "lam"
    case Exprs.Forall{binder}:
      "forallE"
    case Exprs.Let{name, typ, val, body, nondep}:
      "letE"
    case Exprs.Proj{name, index, structure}:
      "proj"
    case Exprs.NatLiteral{text}:
      "natVal"
    case Exprs.StringLiteral{text}:
      "strVal"
    case Exprs.Mdata{expr, data}:
      "mdata"

def exprs(rows: List<&2, Table.Indexed<Exprs.T>>) -> List<&2, Json.T>:
  match rows:
    case Nil{}:
      Nil{}
    case Con{Table.Indexed{index, +node}, rest}:
      Con{Json.Items{[Json.Number{index}, Json.Text{expr_kind(node)},
        numbers(Exprs.name_references(node)), numbers(Exprs.level_references(node)),
        numbers(Exprs.expr_references(node))]}, exprs(rest)}

def metadata(value: Export.Metadata) -> Json.T:
  match value:
    case Export.Metadata{name, version, hash, lean, format}:
      Json.Items{[Json.Text{name}, Json.Text{version}, Json.Text{hash}, Json.Text{lean}, Json.Text{format}]}

def summary(+source: Export.T) -> String:
  Json.to_string(Json.Items{[
    metadata(Export.metadata(source)), counts(Export.counts(source)),
    Json.Items{names(Export.names(source), source)},
    Json.Items{decls(Export.declarations(source))},
    Json.Items{exprs(Export.expressions(source))},
    Json.Number{Nat.show(List.length(&2, Decls.Group, Export.groups(source)))}
  ]})

def result(value: Result<&2, &2, Export.Error, Export.T>) -> IO(Unit):
  match value:
    case Done{source}:
      IO.print(summary(source))
    case Fail{Export.ImportError{line, message}}:
      IO.die(Unit, 1, U32.show(line) ++ ":" ++ message)

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Con{source, Nil{}}:
      result(Export.read_string(source))
    case other:
      IO.die(Unit, 64, "usage: import_export NDJSON")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
