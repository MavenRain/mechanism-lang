open Mechanism_kernel
open Mechanism_surface

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let trace message =
  if Array.exists (String.equal "--trace") Sys.argv then Printf.eprintf "TRACE %s\n%!" message
let read root path = Mechanism_import.Io.attempt (fun () ->
  In_channel.with_open_bin (Filename.concat root path) In_channel.input_all)
let common_prefix left right = Seq.zip (String.to_seq left) (String.to_seq right)
  |> Seq.take_while (fun (a, b) -> Char.equal a b) |> Seq.map fst |> String.of_seq
let prefix name members = List.map (fun member -> name ^ "_" ^ member) members
let core = ["Category"; "Hom"; "id"; "comp"; "idComp"; "compId"; "assoc"; "eqTrans"; "eqCongr"]
let functor_members = ["Functor"; "functorObj"; "functorMap"; "functorMapId"; "functorMapComp"; "eqCongr"]
let nat = ["targetEqSymm"; "NatTrans"; "natApp"; "naturality"; "idNat"; "vcompLaw"; "vcomp"]
let sides = ["Source"; "Middle"; "Target"]
let edges = ["First"; "Second"; "Composite"]
let heterogeneous = prefix "Source" core @ prefix "Target" core @ functor_members
let shared_nat_members = List.concat_map (fun side -> prefix side core) sides
  @ List.concat_map (fun edge -> prefix edge (prefix "Base" heterogeneous @ nat)) edges
  @ prefix "Compose" (List.concat_map (fun side -> prefix ("Base_" ^ side) core) sides
    @ List.concat_map (fun edge -> prefix ("Base_" ^ edge) functor_members) edges
    @ ["Base_compFunctor"; "targetEqSymm"; "whiskerRight"; "whiskerLeft"]
    @ List.concat_map (fun edge -> prefix edge ["NatTrans"; "natApp"; "naturality"]) edges)
  @ ["hcomp"]
let whiskering = List.concat_map (fun side -> prefix ("Base_" ^ side) core) sides
  @ List.concat_map (fun edge -> prefix ("Base_" ^ edge) functor_members) edges
  @ ["Base_compFunctor"; "targetEqSymm"; "whiskerRight"; "whiskerLeft"]
  @ List.concat_map (fun edge -> prefix edge ["NatTrans"; "natApp"; "naturality"]) edges
let lan_members = prefix "Base" whiskering
  @ ["recordFirst"; "recordSecond"; "coconeFirst"; "coconeSecond";
     "LanCocone"; "LanFactor"; "LanSolution"; "LanTail"; "LeftKanExtension";
     "lanFunctor"; "lanTail"; "lanUnit"; "lanSolve"; "lanDesc"; "lanFac";
     "lanUniq"; "desc_unique"]
let shared_members = prefix "Base" shared_nat_members @ prefix "Lan" lan_members
  @ ["unitNat"; "descNat"]
let laws_members = ["CoconeEq"; "descId"; "descCongr"]
let shared_fixture = ["sharedLanInput"; "pathTrans"; "IndexedEnd"; "DoubleEnd"; "IndexedPlain";
  "Constant"; "Components"; "componentLaw"; "Alpha"; "Forget"; "Lift"; "OtherComponents";
  "otherComponentLaw"; "Beta"; "forgetUnit"; "forgetLan"; "Original"; "Extension"; "Eta";
  "Cocone"; "OtherCocone"; "Chosen"; "OtherChosen"; "Mediator"; "OtherMediator";
  "lanUnitResult"; "lanMapResult"; "lanDescFirst"; "lanDescSecond"; "factorWitness";
  "externalFactorWitness"; "uniqueWitness"; "ComposedMediator"; "lanVerticalFirst";
  "lanVerticalSecond"]
