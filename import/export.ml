type error = { line : int; message : string }
type metadata = {
  exporter_name : string; exporter_version : string;
  lean_githash : string; lean_version : string; format_version : string;
}
type counts = { lines : int; names : int; levels : int; exprs : int; declarations : int }

type t = {
  meta : metadata;
  totals : counts;
  names : (int, Names.t) Hashtbl.t;
  name_text : (int, string) Hashtbl.t;
  name_keys : (int, string) Hashtbl.t;
  canonical_names : (int, int) Hashtbl.t;
  levels : (int, Levels.t) Hashtbl.t;
  exprs : (int, Exprs.t) Hashtbl.t;
  expr_lines : (int, int) Hashtbl.t;
  expression_order : (int * Exprs.t) list;
  decls : (string, Decls.t) Hashtbl.t;
  declaration_order : Decls.t list;
  inductive_groups : Decls.group list;
}

type state = {
  names : (int, Names.t) Hashtbl.t;
  name_text : (int, string) Hashtbl.t;
  name_keys : (int, string) Hashtbl.t;
  canonical_names : (int, int) Hashtbl.t;
  interned_names : (string, int) Hashtbl.t;
  levels : (int, Levels.t) Hashtbl.t;
  exprs : (int, Exprs.t) Hashtbl.t;
  expr_lines : (int, int) Hashtbl.t;
  decls : (string, Decls.t) Hashtbl.t;
  mutable reversed_expressions : (int * Exprs.t) list;
  mutable reversed_declarations : Decls.t list;
  mutable reversed_groups : Decls.group list;
}

let ( let* ) = Result.bind

let metadata (export : t) = export.meta
let counts (export : t) = export.totals
let name (export : t) index = Hashtbl.find_opt export.names index
let names (export : t) =
  List.sort (fun (left, _) (right, _) -> Int.compare left right)
    (Hashtbl.fold (fun index node acc -> (index, node) :: acc) export.names [])
let name_string (export : t) index = Hashtbl.find_opt export.name_text index
let canonical_name (export : t) index = Hashtbl.find_opt export.canonical_names index
let level (export : t) index = Hashtbl.find_opt export.levels index
let expr (export : t) index = Hashtbl.find_opt export.exprs index
let expr_line (export : t) index = Hashtbl.find_opt export.expr_lines index
let expressions (export : t) = export.expression_order
let declaration (export : t) index =
  Option.bind (Hashtbl.find_opt export.name_keys index) (Hashtbl.find_opt export.decls)
let declarations (export : t) = export.declaration_order
let groups (export : t) = export.inductive_groups

let initial () =
  let state = {
    names = Hashtbl.create 16384; name_text = Hashtbl.create 16384;
    name_keys = Hashtbl.create 16384; canonical_names = Hashtbl.create 16384;
    interned_names = Hashtbl.create 16384; levels = Hashtbl.create 256;
    exprs = Hashtbl.create 131072; expr_lines = Hashtbl.create 131072;
    decls = Hashtbl.create 4096; reversed_expressions = [];
    reversed_declarations = []; reversed_groups = [];
  } in
  Hashtbl.add state.names 0 Names.Anonymous;
  Hashtbl.add state.name_text 0 "";
  Hashtbl.add state.name_keys 0 "";
  Hashtbl.add state.canonical_names 0 0;
  Hashtbl.add state.interned_names "" 0;
  Hashtbl.add state.levels 0 Levels.Zero;
  state

let parse_metadata value =
  let* fields = Ndjson.exact_object ["meta"] value in
  let* meta = Ndjson.field "meta" fields in
  let* fields = Ndjson.exact_object ["exporter"; "lean"; "format"] meta in
  let* exporter = Ndjson.field "exporter" fields in
  let* exporter = Ndjson.exact_object ["name"; "version"] exporter in
  let* exporter_name = Ndjson.string_field "name" exporter in
  let* exporter_version = Ndjson.string_field "version" exporter in
  let* lean = Ndjson.field "lean" fields in
  let* lean = Ndjson.exact_object ["githash"; "version"] lean in
  let* lean_githash = Ndjson.string_field "githash" lean in
  let* lean_version = Ndjson.string_field "version" lean in
  let* format = Ndjson.field "format" fields in
  let* format = Ndjson.exact_object ["version"] format in
  let* format_version = Ndjson.string_field "version" format in
  if format_version = "3.1.0" then
    Ok { exporter_name; exporter_version; lean_githash; lean_version; format_version }
  else Error ("unsupported export format " ^ format_version ^ ", expected 3.1.0")

let references table kind indices =
  let rec check = function
    | [] -> Ok ()
    | index :: rest ->
        if Hashtbl.mem table index then check rest
        else Error (Printf.sprintf "undefined or forward %s index %d" kind index)
  in
  check indices

let lookup table kind index =
  Option.to_result ~none:(Printf.sprintf "undefined %s index %d" kind index)
    (Hashtbl.find_opt table index)

