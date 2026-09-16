open Mechanism_kernel
open Mechanism_surface

let ( let* ) = Result.bind
let require message condition = if condition then Ok () else Error message
let preview text =
  if String.length text <= 512 then text else String.sub text 0 512 ^ "..."
let kernel result = Result.map_error (fun error -> preview (Error.to_string error)) result
let trace step =
  if Array.exists (String.equal "--trace") Sys.argv then Printf.eprintf "TRACE %s\n%!" step
let read root path = Mechanism_import.Io.attempt
  (fun () -> In_channel.with_open_bin (Filename.concat root path) In_channel.input_all)
let untrusted name entry = match entry with
  | Global.Def _ -> Ok ()
  | Global.Axiom _ | Global.Prim _ -> Error ("trusted entry: " ^ name)

let core_members = ["Category"; "Hom"; "id"; "comp"; "idComp"; "compId";
  "assoc"; "eqTrans"; "eqCongr"]
let edge_members = ["Functor"; "functorObj"; "functorMap"; "functorMapId";
  "functorMapComp"; "eqCongr"]
let nat_members = ["NatTrans"; "natApp"; "naturality"]
let sides = ["Source"; "Middle"; "Target"]
let edges = ["First"; "Second"; "Composite"]
let instances = ["Mixed"; "Wide"; "Run"]
let fixture_members = ["MixedSourceHom"; "MixedMiddleHom"; "MixedTargetHom"; "MixedCoconeSort"; "MixedSolutionSort"; "MixedTailSort"; "MixedExtensionSort"; "MixedCoconeContract"; "MixedFactorContract"; "MixedUniqueContract"; "WideSourceHom"; "WideMiddleHom"; "WideTargetHom"; "WideCoconeSort"; "WideSolutionSort"; "WideTailSort"; "WideExtensionSort"; "WideCoconeContract"; "WideFactorContract"; "WideUniqueContract"; "heterogeneousLanInput"; "pathTrans"; "IndexedEnd"; "DoubleEnd"; "IndexedPlain"; "Constant"; "Components"; "componentLaw"; "Alpha"; "Forget"; "Lift"; "OtherComponents"; "otherComponentLaw"; "Beta"; "forgetUnit"; "forgetLan"; "Original"; "Extension"; "Eta"; "Cocone"; "OtherCocone"; "Chosen"; "OtherChosen"; "Mediator"; "OtherMediator"; "lanUnitResult"; "lanMapResult"; "lanDescFirst"; "lanDescSecond"; "factorWitness"; "uniqueWitness"]