let laws_fixture = ["WideIdContract"; "WideCongrContract"; "IdentityChosen"; "IdentityMediator";
  "identityLaw"; "AlternativeChosen"; "otherExternalFactor"; "AlternativeOtherChosen";
  "congrLaw"; "otherCongrLaw"; "lanCertified"; "lanIdentityAt"; "lanIdentityReferenceAt";
  "lanCongrAt"; "lanCongrReferenceAt"; "lanOtherCongrAt"; "lanOtherCongrReferenceAt";
  "lanIdentity"; "lanIdentityReference"; "lanCongr"; "lanCongrReference"; "lanOtherCongr";
  "lanOtherCongrReference"]
let expected_entries = prefix "Run" shared_members
  @ List.concat_map (fun name -> prefix name heterogeneous)
    ["ExternalFirst"; "ExternalSecond"; "ExternalComposite"]
  @ prefix "ExternalNat" (prefix "Base" heterogeneous @ nat)
  @ shared_fixture
  @ List.concat_map (fun instance -> prefix instance (laws_members @ prefix "Base" shared_members))
    ["Wide"; "Laws"]
  @ prefix "SeparateTarget" core @ laws_fixture
  |> List.sort String.compare
let paths = ["prelude/cat/category-core.mech"; "prelude/cat/heterogeneous-functor.mech";
  "prelude/cat/composable-functors.mech"; "prelude/cat/heterogeneous-nattrans.mech";
  "prelude/cat/heterogeneous-whiskering.mech"; "prelude/cat/shared-nattrans.mech";
  "prelude/cat/heterogeneous-left-kan.mech"; "prelude/cat/shared-left-kan.mech";
  "prelude/cat/left-kan-laws.mech"; "test/fixtures/prelude/left-kan-laws.mech";
  "test/fixtures/prelude/shared-left-kan-runtime.mech";
  "test/fixtures/prelude/left-kan-laws-runtime.mech"]
let negatives = ["unequal-cocones"; "wrong-solution"; "missing-equality";
  "wrong-identity"; "nominal-equality"; "wrong-conclusion"]
let computations = ["lanIdentity", 4492; "lanIdentityReference", 4492;
  "lanCongr", 3822; "lanCongrReference", 3822;
  "lanOtherCongr", 4937; "lanOtherCongrReference", 4937]

