open Mechanism_import
let () =
  match Array.to_list Sys.argv with
  | [_program; source] -> Ndjson.parse source |> Result.fold
      ~ok:(fun value -> print_endline (Ndjson.to_string value))
      ~error:(fun message -> prerr_endline message; exit 1)
  | [] | [_] | _ :: _ :: _ :: _ -> exit 64
