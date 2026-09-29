open Mechanism_import
open Mechanism_kernel
let num n = Ndjson.Number (string_of_int n)
let text s = Ndjson.String s
let strings xs = Ndjson.Array (List.map text xs)
let emit value = print_endline (Ndjson.to_string value)
let import_error (e : Export.error) =
  prerr_endline (string_of_int e.line ^ ":" ^ e.message); exit 1
let text_error message = prerr_endline message; exit 1
let report source = Report.prepare source |> Result.fold ~error:import_error ~ok:(fun value ->
  emit (Ndjson.Array [strings (Report.summary value);
    Ndjson.Array (List.map Report.entry_record value.Report.entries)]))
let target = function
  | Ndjson.Array [Ndjson.String name; Ndjson.String target] -> Ok (name, target)
  | Ndjson.Array _ | Ndjson.String _ | Ndjson.Number _ | Ndjson.Object _
  | Ndjson.Bool _ | Ndjson.Null -> Error "expected target pair"
let targets value = Result.bind (Ndjson.parse value) (function
  | Ndjson.Array values ->
      List.fold_left (fun acc value -> Result.bind acc (fun xs ->
        Result.map (fun x -> x :: xs) (target value))) (Ok []) values |> Result.map List.rev
  | Ndjson.String _ | Ndjson.Number _ | Ndjson.Object _ | Ndjson.Bool _ | Ndjson.Null ->
      Error "expected targets array")
let mapping source target_rows =
  Mapping.inventory ~targets:target_rows source |> Result.fold ~error:text_error ~ok:(fun m ->
    emit (Ndjson.Array [text (Mapping.map_tsv m); text (Mapping.never_tsv m);
      num m.Mapping.external_declared; num m.Mapping.const_names]))
let lower mode source =
  Translate.translate source |> Result.fold ~error:import_error ~ok:(fun table ->
    let selected = List.find_opt (fun (r : Translate.row) -> r.name = "test") (Translate.rows table) in
    Option.fold ~none:(fun () -> text_error "missing test declaration")
      ~some:(fun row () ->
        let calls = ref 0 in
        let resolve ~name ~levels =
          incr calls;
          match mode with
          | "lower-univ" -> Ok (Term.Univ Level.zero)
          | "lower-var" -> Ok (Term.Var 0)
          | "lower-global" -> Ok (Term.Global "uninstalled")
          | "lower-nat" -> Ok Prim.nat_ty
          | "lower-probe" ->
              let u = Option.value ~default:Level.zero (Level.var 1) in
              let v = Option.value ~default:Level.zero (Level.var 0) in
              if !calls = 1 && name = "F" &&
                List.equal Level.equal levels [Level.succ (Level.imax u v); Level.max u v]
              then Ok (Term.Univ Level.zero)
              else Error "wrong source reference or universe substitution"
          | _ -> Error ("no checked mapping for " ^ name) in
        let polls = ref 0 in
        let limit = match mode with "lower-zero" -> 0 | "lower-one" -> 1 | _ -> 100000 in
        let budget = Budget.of_poll (fun () -> incr polls; !polls > limit) in
        let result = Translate.lower_type ~budget ~resolve Global.initial table row in
        let status, message = Result.fold
          ~ok:(fun term -> "KERNEL_TYPE", Pp.term [] term)
          ~error:(function
            | Translate.Deferred message -> "DEFERRED", message
            | Translate.Unsupported message -> "UNSUPPORTED", message
            | Translate.Kernel_error error -> "KERNEL_ERROR", Error.to_string error) result in
        emit (strings [status; message])) selected ())
let () =
  match Array.to_list Sys.argv with
  | [_program; mode; source; target_text] ->
      Export.read_string source |> Result.fold ~error:import_error ~ok:(fun source ->
        match mode with
        | "report" -> report source
        | "mapping" -> targets target_text |> Result.fold ~error:text_error ~ok:(mapping source)
        | "mapping-foundation" -> mapping source (Mapping.foundation_targets source)
        | _ -> lower mode source)
  | [] | [_] | [_; _] | [_; _; _] | _ :: _ :: _ :: _ :: _ :: _ -> exit 64
