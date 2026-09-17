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
let sides = ["Source"; "Middle"; "Target"]
let edges = ["First"; "Second"; "Composite"]
let heterogeneous = prefix "Source" core @ prefix "Target" core @ functor_members
let members = List.concat_map (fun side -> prefix side core) sides
  @ List.concat_map (fun edge -> prefix edge (prefix "Base" heterogeneous @ nat)) edges
  @ prefix "Compose" (List.concat_map (fun side -> prefix ("Base_" ^ side) core) sides
    @ List.concat_map (fun edge -> prefix ("Base_" ^ edge) functor_members) edges
    @ ["Base_compFunctor"; "targetEqSymm"; "whiskerRight"; "whiskerLeft"]
    @ List.concat_map (fun edge -> prefix edge ["NatTrans"; "natApp"; "naturality"]) edges)
  @ ["hcomp"]
let fixture_members = ["mixedFirstSort"; "mixedSecondSort"; "mixedCompositeSort";
  "mixedRight"; "mixedLeft"; "mixedHorizontal"; "wideHorizontal";
  "sharedNatInput"; "pathTrans";
  "IndexedEnd"; "End"; "PairEnd"; "Constant"; "Left"; "Right"; "components";
  "componentLaw"; "Alpha"; "Beta"; "Gamma"; "FH"; "GI"; "Horizontal";
  "Vertical"; "Identity"; "horizontalFirst"; "horizontalSecond"; "verticalValue";
  "identityValue"; "naturalityWitness"]
let negatives = ["horizontal-endpoint"; "vertical-middle"; "missing-naturality";
  "nominal-equality"; "first-sort"; "erased-component"]
let paths = ["prelude/cat/category-core.mech"; "prelude/cat/heterogeneous-functor.mech";
  "prelude/cat/composable-functors.mech"; "prelude/cat/heterogeneous-nattrans.mech";
  "prelude/cat/heterogeneous-whiskering.mech"; "prelude/cat/shared-nattrans.mech"]

let suite root =
  let* templates = read_all root paths in
  let* fixtures = read_all root ["test/fixtures/prelude/shared-nattrans.mech";
    "test/fixtures/prelude/shared-nattrans-runtime.mech"] in
  let* parsed = kernel (Parser.parse (templates ^ fixtures)) in
  let* roundtrip = kernel (Parser.parse (Syntax.print parsed)) in
  let* () = require "parse/print changed" (parsed = roundtrip) in
  let* globals, rows = kernel (Elab.elab_program_in Global.initial parsed) in
  let expected_entries = List.concat_map (fun instance -> prefix instance members)
      ["Mixed"; "Wide"; "Run"] @ prefix "External" heterogeneous @ fixture_members
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
    (fun instance -> prefix instance sides) ["Mixed"; "Wide"; "Run"] |> List.sort String.compare in
  let* () = require "shared family inventory changed" (List.map fst families = expected_families) in
  let* () = List.fold_left (fun acc (name, (family : Positivity.family)) -> let* () = acc in
    match family.f_status with
    | Positivity.Complete _ -> require ("unchecked family: " ^ name) family.f_positive
    | Positivity.Provisional | Positivity.Builtin -> Error ("unexpected family: " ^ name))
    (Ok ()) families in
  let computations = ["horizontalFirst", 37; "horizontalSecond", 43;
    "verticalValue", 48; "identityValue", 37] in
  let* () = List.fold_left (fun acc (name, expected) -> let* () = acc in
    let* value = kernel (Eval.eval globals [] (Term.Global name)) in
    let* normal = kernel (Eval.quote globals 0 value) in
    require ("wrong computation: " ^ name)
      (normal = Term.Lit (Literal.LInt (Bignum.of_int expected)))) (Ok ()) computations in
  let directory = "test/neg/shared-nattrans/" in
  let* files = Mechanism_import.Io.attempt (fun () -> Sys.readdir (Filename.concat root directory)) in
  let present = Array.to_list files |> List.filter (fun name -> Filename.check_suffix name ".mech")
    |> List.map Filename.remove_extension |> List.sort String.compare in
  let* () = require "negative inventory changed" (present = List.sort String.compare negatives) in
  let* () = List.fold_left (fun acc name -> let* () = acc in
    let* negative = read root (directory ^ name ^ ".mech") in
    let* expected = read root (directory ^ name ^ ".err") in
    let expected = String.trim expected in
    let* () = require (name ^ ": empty refusal prefix") (String.length expected > 20) in
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
      Printf.printf "PRELUDE-SHARED-NATTRANS-OK entries=%d families=%d computations=%d negatives=%d\n"
        entries families computations negatives)
    ~error:(fun message -> Printf.printf "PRELUDE-SHARED-NATTRANS-FAIL %s\n" message; exit 1)
