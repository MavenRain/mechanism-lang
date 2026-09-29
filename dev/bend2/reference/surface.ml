open Mechanism_kernel
open Mechanism_surface

let check globals source =
  Elab.check_text globals source |> Result.fold
    ~ok:(fun rows -> print_endline (Elab.checked_form rows))
    ~error:(fun error -> prerr_endline (Error.to_string error); exit 1)

let () =
  match Array.to_list Sys.argv with
  | [_; source] -> check Global.initial source
  | [_; "--empty"; source] -> check Global.empty source
  | [] | [_] | [_; _; _] | _ :: _ :: _ :: _ :: _ -> exit 64
