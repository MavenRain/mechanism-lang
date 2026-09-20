open Mechanism_kernel
open Mechanism_surface

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let trace message =
  if Array.exists (String.equal "--trace") Sys.argv then Printf.eprintf "TRACE %s\n%!" message
let read root path = Mechanism_import.Io.attempt (fun () ->
  In_channel.with_open_bin (Filename.concat root path) In_channel.input_all)
let read_all root paths = List.fold_left (fun acc path ->
  let* text = acc in let* part = read root path in Ok (text ^ "\n" ^ part)) (Ok "") paths
let prefix name members = List.map (fun member -> name ^ "_" ^ member) members
let core = ["Category"; "Hom"; "id"; "comp"; "idComp"; "compId"; "assoc"; "eqTrans"; "eqCongr"]
let functor_members = ["Functor"; "functorObj"; "functorMap"; "functorMapId"; "functorMapComp"; "eqCongr"]
let nat = ["targetEqSymm"; "NatTrans"; "natApp"; "naturality"; "idNat"; "vcompLaw"; "vcomp"]
let operations = prefix "Base" (prefix "Source" core @ prefix "Target" core @ functor_members) @ nat
let law_members = prefix "Ops" operations @
  ["NatTransEq"; "eqRefl"; "eqSymm"; "eqTrans"; "idVcomp"; "vcompId"; "vcompAssoc"; "vcompCongr"]
let sides = ["Source"; "Middle"; "Target"]
let edges = ["First"; "Second"; "Composite"]
let heterogeneous = prefix "Source" core @ prefix "Target" core @ functor_members
let shared_members = List.concat_map (fun side -> prefix side core) sides
  @ List.concat_map (fun edge -> prefix edge (prefix "Base" heterogeneous @ nat)) edges
  @ prefix "Compose" (List.concat_map (fun side -> prefix ("Base_" ^ side) core) sides
    @ List.concat_map (fun edge -> prefix ("Base_" ^ edge) functor_members) edges
    @ ["Base_compFunctor"; "targetEqSymm"; "whiskerRight"; "whiskerLeft"]
    @ List.concat_map (fun edge -> prefix edge ["NatTrans"; "natApp"; "naturality"]) edges)
  @ ["hcomp"]
let preservation_members = prefix "Ops" shared_members
  @ List.concat_map (fun edge -> prefix edge law_members) edges
  @ ["whiskerRightId"; "whiskerRightVcomp"; "whiskerRightCongr";
     "whiskerLeftId"; "whiskerLeftVcomp"; "whiskerLeftCongr"]
let members = prefix "W" preservation_members @
  ["NatTransEqAt"; "idHcomp"; "hcompId"; "hcompIdId"; "hcompCongr"; "hcompExchange"; "targetMiddleFour"; "hcompVcomp"]
let shared_fixture_members = ["sharedNatInput"; "pathTrans";
  "IndexedEnd"; "End"; "PairEnd"; "Constant"; "Left"; "Right"; "components";
  "componentLaw"; "Alpha"; "Beta"; "Gamma"; "FH"; "GI"; "Horizontal";
  "Vertical"; "Identity"; "horizontalFirst"; "horizontalSecond"; "verticalValue";
  "identityValue"; "naturalityWitness"]
let fixture_members = ["Delta"; "FirstIdentityAlpha"; "SecondIdentityBeta"; "pairCode"; "certifiedHorizontal"; "idLeftLhs"; "idLeftRhs"; "idLeftLaw"; "idLeftLeftAt"; "idLeftRightAt"; "idRightLhs"; "idRightRhs"; "idRightLaw"; "idRightLeftAt"; "idRightRightAt"; "idBothLhs"; "idBothRhs"; "idBothLaw"; "idBothLeftAt"; "idBothRightAt"; "congruenceLhs"; "congruenceRhs"; "congruenceLaw"; "congruenceLeftAt"; "congruenceRightAt"; "exchangeLhs"; "exchangeRhs"; "exchangeLaw"; "exchangeLeftAt"; "exchangeRightAt"; "exchangeSwappedLhs"; "exchangeSwappedRhs"; "exchangeSwappedLaw"; "exchangeSwappedLeftAt"; "exchangeSwappedRightAt"; "interchangeLhs"; "interchangeRhs"; "interchangeLaw"; "interchangeLeftAt"; "interchangeRightAt"]
let negatives = ["false-components"; "one-component"; "erased-object"; "missing-first-congruence"; "missing-second-congruence"; "nominal-target"; "wrong-endpoint"]
let paths = ["prelude/cat/category-core.mech"; "prelude/cat/heterogeneous-functor.mech"; "prelude/cat/composable-functors.mech"; "prelude/cat/heterogeneous-nattrans.mech"; "prelude/cat/heterogeneous-whiskering.mech"; "prelude/cat/shared-nattrans.mech"; "prelude/cat/nattrans-laws.mech"; "prelude/cat/whiskering-laws.mech"; "prelude/cat/horizontal-laws.mech"]

