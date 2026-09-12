open Mechanism_kernel
open Mechanism_surface

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let read root path = Mechanism_import.Io.attempt
  (fun () -> In_channel.with_open_bin (Filename.concat root path) In_channel.input_all)

let suite root =
  let* source = read root "prelude/cat/category.mech" in
  let* client = read root "test/fixtures/prelude/nattrans.mech" in
  let* parsed = kernel (Parser.parse (source ^ client)) in
  let* printed = kernel (Parser.parse (Syntax.print parsed)) in
  let* () = require "nattrans parse/print changed" (parsed = printed) in
  let* templates, template_rows = kernel (Elab.check_in Global.empty source) in
  let* () = require "symbolic nattrans escaped"
    (Global.StringMap.is_empty templates.entries
      && Global.StringMap.is_empty templates.families && template_rows = []) in
  let* globals, rows = kernel (Elab.check_in Global.empty (source ^ client)) in
  let* () = require "unexpected axioms" (Elab.axiom_names rows = []) in
  let* () = Global.StringMap.fold (fun name entry acc -> let* () = acc in
    match entry with
    | Global.Def _ -> Ok ()
    | Global.Axiom _ | Global.Prim _ -> Error ("trusted entry: " ^ name))
    globals.entries (Ok ()) in
  let* () = Global.StringMap.fold (fun name (family : Positivity.family) acc ->
    let* () = acc in
    match family.f_status with
    | Positivity.Complete _ -> require ("unchecked family: " ^ name) family.f_positive
    | Positivity.Provisional | Positivity.Builtin -> Error ("unexpected family: " ^ name))
    globals.families (Ok ()) in
  let instances = ["Small"; "Types"; "Up"; "Higher"] in
  let expected_families = ["Higher"; "Path"; "Point"; "Small"; "Tiny"; "Types"; "Up"] in
  let installed = Global.StringMap.bindings globals.families |> List.map fst in
  let* () = require "nattrans instance inventory changed"
    (installed = expected_families) in
  let* () = require (Printf.sprintf "nattrans entry inventory changed: %d" (List.length rows))
    (List.length rows = 123 && Global.StringMap.cardinal globals.entries = 123) in
  let members = ["eqSymm"; "NatTrans"; "natApp"; "naturality";
    "idNat"; "vcompLaw"; "vcomp"; "whiskerRight"; "whiskerLeft"] in
  let* () = List.fold_left (fun acc instance -> let* () = acc in
    List.fold_left (fun acc member -> let* () = acc in
      require ("missing nattrans member: " ^ instance ^ "_" ^ member)
        (Option.is_some (Global.find (instance ^ "_" ^ member) globals)))
      (Ok ()) members) (Ok ()) instances in
  let zero = Term.In (Shape.SMu ("Tiny", []), Term.ACtor "zero", []) in
  let one = Term.In (Shape.SMu ("Tiny", []), Term.ACtor "next", [zero]) in
  let two = Term.In (Shape.SMu ("Tiny", []), Term.ACtor "next", [one]) in
  let computations = ["componentValue", one; "identityValue", one;
    "verticalForward", one; "verticalReverse", zero; "verticalNested", two;
    "rightValue", one; "leftValue", one; "leftOther", zero] in
  let* () = List.fold_left (fun acc (name, expected) -> let* () = acc in
    let* value = kernel (Eval.eval globals [] (Term.Global name)) in
    let* actual = kernel (Eval.quote globals 0 value) in
    require ("wrong computation: " ^ name ^ " = " ^ Pp.term [] actual)
      (actual = expected)) (Ok ()) computations in
  let negatives = ["wrong-endpoint"; "wrong-naturality"; "missing-law";
    "erased-object"; "wrong-middle"; "wrong-whisker-endpoint"] in
  let* listed = Mechanism_import.Io.attempt
    (fun () -> Sys.readdir (Filename.concat root "test/neg/nattrans")) in
  let present = Array.to_list listed
    |> List.filter (fun name -> Filename.check_suffix name ".mech")
    |> List.map Filename.remove_extension |> List.sort String.compare in
  let* () = require "nattrans negative inventory changed"
    (present = List.sort String.compare negatives) in
  let* () = List.fold_left (fun acc name -> let* () = acc in
    let path = "test/neg/nattrans/" ^ name in
    let* text = read root (path ^ ".mech") in
    let* expected = read root (path ^ ".err") in
    let result = Elab.check_in globals text in
    Result.fold result ~ok:(fun _ -> Error (name ^ ": expected refusal"))
      ~error:(fun error -> let actual = Error.to_string error ^ "\n" in
        require (name ^ ": " ^ actual) (actual = expected))) (Ok ()) negatives in
  let* () = Result.fold
    (Elab.check_in ~budget:(Budget.of_poll (fun () -> true)) Global.empty source)
    ~ok:(fun _ -> Error "nattrans budget: expected refusal")
    ~error:(fun error -> require "nattrans budget: wrong refusal"
      (error = Error.Budget_exhausted Check.budget_msg)) in
  Ok (List.length rows, List.length instances, List.length computations,
    List.length negatives + 1)

let () =
  let root = Array.to_list Sys.argv |> List.to_seq |> Seq.drop 1 |> Seq.uncons
    |> Option.map fst |> Option.value ~default:"." in
  Result.fold (suite root)
    ~ok:(fun (entries, instances, computations, negatives) ->
      Printf.printf "PRELUDE-NATTRANS-OK entries=%d instances=%d computations=%d negatives=%d\n"
        entries instances computations negatives)
    ~error:(fun message -> Printf.printf "PRELUDE-NATTRANS-FAIL %s\n" message; exit 1)
