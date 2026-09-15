open Mechanism_kernel
open Mechanism_surface

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let trace step =
  if Array.exists (String.equal "--trace") Sys.argv then Printf.eprintf "TRACE %s\n%!" step
let read root path = Mechanism_import.Io.attempt
  (fun () -> In_channel.with_open_bin (Filename.concat root path) In_channel.input_all)
let untrusted name entry = match entry with
  | Global.Def _ -> Ok ()
  | Global.Axiom _ | Global.Prim _ -> Error ("trusted entry: " ^ name)

let suite root =
  let* core = read root "prelude/cat/category-core.mech" in
  let* functors = read root "prelude/cat/heterogeneous-functor.mech" in
  let* transformations = read root "prelude/cat/heterogeneous-nattrans.mech" in
  let source = core ^ functors ^ transformations in
  let* generic = read root "test/fixtures/prelude/heterogeneous-nattrans.mech" in
  let* runtime = read root "test/fixtures/prelude/heterogeneous-nattrans-runtime.mech" in
  let text = source ^ generic ^ runtime in
  let () = trace "parse and roundtrip" in
  let* parsed = kernel (Parser.parse text) in
  let* roundtrip = kernel (Parser.parse (Syntax.print parsed)) in
  let* () = require "heterogeneous natural transformation parse/print changed" (parsed = roundtrip) in
  let () = trace "symbolic templates" in
  let* templates, template_rows = kernel (Elab.check_in Global.empty source) in
  let* () = require "symbolic definitions escaped"
    (Global.StringMap.is_empty templates.entries
      && Global.StringMap.is_empty templates.families && template_rows = []) in
  let () = trace "specializations and fixtures" in
  let* globals, rows = kernel (Elab.check_in Global.initial text) in
  let* () = require "unexpected axioms" (Elab.axiom_names rows = []) in
  let* () = require (Printf.sprintf "entry inventory changed: %d" (List.length rows))
    (List.length rows = 150 && Global.StringMap.cardinal globals.entries
      = Global.StringMap.cardinal Global.initial.entries + 150) in
  let* () = Global.StringMap.fold (fun name entry acc -> let* () = acc in
    Option.fold (Global.find name Global.initial)
      ~some:(fun initial () -> require ("builtin changed: " ^ name) (entry = initial))
      ~none:(fun () -> untrusted name entry) ())
    globals.entries (Ok ()) in
  let expected_families = ["Equal_Base_Source"; "Equal_Base_Target";
    "Mixed_Base_Source"; "Mixed_Base_Target"; "Path"; "Point";
    "Run_Base_Source"; "Run_Base_Target"; "Wide_Base_Source"; "Wide_Base_Target"] in
  let families = Global.StringMap.bindings globals.families
    |> List.filter (fun (name, _) -> not (Global.StringMap.mem name Global.initial.families)) in
  let* () = require "family inventory changed" (List.map fst families = expected_families) in
  let* () = List.fold_left (fun acc (name, (family : Positivity.family)) ->
    let* () = acc in
    match family.f_status with
    | Positivity.Complete _ -> require ("unchecked family: " ^ name) family.f_positive
    | Positivity.Provisional | Positivity.Builtin -> Error ("unexpected family: " ^ name))
    (Ok ()) families in
  let members = ["NatTrans"; "natApp"; "naturality"; "idNat"; "vcomp";
    "vcompLaw"; "targetEqSymm"; "Base_Functor"; "Base_functorObj";
    "Base_functorMap"; "Base_functorMapId"; "Base_functorMapComp"; "Base_eqCongr"] in
  let core_members = ["Category"; "Hom"; "id"; "comp"; "idComp"; "compId";
    "assoc"; "eqTrans"; "eqCongr"] in
  let instances = ["Mixed"; "Wide"; "Equal"; "Run"] in
  let* () = List.fold_left (fun acc instance -> let* () = acc in
    let names = members @ List.concat_map (fun side ->
      List.map (fun name -> "Base_" ^ side ^ "_" ^ name) core_members) ["Source"; "Target"] in
    List.fold_left (fun acc member -> let* () = acc in
      let name = instance ^ "_" ^ member in
      require ("missing member: " ^ name) (Option.is_some (Global.find name globals)))
      (Ok ()) names) (Ok ()) instances in
  let computations = ["componentValue", "37"; "identityValue", "42";
    "verticalForward", "74"; "verticalReverse", "37"; "verticalNested", "111"] in
  let* () = List.fold_left (fun acc (name, expected) -> let* () = acc in
    let () = trace ("computation " ^ name) in
    let* value = kernel (Eval.eval globals [] (Term.Global name)) in
    let* normal = kernel (Eval.whnf globals value) in
    let* literal = Value.as_lit normal |> Option.to_result ~none:("not a literal: " ^ name) in
    match literal with
    | Literal.LInt value -> require ("wrong computation: " ^ name) (Bignum.to_string value = expected)
    | Literal.LString _ -> Error ("not a natural number: " ^ name))
    (Ok ()) computations in
  let negatives = ["erased-object"; "missing-law"; "nominal-category"; "nominal-equality";
    "target-record"; "wrong-endpoint"; "wrong-middle"; "wrong-nat-sort";
    "wrong-naturality"; "wrong-source-level"; "wrong-target-level"] in
  let directory = "test/neg/heterogeneous-nattrans/" in
  let* listed = Mechanism_import.Io.attempt (fun () -> Sys.readdir (Filename.concat root directory)) in
  let present = Array.to_list listed
    |> List.filter (fun name -> Filename.check_suffix name ".mech")
    |> List.map Filename.remove_extension |> List.sort String.compare in
  let* () = require "negative inventory changed" (present = negatives) in
  let* prefixes = List.fold_left (fun acc name ->
    let* rows = acc in
    let* expected = read root (directory ^ name ^ ".err") in
    let expected = String.trim expected in
    let* () = require (name ^ ": empty refusal prefix")
      (String.length expected > String.length "mismatch:") in
    Ok ((name, expected) :: rows)) (Ok []) negatives in
  let prefixes = List.rev prefixes in
  let* () = List.fold_left (fun acc (name, expected) -> let* () = acc in
    List.fold_left (fun acc (other, text) -> let* () = acc in
      require (name ^ ": refusal prefix also matches " ^ other)
        (String.equal name other || not (String.starts_with ~prefix:expected text)))
      (Ok ()) prefixes) (Ok ()) prefixes in
  let refusals = List.map (fun (name, expected) ->
    let () = trace ("negative " ^ name) in
    let* negative = read root (directory ^ name ^ ".mech") in
    Result.fold (Elab.check_in globals negative)
      ~ok:(fun _ -> Error (name ^ ": expected refusal"))
      ~error:(fun error ->
        let message = Error.to_string error in
        let () = trace ("refusal " ^ name ^ " " ^ message) in
        require (name ^ ": wrong refusal: " ^ message)
          (String.starts_with ~prefix:expected message))) prefixes in
  let* () = List.fold_left (fun acc result -> let* () = acc in result) (Ok ()) refusals in
  let* () = Result.fold (Elab.check_in Global.empty
    (source ^ "specialize MechHeterogeneousNatTrans (0, 1, 2) as Bad\n"))
    ~ok:(fun _ -> Error "universe arity: expected refusal")
    ~error:(fun error -> require "universe arity: wrong refusal"
      (Error.to_string error =
        "universe: the family schema MechHeterogeneousNatTrans expects 4 universe arguments, got 3")) in
  let* () = Result.fold
    (Elab.check_in ~budget:(Budget.of_poll (fun () -> true)) Global.empty source)
    ~ok:(fun _ -> Error "budget: expected refusal")
    ~error:(fun error -> require "budget: wrong refusal"
      (error = Error.Budget_exhausted Check.budget_msg)) in
  Ok (List.length rows, List.length instances, List.length computations, List.length negatives + 2)

let () =
  let root = Array.to_list Sys.argv |> List.to_seq |> Seq.drop 1 |> Seq.uncons
    |> Option.map fst |> Option.value ~default:"." in
  Result.fold (suite root)
    ~ok:(fun (entries, instances, computations, negatives) ->
      Printf.printf "PRELUDE-HETEROGENEOUS-NATTRANS-OK entries=%d instances=%d computations=%d negatives=%d\n"
        entries instances computations negatives)
    ~error:(fun message -> Printf.printf "PRELUDE-HETEROGENEOUS-NATTRANS-FAIL %s\n" message; exit 1)
