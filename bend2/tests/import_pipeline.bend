import Base
import ../import/ndjson.bend as Json
import ../import/export.bend as Export
import ../import/translate.bend as Translate
import ../import/render.bend as Render
import ../import/lower.bend as Lower
import ../import/report.bend as Report
import ../import/mapping.bend as Mapping
import ../kernel/budget.bend as Budget
import ../kernel/term.bend as Term
import ../kernel/level.bend as Level
import ../kernel/pp.bend as Pp
import ../kernel/prim.bend as Prim
import ../kernel/global.bend as Global

def print_json(value: Json.T) -> IO(Unit):
  IO.print(Json.to_string(value))

def export_error(error: Export.Error) -> IO(Unit):
  match error:
    case Export.ImportError{line, message}:
      IO.die(Unit, 1, U32.show(line) ++ ":" ++ message)

def report_result(result: Result<&2, &2, Export.Error, Report.T>) -> IO(Unit):
  match result:
    case Fail{error}:
      export_error(error)
    case Done{Report.Report{+source, +types, +entries}}:
      print_json(Json.Items{[Render.strings(Report.summary(Report.Report{source, types, entries})),
        Json.Items{Report.entry_records(entries)}]})

def mapping_summary(+inventory: Mapping.T) -> Json.T:
  match inventory:
    case Mapping.Inventory{+rows, +never, +declared, +constants}:
      +value : Mapping.T = Mapping.Inventory{rows, never, declared, constants}
      Json.Items{[Json.Text{Mapping.map_tsv(value)}, Json.Text{Mapping.never_tsv(value)},
        Json.Number{Nat.show(declared)}, Json.Number{Nat.show(constants)}]}

def mapping_result(result: Result<&2, &2, String, Mapping.T>) -> IO(Unit):
  match result:
    case Fail{message}:
      IO.die(Unit, 1, message)
    case Done{value}:
      print_json(mapping_summary(value))

def target(value: Json.T) -> Result<&2, &2, String, Mapping.Target>:
  match value:
    case Json.Items{Con{Json.Text{name}, Con{Json.Text{target}, Nil{}}}}:
      Done{Mapping.Target{name, target}}
    case other:
      Fail{"expected target pair"}

def target_rows(items: List<&2, Json.T>) -> Result<&2, &2, String, List<&2, Mapping.Target>>:
  match items:
    case Nil{}:
      Done{Nil{}}
    case Con{head, rest}:
      do Result<&2, &2, String, List<&2, Mapping.Target>>:
        head : Mapping.Target <- target(head)
        rest : List<&2, Mapping.Target> <- target_rows(rest)
        Done{Con{head, rest}}

def targets(value: Json.T) -> Result<&2, &2, String, List<&2, Mapping.Target>>:
  match value:
    case Json.Items{items}:
      target_rows(items)
    case other:
      Fail{"expected targets array"}

def parsed_targets(result: Result<&2, &2, String, Json.T>, source: Export.T) -> IO(Unit):
  match result:
    case Fail{message}:
      IO.die(Unit, 1, message)
    case Done{value}:
      mapping_result(do Result<&2, &2, String, Mapping.T>:
        targets : List<&2, Mapping.Target> <- targets(value)
        Mapping.inventory(source, targets))

def probe_result(valid: Bool) -> Lower.Resolver & Result<&2, &2, String, Term.T>:
  match valid:
    case False{}:
      (Lower.missing("extra resolution: "), Fail{"wrong source reference or universe substitution"})
    case True{}:
      (Lower.missing("extra resolution: "), Done{Term.Univ{Level.zero()}})

def probe_call(name: String, levels: List<&2, Level.T>) -> Lower.Resolver & Result<&2, &2, String, Term.T>:
  match levels:
    case Con{a, Con{b, Nil{}}}:
      +u = Level.var(1n)
      +v = Level.var(0n)
      probe_result(Bool.and(String.eq(name, "F"), Bool.and(
        Level.equal(a, Level.succ(Level.imax(u, v))), Level.equal(b, Level.max(u, v)))))
    case other:
      probe_result(False{})

def resolver(mode: String) -> Lower.Resolver:
  match mode:
    case "lower-univ":
      Lower.fixed(Term.Univ{Level.zero()})
    case "lower-var":
      Lower.fixed(Term.Var{0n})
    case "lower-global":
      Lower.fixed(Term.Global{"uninstalled"})
    case "lower-nat":
      Lower.fixed(Prim.nat_ty())
    case "lower-probe":
      Lower.Resolver{probe_call}
    case other:
      Lower.missing("no checked mapping for ")

def row_name(row: Translate.Row) -> String:
  match row:
    case Translate.Row{declaration, name, params, root, deps}:
      name

def choose_row(yes: Bool, row: Translate.Row, rest: Maybe<&2, Translate.Row>) -> Maybe<&2, Translate.Row>:
  match yes:
    case True{}:
      Some{row}
    case False{}:
      rest

def test_row(rows: List<&2, Translate.Row>) -> Maybe<&2, Translate.Row>:
  match rows:
    case Nil{}:
      None{}
    case Con{+row, rest}:
      choose_row(String.eq(row_name(row), "test"), row, test_row(rest))

def lower_result(result: Result<&2, &2, Lower.Error, Term.T>) -> IO(Unit):
  match result:
    case Done{term}:
      print_json(Json.Items{[Json.Text{"KERNEL_TYPE"}, Json.Text{Pp.term([], term)}]})
    case Fail{Lower.Deferred{message}}:
      print_json(Json.Items{[Json.Text{"DEFERRED"}, Json.Text{message}]})
    case Fail{Lower.Unsupported{message}}:
      print_json(Json.Items{[Json.Text{"UNSUPPORTED"}, Json.Text{message}]})
    case Fail{Lower.KernelError{error}}:
      print_json(Json.Items{[Json.Text{"KERNEL_ERROR"}, Json.Text{Lower.message(Lower.KernelError{error})}]})

def budget(mode: String) -> Budget.T:
  match mode:
    case "lower-zero":
      Budget.of_polls(0n)
    case "lower-one":
      Budget.of_polls(1n)
    case other:
      Budget.of_polls(100000n)

def lower_row(row: Maybe<&2, Translate.Row>, table: Translate.T, +mode: String) -> IO(Unit):
  match row:
    case None{}:
      IO.die(Unit, 1, "missing test declaration")
    case Some{row}:
      lower_result(Lower.lower_type(resolver(mode), Global.initial(), table, row, budget(mode)))

def translated(result: Result<&2, &2, Export.Error, Translate.T>, mode: String) -> IO(Unit):
  match result:
    case Fail{error}:
      export_error(error)
    case Done{+table}:
      lower_row(test_row(Translate.rows(table)), table, mode)

def dispatch(mode: String, +source: Export.T, target_text: String) -> IO(Unit):
  match mode:
    case "report":
      report_result(Report.prepare(source))
    case "mapping":
      parsed_targets(Json.parse(target_text), source)
    case "mapping-foundation":
      mapping_result(Mapping.inventory(source, Mapping.foundation_targets(source)))
    case other:
      translated(Translate.translate(source), other)

def parsed(result: Result<&2, &2, Export.Error, Export.T>, mode: String, targets: String) -> IO(Unit):
  match result:
    case Fail{error}:
      export_error(error)
    case Done{source}:
      dispatch(mode, source, targets)

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Con{mode, Con{source, Con{targets, Nil{}}}}:
      parsed(Export.read_string(source), mode, targets)
    case other:
      IO.die(Unit, 64, "usage: import_pipeline MODE NDJSON TARGETS")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
