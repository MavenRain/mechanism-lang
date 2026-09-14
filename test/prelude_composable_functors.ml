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
let rec member_lines = function
  | [] | "end" :: _ -> []
  | line :: rest -> line :: member_lines rest
let rec skip_header = function
  | [] -> []
  | line :: rest -> if String.starts_with ~prefix:"def Functor " line
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
let sides = ["Source"; "Middle"; "Target"]
let edges = ["First"; "Second"; "Composite"]
let instances = ["Mixed"; "Equal"; "Run"]
let fixture_members = ["mixedComposition"; "mixedSourceHom"; "mixedMiddleHom";
  "mixedTargetHom"; "mixedFirstSort"; "mixedSecondSort"; "mixedCompositeSort";
  "compositeIdentityLaw"; "compositeCompositionLaw"; "composableInput";
  "SourceCategory"; "MiddleCategory"; "TargetCategory"; "Lift"; "Raise";
  "Composed"; "objectValue"; "mapValue"; "compositionValue"; "identityValue"]

let suite root =
  let* core = read root "prelude/cat/category-core.mech" in
  let* pair = read root "prelude/cat/heterogeneous-functor.mech" in
  let* composition = read root "prelude/cat/composable-functors.mech" in
  let pair_body = String.concat "\n" (skip_header (String.split_on_char '\n' pair)) ^ "\n" in
  let* () = require "missing canonical functor members" (String.length pair_body > 1) in
  let* () = List.fold_left (fun acc (edge, source, target, levels) -> let* () = acc in
    let side_mapping old replacement = (old, replacement) ::
      List.map (fun member -> old ^ "_" ^ member, replacement ^ "_" ^ member) core_members in
    let mapping = List.map (fun member -> member, edge ^ "_" ^ member) edge_members
      @ side_mapping "Source" source @ side_mapping "Target" target in
    require ("functor mirror changed: " ^ edge)
      (contains composition (rename_words levels mapping pair_body))) (Ok ()) [
        "First", "Source", "Middle", ["u", "u"; "v", "v"; "w", "w"; "z", "z"];
        "Second", "Middle", "Target", ["u", "w"; "v", "z"; "w", "p"; "z", "q"];
        "Composite", "Source", "Target", ["u", "u"; "v", "v"; "w", "p"; "z", "q"]] in
  let source = core ^ composition in
  let* generic = read root "test/fixtures/prelude/composable-functors.mech" in
  let* runtime = read root "test/fixtures/prelude/composable-functors-runtime.mech" in
  let text = source ^ generic ^ runtime in
  let* parsed = kernel (Parser.parse text) in
  let* roundtrip = kernel (Parser.parse (Syntax.print parsed)) in
  let* () = require "composable functor parse/print changed" (parsed = roundtrip) in
  let* templates, template_rows = kernel (Elab.check_in Global.empty source) in
  let* () = require "symbolic definitions escaped"
    (Global.StringMap.is_empty templates.entries
      && Global.StringMap.is_empty templates.families && template_rows = []) in
  let* globals, rows = kernel (Elab.check_in Global.initial text) in
  let* () = require "unexpected axioms" (Elab.axiom_names rows = []) in
  let members = "compFunctor" ::
    List.concat_map (fun side -> List.map (fun name -> side ^ "_" ^ name) core_members) sides
    @ List.concat_map (fun edge -> List.map (fun name -> edge ^ "_" ^ name) edge_members) edges in
  let expected_entries = fixture_members @ List.concat_map (fun instance ->
    List.map (fun member -> instance ^ "_" ^ member) members) instances |> List.sort String.compare in
  let entries = Global.StringMap.bindings globals.entries
    |> List.filter (fun (name, _) -> not (Global.StringMap.mem name Global.initial.entries)) in
  let* () = require "composable entry inventory changed"
    (List.map fst entries = expected_entries && List.length rows = List.length expected_entries) in
  let* () = Global.StringMap.fold (fun name entry acc -> let* () = acc in
    Option.fold (Global.find name Global.initial)
      ~some:(fun initial -> require ("builtin changed: " ^ name) (entry = initial))
      ~none:(untrusted name entry))
    globals.entries (Ok ()) in
  let expected_families = ["High"; "Higher"] @ List.concat_map (fun instance ->
    List.map (fun side -> instance ^ "_" ^ side) sides) instances |> List.sort String.compare in
  let families = Global.StringMap.bindings globals.families
    |> List.filter (fun (name, _) -> not (Global.StringMap.mem name Global.initial.families)) in
  let* () = require "shared family inventory changed" (List.map fst families = expected_families) in
  let* () = List.fold_left (fun acc (name, (family : Positivity.family)) -> let* () = acc in
    match family.f_status with
    | Positivity.Complete _ -> require ("unchecked family: " ^ name) family.f_positive
    | Positivity.Provisional | Positivity.Builtin -> Error ("unexpected family: " ^ name))
    (Ok ()) families in
  let computations = ["objectValue", "78"; "mapValue", "40";
    "compositionValue", "80"; "identityValue", "37"] in
  let* () = List.fold_left (fun acc (name, expected) -> let* () = acc in
    let* value = kernel (Eval.eval globals [] (Term.Global name)) in
    let* normal = kernel (Eval.whnf globals value) in
    let* literal = Value.as_lit normal |> Option.to_result ~none:("not a literal: " ^ name) in
    match literal with
    | Literal.LInt value -> require ("wrong computation: " ^ name) (Bignum.to_string value = expected)
    | Literal.LString _ -> Error ("not a natural number: " ^ name)) (Ok ()) computations in
  let negatives = ["erased-object"; "middle-record"; "missing-law"; "nominal-category";
    "nominal-equality"; "reversed-functors"; "wrong-identity-law";
    "wrong-middle-level"; "wrong-source-level"; "wrong-target-level"] in
  let expected_messages = [
    "middle-record",
      "the term has type (Lan SPi w obj (Ran SPi w _ D E) (Lan SPi w map (Ran SPi 0 x D "
      ^ "(Ran SPi 0 y D (Ran SPi w _ (Out SPi 0 y D (APt 0 y) (Out SPi 0 x D (APt 0 x) (Elim "
      ^ "SPi w hom (Ran SPi 0 x D (Ran SPi 0 y D Type 4)) other as self";
    "missing-law", "expected type is (Lan SPi w map ";
    "nominal-category", "Equal_Source";
    "nominal-equality", "Equal_Middle";
    "reversed-functors",
      "the term has type (Lan SPi w obj (Ran SPi w _ D E) (Lan SPi w map (Ran SPi 0 x D "
      ^ "(Ran SPi 0 y D (Ran SPi w _ (Out SPi 0 y D (APt 0 y) (Out SPi 0 x D (APt 0 x) (Elim "
      ^ "SPi w hom (Ran SPi 0 x D (Ran SPi 0 y D Type 1)) d as self return (Ran SPi 0 x D "
      ^ "(Ran SPi 0 y D Type 1)) with | (ALeg 0) x y => x))) (Out SPi 0 y E (APt 0 (Out SPi w "
      ^ "_ D (APt w y) obj)) (Out SPi 0 x E (APt 0 (Out SPi w _ D (APt w x) obj)) (Elim SPi w "
      ^ "hom (Ran SPi 0 x E (Ran SPi 0 y E Type 1)) e as self return (Ran SPi 0 x E (Ran SPi "
      ^ "0 y E Type 1)) with | (ALeg 0) x y => x)))))) (Lan SPi w mapId (Ran SPi 0 x D (Lan "
      ^ "SMu Equal_Target [";
    "wrong-identity-law", "the constructor categoryRefl of Run_Target";
    "wrong-middle-level", "the term has type (Ran SPi 0 Obj Type 3";
    "wrong-source-level", "the term has type (Ran SPi 0 Obj Type 1";
    "wrong-target-level", "the term has type (Ran SPi 0 Obj Type 5"] in
  let directory = "test/neg/composable-functors/" in
  let* listed = Mechanism_import.Io.attempt (fun () -> Sys.readdir (Filename.concat root directory)) in
  let present = Array.to_list listed |> List.filter (fun name -> Filename.check_suffix name ".mech")
    |> List.map Filename.remove_extension |> List.sort String.compare in
  let* () = require "negative inventory changed" (present = negatives) in
  let* () = List.fold_left (fun acc name -> let* () = acc in
    let* negative = read root (directory ^ name ^ ".mech") in
    Result.fold (Elab.check_in globals negative)
      ~ok:(fun _ -> Error (name ^ ": expected refusal"))
      ~error:(fun error -> match error with
        | Error.Mismatch _ -> Option.fold (List.assoc_opt name expected_messages)
            ~some:(fun expected -> require (name ^ ": wrong mismatch refusal: " ^ Error.to_string error)
              (contains (Error.to_string error) expected))
            ~none:(Error (name ^ ": expected quantity refusal"))
        | Error.Quantity message -> require (name ^ ": wrong quantity refusal")
            (String.equal name "erased-object"
              && message = "the erased binder x is read in a runtime position")
        | unexpected -> Error (name ^ ": wrong refusal: " ^ Error.to_string unexpected)))
    (Ok ()) negatives in
  let* () = Result.fold (Elab.check_in Global.empty
    (source ^ "specialize MechComposableFunctors (0, 1, 2, 3, 4) as Bad\n"))
    ~ok:(fun _ -> Error "universe arity: expected refusal")
    ~error:(fun error -> require "universe arity: wrong refusal"
      (error = Error.Universe "the family schema MechComposableFunctors expects 6 universe arguments, got 5")) in
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
      Printf.printf "PRELUDE-COMPOSABLE-FUNCTORS-OK entries=%d instances=%d computations=%d negatives=%d\n"
        entries instances computations negatives)
    ~error:(fun message -> Printf.printf "PRELUDE-COMPOSABLE-FUNCTORS-FAIL %s\n" message; exit 1)
