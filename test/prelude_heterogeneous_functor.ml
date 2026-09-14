open Mechanism_kernel
open Mechanism_surface

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let read root path = Mechanism_import.Io.attempt
  (fun () -> In_channel.with_open_bin (Filename.concat root path) In_channel.input_all)
let rec prefix pattern chars = match pattern, chars with
  | [], [] -> true
  | [], _ :: _ -> true
  | _ :: _, [] -> false
  | head :: rest, char :: tail -> Char.equal head char && prefix rest tail
let rec scan pattern chars = prefix pattern chars || (match chars with
  | [] -> false
  | _ :: tail -> scan pattern tail)
let contains haystack needle =
  scan (List.of_seq (String.to_seq needle)) (List.of_seq (String.to_seq haystack))
let rename_core text = String.split_on_char '\n' text
  |> List.map (fun line -> String.split_on_char ' ' line
    |> List.map (fun word ->
      if String.equal word "MechCategoryCore" then "MechCategory" else word)
    |> String.concat " ")
  |> String.concat "\n"
let first_lines count text = String.split_on_char '\n' text
  |> List.filteri (fun index _ -> index < count) |> String.concat "\n"
let untrusted name entry = match entry with
  | Global.Def _ -> Ok ()
  | Global.Axiom _ | Global.Prim _ -> Error ("trusted entry: " ^ name)