let display_component text =
  let ordinary c =
    (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
    (c >= '0' && c <= '9') || c = '_' || c = '\''
  in
  let numeric = String.length text > 0 && String.for_all (fun c -> c >= '0' && c <= '9') text in
  if String.length text > 0 && String.for_all ordinary text && not numeric then text
  else "«" ^ Ndjson.to_string (Ndjson.String text) ^ "»"

let add_name state index tag value =
  if Hashtbl.mem state.names index then Error ("duplicate name index " ^ string_of_int index)
  else
    let* node = Names.parse tag value in
    let* () = references state.names "name" (Names.references node) in
    let* key, text =
      match node with
      | Names.Anonymous -> Ok ("", "")
      | Names.Str (pre, component) ->
          let* key = lookup state.name_keys "name" pre in
          let* prefix = lookup state.name_text "name" pre in
          let printed = display_component component in
          Ok (key ^ "s" ^ string_of_int (String.length component) ^ ":" ^ component,
            if prefix = "" then printed else prefix ^ "." ^ printed)
      | Names.Num (pre, number) ->
          let* key = lookup state.name_keys "name" pre in
          let* prefix = lookup state.name_text "name" pre in
          let printed = string_of_int number in
          Ok (key ^ "n" ^ printed ^ ":", if prefix = "" then printed else prefix ^ "." ^ printed)
    in
    let canonical = Option.value ~default:index (Hashtbl.find_opt state.interned_names key) in
    Hashtbl.replace state.interned_names key canonical;
    Hashtbl.add state.names index node;
    Hashtbl.add state.name_keys index key;
    Hashtbl.add state.name_text index text;
    Hashtbl.add state.canonical_names index canonical;
    Ok ()

let add_level state index tag value =
  if Hashtbl.mem state.levels index then Error ("duplicate level index " ^ string_of_int index)
  else
    let* node = Levels.parse tag value in
    let* () = references state.names "name" (Levels.name_references node) in
    let* () = references state.levels "level" (Levels.level_references node) in
    Hashtbl.add state.levels index node;
    Ok ()

let add_expr state line index tag value =
  if Hashtbl.mem state.exprs index then Error ("duplicate expression index " ^ string_of_int index)
  else
    let* node = Exprs.parse tag value in
    let* () = references state.names "name" (Exprs.name_references node) in
    let* () = references state.levels "level" (Exprs.level_references node) in
    let* () = references state.exprs "expression" (Exprs.expr_references node) in
    Hashtbl.add state.exprs index node;
    Hashtbl.add state.expr_lines index line;
    state.reversed_expressions <- (index, node) :: state.reversed_expressions;
    Ok ()

let add_declarations state line tag value =
  let* rows, group = Decls.parse ~line tag value in
  let add (declaration : Decls.t) =
    let* () = references state.names "name" (Decls.name_references declaration) in
    let* () = references state.exprs "expression" (Decls.expr_references declaration) in
    let* key = lookup state.name_keys "declaration name" declaration.name in
    if Hashtbl.mem state.decls key then
      let* text = lookup state.name_text "declaration name" declaration.name in
      Error ("duplicate declaration " ^ text)
    else (
      Hashtbl.add state.decls key declaration;
      state.reversed_declarations <- declaration :: state.reversed_declarations;
      Ok ())
  in
  let* _added = Ndjson.traverse add rows in
  Option.iter (fun group -> state.reversed_groups <- group :: state.reversed_groups) group;
  Ok ()

let indexed state line (key, index) (tag, value) =
  let* index = Ndjson.nat index in
  match () with
  | () when key = "in" -> add_name state index tag value
  | () when key = "il" -> add_level state index tag value
  | () when key = "ie" -> add_expr state line index tag value
  | () -> Error "expected name, level, or expression index"

let item state line value =
  let* fields = Ndjson.object_fields value in
  match fields with
  | [(tag, value)] ->
      if tag = "meta" then Error "duplicate metadata"
      else add_declarations state line tag value
  | [((key, _) as first); second] ->
      if key = "in" || key = "il" || key = "ie" then indexed state line first second
      else indexed state line second first
  | [] | _ :: _ :: _ :: _ -> Error "invalid export object shape"

let at_line line result = Result.map_error (fun message -> { line; message }) result

let finish state meta lines = {
  meta;
  totals = {
    lines; names = Hashtbl.length state.names - 1; levels = Hashtbl.length state.levels - 1;
    exprs = Hashtbl.length state.exprs; declarations = Hashtbl.length state.decls;
  };
  names = state.names; name_text = state.name_text; name_keys = state.name_keys;
  canonical_names = state.canonical_names; levels = state.levels;
  exprs = state.exprs; expr_lines = state.expr_lines; decls = state.decls;
  expression_order = List.rev state.reversed_expressions;
  declaration_order = List.rev state.reversed_declarations;
  inductive_groups = List.rev state.reversed_groups;
}

let read_lines next =
  let* first = Option.to_result ~none:{ line = 1; message = "missing initial metadata" } (next ()) in
  let* first = at_line 1 (Ndjson.parse first) in
  let* meta = at_line 1 (parse_metadata first) in
  let state = initial () in
  let rec go previous =
    let step text () =
      let line = previous + 1 in
      let* value = at_line line (Ndjson.parse text) in
      let* () = at_line line (item state line value) in
      go line
    in
    Option.fold ~none:(fun () -> Ok (finish state meta previous)) ~some:step (next ()) ()
  in
  go 1

let read_string source =
  let reversed = List.rev (String.split_on_char '\n' source) in
  let lines =
    match reversed with
    | "" :: rest -> List.rev rest
    | [] | _ :: _ -> List.rev reversed
  in
  let remaining = ref lines in
  read_lines (fun () ->
    match !remaining with
    | [] -> None
    | line :: rest -> remaining := rest; Some line)

let read_file path =
  let* parsed =
    Result.map_error (fun message -> { line = 0; message })
      (Io.attempt (fun () -> In_channel.with_open_bin path
        (fun channel -> read_lines (fun () -> In_channel.input_line channel))))
  in
  parsed