let suite root =
  let* core = read root "prelude/cat/category-core.mech" in
  let* functors = read root "prelude/cat/composable-functors.mech" in
  let* transformations = read root "prelude/cat/heterogeneous-whiskering.mech" in
  let* extensions = read root "prelude/cat/heterogeneous-left-kan.mech" in
  let source = core ^ functors ^ transformations ^ extensions in
  let* generic = read root "test/fixtures/prelude/heterogeneous-left-kan.mech" in
  let* runtime = read root "test/fixtures/prelude/heterogeneous-left-kan-runtime.mech" in
  let text = source ^ generic ^ runtime in
  let () = trace "parse and roundtrip" in
  let* parsed = kernel (Parser.parse text) in
  let* roundtrip = kernel (Parser.parse (Syntax.print parsed)) in
  let* () = require "heterogeneous left Kan parse/print changed" (parsed = roundtrip) in
  let () = trace "symbolic templates, specializations and fixtures" in
  let* globals, rows = kernel (Elab.check_in Global.initial text) in
  let* () = require "unexpected axioms" (Elab.axiom_names rows = []) in
  let members = ["targetEqSymm"; "whiskerRight"; "whiskerLeft"; "Base_compFunctor"]
    @ List.concat_map (fun side -> List.map (fun name -> "Base_" ^ side ^ "_" ^ name) core_members) sides
    @ List.concat_map (fun edge -> List.map (fun name -> "Base_" ^ edge ^ "_" ^ name) edge_members) edges
    @ List.concat_map (fun edge -> List.map (fun name -> edge ^ "_" ^ name) nat_members) edges in
  let members = ["recordFirst"; "recordSecond"; "coconeFirst"; "coconeSecond"; "LanCocone"; "LanFactor"; "LanSolution"; "LanTail"; "LeftKanExtension"; "lanFunctor"; "lanTail"; "lanUnit"; "lanSolve"; "lanDesc"; "lanFac"; "lanUniq"; "desc_unique"]
    @ List.map (fun name -> "Base_" ^ name) members in
  let nominal_entries = List.concat_map (fun instance ->
    List.map (fun member -> instance ^ "_" ^ member) core_members)
    ["EqualSource"; "EqualMiddle"] in
  let expected_entries = fixture_members @ nominal_entries @ List.concat_map (fun instance ->
    List.map (fun member -> instance ^ "_" ^ member) members) instances |> List.sort String.compare in
  let entries = Global.StringMap.bindings globals.entries
    |> List.filter (fun (name, _) -> not (Global.StringMap.mem name Global.initial.entries)) in
  let* () = require (Printf.sprintf "entry inventory changed: %d" (List.length rows))
    (List.map fst entries = expected_entries && List.length rows = List.length expected_entries) in
  let* () = Global.StringMap.fold (fun name entry acc -> let* () = acc in
    Option.fold (Global.find name Global.initial)
      ~some:(fun initial () -> require ("builtin changed: " ^ name) (entry = initial))
      ~none:(fun () -> untrusted name entry) ())
    globals.entries (Ok ()) in
  let expected_families = ["Path"; "Point"; "EqualSource"; "EqualMiddle"] @ List.concat_map (fun instance ->
    List.map (fun side -> instance ^ "_Base_Base_" ^ side) sides) instances |> List.sort String.compare in
  let families = Global.StringMap.bindings globals.families
    |> List.filter (fun (name, _) -> not (Global.StringMap.mem name Global.initial.families)) in
  let* () = require "family inventory changed" (List.map fst families = expected_families) in
  let* () = List.fold_left (fun acc (name, (family : Positivity.family)) ->
    let* () = acc in
    match family.f_status with
    | Positivity.Complete _ -> require ("unchecked family: " ^ name) family.f_positive
    | Positivity.Provisional | Positivity.Builtin -> Error ("unexpected family: " ^ name))
    (Ok ()) families in
  let computations = ["lanUnitResult", "38"; "lanMapResult", "40";
    "lanDescFirst", "42"; "lanDescSecond", "50"] in
  let* () = List.fold_left (fun acc (name, expected) -> let* () = acc in
    let () = trace ("computation " ^ name) in
    let* value = kernel (Eval.eval globals [] (Term.Global name)) in
    let* normal = kernel (Eval.whnf globals value) in
    let* literal = Value.as_lit normal |> Option.to_result ~none:("not a literal: " ^ name) in
    match literal with
    | Literal.LInt value -> require ("wrong computation: " ^ name) (Bignum.to_string value = expected)
    | Literal.LString _ -> Error ("not a natural number: " ^ name))
    (Ok ()) computations in
  let negatives = ["cocone-endpoint"; "cocone-sort"; "extension-sort"; "middle-level"; "missing-factor"; "missing-uniqueness"; "nominal-category"; "solution-sort"; "source-level"; "tail-sort"; "target-level"; "uniqueness-endpoint"; "wrong-factor"] in
  let directory = "test/neg/heterogeneous-left-kan/" in
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
        let () = trace ("refusal " ^ name ^ " " ^ preview message) in
        require (name ^ ": wrong refusal: " ^ preview message)
          (String.starts_with ~prefix:expected message))) prefixes in
  let* () = List.fold_left (fun acc result -> let* () = acc in result) (Ok ()) refusals in
  let* () = Result.fold (Elab.check_in Global.empty
    (source ^ "specialize MechHeterogeneousLeftKan (0, 1, 2, 3, 4) as Bad\n"))
    ~ok:(fun _ -> Error "universe arity: expected refusal")
    ~error:(fun error -> require "universe arity: wrong refusal"
      (Error.to_string error =
        "universe: the family schema MechHeterogeneousLeftKan expects 6 universe arguments, got 5")) in
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
      Printf.printf "PRELUDE-HETEROGENEOUS-LEFT-KAN-OK entries=%d instances=%d computations=%d negatives=%d\n"
        entries instances computations negatives)
    ~error:(fun message -> Printf.printf "PRELUDE-HETEROGENEOUS-LEFT-KAN-FAIL %s\n" message; exit 1)
