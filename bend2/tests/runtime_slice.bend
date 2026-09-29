import Base
import ../kernel/common.bend as P
import ../kernel/eterm.bend as E
import ../kernel/erase.bend as Erase
import ../kernel/literal.bend as Literal
import ../kernel/bignum.bend as Bignum

def Rows() -> Data:
  List<&2, P.Pair<String, Erase.Entry>>

def Catalog() -> Data:
  List<&2, P.Pair<String, List<&2, String>>>

def member(names: List<&2, String>, +name: String) -> Bool:
  match names:
    case Nil{}: False{}
    case Con{head, rest}: String.eq(head, name) || member(rest, name)

def insert(+names: List<&2, String>, +name: String) -> List<&2, String>:
  Bool.pick(List<&2, String>, member(names, name), names, Con{name, names})

law references:
  for names: List<&2, String>
  for term: E.Ktm
  List<&2, String>

@unsafe def reference_list(names: List<&2, String>, terms: List<&2, E.Ktm>) -> List<&2, String>:
  match terms:
    case Nil{}: names
    case Con{term, rest}: reference_list(references(names, term), rest)

@unsafe def reference_branches(names: List<&2, String>, branches: List<&2, E.Branch<E.Ktm>>) -> List<&2, String>:
  match branches:
    case Nil{}: names
    case Con{E.Branch{tag, arity, body}, rest}: reference_branches(references(names, body), rest)

@unsafe def references(names, term):
  match term:
    case E.KVar{index}: names
    case E.KLit{value}: names
    case E.KErased{}: names
    case E.KGlobal{name}: insert(names, name)
    case E.KLet{name, value, body}: references(references(names, value), body)
    case E.KClos{E.Fid{name}, arity, captures}: reference_list(insert(names, name), captures)
    case E.KDelay{E.Fid{name}, captures}: reference_list(insert(names, name), captures)
    case E.KApp{head, args}: reference_list(references(names, head), args)
    case E.KTail{head, args}: reference_list(references(names, head), args)
    case E.KStruct{tid, fields}: reference_list(names, fields)
    case E.KTag{tid, tag, fields}: reference_list(names, fields)
    case E.KProj{tid, index, value}: references(names, value)
    case E.KForce{value}: references(names, value)
    case E.KCase{tid, scrut, branches}: reference_branches(references(names, scrut), branches)

def lookup(catalog: Catalog, +name: String) -> Maybe<&2, List<&2, String>>:
  match catalog:
    case Nil{}: None{}
    case Con{P.Pair{key, deps}, rest}:
      Bool.pick(Maybe<&2, List<&2, String>>, String.eq(key, name), Some{deps}, lookup(rest, name))

def add_function(found: Maybe<&2, List<&2, String>>, catalog: Catalog, +name: String, body: E.Ktm) -> Result<&2, &2, String, Catalog>:
  match found:
    case None{}: Done{Con{P.Pair{name, references([], body)}, catalog}}
    case Some{deps}: Fail{"duplicate erased function: " ++ name}

def catalog_decls(decls: List<&2, E.Kdecl>, +catalog: Catalog) -> Result<&2, &2, String, Catalog>:
  match decls:
    case Nil{}: Done{catalog}
    case Con{E.KRec{tids}, rest}: catalog_decls(rest, catalog)
    case Con{E.KFun{E.Fid{+name}, params, result, body}, rest}:
      do Result<&2, &2, String, Catalog>:
        catalog : Catalog <- add_function(lookup(catalog, name), catalog, name, body)
        catalog_decls(rest, catalog)

def catalog_rows(rows: Rows, catalog: Catalog) -> Result<&2, &2, String, Catalog>:
  match rows:
    case Nil{}: Done{catalog}
    case Con{P.Pair{name, Erase.Dropped{}}, rest}: catalog_rows(rest, catalog)
    case Con{P.Pair{name, Erase.Postulate{repr}}, rest}: catalog_rows(rest, catalog)
    case Con{P.Pair{name, Erase.Code{decls}}, rest}:
      do Result<&2, &2, String, Catalog>:
        catalog : Catalog <- catalog_decls(decls, catalog)
        catalog_rows(rest, catalog)

def dependencies(found: Maybe<&2, List<&2, String>>) -> List<&2, String>:
  match found:
    case None{}: []
    case Some{names}: names

law visit:
  for +catalog: Catalog
  for +needed: List<&2, String>
  for pending: List<&2, String>
  List<&2, String>

@unsafe def visit_one(seen: Bool, +catalog: Catalog, needed: List<&2, String>, +name: String, rest: List<&2, String>) -> List<&2, String>:
  match seen:
    case True{}: visit(catalog, needed, rest)
    case False{}: visit(catalog, Con{name, needed}, List.append(&2, String, dependencies(lookup(catalog, name)), rest))

@unsafe def visit(catalog, needed, pending):
  match pending:
    case Nil{}: needed
    case Con{+name, rest}: visit_one(member(needed, name), catalog, needed, name, rest)

def select_decls(decls: List<&2, E.Kdecl>, +needed: List<&2, String>) -> List<&2, E.Kdecl>:
  match decls:
    case Nil{}: []
    case Con{E.KRec{tids}, rest}: Con{E.KRec{tids}, select_decls(rest, needed)}
    case Con{E.KFun{E.Fid{+name}, params, result, body}, rest}:
      +tail = select_decls(rest, needed)
      Bool.pick(List<&2, E.Kdecl>, member(needed, name), Con{E.KFun{E.Fid{name}, params, result, body}, tail}, tail)

