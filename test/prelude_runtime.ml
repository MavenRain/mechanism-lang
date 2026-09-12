open Mechanism_kernel

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let io = Mechanism_import.Io.attempt

let run source directory exports =
  let* text = io (fun () -> In_channel.with_open_bin source In_channel.input_all) in
  let* globals, rows = kernel (Mechanism_surface.Elab.check_in Global.initial text) in
  let* () = require "runtime fixture declares axioms"
    (Mechanism_surface.Elab.axiom_names rows = []) in
  let* erased = kernel (Erase.program globals rows) in
  List.fold_left (fun acc export -> let* () = acc in
    let* () = require "invalid export path" (Filename.basename export = export) in
    let* value = kernel (Eval.eval globals [] (Term.Global export)) in
    let* normal = kernel (Eval.whnf globals value) in
    let* literal = Value.as_lit normal |> Option.to_result ~none:("not a literal: " ^ export) in
    let* answer = match literal with
      | Literal.LInt n -> Ok (Bignum.to_string n)
      | Literal.LString _ -> Error ("not a natural number: " ^ export) in
    let* bytes = kernel (Mechanism_wasm.Emit.program Global.initial erased ~export) in
    let* () = io (fun () -> Out_channel.with_open_bin
      (Filename.concat directory (export ^ ".wasm"))
      (fun channel -> Out_channel.output_string channel bytes)) in
    Printf.printf "%s\t%s\n%!" export answer;
    Ok ()) (Ok ()) exports

let main arguments = match arguments with
  | source :: directory :: first :: rest -> run source directory (first :: rest)
  | [] | [_] | [_; _] -> Error "usage: prelude_runtime SOURCE DIRECTORY EXPORT..."

let () =
  let arguments = Array.to_list Sys.argv |> List.to_seq |> Seq.drop 1 |> List.of_seq in
  Result.fold (main arguments) ~ok:(fun () -> ())
    ~error:(fun message -> prerr_endline message; exit 1)
