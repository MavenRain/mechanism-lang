open Mechanism_kernel
open Mechanism_surface

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let trace step =
  if Array.exists (String.equal "--trace") Sys.argv then Printf.eprintf "TRACE %s\n%!" step
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
let rec member_lines = function
  | [] -> []
  | line :: _ when String.starts_with ~prefix:"def idNat " line -> []
  | line :: rest -> line :: member_lines rest
let rec skip_header = function
  | [] -> []
  | line :: rest -> if String.starts_with ~prefix:"def NatTrans " line
      then member_lines (line :: rest) else skip_header rest
let in_range low high char =
  Char.code char >= Char.code low && Char.code char <= Char.code high
let word char = in_range 'a' 'z' char || in_range 'A' 'Z' char
  || in_range '0' '9' char || Char.equal char '_'
let token_text token = String.of_seq (List.to_seq (List.rev token))
let rename_words levels mapping text =
  let rename previous token = Option.value
    (List.assoc_opt token (if String.equal previous "succ" then levels else mapping))
    ~default:token in
  let step (pieces, token, previous) char = if word char then (pieces, char :: token, previous)
    else
      let current = token_text token in
      (String.make 1 char :: rename previous current :: pieces, [],
        if String.length current = 0 then previous else current) in
  let pieces, token, previous = Seq.fold_left step ([], [], "") (String.to_seq text) in
  String.concat "" (List.rev (rename previous (token_text token) :: pieces))
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
let instances = ["Mixed"; "Wide"; "Equal"; "Run"]
let fixture_members = ["MixedSourceHom"; "MixedMiddleHom"; "MixedTargetHom"; "MixedFirstNatSort"; "MixedSecondNatSort"; "MixedCompositeNatSort"; "MixedRight"; "MixedLeft"; "WideSourceHom"; "WideMiddleHom"; "WideTargetHom"; "WideFirstNatSort"; "WideSecondNatSort"; "WideCompositeNatSort"; "WideRight"; "WideLeft"; "heterogeneousWhiskerInput"; "pathTrans"; "IndexedEnd"; "DoubleEnd"; "IndexedPlain"; "Constant"; "Components"; "componentLaw"; "Alpha"; "pathShift"; "Shift"; "Drop"; "Lift"; "FirstComponents"; "firstComponentLaw"; "FirstAlpha"; "RightFunctor"; "LeftFunctor"; "RightResult"; "LeftResult"; "whiskerRightFirst"; "whiskerRightSecond"; "whiskerLeftFirst"; "whiskerLeftSecond"; "rightNaturalityWitness"; "leftNaturalityWitness"]

