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
let members = prefix "Ops" operations @
  ["NatTransEq"; "eqRefl"; "eqSymm"; "eqTrans"; "idVcomp"; "vcompId"; "vcompAssoc"; "vcompCongr"]
let fixture_members = ["mixedComponents"; "mixedRefl"; "mixedSymm"; "mixedTrans";
  "mixedLeftId"; "mixedRightId"; "mixedAssoc"; "mixedCongr"; "wideRelation"; "wideAssoc";
  "heterogeneousNatInput"; "pathTrans"; "IndexedEnd"; "DoubleEnd"; "Constant"; "Components";
  "componentLaw"; "Alpha"; "BetaComponents"; "betaComponentLaw"; "Beta"; "componentValue";
  "identityValue"; "verticalForward"; "verticalReverse"; "verticalNested"; "naturalityWitness";
  "LeftIdentity"; "RightIdentity"; "AssocLeft"; "AssocRight"; "leftIdentityLaw"; "rightIdentityLaw";
  "assocLaw"; "reflLaw"; "symmLaw"; "transLaw"; "CongrLeft"; "CongrRight"; "congrLaw";
  "certifiedComponent"; "leftIdentityAt"; "rightIdentityAt"; "assocLeftAt"; "assocRightAt";
  "congrLeftAt"; "congrRightAt"]
let negatives = ["false-components"; "one-component"; "nominal-equality";
  "wrong-endpoint"; "trans-middle"; "missing-congruence"]
let paths = ["prelude/cat/category-core.mech"; "prelude/cat/heterogeneous-functor.mech";
  "prelude/cat/heterogeneous-nattrans.mech"; "prelude/cat/nattrans-laws.mech"]

let suite root =
  let* templates = read_all root paths in
  let* fixtures = read_all root ["test/fixtures/prelude/nattrans-laws.mech";
    "test/fixtures/prelude/heterogeneous-nattrans-runtime.mech";
    "test/fixtures/prelude/nattrans-laws-runtime.mech"] in
  let* parsed = kernel (Parser.parse (templates ^ fixtures)) in
  let* roundtrip = kernel (Parser.parse (Syntax.print parsed)) in
  let* () = require "parse/print changed" (parsed = roundtrip) in
  let* globals, rows = kernel (Elab.elab_program_in Global.initial parsed) in
  let expected_entries = List.concat_map (fun instance -> prefix instance members)
      ["Mixed"; "Wide"; "Laws"; "Separate"] @ prefix "Run" operations @ fixture_members
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
  let expected_families = ["Path"; "Point"] @ List.concat_map
    (fun instance -> prefix instance ["Ops_Base_Source"; "Ops_Base_Target"]) ["Mixed"; "Wide"]
    @ ["Run_Base_Source"; "Run_Base_Target"; "Separate_Ops_Base_Source";
       "Separate_Ops_Base_Target"] |> List.sort String.compare in
  let* () = require "shared family inventory changed" (List.map fst families = expected_families) in
  let* () = List.fold_left (fun acc (name, (family : Positivity.family)) -> let* () = acc in
    match family.f_status with
    | Positivity.Complete _ -> require ("unchecked family: " ^ name) family.f_positive
    | Positivity.Provisional | Positivity.Builtin -> Error ("unexpected family: " ^ name))
    (Ok ()) families in
  let functions = ["leftIdentityAt", 1; "rightIdentityAt", 1; "assocLeftAt", 3;
    "assocRightAt", 3; "congrLeftAt", 2; "congrRightAt", 2] in
  let computations = List.concat_map (fun payload ->
    List.map (fun (name, factor) -> name, payload, factor * payload) functions) [37; 41] in
  let* () = List.fold_left (fun acc (name, payload, expected) -> let* () = acc in
    let source = Printf.sprintf "def lawProbe : Nat := %s %d" name payload in
    let* probe, _ = kernel (Elab.check_in globals source) in
    let* value = kernel (Eval.eval probe [] (Term.Global "lawProbe")) in
    let* normal = kernel (Eval.quote probe 0 value) in
    require ("wrong computation: " ^ name ^ "/" ^ string_of_int payload)
      (normal = Term.Lit (Literal.LInt (Bignum.of_int expected)))) (Ok ()) computations in
  let directory = "test/neg/nattrans-laws/" in
  let* files = Mechanism_import.Io.attempt (fun () -> Sys.readdir (Filename.concat root directory)) in
  let present = Array.to_list files |> List.filter (fun name -> Filename.check_suffix name ".mech")
    |> List.map Filename.remove_extension |> List.sort String.compare in
  let* () = require "negative inventory changed" (present = List.sort String.compare negatives) in
  let* () = List.fold_left (fun acc name -> let* () = acc in
    let* negative = read root (directory ^ name ^ ".mech") in
    let* expected = read root (directory ^ name ^ ".err") in
    let expected = String.trim expected in
    let* () = require (name ^ ": empty refusal prefix") (String.length expected > 40) in
    Result.fold (Elab.check_in globals negative)
      ~ok:(fun _ -> Error (name ^ ": expected refusal"))
      ~error:(fun error ->
        let message = Error.to_string error in
        let excerpt = String.of_seq (Seq.take 240 (String.to_seq message)) in
        let () = trace (name ^ ": " ^ excerpt) in
        require (name ^ ": wrong refusal: " ^ message)
          (String.starts_with ~prefix:expected message))) (Ok ()) negatives in
  Ok (List.length rows, List.length families, List.length computations, List.length negatives)

let () =
  let root = Array.to_list Sys.argv |> List.to_seq |> Seq.drop 1 |> Seq.uncons
    |> Option.map fst |> Option.value ~default:"." in
  Result.fold (suite root)
    ~ok:(fun (entries, families, computations, negatives) ->
      Printf.printf "PRELUDE-NATTRANS-LAWS-OK entries=%d families=%d computations=%d negatives=%d\n"
        entries families computations negatives)
    ~error:(fun message -> Printf.printf "PRELUDE-NATTRANS-LAWS-FAIL %s\n" message; exit 1)
