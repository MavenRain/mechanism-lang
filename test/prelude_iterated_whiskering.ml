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
let triangle_members = prefix "Base"
  (List.concat_map (fun side -> prefix side core) sides
   @ List.concat_map (fun edge -> prefix edge functor_members) edges @ ["compFunctor"])
  @ List.concat_map (fun edge -> prefix edge ["NatTrans"; "natApp"; "naturality"]) edges
  @ ["targetEqSymm"; "whiskerRight"; "whiskerLeft"]
let roles = ["Source"; "Middle1"; "Middle2"; "Target"]
let members = List.concat_map (fun side -> prefix side core) roles
  @ List.concat_map (fun triangle -> prefix triangle triangle_members) ["ABC"; "BCD"; "ACD"; "ABD"]
  @ ["whiskerRightComp"; "whiskerLeftComp"; "whiskerCommute"]
let clone witness suffixes = List.map (fun suffix -> witness ^ suffix) suffixes
let hops = [""; "M1M2"; "M2T"]
let witnesses = clone "Indexed" ["Source"; "Middle1"; "Middle2"]
  @ clone "End" ["Middle1"; "Middle2"; "Target"] @ clone "PairEnd" ["Middle2"; "Target"]
let fixture_members = ["pathTrans"; "pathCongr"] @ witnesses @ ["IndexedMap"; "Shift"; "Stretch"]
  @ clone "Constant" hops @ ["Left"; "Right"; "swap"; "Conjugate"; "components"]
  @ List.concat_map (fun hop -> ["componentLaw" ^ hop; "Alpha" ^ hop]) hops
  @ ["rightLhs"; "rightRhs"; "rightLaw"; "leftLhs"; "leftRhs"; "leftLaw"; "mixedLhs"; "mixedRhs"; "mixedLaw";
     "certified"; "pairCode"; "rightLeftAt"; "rightRightAt"; "leftLeftAt"; "leftRightAt"; "mixedLeftAt"; "mixedRightAt"]
let negatives = ["false-components"; "wrong-precomposition-order"; "missing-postcomposition";
  "missing-precomposition"; "nominal-target"; "incompatible-endpoints"]
let paths = ["prelude/cat/category-core.mech"; "prelude/cat/heterogeneous-functor.mech";
  "prelude/cat/composable-functors.mech"; "prelude/cat/heterogeneous-whiskering.mech";
  "prelude/cat/iterated-whiskering.mech"; "test/fixtures/prelude/iterated-whiskering.mech"]
let functions = ["rightLeftAt", 4, 32; "rightRightAt", 4, 32;
  "leftLeftAt", 101, 1314; "leftRightAt", 101, 1314;
  "mixedLeftAt", 101, 1317; "mixedRightAt", 101, 1317]

let suite root =
  let* templates = read_all root paths in
  let* fixture = read root "test/fixtures/prelude/iterated-whiskering-runtime.mech" in
  let* parsed = kernel (Parser.parse (templates ^ "\n" ^ fixture)) in
  let* roundtrip = kernel (Parser.parse (Syntax.print parsed)) in
  let* () = require "parse/print changed" (parsed = roundtrip) in
  let* declarations = kernel (Parser.parse fixture) in
  let header_size = List.length roles + 2 in
  let header = List.filteri (fun index _ -> index < header_size) declarations in
  let definitions = List.filteri (fun index _ -> index >= header_size) declarations in
  let* () = require "runtime fixture header missing" (List.length header = header_size) in
  let* template_declarations = kernel (Parser.parse templates) in
  let* globals, rows = kernel (Elab.elab_program_in Global.initial (template_declarations @ header)) in
  let () = trace "specializations checked" in
  let* globals, rows = List.fold_left (fun acc declaration ->
    let* globals, rows = acc in
    let printed = Syntax.print [declaration] in
    let label = String.of_seq (Seq.take 100 (String.to_seq printed)) in
    let* globals, next = Elab.elab_program_in globals [declaration]
      |> Result.map_error (fun error -> label ^ ": " ^ Error.to_string error) in
    Ok (globals, rows @ next)) (Ok (globals, rows)) definitions in
  let expected_entries = List.concat_map (fun instance -> prefix instance members) ["Mixed"; "Wide"; "Run"]
    @ List.concat_map (fun role -> prefix role core) roles @ prefix "SeparateTarget" core @ fixture_members |> List.sort String.compare in
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
  let expected_families = ["Path"; "SeparateTarget"] @ roles
    @ List.concat_map (fun instance -> prefix instance roles)
      ["Mixed"; "Wide"] |> List.sort String.compare in
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
  let directory = "test/neg/iterated-whiskering/" in
  let* files = Mechanism_import.Io.attempt (fun () -> Sys.readdir (Filename.concat root directory)) in
  let present = Array.to_list files |> List.filter (fun name -> Filename.check_suffix name ".mech")
    |> List.map Filename.remove_extension |> List.sort String.compare in
  let* () = require "negative inventory changed" (present = List.sort String.compare negatives) in
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
        let digest = Digest.to_hex (Digest.string excerpt) in
        let () = trace (name ^ " digest=" ^ digest ^ " prefix=" ^ excerpt) in
        if String.starts_with ~prefix:expected message && String.equal digest fingerprint
        then Ok failures else Ok ((name ^ ": diagnostic changed") :: failures)))
    (Ok []) negatives in
  let* () = require (String.concat "; " (List.rev failures)) (failures = []) in
  Ok (List.length rows, List.length families, List.length computations, List.length negatives)

let () =
  let root = Array.to_list Sys.argv |> List.to_seq |> Seq.drop 1 |> Seq.uncons
    |> Option.map fst |> Option.value ~default:"." in
  Result.fold (suite root)
    ~ok:(fun (entries, families, computations, negatives) ->
      Printf.printf "PRELUDE-ITERATED-WHISKERING-OK entries=%d families=%d computations=%d negatives=%d\n"
        entries families computations negatives)
    ~error:(fun message -> Printf.printf "PRELUDE-ITERATED-WHISKERING-FAIL %s\n" message; exit 1)