let suite root =
  let* templates = read_all root paths in
  let* fixtures = read_all root ["test/fixtures/prelude/horizontal-laws.mech";
    "test/fixtures/prelude/shared-nattrans-runtime.mech";
    "test/fixtures/prelude/horizontal-laws-runtime.mech"] in
  let* parsed = kernel (Parser.parse (templates ^ fixtures)) in
  let* roundtrip = kernel (Parser.parse (Syntax.print parsed)) in
  let* () = require "parse/print changed" (parsed = roundtrip) in
  let () = trace (Printf.sprintf "parsed cpu=%.3f" (Sys.time ())) in
  let* globals, rows = kernel (Elab.elab_program_in Global.initial parsed) in
  let () = trace (Printf.sprintf "elaborated cpu=%.3f" (Sys.time ())) in
  let expected_entries = List.concat_map (fun instance -> prefix instance members)
      ["Mixed"; "Wide"; "Laws"] @ prefix "Run" shared_members
      @ prefix "External" heterogeneous @ prefix "SeparateTarget" core
      @ shared_fixture_members @ fixture_members
      |> List.sort String.compare in
  let* () = require "entry inventory changed"
    (List.sort String.compare (List.map fst rows) = expected_entries) in
  let* () = require "unexpected axioms" (Elab.axiom_names rows = []) in
  let* () = Global.StringMap.fold (fun name entry acc -> let* () = acc in
    Option.fold (Global.find name Global.initial)
      ~some:(fun initial () -> require ("builtin changed: " ^ name) (initial = entry))
      ~none:(fun () -> match entry with
        | Global.Def _ -> Ok ()
        | Global.Axiom _ | Global.Prim _ -> Error ("trusted entry: " ^ name)) ())
    globals.entries (Ok ()) in
  let families = Global.StringMap.bindings globals.families
    |> List.filter (fun (name, _) -> not (Global.StringMap.mem name Global.initial.families)) in
  let expected_families = ["Path"; "Point"; "SeparateTarget"] @ List.concat_map
    (fun instance -> prefix instance ["W_Ops_Source"; "W_Ops_Middle"; "W_Ops_Target"]) ["Mixed"; "Wide"]
    @ ["Run_Source"; "Run_Middle"; "Run_Target"] |> List.sort String.compare in
  let* () = require "shared family inventory changed" (List.map fst families = expected_families) in
  let* () = List.fold_left (fun acc (name, (family : Positivity.family)) -> let* () = acc in
    match family.f_status with
    | Positivity.Complete _ -> require ("unchecked family: " ^ name) family.f_positive
    | Positivity.Provisional | Positivity.Builtin -> Error ("unexpected family: " ^ name))
    (Ok ()) families in
  let functions = [
    ("idLeftLeftAt", 1, 713);
    ("idLeftRightAt", 1, 713);
    ("idRightLeftAt", 300, 2607);
    ("idRightRightAt", 300, 2607);
    ("idBothLeftAt", 100, 1307);
    ("idBothRightAt", 100, 1307);
    ("congruenceLeftAt", 3, 726);
    ("congruenceRightAt", 3, 726);
    ("exchangeLeftAt", 3, 726);
    ("exchangeRightAt", 3, 726);
    ("exchangeSwappedLeftAt", 1, 713); ("exchangeSwappedRightAt", 1, 713);
    ("interchangeLeftAt", 300, 3105);
    ("interchangeRightAt", 300, 3105)] in
  let computations = List.concat_map (fun payload ->
    List.map (fun (name, scale, offset) -> name, payload, scale * payload + offset) functions) [37; 41] in
  let* () = List.fold_left (fun acc (name, payload, expected) -> let* () = acc in
    let source = Printf.sprintf "def lawProbe : Nat := %s %d" name payload in
    let* probe, _ = kernel (Elab.check_in globals source) in
    let* value = kernel (Eval.eval probe [] (Term.Global "lawProbe")) in
    let* normal = kernel (Eval.quote probe 0 value) in
    require ("wrong computation: " ^ name ^ "/" ^ string_of_int payload)
      (normal = Term.Lit (Literal.LInt (Bignum.of_int expected)))) (Ok ()) computations in
  let () = trace (Printf.sprintf "computed cpu=%.3f" (Sys.time ())) in
  let directory = "test/neg/horizontal-laws/" in
  let* files = Mechanism_import.Io.attempt (fun () -> Sys.readdir (Filename.concat root directory)) in
  let present = Array.to_list files |> List.filter (fun name -> Filename.check_suffix name ".mech")
    |> List.map Filename.remove_extension |> List.sort String.compare in
  let* () = require "negative inventory changed" (present = List.sort String.compare negatives) in
  let* failures = List.fold_left (fun acc name -> let* failures = acc in
    let* negative = read root (directory ^ name ^ ".mech") in
    let* expected = read root (directory ^ name ^ ".err") in
    let expected = String.trim expected in
    let* fingerprint = read root (directory ^ name ^ ".digest") in
    let fingerprint = String.trim fingerprint in
    let* () = require (name ^ ": invalid diagnostic digest") (String.length fingerprint = 32) in
    let* () = require (name ^ ": empty refusal prefix") (String.length expected > 40) in
    Result.fold (Elab.check_in globals negative)
      ~ok:(fun _ -> Ok ((name ^ ": expected refusal") :: failures))
      ~error:(fun error ->
        let message = Error.to_string error in
        let () = trace (name ^ " " ^ message) in
        if String.starts_with ~prefix:expected message
            && String.equal fingerprint (Digest.to_hex (Digest.string message))
         then Ok failures
        else Ok ((name ^ ": refusal changed: "
          ^ String.of_seq (Seq.take 240 (String.to_seq message))) :: failures))) (Ok []) negatives in
  let* () = require (String.concat "; " (List.rev failures)) (failures = []) in
  Ok (List.length rows, List.length families, List.length computations, List.length negatives)

let () =
  let root = Array.to_list Sys.argv |> List.to_seq |> Seq.drop 1 |> Seq.uncons
    |> Option.map fst |> Option.value ~default:"." in
  Result.fold (suite root)
    ~ok:(fun (entries, families, computations, negatives) ->
      Printf.printf "PRELUDE-HORIZONTAL-LAWS-OK entries=%d families=%d computations=%d negatives=%d\n"
        entries families computations negatives)
    ~error:(fun message -> Printf.printf "PRELUDE-HORIZONTAL-LAWS-FAIL %s\n" message; exit 1)