let suite root =
  let* core = read root "prelude/cat/category-core.mech" in
  let* functors = read root "prelude/cat/composable-functors.mech" in
  let* transformations = read root "prelude/cat/heterogeneous-whiskering.mech" in
  let* canonical = read root "prelude/cat/heterogeneous-nattrans.mech" in
  let canonical_body = String.concat "\n" (skip_header (String.split_on_char '\n' canonical)) ^ "\n" in
  let* () = require "missing canonical natural transformation members" (String.length canonical_body > 1) in
  let* () = List.fold_left (fun acc (edge, source, target, levels) -> let* () = acc in
    let side_mapping old replacement = (old, replacement) ::
      List.map (fun member -> old ^ "_" ^ member, replacement ^ "_" ^ member) core_members in
    let mapping = List.map (fun member -> member, edge ^ "_" ^ member) nat_members
      @ List.map (fun member -> "Base_" ^ member, "Base_" ^ edge ^ "_" ^ member) edge_members
      @ side_mapping "Base_Source" ("Base_" ^ source)
      @ side_mapping "Base_Target" ("Base_" ^ target) in
    require ("natural transformation mirror changed: " ^ edge)
      (contains transformations (rename_words levels mapping canonical_body))) (Ok ()) [
        "First", "Source", "Middle", ["u", "u"; "v", "v"; "w", "w"; "z", "z"];
        "Second", "Middle", "Target", ["u", "w"; "v", "z"; "w", "p"; "z", "q"];
        "Composite", "Source", "Target", ["u", "u"; "v", "v"; "w", "p"; "z", "q"]] in
  let source = core ^ functors ^ transformations in
  let* generic = read root "test/fixtures/prelude/heterogeneous-whiskering.mech" in
  let* runtime = read root "test/fixtures/prelude/heterogeneous-whiskering-runtime.mech" in
  let text = source ^ generic ^ runtime in
  let () = trace "parse and roundtrip" in
  let* parsed = kernel (Parser.parse text) in
  let* roundtrip = kernel (Parser.parse (Syntax.print parsed)) in
  let* () = require "heterogeneous whiskering parse/print changed" (parsed = roundtrip) in
  let () = trace "symbolic templates, specializations and fixtures" in
  let* globals, rows = kernel (Elab.check_in Global.initial text) in
  let* () = require "unexpected axioms" (Elab.axiom_names rows = []) in
  let members = ["targetEqSymm"; "whiskerRight"; "whiskerLeft"; "Base_compFunctor"]
    @ List.concat_map (fun side -> List.map (fun name -> "Base_" ^ side ^ "_" ^ name) core_members) sides
    @ List.concat_map (fun edge -> List.map (fun name -> "Base_" ^ edge ^ "_" ^ name) edge_members) edges
    @ List.concat_map (fun edge -> List.map (fun name -> edge ^ "_" ^ name) nat_members) edges in
  let expected_entries = fixture_members @ List.concat_map (fun instance ->
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
  let expected_families = ["Path"; "Point"] @ List.concat_map (fun instance ->
    List.map (fun side -> instance ^ "_Base_" ^ side) sides) instances |> List.sort String.compare in
  let families = Global.StringMap.bindings globals.families
    |> List.filter (fun (name, _) -> not (Global.StringMap.mem name Global.initial.families)) in
  let* () = require "family inventory changed" (List.map fst families = expected_families) in
  let* () = List.fold_left (fun acc (name, (family : Positivity.family)) ->
    let* () = acc in
    match family.f_status with
    | Positivity.Complete _ -> require ("unchecked family: " ^ name) family.f_positive
    | Positivity.Provisional | Positivity.Builtin -> Error ("unexpected family: " ^ name))
    (Ok ()) families in
  let computations = ["whiskerRightFirst", "42"; "whiskerRightSecond", "44";
    "whiskerLeftFirst", "46"; "whiskerLeftSecond", "48"] in
  let* () = List.fold_left (fun acc (name, expected) -> let* () = acc in
    let () = trace ("computation " ^ name) in
    let* value = kernel (Eval.eval globals [] (Term.Global name)) in
    let* normal = kernel (Eval.whnf globals value) in
    let* literal = Value.as_lit normal |> Option.to_result ~none:("not a literal: " ^ name) in
    match literal with
    | Literal.LInt value -> require ("wrong computation: " ^ name) (Bignum.to_string value = expected)
    | Literal.LString _ -> Error ("not a natural number: " ^ name))
    (Ok ()) computations in
  let negatives = ["composite-nat-sort"; "erased-object"; "first-nat-sort"; "left-endpoint"; "middle-record"; "missing-law"; "nominal-category"; "nominal-equality"; "right-endpoint"; "second-nat-sort"; "wrong-middle-level"; "wrong-naturality"; "wrong-source-level"; "wrong-target-level"] in
  let directory = "test/neg/heterogeneous-whiskering/" in
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
    (source ^ "specialize MechHeterogeneousWhiskering (0, 1, 2, 3, 4) as Bad\n"))
    ~ok:(fun _ -> Error "universe arity: expected refusal")
    ~error:(fun error -> require "universe arity: wrong refusal"
      (Error.to_string error =
        "universe: the family schema MechHeterogeneousWhiskering expects 6 universe arguments, got 5")) in
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
      Printf.printf "PRELUDE-HETEROGENEOUS-WHISKERING-OK entries=%d instances=%d computations=%d negatives=%d\n"
        entries instances computations negatives)
    ~error:(fun message -> Printf.printf "PRELUDE-HETEROGENEOUS-WHISKERING-FAIL %s\n" message; exit 1)
