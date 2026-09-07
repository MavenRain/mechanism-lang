open Mechanism_kernel
module Names_set = Set.Make (Int)

type outcome = Kernel of Term.t | Deferred of string | Unsupported of string | Failed of string
type entry = { row : Translate.row; outcome : outcome }
type t = { source : Export.t; types : Translate.t; entries : entry list }

let prepare source =
  Translate.translate source |> Result.map (fun types ->
      let entries = Translate.rows types |> List.map (fun row ->
          let resolve ~name ~levels:_ = Error ("no checked prelude mapping for " ^ name) in
          let outcome = Translate.lower_type ~resolve Global.initial types row
            |> Result.fold ~ok:(fun term -> Kernel term) ~error:(function
                | Translate.Deferred detail -> Deferred detail
                | Translate.Unsupported detail -> Unsupported detail
                | Translate.Kernel_error error -> Failed (Error.to_string error)) in
          { row; outcome }) in
      { source; types; entries })

let external_name name =
  not (List.exists (fun prefix -> String.equal name prefix
        || String.starts_with ~prefix:(prefix ^ ".") name)
      [ "UnifiedAggregation"; "ArrowCat"; "CompCatTheory" ])

let constant_names source =
  Export.expressions source |> List.fold_left (fun found (_id, expr) ->
      match expr with
      | Exprs.Const { name; universes = _ } ->
          Export.canonical_name source name
          |> Option.fold ~none:found ~some:(fun name -> Names_set.add name found)
      | Exprs.Bvar _ | Exprs.Sort _ | Exprs.App _ | Exprs.Lam _
      | Exprs.Forall _ | Exprs.Let _ | Exprs.Proj _ | Exprs.Nat _
      | Exprs.String _ | Exprs.Mdata _ -> found) Names_set.empty

let kind entry = Decls.kind_name entry.row.declaration.kind
let kinds = [ "axiom"; "def"; "thm"; "opaque"; "quot"; "inductive"; "constructor"; "recursor" ]
let status = function
  | Kernel _ -> "KERNEL_TYPE" | Deferred _ -> "DEFERRED"
  | Unsupported _ -> "UNSUPPORTED" | Failed _ -> "KERNEL_ERROR"
let count_status wanted entries =
  List.fold_left (fun n entry -> if String.equal (status entry.outcome) wanted then n + 1 else n) 0 entries

let summary report =
  let counts = Export.counts report.source in
  let names = constant_names report.source in
  let external_referenced = Names_set.cardinal (Names_set.filter (fun id ->
      Export.name_string report.source id |> Option.fold ~none:false ~some:external_name) names) in
  let external_declared = List.fold_left (fun n entry ->
      if external_name entry.row.name then n + 1 else n) 0 report.entries in
  let header = Printf.sprintf
      "IMPORT-GRAMMAR lines=%d names=%d levels=%d expressions=%d format=3.1.0"
      counts.lines counts.names counts.levels counts.exprs in
  let census = Printf.sprintf
      "IMPORT-COUNTS declarations=%d external_referenced=%d external_declared=%d const_names=%d"
      counts.declarations external_referenced external_declared (Names_set.cardinal names) in
  let rows = List.map (fun k ->
      let entries = List.filter (fun entry -> String.equal (kind entry) k) report.entries in
      Printf.sprintf "PARITY-KIND %s declared=%d scoped_types=%d kernel_types=%d deferred=%d unsupported=%d kernel_errors=%d NEVER=0"
        k (List.length entries) (List.length entries)
        (count_status "KERNEL_TYPE" entries) (count_status "DEFERRED" entries)
        (count_status "UNSUPPORTED" entries) (count_status "KERNEL_ERROR" entries)) kinds in
  header :: census :: rows @
  [ "PARITY-STATUS mapping=not-checked values=M1 NEVER=0 (no NEVER ledger applied)" ]

let number n = Ndjson.Number (string_of_int n)
let string s = Ndjson.String s
let object_ fields = Ndjson.Object fields
let array f xs = Ndjson.Array (List.map f xs)
let pair a b = array number [ a; b ]
let name_record (id, name) =
  let field = match name with
    | Names.Anonymous -> "anonymous", Ndjson.Bool true
    | Names.Str (pre, text) -> "str", object_ [ "pre", number pre; "str", string text ]
    | Names.Num (pre, n) -> "num", object_ [ "pre", number pre; "i", number n ] in
  object_ [ "name", number id; field ]
