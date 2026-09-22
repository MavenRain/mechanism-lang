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
  let* source = acc in let* next = read root path in Ok (source ^ "\n" ^ next)) (Ok "") paths
let prefix name members = List.map (fun member -> name ^ "_" ^ member) members
let core = ["Category"; "Hom"; "id"; "comp"; "idComp"; "compId"; "assoc"; "eqTrans"; "eqCongr"]
let functor_members = ["Functor"; "functorObj"; "functorMap"; "functorMapId"; "functorMapComp"; "eqCongr"]
let sides = ["Source"; "Middle"; "Target"]
let edges = ["First"; "Second"; "Composite"]
let whiskering_members = prefix "Base"
  (List.concat_map (fun side -> prefix side core) sides
   @ List.concat_map (fun edge -> prefix edge functor_members) edges @ ["compFunctor"])
  @ List.concat_map (fun edge -> prefix edge ["NatTrans"; "natApp"; "naturality"]) edges
  @ ["targetEqSymm"; "whiskerRight"; "whiskerLeft"]
let nattrans_members = prefix "Base"
  (prefix "Source" core @ prefix "Target" core @ functor_members)
  @ ["targetEqSymm"; "NatTrans"; "natApp"; "naturality"; "idNat"; "vcompLaw"; "vcomp"]
let triangle_members = List.concat_map (fun side -> prefix side core) sides
  @ List.concat_map (fun edge -> prefix edge nattrans_members) edges
  @ prefix "Compose" whiskering_members @ ["hcomp"]
let roles = ["Source"; "Target"]
let members = List.concat_map (fun side -> prefix side core) roles
  @ List.concat_map (fun identity -> prefix identity (prefix "Base" core @ ["Functor"; "idFunctor"]))
      ["SourceIdentity"; "TargetIdentity"]
  @ List.concat_map (fun triangle -> prefix triangle triangle_members) ["Before"; "After"]
  @ ["whiskerRightIdFunctor"; "whiskerLeftIdFunctor"; "hcompIdFunctorLeft"; "hcompIdFunctorRight"]
let fixture_members = ["heterogeneousNatInput"; "pathTrans"; "IndexedEnd"; "DoubleEnd";
  "Constant"; "Components"; "componentLaw"; "Alpha"; "BetaComponents"; "betaComponentLaw";
  "Beta"; "componentValue"; "identityValue"; "verticalForward"; "verticalReverse";
  "verticalNested"; "naturalityWitness"; "PreUnit"; "PostUnit"; "LeftUnit"; "RightUnit";
  "preUnitLaw"; "postUnitLaw"; "leftUnitLaw"; "rightUnitLaw"; "unitCertified";
  "preUnitAt"; "postUnitAt"; "leftUnitAt"; "rightUnitAt"; "unitReferenceAt";
  "leftUnitLawAbstract"; "rightUnitLawAbstract"; "preUnitLawAbstract"; "postUnitLawAbstract"]
let negatives = ["false-components"; "wrong-object"; "nominal-target";
  "incompatible-endpoints"; "left-reflexivity"; "right-reflexivity"]
let reflexivity_negatives = ["left-reflexivity"; "right-reflexivity"]
let paths = ["prelude/cat/category-core.mech"; "prelude/cat/heterogeneous-functor.mech";
  "prelude/cat/composable-functors.mech"; "prelude/cat/heterogeneous-nattrans.mech";
  "prelude/cat/heterogeneous-whiskering.mech"; "prelude/cat/shared-nattrans.mech";
  "prelude/cat/identity-functor.mech"; "prelude/cat/nattrans-units.mech";
  "test/fixtures/prelude/nattrans-units.mech"]
let functions = List.map (fun name -> name, 203, 707)
  ["preUnitAt"; "postUnitAt"; "leftUnitAt"; "rightUnitAt"; "unitReferenceAt"]

