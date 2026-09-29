import Base
import ./ndjson.bend as Json
import ./export.bend as Export
import ./translate.bend as Translate
import ./lower.bend as Lower
import ./render.bend as Render
import ./table.bend as Table
import ./exprs.bend as Exprs
import ./decls.bend as Decls
import ./names.bend as Names
import ../kernel/global.bend as Global
import ../kernel/term.bend as Term
import ../kernel/pp.bend as Pp
import ../kernel/error.bend as KernelError

type Outcome is Data:
  Kernel{term: Term.T}
  Deferred{detail: String}
  Unsupported{detail: String}
  Failed{detail: String}

type Entry is Data:
  Entry{row: Translate.Row, outcome: Outcome}

type T is Data:
  Report{source: Export.T, types: Translate.T, entries: List<&2, Entry>}

type OutputFile is Data:
  OutputFile{name: String, contents: String}

def outcome(value: Result<&2, &2, Lower.Error, Term.T>) -> Outcome:
  match value:
    case Done{term}:
      Kernel{term}
    case Fail{Lower.Deferred{detail}}:
      Deferred{detail}
    case Fail{Lower.Unsupported{detail}}:
      Unsupported{detail}
    case Fail{Lower.KernelError{error}}:
      Failed{KernelError.to_string(error)}

def prepare_entries(rows: List<&2, Translate.Row>, +table: Translate.T) -> List<&2, Entry>:
  match rows:
    case Nil{}:
      Nil{}
    case Con{+row, rest}:
      Con{Entry{row, outcome(Lower.lower_default(Lower.missing("no checked prelude mapping for "), Global.initial(), table, row))},
        prepare_entries(rest, table)}

def prepare(+source: Export.T) -> Result<&2, &2, Export.Error, T>:
  do Result<&2, &2, Export.Error, T>:
    +table : Translate.T <- Translate.translate(source)
    Done{Report{source, table, prepare_entries(Translate.rows(table), table)}}

def namespace(+name: String, +prefix: String) -> Bool:
  Bool.or(String.eq(name, prefix), String.starts_with(name, prefix ++ "."))

def external_name(+name: String) -> Bool:
  Bool.not(Bool.or(namespace(name, "UnifiedAggregation"), Bool.or(namespace(name, "ArrowCat"), namespace(name, "CompCatTheory"))))

def canonical_constant(value: Maybe<&2, String>, names: Translate.IdSet) -> Translate.IdSet:
  match value:
    case None{}:
      names
    case Some{name}:
      Translate.set_add(names, name)

def constant(node: Exprs.T, source: Export.T, names: Translate.IdSet) -> Translate.IdSet:
  match node:
    case Exprs.Const{name, universes}:
      canonical_constant(Export.canonical_name(source, name), names)
    case other:
      names

def constants(rows: List<&2, Table.Indexed<Exprs.T>>, +source: Export.T, names: Translate.IdSet) -> Translate.IdSet:
  match rows:
    case Nil{}:
      names
    case Con{Table.Indexed{id, node}, rest}:
      constants(rest, source, constant(node, source, names))

def constant_names(+source: Export.T) -> Translate.IdSet:
  constants(Export.expressions(source), source, Translate.empty_set())

def status(outcome: Outcome) -> String:
  match outcome:
    case Kernel{term}:
      "KERNEL_TYPE"
    case Deferred{detail}:
      "DEFERRED"
    case Unsupported{detail}:
      "UNSUPPORTED"
    case Failed{detail}:
      "KERNEL_ERROR"

def kind(entry: Entry) -> String:
  match entry:
    case Entry{Translate.Row{Decls.Decl{name, params, typ, kind, line}, display, parameters, root, deps}, outcome}:
      Decls.kind_name(kind)

def entry_status(entry: Entry) -> String:
  match entry:
    case Entry{row, result}:
      status(result)

def increment(yes: Bool, n: Nat) -> Nat:
  match yes:
    case True{}:
      1n+n
    case False{}:
      n

def count_status(+wanted: String, entries: List<&2, Entry>) -> Nat:
  match entries:
    case Nil{}:
      0n
    case Con{entry, rest}:
      increment(String.eq(wanted, entry_status(entry)), count_status(wanted, rest))

def keep(yes: Bool, entry: Entry, rest: List<&2, Entry>) -> List<&2, Entry>:
  match yes:
    case True{}:
      Con{entry, rest}
    case False{}:
      rest

def filter_kind(+wanted: String, entries: List<&2, Entry>) -> List<&2, Entry>:
  match entries:
    case Nil{}:
      Nil{}
    case Con{+entry, rest}:
      keep(String.eq(wanted, kind(entry)), entry, filter_kind(wanted, rest))

def external_display(value: Maybe<&2, String>) -> Bool:
  match value:
    case None{}:
      False{}
    case Some{name}:
      external_name(name)

def count_referenced(ids: List<&2, String>, +source: Export.T) -> Nat:
  match ids:
    case Nil{}:
      0n
    case Con{id, rest}:
      increment(external_display(Export.name_string(source, id)), count_referenced(rest, source))

def count_declared(entries: List<&2, Entry>) -> Nat:
  match entries:
    case Nil{}:
      0n
    case Con{Entry{Translate.Row{decl, name, params, root, deps}, result}, rest}:
      increment(external_name(name), count_declared(rest))

