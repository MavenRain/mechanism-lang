import Base
import ./relational_common.bend as R
import ../kernel/common.bend as C
import ../kernel/global.bend as G
import ../kernel/term.bend as T
import ../surface/syntax.bend as S
import ../surface/parser.bend as Parser
import ../surface/elab.bend as Elab

type Probe is Data:
  Probe{name: String, payload: U32, expected: U32}

def probes(rows: List<&2, Probe>, +globals: G.T) -> Result<&2, &2, String, Unit>:
  match rows:
    case Nil{}: Done{Unit{}}
    case Con{Probe{+name, +payload, expected}, rest}: do Result<&2, &2, String, Unit>:
      checked : Elab.Checked <- R.checked(globals, "def lawProbe : Nat := " ++ name ++ " " ++ U32.show(payload))
      actual : T.T <- R.normalize(R.globals(checked), "lawProbe")
      ok : Unit <- R.require(T.equal(actual, R.natural(expected)), "wrong computation: " ++ name ++ "/" ++ U32.show(payload))
      probes(rest, globals)

def inventory(+checked: Elab.Checked, entries: List<&2, String>, families: List<&2, String>) -> Result<&2, &2, String, Unit>:
  R.inventories(G.initial(), R.globals(checked), R.rows(checked), entries, families)

def sequential(declarations: List<&2, S.Decl>, +checked: Elab.Checked) -> Result<&2, &2, String, Elab.Checked>:
  match declarations:
    case Nil{}: Done{checked}
    case Con{declaration, rest}: do Result<&2, &2, String, Elab.Checked>:
      +next : Elab.Checked <- R.elaborated(R.globals(checked), [declaration])
      sequential(rest, C.Pair{R.globals(next), List.append(&2, C.Pair<String, G.Entry>, R.rows(checked), R.rows(next))})

def staged(templates: String, fixture: String, +header_size: Nat) -> Result<&2, &2, String, Elab.Checked>:
  do Result<&2, &2, String, Elab.Checked>:
    template : List<&2, S.Decl> <- R.kernel(List<&2, S.Decl>, Parser.program(templates))
    +declarations : List<&2, S.Decl> <- R.kernel(List<&2, S.Decl>, Parser.program(fixture))
    +header : List<&2, S.Decl> = List.take(&2, S.Decl, declarations, header_size)
    ok : Unit <- R.require(Nat.is_eq(R.length(S.Decl, header), header_size), "runtime fixture header missing")
    checked : Elab.Checked <- R.elaborated(G.initial(), List.append(&2, S.Decl, template, header))
    sequential(List.drop(&2, S.Decl, declarations, header_size), checked)

def new_entries(entries: Elab.Rows, +initial: G.T) -> Elab.Rows:
  match entries:
    case Nil{}: []
    case Con{+row, rest}:
      match row:
        case C.Pair{+name, +value}:
          +tail = new_entries(rest, initial)
          Bool.pick(Elab.Rows, R.found(G.find(name, initial)), tail, C.Pair{name, value} <> tail)

def global_inventory(globals: G.T, expected: List<&2, String>) -> Result<&2, &2, String, Unit>:
  match globals:
    case G.Globals{entries, families, entry_index, family_index}: R.require(R.same_names(R.entry_names(new_entries(entries, G.initial())), expected), "global entry inventory changed")

def globals_count(globals: G.T) -> Nat:
  match globals:
    case G.Globals{entries, families, entry_index, family_index}: R.length(C.Pair<String,G.Entry>,entries)

def initial_count() -> Nat:
  globals_count(G.initial())

def count_initial(checked: Elab.Checked, +count: Nat, expected_families: List<&2, String>) -> Result<&2, &2, String, Unit>:
  match checked:
    case C.Pair{G.Globals{+entries, +families, entry_index, family_index}, +rows}: do Result<&2, &2, String, Unit>:
      ok : Unit <- R.require(Bool.and(Nat.is_eq(R.length(C.Pair<String, G.Entry>, rows), count), Bool.and(Nat.is_eq(R.length(C.Pair<String, G.Entry>, new_entries(entries, G.initial())), count), Nat.is_eq(R.length(C.Pair<String, G.Entry>, entries), Nat.add(count, initial_count())))), "entry inventory changed")
      R.inventories(G.initial(), G.of_lists(entries, families), rows, R.entry_names(rows), expected_families)
