import Base
import ./export.bend as Export
import ./report.bend as Report
import ./translate.bend as Translate
import ./table.bend as Table
import ./decls.bend as Decls

type Verdict is Data:
  NameOnly{}
  Unmapped{}
  Never{}

type Row is Data:
  Row{lean_name: String, target: Maybe<&2, String>, verdict: Verdict}

type ExceptionRow is Data:
  ExceptionRow{lean_name: String, ratification: String, milestone: String}

type Target is Data:
  Target{lean_name: String, target: String}

type T is Data:
  Inventory{rows: List<&2, Row>, never: List<&2, ExceptionRow>, external_declared: Nat, const_names: Nat}

def verdict_name(verdict: Verdict) -> String:
  match verdict:
    case NameOnly{}:
      "NAME_ONLY"
    case Unmapped{}:
      "UNMAPPED"
    case Never{}:
      "NEVER"

def omega_reflection(name: String) -> Bool:
  Report.namespace(name, "Lean.Omega")

def clean_char(ch: Char) -> Bool:
  match ch:
    case Chr{0}:
      False{}
    case Chr{9}:
      False{}
    case Chr{10}:
      False{}
    case Chr{13}:
      False{}
    case other:
      True{}

def clean_text(text: String) -> Bool:
  match text:
    case SNil{}:
      True{}
    case SCon{ch, rest}:
      Bool.and(clean_char(ch), clean_text(rest))

def cell(+value: String) -> Bool:
  Bool.and(Bool.not(String.is_empty(value)), Bool.and(Bool.not(String.eq(value, "-")), clean_text(value)))

def guard(ok: Bool, message: String) -> Result<&2, &2, String, Unit>:
  match ok:
    case True{}:
      Done{Unit{}}
    case False{}:
      Fail{message}

def add_external(yes: Bool, name: String, names: Translate.IdSet) -> Translate.IdSet:
  match yes:
    case True{}:
      Translate.set_add(names, name)
    case False{}:
      names

def external_names(ids: List<&2, String>, +source: Export.T, names: Translate.IdSet) -> Result<&2, &2, String, Translate.IdSet>:
  match ids:
    case Nil{}:
      Done{names}
    case Con{+id, rest}:
      do Result<&2, &2, String, Translate.IdSet>:
        +name : String <- Table.require(String, Export.name_string(source, id), "missing canonical source name " ++ id)
        external_names(rest, source, add_external(Report.external_name(name), name, names))

def targets(rows: List<&2, Target>, +names: Translate.IdSet, +found: Table.T<String>) -> Result<&2, &2, String, Table.T<String>>:
  match rows:
    case Nil{}:
      Done{found}
    case Con{Target{+name, +target}, rest}:
      do Result<&2, &2, String, Table.T<String>>:
        Unit <- guard(Bool.and(cell(name), cell(target)), "mapping targets require nonempty TSV cells")
        Unit <- guard(Bool.not(Table.has(String, found, name)), "duplicate mapping target for " ++ name)
        Unit <- guard(Translate.set_has(names, name), "mapping target is not a referenced external constant: " ++ name)
        Unit <- guard(Bool.not(omega_reflection(name)), "mapping target conflicts with ratified NEVER: " ++ name)
        targets(rest, names, Table.put(String, found, name, target))

law merge_names:
  for left: List<&2, String>
  for right: List<&2, String>
  List<&2, String>

@unsafe def merge_order(order: Cmp, left: String, xs: List<&2, String>, right: String, ys: List<&2, String>) -> List<&2, String>:
  match order:
    case GT{}:
      Con{right, merge_names(Con{left, xs}, ys)}
    case other:
      Con{left, merge_names(xs, Con{right, ys})}

@unsafe def merge_names(left, right):
  match left:
    case Nil{}:
      right
    case Con{+head, rest}:
      match right:
        case Nil{}:
          Con{head, rest}
        case Con{+other, tail}:
          merge_order(String.order(head, other), head, rest, other, tail)

law sort_names:
  for names: List<&2, String>
  List<&2, String>

@unsafe def sort_split(split: Export.KeySplit) -> List<&2, String>:
  match split:
    case Export.KeySplit{a, b}:
      merge_names(sort_names(a), sort_names(b))

@unsafe def sort_names(names):
  match names:
    case Nil{}:
      Nil{}
    case Con{one, Nil{}}:
      [one]
    case Con{one, Con{two, rest}}:
      sort_split(Export.split_keys(Con{one, Con{two, rest}}))

def mapped_row(target: Maybe<&2, String>, name: String) -> Row:
  match target:
    case None{}:
      Row{name, None{}, Unmapped{}}
    case Some{target}:
      Row{name, Some{target}, NameOnly{}}

def classify(never: Bool, +name: String, targets: Table.T<String>) -> Row:
  match never:
    case True{}:
      Row{name, None{}, Never{}}
    case False{}:
      mapped_row(Table.get(String, targets, name), name)

def make_rows(names: List<&2, String>, +targets: Table.T<String>) -> List<&2, Row>:
  match names:
    case Nil{}:
      Nil{}
    case Con{+name, rest}:
      Con{classify(omega_reflection(name), name, targets), make_rows(rest, targets)}