let suite root =
  let* core = read root "prelude/cat/category-core.mech" in
  let* functor_source = read root "prelude/cat/heterogeneous-functor.mech" in
  let* category = read root "prelude/cat/category.mech" in
  let* () = require "category-core mirror changed"
    (String.equal (first_lines 70 (rename_core core)) (first_lines 70 category)) in
  let source = core ^ functor_source in
  let* generic = read root "test/fixtures/prelude/heterogeneous-functor.mech" in
  let* runtime = read root "test/fixtures/prelude/heterogeneous-functor-runtime.mech" in
  let text = source ^ generic ^ runtime in
  let* parsed = kernel (Parser.parse text) in
  let* roundtrip = kernel (Parser.parse (Syntax.print parsed)) in
  let* () = require "heterogeneous functor parse/print changed" (parsed = roundtrip) in
  let* templates, template_rows = kernel (Elab.check_in Global.empty source) in
  let* () = require "symbolic definitions escaped"
    (Global.StringMap.is_empty templates.entries
      && Global.StringMap.is_empty templates.families && template_rows = []) in
  let* globals, rows = kernel (Elab.check_in Global.initial text) in
  let* () = require "unexpected axioms" (Elab.axiom_names rows = []) in
  let* () = require "heterogeneous entry inventory changed"
    (List.length rows = 90 && Global.StringMap.cardinal globals.entries
      = Global.StringMap.cardinal Global.initial.entries + 90) in
  let* () = Global.StringMap.fold (fun name entry acc -> let* () = acc in
    Option.fold (Global.find name Global.initial)
      ~some:(fun initial -> require ("builtin changed: " ^ name) (entry = initial))
      ~none:(untrusted name entry))
    globals.entries (Ok ()) in
  let expected_families = ["Equal_Source"; "Equal_Target"; "High";
    "Mixed_Source"; "Mixed_Target"; "Run_Source"; "Run_Target"] in
  let families = Global.StringMap.bindings globals.families
    |> List.filter (fun (name, _) -> not (Global.StringMap.mem name Global.initial.families)) in
  let* () = require "heterogeneous family inventory changed"
    (List.map fst families = expected_families) in
  let* () = List.fold_left (fun acc (name, (family : Positivity.family)) ->
    let* () = acc in
    match family.f_status with
    | Positivity.Complete _ -> require ("unchecked family: " ^ name) family.f_positive
    | Positivity.Provisional | Positivity.Builtin -> Error ("unexpected family: " ^ name))
    (Ok ()) families in
  let members = ["Functor"; "functorObj"; "functorMap"; "functorMapId";
    "functorMapComp"; "eqCongr"] in
  let core_members = ["Category"; "Hom"; "id"; "comp"; "idComp"; "compId";
    "assoc"; "eqTrans"; "eqCongr"] in
  let instances = ["Mixed"; "Equal"; "Run"] in
  let* () = List.fold_left (fun acc instance -> let* () = acc in
    let names = members @ List.concat_map (fun side ->
      List.map (fun name -> side ^ "_" ^ name) core_members) ["Source"; "Target"] in
    List.fold_left (fun acc member -> let* () = acc in
      let name = instance ^ "_" ^ member in
      require ("missing member: " ^ name) (Option.is_some (Global.find name globals)))
      (Ok ()) names) (Ok ()) instances in
  let computations = ["objectValue", "39"; "mapValue", "40"; "compositionValue", "80"] in
  let* () = List.fold_left (fun acc (name, expected) -> let* () = acc in
    let* value = kernel (Eval.eval globals [] (Term.Global name)) in
    let* normal = kernel (Eval.whnf globals value) in
    let* literal = Value.as_lit normal |> Option.to_result ~none:("not a literal: " ^ name) in
    match literal with
    | Literal.LInt value -> require ("wrong computation: " ^ name)
        (Bignum.to_string value = expected)
    | Literal.LString _ -> Error ("not a natural number: " ^ name))
    (Ok ()) computations in
  let negatives = ["erased-object"; "missing-law"; "nominal-category"; "nominal-equality";
    "wrong-composition-law"; "wrong-endpoint"; "wrong-identity-law"; "wrong-object";
    "wrong-source-level"; "wrong-target-level"] in
  let expected_messages = [
    "missing-law", "expected type is (Lan SPi w mapId";
    "nominal-category", "hom (Ran SPi 0 x C (Ran SPi 0 y C Type 1))";
    "nominal-equality", "the term has type (Lan SMu Equal_Source";
    "wrong-composition-law", "the constructor categoryRefl of Mixed_Target";
    "wrong-endpoint", "the term has type (Ran SPi 0 C Type 1 (Ran SPi 0 D Type 3";
    "wrong-identity-law", "the constructor categoryRefl of Run_Target";
    "wrong-object", "the term has type Nat and the expected type is (Lan SMu High";
    "wrong-source-level", "the term has type (Ran SPi 0 Obj Type 1";
    "wrong-target-level", "the term has type (Ran SPi 0 Obj Type 3"] in
  let directory = "test/neg/heterogeneous-functor/" in
  let* listed = Mechanism_import.Io.attempt
    (fun () -> Sys.readdir (Filename.concat root directory)) in
  let present = Array.to_list listed
    |> List.filter (fun name -> Filename.check_suffix name ".mech")
    |> List.map Filename.remove_extension |> List.sort String.compare in
  let* () = require "negative inventory changed" (present = negatives) in
  let* () = List.fold_left (fun acc name -> let* () = acc in
    let* negative = read root (directory ^ name ^ ".mech") in
    Result.fold (Elab.check_in globals negative)
      ~ok:(fun _ -> Error (name ^ ": expected refusal"))
      ~error:(fun error -> match error with
        | Error.Mismatch _ -> Option.fold (List.assoc_opt name expected_messages)
            ~some:(fun expected -> require (name ^ ": wrong mismatch refusal")
              (contains (Error.to_string error) expected))
            ~none:(require (name ^ ": expected quantity refusal")
              (not (String.equal name "erased-object")))
        | Error.Quantity message -> require (name ^ ": wrong quantity refusal")
            (String.equal name "erased-object"
              && message = "the erased binder x is read in a runtime position")
        | unexpected -> Error (name ^ ": wrong refusal: " ^ Error.to_string unexpected)))
    (Ok ()) negatives in
  let* arity = read root (directory ^ "wrong-level-arity.err") in
  let* () = Result.fold (Elab.check_in Global.empty
    (source ^ "specialize MechHeterogeneousFunctor (0, 1, 2) as Bad\n"))
    ~ok:(fun _ -> Error "universe arity: expected refusal")
    ~error:(fun error -> require "universe arity: wrong refusal"
      (Error.to_string error ^ "\n" = arity)) in
  let* () = Result.fold
    (Elab.check_in ~budget:(Budget.of_poll (fun () -> true)) Global.empty source)
    ~ok:(fun _ -> Error "budget: expected refusal")
    ~error:(fun error -> require "budget: wrong refusal"
      (error = Error.Budget_exhausted Check.budget_msg)) in
  Ok (List.length rows, List.length instances, List.length computations,
    List.length negatives + 2)

let () =
  let root = Array.to_list Sys.argv |> List.to_seq |> Seq.drop 1 |> Seq.uncons
    |> Option.map fst |> Option.value ~default:"." in
  Result.fold (suite root)
    ~ok:(fun (entries, instances, computations, negatives) ->
      Printf.printf "PRELUDE-HETEROGENEOUS-FUNCTOR-OK entries=%d instances=%d computations=%d negatives=%d\n"
        entries instances computations negatives)
    ~error:(fun message -> Printf.printf "PRELUDE-HETEROGENEOUS-FUNCTOR-FAIL %s\n" message; exit 1)
