open Mechanism_kernel
open Mechanism_surface

let () =
  match Array.to_list Sys.argv with
  | [_; source] ->
      Result.bind (Elab.check_in Global.initial source)
        (fun (globals, rows) -> Erase.program globals rows)
      |> Result.fold
        ~ok:(fun rows -> print_endline (Erase.print rows))
        ~error:(fun error -> prerr_endline (Error.to_string error); exit 1)
  | [] | [_] | _ :: _ :: _ :: _ -> exit 64