def select_rows(rows: Rows, +needed: List<&2, String>) -> Rows:
  match rows:
    case Nil{}: []
    case Con{P.Pair{name, Erase.Dropped{}}, rest}: Con{P.Pair{name, Erase.Dropped{}}, select_rows(rest, needed)}
    case Con{P.Pair{name, Erase.Postulate{repr}}, rest}: Con{P.Pair{name, Erase.Postulate{repr}}, select_rows(rest, needed)}
    case Con{P.Pair{name, Erase.Code{decls}}, rest}: Con{P.Pair{name, Erase.Code{select_decls(decls, needed)}}, select_rows(rest, needed)}

def select(+rows: Rows, export: String) -> Result<&2, &2, String, Rows>:
  do Result<&2, &2, String, Rows>:
    catalog : Catalog <- catalog_rows(rows, [])
    return select_rows(rows, visit(catalog, [], [export]))

def require(condition: Bool, message: String) -> Result<&2, &2, String, Unit>:
  Bool.pick(Result<&2, &2, String, Unit>, condition, Done{Unit{}}, Fail{message})

def includes(actual: List<&2, String>, +expected: List<&2, String>) -> Bool:
  match actual:
    case Nil{}: True{}
    case Con{head, rest}: member(expected, head) && includes(rest, expected)

def same_set(+actual: List<&2, String>, +expected: List<&2, String>) -> Bool:
  includes(actual, expected) && includes(expected, actual)

def reference_test() -> Result<&2, &2, String, Unit>:
  +tid : E.Tid = E.Tid{"slice-test"}
  body : E.Ktm = E.KStruct{tid, [E.KGlobal{"direct"}, E.KGlobal{"direct"}, E.KLet{"x", E.KGlobal{"bound"}, E.KGlobal{"body"}}, E.KClos{E.Fid{"closure"}, 1n, [E.KGlobal{"capture"}]}, E.KDelay{E.Fid{"delay"}, [E.KGlobal{"delayed-capture"}]}, E.KApp{E.KGlobal{"call"}, [E.KGlobal{"argument"}]}, E.KTail{E.KGlobal{"tail"}, [E.KGlobal{"tail-argument"}]}, E.KProj{tid, 0n, E.KGlobal{"projection"}}, E.KTag{tid, 0n, [E.KGlobal{"payload"}]}, E.KCase{tid, E.KGlobal{"scrutinee"}, [E.Branch{0n, 1n, E.KGlobal{"branch"}}]}, E.KForce{E.KGlobal{"forced"}}, E.KVar{0n}, E.KErased{}, E.KLit{Literal.LInt{Bignum.zero()}}]}
  expected : List<&2, String> = ["direct", "bound", "body", "closure", "capture", "delay", "delayed-capture", "call", "argument", "tail", "tail-argument", "projection", "payload", "scrutinee", "branch", "forced"]
  require(same_set(references([], body), expected), "runtime reference traversal")

def fn(name: String, body: E.Ktm) -> E.Kdecl:
  E.KFun{E.Fid{name}, [], E.RI31{}, body}

def duplicate(result: Result<&2, &2, String, Rows>) -> Result<&2, &2, String, Unit>:
  match result:
    case Done{rows}: Fail{"duplicate function accepted"}
    case Fail{+message}: require(String.eq(message, "duplicate erased function: root"), "wrong duplicate refusal: " ++ message)

def row_names(rows: Rows) -> List<&2, String>:
  match rows:
    case Nil{}: []
    case Con{P.Pair{name, entry}, rest}: Con{name, row_names(rest)}

def same_names(left: List<&2, String>, right: List<&2, String>) -> Bool:
  match left:
    case Nil{}:
      match right:
        case Nil{}: True{}
        case other: False{}
    case Con{name, rest}:
      match right:
        case Nil{}: False{}
        case Con{other, tail}: String.eq(name, other) && same_names(rest, tail)

def self_test() -> Result<&2, &2, String, Unit>:
  +tid : E.Tid = E.Tid{"slice-test"}
  +root = fn("root", E.KApp{E.KGlobal{"middle"}, []})
  +middle = fn("middle", E.KClos{E.Fid{"lifted"}, 0n, []})
  +lifted = fn("lifted", E.KGlobal{"root"})
  dead = fn("dead", E.KGlobal{"unavailable"})
  rows : Rows = [P.Pair{"entry", Erase.Code{[E.KRec{[tid]}, root]}}, P.Pair{"helpers", Erase.Code{[middle, lifted, dead]}}, P.Pair{"type", Erase.Dropped{}}, P.Pair{"postulate", Erase.Postulate{E.RI31{}}}]
  +expected : Rows = [P.Pair{"entry", Erase.Code{[E.KRec{[tid]}, root]}}, P.Pair{"helpers", Erase.Code{[middle, lifted]}}, P.Pair{"type", Erase.Dropped{}}, P.Pair{"postulate", Erase.Postulate{E.RI31{}}}]
  do Result<&2, &2, String, Unit>:
    checked : Unit <- reference_test()
    +selected : Rows <- select(rows, "root")
    checked : Unit <- require(String.eq(Erase.print(selected), Erase.print(expected)) && same_names(row_names(selected), row_names(expected)), "runtime function closure or retained metadata")
    checked : Unit <- require(member(references([], E.KGlobal{"unavailable"}), "unavailable"), "unknown references must remain visible")
    duplicate(select([P.Pair{"duplicate", Erase.Code{[root, root]}}], "root"))