let binder_info = function
  | Exprs.Default -> "default" | Exprs.Implicit -> "implicit"
  | Exprs.Strict_implicit -> "strictImplicit" | Exprs.Inst_implicit -> "instImplicit"

let level_record (id, level) =
  let fields = match level with
    | Translate.Zero -> [ "zero", Ndjson.Bool true ]
    | Translate.Succ a -> [ "succ", number a ]
    | Translate.Max (a, b) -> [ "max", pair a b ]
    | Translate.Imax (a, b) -> [ "imax", pair a b ]
    | Translate.Param { source_name; name } ->
        [ "param", object_ [ "source_name", number source_name; "name", string name ] ] in
  object_ (("level", number id) :: fields)

let binder (b : Translate.binder) = object_
    [ "name", string b.name; "type", number b.typ; "body", number b.body;
      "binderInfo", string (binder_info b.info) ]

let node_record (id, node) =
  let field = match node with
    | Translate.Bound n -> "bvar", number n
    | Translate.Sort n -> "sort", number n
    | Translate.Constant { source_name; name; universes } ->
        "const", object_ [ "source_name", number source_name; "name", string name;
                           "universes", array number universes ]
    | Translate.Apply (fn, arg) -> "app", object_ [ "fn", number fn; "arg", number arg ]
    | Translate.Lambda b -> "lam", binder b
    | Translate.Pi b -> "forallE", binder b
    | Translate.Let { name; typ; value; body; nondep } -> "letE", object_
        [ "name", string name; "type", number typ; "value", number value;
          "body", number body; "nondep", Ndjson.Bool nondep ]
    | Translate.Projection { source_type_name; type_name; index; structure } -> "proj", object_
        [ "source_type_name", number source_type_name; "typeName", string type_name;
          "idx", number index; "struct", number structure ]
    | Translate.Nat n -> "natVal", string n
    | Translate.String s -> "strVal", string s
    | Translate.Metadata { expr; data } ->
        "mdata", object_ [ "expr", number expr; "data", object_ data ] in
  object_ [ "expr", number id; field ]

let entry_record entry =
  let row = entry.row in
  let detail = match entry.outcome with
    | Kernel term -> [ "kernel_debug", string (Pp.term [] term) ]
    | Deferred reason | Unsupported reason | Failed reason -> [ "reason", string reason ] in
  object_ [ "declaration", object_
      ([ "source_name", number row.declaration.name;
         "name", string row.name; "kind", string (kind entry);
         "source_line", number row.declaration.line;
         "source_level_params", array number row.declaration.level_params;
         "parameters", array (fun (id, name) -> object_
             [ "source_name", number id; "name", string name ]) row.parameters;
         "type", number row.root; "dependencies", array string row.dependencies;
         "status", string (status entry.outcome) ] @ detail) ]

let write ~directory report =
  let write_json channel value =
    output_string channel (Ndjson.to_string value); output_char channel '\n' in
  let metadata = Export.metadata report.source in
  let manifest = object_
      [ "schema", string "mechanism-types-0.1";
        "format_version", string metadata.format_version;
        "lean_version", string metadata.lean_version;
        "lean_githash", string metadata.lean_githash;
        "exporter_name", string metadata.exporter_name;
        "exporter_version", string metadata.exporter_version;
        "declarations", number (List.length report.entries);
        "mapping_checked", Ndjson.Bool false;
        "values_translated", Ndjson.Bool false ] in
  Io.publish ~directory
    [ "manifest.json", (fun channel -> write_json channel manifest);
      "types.ndjson", (fun channel ->
          List.iter (fun name -> write_json channel (name_record name)) (Export.names report.source);
          List.iter (fun level -> write_json channel (level_record level)) (Translate.levels report.types);
          List.iter (fun node -> write_json channel (node_record node)) (Translate.nodes report.types);
          List.iter (fun entry -> write_json channel (entry_record entry)) report.entries);
      "summary.txt", (fun channel ->
          List.iter (fun line -> output_string channel line; output_char channel '\n') (summary report)) ]
