open Mechanism_kernel
open Mechanism_surface

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let read root path = Mechanism_import.Io.attempt
  (fun () -> In_channel.with_open_bin (Filename.concat root path) In_channel.input_all)

let suite root =
  let* source = read root "prelude/cat/category.mech" in
  let* client = read root "test/fixtures/prelude/category.mech" in
  let* parsed = kernel (Parser.parse source) in
  let* printed = kernel (Parser.parse (Syntax.print parsed)) in
  let* () = require "category parse/print changed" (parsed = printed) in
  let* templates, template_rows = kernel (Elab.check_in Global.empty source) in
  let* () = require "symbolic category escaped"
    (Global.StringMap.is_empty templates.entries
      && Global.StringMap.is_empty templates.families && template_rows = []) in
  let* globals, rows = kernel (Elab.check_in Global.empty (source ^ client)) in
  let* () = require "unexpected axioms" (Elab.axiom_names rows = []) in
  let expected_families = ["Higher"; "IndexedValue"; "Point"; "Small";
    "Tiny"; "Types"; "Up"] in
  let* () = require "category inventory changed"
    (List.length rows = 121 && Global.StringMap.cardinal globals.entries = 121
      && List.map fst (Global.StringMap.bindings globals.families) = expected_families) in
  let* () = Global.StringMap.fold (fun name entry acc -> let* () = acc in
    match entry with
    | Global.Def _ -> Ok ()
    | Global.Axiom _ | Global.Prim _ -> Error ("trusted entry: " ^ name))
    globals.entries (Ok ()) in
  let zero = Term.In (Shape.SMu ("Tiny", []), Term.ACtor "zero", []) in
  let one = Term.In (Shape.SMu ("Tiny", []), Term.ACtor "next", [zero]) in
  let tiny = Term.Lan (Shape.SMu ("Tiny", []), Rules.diagram_of []) in
  let computations = ["identityValue", one; "compositionValue", zero;
    "reverseValue", one; "heterogeneousValue", one; "upValue", tiny;
    "higherValue", tiny; "capturedValue", one] in
  let* () = List.fold_left (fun acc (name, expected) -> let* () = acc in
    let* value = kernel (Eval.eval globals [] (Term.Global name)) in
    let* actual = kernel (Eval.quote globals 0 value) in
    require ("wrong computation: " ^ name ^ " = " ^ Pp.term [] actual)
      (actual = expected)) (Ok ()) computations in
  let quotations = ["capturedClosed"; "indexedClosed"; "indexedMotiveClosed"] in
  let* () = List.fold_left (fun acc name -> let* () = acc in
    let term = Term.Global name in
    let* expected = kernel (Check.infer (Check.make globals Budget.unlimited)
      Quantity.Zero term) in
    let* value = kernel (Eval.eval globals [] term) in
    let* quoted = kernel (Eval.quote globals 0 value) in
    let* () = kernel (Check.check_term globals quoted expected) in
    let* quoted_value = kernel (Eval.eval globals [] quoted) in
    let* same = kernel (Conv.conv Check.ops
      (Check.make globals Budget.unlimited) ~ty:expected value quoted_value) in
    require ("quotation changed " ^ name) same)
    (Ok ()) quotations in
  let negatives = ["unequal-endpoint"; "wrong-composition"; "wrong-law";
    "wrong-dependent-index"; "wrong-index-carrier"; "erased-endpoint";
    "nominal-equality"; "wrong-level-arity"] in
  (* The inventory is measured from the directory, so a new or a dropped
     negative cannot pass unnoticed through the literal list above. *)
  let* listed = Mechanism_import.Io.attempt
    (fun () -> Sys.readdir (Filename.concat root "test/neg/category")) in
  let present = Array.to_list listed
    |> List.filter (fun name -> Filename.check_suffix name ".mech")
    |> List.map Filename.remove_extension |> List.sort String.compare in
  let* () = require "category negative inventory changed"
    (present = List.sort String.compare negatives) in
  let* () = List.fold_left (fun acc name -> let* () = acc in
    let path = "test/neg/category/" ^ name in
    let* text = read root (path ^ ".mech") in
    let* expected = read root (path ^ ".err") in
    (* Template catalogs last for one source check. All other refusals
       use the already checked globals to isolate the invalid client. *)
    let result = if String.equal name "wrong-level-arity" then
      Elab.check_in Global.empty (source ^ text) else Elab.check_in globals text in
    Result.fold result ~ok:(fun _ -> Error (name ^ ": expected refusal"))
      ~error:(fun error -> let actual = Error.to_string error ^ "\n" in
        require (name ^ ": " ^ actual) (actual = expected))) (Ok ()) negatives in
  let* () = Result.fold
    (Elab.check_in ~budget:(Budget.of_poll (fun () -> true)) Global.empty source)
    ~ok:(fun _ -> Error "category budget: expected refusal")
    ~error:(fun error -> require "category budget: wrong refusal"
      (error = Error.Budget_exhausted Check.budget_msg)) in
  Ok (List.length rows, List.length computations, List.length negatives + 1,
    List.length quotations)

let () =
  let root = Array.to_list Sys.argv |> List.to_seq |> Seq.drop 1 |> Seq.uncons
    |> Option.map fst |> Option.value ~default:"." in
  Result.fold (suite root)
    ~ok:(fun (entries, computations, negatives, quotation) ->
      Printf.printf "PRELUDE-CATEGORY-OK entries=%d computations=%d negatives=%d quotation=%d\n"
        entries computations negatives quotation)
    ~error:(fun message -> Printf.printf "PRELUDE-CATEGORY-FAIL %s\n" message; exit 1)
