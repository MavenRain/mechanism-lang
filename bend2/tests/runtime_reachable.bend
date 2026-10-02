import Base
import ./runtime_slice.bend as Slice
import ../kernel/common.bend as P
import ../kernel/global.bend as Global
import ../kernel/erase.bend as Erase
import ../kernel/error.bend as Error

def SourceRows() -> Data:
  List<&2, P.Pair<String, Global.Entry>>

def kernel(-A: Data, result: Result<&2, &2, Error.T, A>) -> Result<&2, &2, String, A>:
  match result:
    case Fail{error}: Fail{Error.to_string(error)}
    case Done{value}: Done{value}

def erased(found: Maybe<&2, Global.Entry>, globals: Global.T, name: String) -> Result<&2, &2, String, Erase.Entry>:
  match found:
    case None{}: Fail{"missing runtime dependency: " ++ name}
    case Some{entry}: kernel(Erase.Entry, Erase.entry(globals, name, entry))

law visit:
  for +globals: Global.T
  for +catalog: Slice.Catalog
  for rows: Slice.Rows
  for +seen: List<&2, String>
  for pending: List<&2, String>
  Result<&2, &2, String, Slice.Rows>

@unsafe def visit_new(found: Maybe<&2, List<&2, String>>, +globals: Global.T, +catalog: Slice.Catalog, rows: Slice.Rows, +seen: List<&2, String>, +name: String, rest: List<&2, String>) -> Result<&2, &2, String, Slice.Rows>:
  match found:
    case Some{deps}: visit(globals, catalog, rows, Con{name, seen}, List.append(&2, String, deps, rest))
    case None{}:
      do Result<&2, &2, String, Slice.Rows>:
        +entry : Erase.Entry <- erased(Global.find(name, globals), globals, name)
        +catalog : Slice.Catalog <- Slice.catalog_rows([P.Pair{name, entry}], catalog)
        visit(globals, catalog, Con{P.Pair{name, entry}, rows}, Con{name, seen},
          List.append(&2, String, Slice.dependencies(Slice.lookup(catalog, name)), rest))

@unsafe def visit_seen(visited: Bool, globals: Global.T, +catalog: Slice.Catalog, rows: Slice.Rows, seen: List<&2, String>, +name: String, rest: List<&2, String>) -> Result<&2, &2, String, Slice.Rows>:
  match visited:
    case True{}: visit(globals, catalog, rows, seen, rest)
    case False{}: visit_new(Slice.lookup(catalog, name), globals, catalog, rows, seen, name, rest)

@unsafe def visit(globals, catalog, rows, seen, pending):
  match pending:
    case Nil{}: Done{rows}
    case Con{+name, rest}: visit_seen(Slice.member(seen, name), globals, catalog, rows, seen, name, rest)

def find_row(rows: Slice.Rows, +name: String) -> Maybe<&2, Erase.Entry>:
  match rows:
    case Nil{}: None{}
    case Con{P.Pair{key, entry}, rest}:
      Bool.pick(Maybe<&2, Erase.Entry>, String.eq(key, name), Some{entry}, find_row(rest, name))

def keep_row(found: Maybe<&2, Erase.Entry>, name: String, rest: Slice.Rows) -> Slice.Rows:
  match found:
    case None{}: rest
    case Some{entry}: Con{P.Pair{name, entry}, rest}

def ordered(source: SourceRows, +rows: Slice.Rows) -> Slice.Rows:
  match source:
    case Nil{}: []
    case Con{P.Pair{+name, entry}, rest}: keep_row(find_row(rows, name), name, ordered(rest, rows))

def select_many(globals: Global.T, source: SourceRows, exports: List<&2, String>) -> Result<&2, &2, String, Slice.Rows>:
  do Result<&2, &2, String, Slice.Rows>:
    rows : Slice.Rows <- visit(globals, [], [], [], exports)
    return ordered(source, rows)

def select(globals: Global.T, source: SourceRows, +export: String) -> Result<&2, &2, String, Slice.Rows>:
  do Result<&2, &2, String, Slice.Rows>:
    rows : Slice.Rows <- select_many(globals, source, [export])
    Slice.select(rows, export)

def missing(result: Result<&2, &2, String, Slice.Rows>) -> Result<&2, &2, String, Unit>:
  match result:
    case Fail{message}: Slice.require(String.eq(message, "missing runtime dependency: missing"), "missing dependency diagnostic")
    case Done{rows}: Fail{"missing runtime dependency accepted"}

def named_rows(rows: Slice.Rows, +names: List<&2, String>) -> Slice.Rows:
  match rows:
    case Nil{}: []
    case Con{P.Pair{+name, entry}, rest}:
      +tail = named_rows(rest, names)
      Bool.pick(Slice.Rows, Slice.member(names, name), Con{P.Pair{name, entry}, tail}, tail)

def self_test(+globals: Global.T, +source: SourceRows) -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +selected : Slice.Rows <- select(globals, source, "root")
    checked : Unit <- Slice.require(Slice.same_names(Slice.row_names(selected), ["helper", "alias", "root"]), "reachable declaration order or unused declaration")
    full : Slice.Rows <- kernel(Slice.Rows, Erase.program(globals, source))
    full : Slice.Rows <- Slice.select(full, "root")
    expected : Slice.Rows = named_rows(full, ["helper", "alias", "root"])
    checked : Unit <- Slice.require(String.eq(Erase.print(selected), Erase.print(expected)), "reachable erasure differs from complete erasure")
    shared : Slice.Rows <- select_many(globals, source, ["root", "alias", "root"])
    checked : Unit <- Slice.require(Slice.same_names(Slice.row_names(shared), ["helper", "alias", "root"]), "shared dependencies or repeated export")
    dropped : Slice.Rows <- select(globals, source, "hidden")
    checked : Unit <- Slice.require(String.eq(Erase.print(dropped), Erase.print([P.Pair{"hidden", Erase.Dropped{}}])), "dropped declaration traversal")
    checked : Unit <- missing(select(globals, source, "missing"))
    return Unit{}