let suite root =
  let* templates = read_all root paths in
  let* fixture = read_all root ["test/fixtures/prelude/heterogeneous-nattrans-runtime.mech";
    "test/fixtures/prelude/nattrans-units-runtime.mech"] in
  let* parsed = kernel (Parser.parse (templates ^ "\n" ^ fixture)) in
  let* roundtrip = kernel (Parser.parse (Syntax.print parsed)) in
  let* () = require "parse/print changed" (parsed = roundtrip) in
  let definition_bodies = List.filter_map (fun (decl : Syntax.decl) -> match decl with
    | Syntax.DDef (name, _, body) -> Some (name, Syntax.at 0 body)
    | Syntax.DPoly _ | Syntax.DPolyMu _ | Syntax.DPolyGroup _ | Syntax.DPolyCompose _
    | Syntax.DSpecialize _ | Syntax.DAxiom _ | Syntax.DMu _ | Syntax.DRec _ -> None) parsed in
  let identifier_char c = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
    || (c >= '0' && c <= '9') || Char.equal c '_' in
  let identifiers name = List.assoc_opt name definition_bodies |> Option.value ~default:""
    |> String.map (fun c -> if identifier_char c then c else ' ')
    |> String.split_on_char ' ' |> List.filter (fun token -> not (String.equal token "")) in
  let* () = List.fold_left (fun acc (name, head) -> let* () = acc in
    require ("unit head changed: " ^ name)
      (match identifiers name with first :: _ -> String.equal first head | [] -> false))
    (Ok ()) ["PreUnit", "Units_Before_Compose_whiskerRight"; "PostUnit", "Units_After_Compose_whiskerLeft";
      "LeftUnit", "Units_Before_hcomp"; "RightUnit", "Units_After_hcomp"] in
  let* () = List.fold_left (fun acc (case, unit) -> let* () = acc in
    let mentioned = identifiers (case ^ "UnitAt") in
    require ("case rebound: " ^ case ^ "UnitAt")
      (List.mem unit mentioned && List.mem (case ^ "UnitLaw") mentioned))
    (Ok ()) ["pre", "PreUnit"; "post", "PostUnit"; "left", "LeftUnit"; "right", "RightUnit"] in
  let template_types = List.concat_map (fun (decl : Syntax.decl) -> match decl with
    | Syntax.DPolyCompose (_, name, _, defs) when String.equal name "MechNatTransUnits" ->
      List.map (fun (member : Syntax.rec_def) -> member.rd_name, Syntax.at 0 member.rd_ty) defs
    | Syntax.DPolyGroup (_, head, _, defs) when String.equal head.Syntax.fm_name "MechNatTransUnits" ->
      List.map (fun (member : Syntax.rec_def) -> member.rd_name, Syntax.at 0 member.rd_ty) defs
    | Syntax.DPolyCompose _ | Syntax.DPolyGroup _ | Syntax.DDef _ | Syntax.DPoly _ | Syntax.DPolyMu _
    | Syntax.DSpecialize _ | Syntax.DAxiom _ | Syntax.DMu _ | Syntax.DRec _ -> []) parsed in
  let normalise text = String.map (fun c ->
      if Char.equal c '\t' || Char.equal c '\n' then ' ' else c) text
    |> String.split_on_char ' ' |> List.filter (fun token -> not (String.equal token ""))
    |> String.concat " " in
  let type_text member =
    List.assoc_opt member template_types |> Option.value ~default:"" |> normalise in
  (* Exact statement pins, written by hand from prelude/cat/nattrans-units.mech.
     The printer names universe parameters by index: u0 is u, u2 is w. *)
  let whisker_right_statement = "(0 C : Sort (succ u0)) -> (0 D : Sort (succ u2)) -> \
      (c : Source_Category C) -> (d : Target_Category D) -> \
      (F : Before_Second_Base_Functor C D c d) -> (G : Before_Second_Base_Functor C D c d) -> \
      (alpha : Before_Second_NatTrans C D c d F G) -> (x : C) -> \
      Target (Target_Hom D d (F.1 x) (G.1 x)) \
      ((Before_Compose_whiskerRight C C D c c d F G alpha (SourceIdentity_idFunctor C c)).1 x) \
      (alpha.1 x)" in
  let whisker_left_statement = "(0 C : Sort (succ u0)) -> (0 D : Sort (succ u2)) -> \
      (c : Source_Category C) -> (d : Target_Category D) -> \
      (F : After_First_Base_Functor C D c d) -> (G : After_First_Base_Functor C D c d) -> \
      (alpha : After_First_NatTrans C D c d F G) -> (x : C) -> \
      Target (Target_Hom D d (F.1 x) (G.1 x)) \
      ((After_Compose_whiskerLeft C D D c d d (TargetIdentity_idFunctor D d) F G alpha).1 x) \
      (alpha.1 x)" in
  let* () = List.fold_left (fun acc (member, statement) -> let* () = acc in
    require ("whisker law restated: " ^ member) (String.equal (type_text member) statement))
    (Ok ()) ["whiskerRightIdFunctor", whisker_right_statement;
      "whiskerLeftIdFunctor", whisker_left_statement] in
  let* globals, rows = kernel (Elab.elab_program_in Global.initial parsed) in
  let () = trace "templates and clients checked" in
  let expected_entries = List.concat_map (fun instance -> prefix instance members) ["UnitsMixed"; "UnitsWide"; "Units"]
    @ prefix "Run" nattrans_members @ prefix "SeparateTarget" core @ fixture_members |> List.sort String.compare in
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
  let expected_families = ["Point"; "Path"; "SeparateTarget"; "Run_Base_Source"; "Run_Base_Target"]
    @ List.concat_map (fun instance -> prefix instance roles)
      ["UnitsMixed"; "UnitsWide"] |> List.sort String.compare in
  let* () = require "shared family inventory changed" (List.map fst families = expected_families) in
  let* () = List.fold_left (fun acc (name, (family : Positivity.family)) -> let* () = acc in
    match family.f_status with
    | Positivity.Complete _ -> require ("unchecked family: " ^ name) family.f_positive
    | Positivity.Provisional | Positivity.Builtin -> Error ("unexpected family: " ^ name))
    (Ok ()) families in
  let computations = List.concat_map (fun payload ->
    List.map (fun (name, scale, offset) -> name, payload, scale * payload + offset) functions) [0; 37; 41] in
  let* () = List.fold_left (fun acc (name, payload, expected) -> let* () = acc in
    let source = Printf.sprintf "def lawProbe : Nat := %s %d" name payload in
    let* probe, _ = kernel (Elab.check_in globals source) in
    let* value = kernel (Eval.eval probe [] (Term.Global "lawProbe")) in
    let* normal = kernel (Eval.quote probe 0 value) in
    require ("wrong computation: " ^ name ^ "/" ^ string_of_int payload)
      (normal = Term.Lit (Literal.LInt (Bignum.of_int expected)))) (Ok ()) computations in
  let () = trace "computations checked" in
  let directory = "test/neg/nattrans-units/" in
  let* files = Mechanism_import.Io.attempt (fun () -> Sys.readdir (Filename.concat root directory)) in
  let present = Array.to_list files |> List.filter (fun name -> Filename.check_suffix name ".mech")
    |> List.map Filename.remove_extension |> List.sort String.compare in
  let* () = require "negative inventory changed" (present = List.sort String.compare negatives) in
  let* fingerprints = List.fold_left (fun acc name -> let* found = acc in
    let* fingerprint = read root (directory ^ name ^ ".digest") in
    Ok (String.trim fingerprint :: found)) (Ok []) negatives in
  let* () = require "refusal digests are not pairwise distinct"
    (List.length (List.sort_uniq String.compare fingerprints) = List.length negatives) in
  let* failures = List.fold_left (fun acc name -> let* failures = acc in
    let* negative = read root (directory ^ name ^ ".mech") in
    let* expected = read root (directory ^ name ^ ".err") in
    let* fingerprint = read root (directory ^ name ^ ".digest") in
    let expected = String.trim expected in
    let fingerprint = String.trim fingerprint in
    let* () = require (name ^ ": invalid refusal oracle")
      (String.length expected > 0 && String.length fingerprint = 32) in
    Result.fold (Elab.check_in globals negative)
      ~ok:(fun _ -> Ok ((name ^ ": expected refusal") :: failures))
      ~error:(fun error ->
        let message = Error.to_string error in
        let excerpt = String.of_seq (Seq.take 256 (String.to_seq message)) in
        let window = if List.mem name reflexivity_negatives then message else excerpt in
        let digest = Digest.to_hex (Digest.string window) in
        let () = trace (name ^ " digest=" ^ digest ^ " prefix=" ^ excerpt) in
        let nominal_target_present = not (String.equal name "nominal-target")
          || List.mem "SeparateTarget" (String.split_on_char ' ' message) in
        if String.starts_with ~prefix:expected message && String.equal digest fingerprint && nominal_target_present
        then Ok failures else Ok ((name ^ ": diagnostic changed") :: failures)))
    (Ok []) negatives in
  let* () = require (String.concat "; " (List.rev failures)) (failures = []) in
  Ok (List.length rows, List.length families, List.length computations, List.length negatives)

let () =
  let root = Array.to_list Sys.argv |> List.to_seq |> Seq.drop 1 |> Seq.uncons
    |> Option.map fst |> Option.value ~default:"." in
  Result.fold (suite root)
    ~ok:(fun (entries, families, computations, negatives) ->
      Printf.printf "PRELUDE-NATTRANS-UNITS-OK entries=%d families=%d computations=%d negatives=%d\n"
        entries families computations negatives)
    ~error:(fun message -> Printf.printf "PRELUDE-NATTRANS-UNITS-FAIL %s\n" message; exit 1)
