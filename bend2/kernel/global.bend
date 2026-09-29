import Base
import ./common.bend as Common
import ./term.bend as Term
import ./prim.bend as Prim
import ./positivity.bend as Positivity
import ./level.bend as Level

type DefEntry is Data:
  DefEntry{ty: Term.T, body: Term.T, reducible: Bool, rec_arg: Maybe<&2, Nat>, partial: Bool}

type Entry is Data:
  Def{value: DefEntry}
  Axiom{ty: Term.T}
  Prim{ty: Term.T, prim: Prim.T}

type T is Data:
  Globals{entries: List<&2, Common.Pair<String, Entry>>, families: List<&2, Common.Pair<String, Positivity.Family>>, entry_index: Map<&2, Maybe<&2, Entry>>, family_index: Map<&2, Maybe<&2, Positivity.Family>>}

def empty() -> T:
  Globals{[], [], Map.new(&2, Maybe<&2, Entry>), Map.new(&2, Maybe<&2, Positivity.Family>)}

def find_entry(entries: List<&2, Common.Pair<String, Entry>>, +name: String) -> Maybe<&2, Entry>:
  match entries:
    case Nil{}:
      None{}
    case Con{Common.Pair{key, value}, rest}:
      selected = Bool.pick(Unit -> Maybe<&2, Entry>, String.eq(key, name), unit => Some{value}, unit => find_entry(rest, name))
      selected(Unit{})

def map_result(-A: Data, result: Map<&2, Maybe<&2, A>> & Maybe<&2, A>) -> Maybe<&2, A>:
  (index, found) = result
  found

def map_present(-A: Data, result: Map<&2, Maybe<&2, A>> & Bool) -> Bool:
  (index, present) = result
  present

def find(name: String, globals: T) -> Maybe<&2, Entry>:
  match globals:
    case Globals{entries, families, entry_index, family_index}:
      map_result(Entry, Map.get(Maybe<&2, Entry>, None{}, entry_index, name))

def remove_entry(entries: List<&2, Common.Pair<String, Entry>>, +name: String) -> List<&2, Common.Pair<String, Entry>>:
  match entries:
    case Nil{}:
      []
    case Con{Common.Pair{+key, value}, rest}:
      +tail = remove_entry(rest, name)
      Bool.pick(List<&2, Common.Pair<String, Entry>>, String.eq(key, name), tail, Common.Pair{key, value} <> tail)

def add(+name: String, +entry: Entry, globals: T) -> T:
  match globals:
    case Globals{+entries, families, +entry_index, family_index}:
      present = map_present(Entry, Map.has(&2, Maybe<&2, Entry>, entry_index, name))
      selected = Bool.pick(Unit -> List<&2, Common.Pair<String, Entry>>, present, unit => remove_entry(entries, name), unit => entries)
      Globals{Common.Pair{name, entry} <> selected(Unit{}), families, Map.set(&2, Maybe<&2, Entry>, entry_index, name, Some{entry}), family_index}

def find_family_entry(families: List<&2, Common.Pair<String, Positivity.Family>>, +name: String) -> Maybe<&2, Positivity.Family>:
  match families:
    case Nil{}:
      None{}
    case Con{Common.Pair{key, value}, rest}:
      selected = Bool.pick(Unit -> Maybe<&2, Positivity.Family>, String.eq(key, name), unit => Some{value}, unit => find_family_entry(rest, name))
      selected(Unit{})

def find_family(name: String, globals: T) -> Maybe<&2, Positivity.Family>:
  match globals:
    case Globals{entries, families, entry_index, family_index}:
      map_result(Positivity.Family, Map.get(Maybe<&2, Positivity.Family>, None{}, family_index, name))

def remove_family(families: List<&2, Common.Pair<String, Positivity.Family>>, +name: String) -> List<&2, Common.Pair<String, Positivity.Family>>:
  match families:
    case Nil{}:
      []
    case Con{Common.Pair{+key, value}, rest}:
      +tail = remove_family(rest, name)
      Bool.pick(List<&2, Common.Pair<String, Positivity.Family>>, String.eq(key, name), tail, Common.Pair{key, value} <> tail)

def add_family(+name: String, +family: Positivity.Family, globals: T) -> T:
  match globals:
    case Globals{entries, +families, entry_index, +family_index}:
      present = map_present(Positivity.Family, Map.has(&2, Maybe<&2, Positivity.Family>, family_index, name))
      selected = Bool.pick(Unit -> List<&2, Common.Pair<String, Positivity.Family>>, present, unit => remove_family(families, name), unit => families)
      Globals{entries, Common.Pair{name, family} <> selected(Unit{}), entry_index, Map.set(&2, Maybe<&2, Positivity.Family>, family_index, name, Some{family})}

def entry_ty(e: Entry) -> Term.T:
  match e:
    case Def{DefEntry{ty, body, reducible, rec_arg, partial}}:
      ty
    case Axiom{ty}:
      ty
    case Prim{ty, prim}:
      ty

def def_of(e: Entry) -> Maybe<&2, DefEntry>:
  match e:
    case Def{value}:
      Some{value}
    case Axiom{ty}:
      None{}
    case Prim{ty, prim}:
      None{}

def find_def_result(e: Maybe<&2, Entry>) -> Maybe<&2, DefEntry>:
  match e:
    case None{}:
      None{}
    case Some{value}:
      def_of(value)

def find_def(name: String, globals: T) -> Maybe<&2, DefEntry>:
  find_def_result(find(name, globals))

def find_prim_result(e: Maybe<&2, Entry>) -> Maybe<&2, Prim.T>:
  match e:
    case Some{Prim{ty, prim}}:
      Some{prim}
    case _:
      None{}

def find_prim(name: String, globals: T) -> Maybe<&2, Prim.T>:
  find_prim_result(find(name, globals))

def add_prims(ps: List<&2, Prim.T>, globals: T) -> T:
  match ps:
    case Nil{}:
      globals
    case Con{+p, rest}:
      add_prims(rest, add(Prim.name(p), Prim{Prim.ty(p), p}, globals))

def initial() -> T:
  add_prims(Prim.catalog(), add("Nat", Axiom{Term.Univ{Level.one()}}, empty()))


def index_rows(-A: Data, rows: List<&2, Common.Pair<String, A>>) -> Map<&2, Maybe<&2, A>>:
  match rows:
    case Nil{}: Map.new(&2, Maybe<&2, A>)
    case Con{Common.Pair{name, value}, rest}:
      Map.set(&2, Maybe<&2, A>, index_rows(A, rest), name, Some{value})

def of_lists(+entries: List<&2, Common.Pair<String, Entry>>, +families: List<&2, Common.Pair<String, Positivity.Family>>) -> T:
  Globals{entries, families, index_rows(Entry, entries), index_rows(Positivity.Family, families)}

def without_family(+name: String, globals: T) -> T:
  match globals:
    case Globals{entries, families, entry_index, family_index}:
      Globals{entries, remove_family(families, name), entry_index, Map.del(&2, Maybe<&2, Positivity.Family>, family_index, name)}
