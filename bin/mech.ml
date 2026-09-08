let usage () =
  prerr_endline "usage: mech import FILE [--out DIR] | diff-parity --export FILE";
  prerr_endline "       mech map-inventory --export FILE [--never]";
  prerr_endline "       mech check|axioms|emit|run|spec-count (inherited arguments)"

let imported path =
  let open Mechanism_import in
  Result.bind (Export.read_file path) Report.prepare
  |> Result.fold ~ok:Fun.id ~error:(fun (error : Export.error) ->
      prerr_endline (Printf.sprintf "mech: %s:%d: %s" path error.line error.message);
      exit 1)

let show report = List.iter print_endline (Mechanism_import.Report.summary report)

let import path output =
  let report = imported path in
  Option.iter (fun directory ->
      Mechanism_import.Report.write ~directory report
      |> Result.iter_error (fun message -> prerr_endline ("mech: " ^ message); exit 1)) output;
  show report

let map_inventory path never =
  let open Mechanism_import in
  let source = Export.read_file path
    |> Result.map_error (fun (error : Export.error) ->
        Printf.sprintf "%s:%d: %s" path error.line error.message) in
  let result = Result.bind source (fun source ->
        Mapping.inventory ~targets:(Mapping.foundation_targets source) source) in
  Result.fold result
    ~ok:(fun inventory -> print_string
        (if never then Mapping.never_tsv inventory else Mapping.map_tsv inventory))
    ~error:(fun message -> prerr_endline ("mech: " ^ message); exit 1)

let () =
  match Array.to_list Sys.argv with
  | [] | [ _ ] -> usage (); exit 64
  | _prog :: "import" :: [ path ] -> import path None
  | _prog :: "import" :: [ path; "--out"; directory ] -> import path (Some directory)
  | _prog :: "diff-parity" :: [ "--export"; path ] -> show (imported path)
  | _prog :: "map-inventory" :: [ "--export"; path ] -> map_inventory path false
  | _prog :: "map-inventory" :: [ "--export"; path; "--never" ] -> map_inventory path true
  | _prog :: ("import" | "diff-parity" | "map-inventory") :: _args -> usage (); exit 64
  | _prog :: cmd :: args -> Kanon.dispatch cmd args
