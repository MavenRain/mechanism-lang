(** Check a mechanism certificate and optionally export exact natural values.
    No Wasm emission or test-only helper is needed at this boundary. *)
open Mechanism_kernel

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let io = Mechanism_import.Io.attempt

let check source exports =
  let* text = io (fun () -> In_channel.with_open_bin source In_channel.input_all) in
  let* globals, entries = kernel (Mechanism_surface.Elab.check_in Global.initial text) in
  let* () = match Mechanism_surface.Elab.axiom_names entries with
    | [] -> Ok ()
    | names -> Error ("certificate declares axioms: " ^ String.concat ", " names) in
  let* values = List.fold_left (fun result name ->
    let* accumulated = result in
    let* value = kernel (Eval.eval globals [] (Term.Global name)) in
    let* normal = kernel (Eval.whnf globals value) in
    let* literal = Value.as_lit normal |> Option.to_result ~none:("not a literal: " ^ name) in
    match literal with
    | Literal.LInt n -> Ok ((name, Bignum.to_string n) :: accumulated)
    | Literal.LString _ -> Error ("not a natural number: " ^ name)) (Ok []) exports in
  let* () = io (fun () ->
    print_endline "CHECKED axioms=0";
    List.iter (fun (name, value) -> Printf.printf "%s\t%s\n" name value) (List.rev values)) in
  Ok ()

let main arguments = match arguments with
  | source :: exports -> check source exports
  | [] -> Error "usage: mech_cert SOURCE [NAT_EXPORT ...]"

let () =
  let arguments = Array.to_list Sys.argv |> List.to_seq |> Seq.drop 1 |> List.of_seq in
  Result.fold (main arguments) ~ok:(fun () -> ())
    ~error:(fun message -> prerr_endline message; exit 1)