let suite root =
  let* source = List.fold_left (fun acc path ->
    let* text = acc in let* part = read root path in Ok (text ^ "\n" ^ part)) (Ok "") paths in
  let* parsed = kernel (Parser.parse source) in
  let* roundtrip = kernel (Parser.parse (Syntax.print parsed)) in
  let* () = require "parse/print changed" (parsed = roundtrip) in
  let* globals, rows = kernel (Elab.elab_program_in Global.initial parsed) in
  let* () = require "unexpected axioms" (Elab.axiom_names rows = []) in
  let* () = Global.StringMap.fold (fun name entry acc -> let* () = acc in
    Option.fold (Global.find name Global.initial)
      ~some:(fun initial () -> require ("builtin changed: " ^ name) (initial = entry))
      ~none:(fun () -> match entry with
        | Global.Def _ -> Ok ()
        | Global.Axiom _ | Global.Prim _ -> Error ("trusted entry: " ^ name)) ())
    globals.entries (Ok ()) in
  let* () = List.fold_left (fun acc instance -> let* () = acc in
    List.fold_left (fun acc member -> let* () = acc in
      let name = instance ^ "_" ^ member in
      require ("missing checked law: " ^ name) (List.mem_assoc name rows)) (Ok ())
      ["CoconeEq"; "descId"; "descCongr"]) (Ok ()) ["Laws"; "Wide"] in
  let* () = List.fold_left (fun acc name -> let* () = acc in
    require ("missing symbolic pin: " ^ name) (List.mem_assoc name rows)) (Ok ())
    ["WideIdContract"; "WideCongrContract"] in
  let inventory = List.sort String.compare (List.map fst rows) in
  let missing = List.filter (fun name -> not (List.mem name inventory)) expected_entries in
  let extra = List.filter (fun name -> not (List.mem name expected_entries)) inventory in
  let* () = require (Printf.sprintf "entry inventory changed: %d missing=[%s] extra=[%s]"
      (List.length rows) (String.concat "; " missing) (String.concat "; " extra))
    (inventory = expected_entries) in
  let families = Global.StringMap.bindings globals.families
    |> List.filter (fun (name, _) -> not (Global.StringMap.mem name Global.initial.families)) in
  let expected_families = ["Path"; "Point"; "Run_Base_Source"; "Run_Base_Middle";
    "Run_Base_Target"; "SeparateTarget"; "Wide_Base_Base_Source";
    "Wide_Base_Base_Middle"; "Wide_Base_Base_Target"] |> List.sort String.compare in
  let* () = require "family reuse changed" (List.map fst families = expected_families) in
  let* () = List.fold_left (fun acc (name, (family : Positivity.family)) -> let* () = acc in
    match family.f_status with
    | Positivity.Complete _ -> require ("unchecked family: " ^ name) family.f_positive
    | Positivity.Provisional | Positivity.Builtin -> Error ("unexpected family: " ^ name))
    (Ok ()) families in
  let* () = List.fold_left (fun acc (name, expected) -> let* () = acc in
    let* value = kernel (Eval.eval globals [] (Term.Global name)) in
    let* normal = kernel (Eval.quote globals 0 value) in
    require ("wrong computation: " ^ name)
      (normal = Term.Lit (Literal.LInt (Bignum.of_int expected)))) (Ok ()) computations in
  let directory = "test/neg/left-kan-laws/" in
  let* files = Mechanism_import.Io.attempt (fun () -> Sys.readdir (Filename.concat root directory)) in
  let present = Array.to_list files |> List.filter (fun name -> Filename.check_suffix name ".mech")
    |> List.map Filename.remove_extension |> List.sort String.compare in
  let* () = require "negative inventory changed" (present = List.sort String.compare negatives) in
  let* cases = List.fold_left (fun acc name -> let* cases = acc in
    let* negative = read root (directory ^ name ^ ".mech") in
    let* expected = read root (directory ^ name ^ ".err") in
    Ok ((name, negative, String.trim expected) :: cases)) (Ok []) negatives in
  let cases = List.rev cases in
  let* () = require "refusal pins are not pairwise distinct"
    (List.length (List.sort_uniq String.compare (List.map (fun (_, _, expected) -> expected) cases))
      = List.length cases) in
  let shared = List.fold_left (fun acc (_, _, expected) ->
    Option.fold ~none:(Some expected) ~some:(fun head -> Some (common_prefix head expected)) acc)
    None cases |> Option.value ~default:"" in
  let* () = List.fold_left (fun acc (name, negative, expected) -> let* () = acc in
    let* () = require (name ^ ": empty refusal prefix") (String.length expected > 64) in
    let* () = require (name ^ ": refusal prefix is the head every negative shares")
      (String.length expected > String.length shared) in
    Result.fold (Elab.check_in globals negative)
      ~ok:(fun _ -> Error (name ^ ": expected refusal"))
      ~error:(fun error ->
        let message = Error.to_string error in
        let excerpt = String.of_seq (Seq.take 240 (String.to_seq message)) in
        let () = trace (name ^ ": " ^ excerpt) in
        require (name ^ ": wrong refusal: " ^ message)
          (String.starts_with ~prefix:expected message))) (Ok ()) cases in
  Ok (List.length rows, List.length families, List.length computations, List.length negatives)

let () =
  let root = Array.to_list Sys.argv |> List.to_seq |> Seq.drop 1 |> Seq.uncons
    |> Option.map fst |> Option.value ~default:"." in
  Result.fold (suite root)
    ~ok:(fun (entries, families, computations, negatives) ->
      Printf.printf "PRELUDE-LEFT-KAN-LAWS-OK entries=%d families=%d computations=%d negatives=%d\n"
        entries families computations negatives)
    ~error:(fun message -> Printf.printf "PRELUDE-LEFT-KAN-LAWS-FAIL %s\n" message; exit 1)
