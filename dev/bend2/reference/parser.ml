open Mechanism_surface
let () =
  match Array.to_list Sys.argv with
  | [_program; source] -> Parser.parse source |> Result.fold
      ~ok:(fun _decls -> print_endline "OK")
      ~error:(fun error -> prerr_endline (Kanon_kernel.Error.to_string error); exit 1)
  | [] | [_] | _ :: _ :: _ :: _ -> exit 64