def never_row(row: Row, rest: List<&2, ExceptionRow>) -> List<&2, ExceptionRow>:
  match row:
    case Row{name, target, Never{}}:
      Con{ExceptionRow{name, "R-V4:omega-reflection", "NEVER"}, rest}
    case Row{name, target, NameOnly{}}:
      rest
    case Row{name, target, Unmapped{}}:
      rest

def never_rows(rows: List<&2, Row>) -> List<&2, ExceptionRow>:
  match rows:
    case Nil{}:
      Nil{}
    case Con{head, rest}:
      never_row(head, never_rows(rest))

def count_declared(declarations: List<&2, Decls.T>, +source: Export.T) -> Nat:
  match declarations:
    case Nil{}:
      0n
    case Con{Decls.Decl{id, levels, typ, kind, line}, rest}:
      Report.increment(Report.external_display(Export.name_string(source, id)), count_declared(rest, source))

def inventory(+source: Export.T, target_rows: List<&2, Target>) -> Result<&2, &2, String, T>:
  do Result<&2, &2, String, T>:
    +constants : Translate.IdSet = Report.constant_names(source)
    +names : Translate.IdSet <- external_names(Export.sort_keys(Translate.set_keys(constants)), source, Translate.empty_set())
    targets : Table.T<String> <- targets(target_rows, names, Table.empty(String))
    +rows : List<&2, Row> = make_rows(sort_names(Translate.set_keys(names)), targets)
    Done{Inventory{rows, never_rows(rows), count_declared(Export.declarations(source), source),
      List.length(&2, String, Translate.set_keys(constants))}}

def add_display(value: Maybe<&2, String>, names: Translate.IdSet) -> Translate.IdSet:
  match value:
    case None{}:
      names
    case Some{name}:
      Translate.set_add(names, name)

def displays(ids: List<&2, String>, +source: Export.T, names: Translate.IdSet) -> Translate.IdSet:
  match ids:
    case Nil{}:
      names
    case Con{id, rest}:
      displays(rest, source, add_display(Export.name_string(source, id), names))

def keep_target(found: Bool, target: Target, rest: List<&2, Target>) -> List<&2, Target>:
  match found:
    case True{}:
      Con{target, rest}
    case False{}:
      rest

def present_targets(targets: List<&2, Target>, +names: Translate.IdSet) -> List<&2, Target>:
  match targets:
    case Nil{}:
      Nil{}
    case Con{Target{+name, target}, rest}:
      keep_target(Translate.set_has(names, name), Target{name, target}, present_targets(rest, names))

def foundation_targets(+source: Export.T) -> List<&2, Target>:
  names = displays(Translate.set_keys(Report.constant_names(source)), source, Translate.empty_set())
  present_targets([
    Target{"Nat", "MechNat"},
    Target{"Nat.zero", "mechZero"},
    Target{"Nat.succ", "mechSucc"},
    Target{"Bool", "MechBool"},
    Target{"Bool.false", "mechFalse"},
    Target{"Bool.true", "mechTrue"},
    Target{"Unit", "MechUnit"},
    Target{"Unit.unit", "mechUnit"},
    Target{"Empty", "MechEmpty"},
    Target{"False", "MechFalse"},
    Target{"Sum", "MechSum"},
    Target{"Sum.inl", "mechInl"},
    Target{"Sum.inr", "mechInr"},
    Target{"Decidable", "MechDecidable"},
    Target{"Decidable.isFalse", "mechIsFalse"},
    Target{"Decidable.isTrue", "mechIsTrue"},
    Target{"Eq", "MechEq"},
    Target{"Eq.refl", "mechRefl"},
    Target{"Eq.rec", "mechJ"},
    Target{"Eq.ndrec", "mechTransport"},
    Target{"Eq.symm", "mechSymm"},
    Target{"Eq.trans", "mechTrans"},
    Target{"congrArg", "mechCongr"}
  ], names)

def target_text(target: Maybe<&2, String>) -> String:
  match target:
    case None{}:
      "-"
    case Some{target}:
      target

def map_lines(rows: List<&2, Row>) -> String:
  match rows:
    case Nil{}:
      ""
    case Con{Row{name, target, verdict}, rest}:
      name ++ "\t" ++ target_text(target) ++ "\t" ++ verdict_name(verdict) ++ "\n" ++ map_lines(rest)

def never_lines(rows: List<&2, ExceptionRow>) -> String:
  match rows:
    case Nil{}:
      ""
    case Con{ExceptionRow{name, ratification, milestone}, rest}:
      name ++ "\t" ++ ratification ++ "\t" ++ milestone ++ "\n" ++ never_lines(rest)

def map_tsv(inventory: T) -> String:
  match inventory:
    case Inventory{rows, never, declared, constants}:
      "lean_name\ttarget\tverdict\n" ++ map_lines(rows)

def never_tsv(inventory: T) -> String:
  match inventory:
    case Inventory{rows, never, declared, constants}:
      "lean_name\tratification\tmilestone\n" ++ never_lines(never)