def grammar(counts: Export.Counts) -> String:
  match counts:
    case Export.Counts{lines, names, levels, exprs, declarations}:
      "IMPORT-GRAMMAR lines=" ++ U32.show(lines) ++ " names=" ++ Nat.show(names) ++
        " levels=" ++ Nat.show(levels) ++ " expressions=" ++ Nat.show(exprs) ++ " format=3.1.0"

def declaration_count(counts: Export.Counts) -> Nat:
  match counts:
    case Export.Counts{lines, names, levels, exprs, declarations}:
      declarations

def kind_summary(+name: String, +entries: List<&2, Entry>) -> String:
  +entries = filter_kind(name, entries)
  +declared = Nat.show(List.length(&2, Entry, entries))
  String.concat(["PARITY-KIND ", name, " declared=", declared, " scoped_types=", declared,
    " kernel_types=", Nat.show(count_status("KERNEL_TYPE", entries)),
    " deferred=", Nat.show(count_status("DEFERRED", entries)),
    " unsupported=", Nat.show(count_status("UNSUPPORTED", entries)),
    " kernel_errors=", Nat.show(count_status("KERNEL_ERROR", entries)), " NEVER=0"])

def kind_summaries(kinds: List<&2, String>, +entries: List<&2, Entry>) -> List<&2, String>:
  match kinds:
    case Nil{}:
      ["PARITY-STATUS mapping=not-checked values=M1 NEVER=0 (no NEVER ledger applied)"]
    case Con{name, rest}:
      Con{kind_summary(name, entries), kind_summaries(rest, entries)}

def summary(report: T) -> List<&2, String>:
  match report:
    case Report{+source, types, +entries}:
      +ids = Translate.set_keys(constant_names(source))
      census = String.concat(["IMPORT-COUNTS declarations=", Nat.show(declaration_count(Export.counts(source))),
        " external_referenced=", Nat.show(count_referenced(ids, source)),
        " external_declared=", Nat.show(count_declared(entries)),
        " const_names=", Nat.show(List.length(&2, String, ids))])
      Con{grammar(Export.counts(source)), Con{census,
        kind_summaries(["axiom", "def", "thm", "opaque", "quot", "inductive", "constructor", "recursor"], entries)}}

def detail(outcome: Outcome) -> Json.Field<Json.T>:
  match outcome:
    case Kernel{term}:
      Json.Field{"kernel_debug", Json.Text{Pp.term([], term)}}
    case Deferred{reason}:
      Json.Field{"reason", Json.Text{reason}}
    case Unsupported{reason}:
      Json.Field{"reason", Json.Text{reason}}
    case Failed{reason}:
      Json.Field{"reason", Json.Text{reason}}

def entry_record(entry: Entry) -> Json.T:
  match entry:
    case Entry{Translate.Row{Decls.Decl{id, levels, typ, kind, line}, name, params, root, deps}, +outcome}:
      Json.Object{[Json.Field{"declaration", Json.Object{[
        Json.Field{"source_name", Json.Number{id}}, Json.Field{"name", Json.Text{name}},
        Json.Field{"kind", Json.Text{Decls.kind_name(kind)}}, Json.Field{"source_line", Json.Number{U32.show(line)}},
        Json.Field{"source_level_params", Render.numbers(levels)}, Json.Field{"parameters", Json.Items{Render.parameters(params)}},
        Json.Field{"type", Json.Number{root}}, Json.Field{"dependencies", Render.strings(deps)},
        Json.Field{"status", Json.Text{status(outcome)}}, detail(outcome)]}}]}

def entry_records(entries: List<&2, Entry>) -> List<&2, Json.T>:
  match entries:
    case Nil{}:
      Nil{}
    case Con{head, rest}:
      Con{entry_record(head), entry_records(rest)}

def name_records(names: List<&2, Table.Indexed<Names.T>>) -> List<&2, Json.T>:
  match names:
    case Nil{}:
      Nil{}
    case Con{head, rest}:
      Con{Render.name_record(head), name_records(rest)}

def json_lines(values: List<&2, Json.T>) -> String:
  match values:
    case Nil{}:
      ""
    case Con{head, rest}:
      Json.to_string(head) ++ "\n" ++ json_lines(rest)

def manifest(meta: Export.Metadata, count: Nat) -> Json.T:
  match meta:
    case Export.Metadata{name, version, hash, lean, format}:
      Json.Object{[Json.Field{"schema", Json.Text{"mechanism-types-0.1"}},
        Json.Field{"format_version", Json.Text{format}}, Json.Field{"lean_version", Json.Text{lean}},
        Json.Field{"lean_githash", Json.Text{hash}}, Json.Field{"exporter_name", Json.Text{name}},
        Json.Field{"exporter_version", Json.Text{version}}, Json.Field{"declarations", Json.Number{Nat.show(count)}},
        Json.Field{"mapping_checked", Json.Boolean{False{}}}, Json.Field{"values_translated", Json.Boolean{False{}}}]}

def files(report: T) -> List<&2, OutputFile>:
  match report:
    case Report{+source, +types, +entries}:
      +report : T = Report{source, types, entries}
      [OutputFile{"manifest.json", Json.to_string(manifest(Export.metadata(source), List.length(&2, Entry, entries))) ++ "\n"},
       OutputFile{"types.ndjson", json_lines(name_records(Export.names(source))) ++
         json_lines(Render.level_records(Translate.levels(types))) ++
         json_lines(Render.node_records(Translate.nodes(types))) ++ json_lines(entry_records(entries))},
       OutputFile{"summary.txt", String.join(summary(report), "\n") ++ "\n"}]
