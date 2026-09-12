open Mechanism_kernel
open Mechanism_surface

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let read root path = Mechanism_import.Io.attempt
  (fun () -> In_channel.with_open_bin (Filename.concat root path) In_channel.input_all)

let suite root =
  let* source = List.fold_left (fun acc path -> let* text = acc in
    let* part = read root path in Ok (text ^ part)) (Ok "")
    ["prelude/cat/category.mech"; "test/fixtures/prelude/nattrans.mech";
     "test/fixtures/prelude/left-kan-identity.mech";
     "test/fixtures/prelude/left-kan-common.mech";
     "test/fixtures/prelude/left-kan.mech"] in
  let* parsed = kernel (Parser.parse source) in
  let* printed = kernel (Parser.parse (Syntax.print parsed)) in
  let* () = require "left-kan parse/print changed" (parsed = printed) in
  let* globals, rows = kernel (Elab.check_in Global.empty source) in
  let* () = require "unexpected axioms" (Elab.axiom_names rows = []) in
  let* () = Global.StringMap.fold (fun name entry acc -> let* () = acc in
    match entry with
    | Global.Def _ -> Ok ()
    | Global.Axiom _ | Global.Prim _ -> Error ("trusted entry: " ^ name))
    globals.entries (Ok ()) in
  let* () = Global.StringMap.fold (fun name (family : Positivity.family) acc ->
    let* () = acc in
    match family.f_status with
    | Positivity.Complete _ -> require ("unchecked family: " ^ name) family.f_positive
    | Positivity.Provisional | Positivity.Builtin -> Error ("unexpected family: " ^ name))
    globals.families (Ok ()) in
  let instances = ["Small"; "Types"; "Up"; "Higher"] in
  let installed = Global.StringMap.bindings globals.families |> List.map fst in
  let* () = require "left-kan instance inventory changed"
    (installed = ["Higher"; "Path"; "Point"; "Small"; "Tiny"; "Types"; "Up"]) in
  let* () = require (Printf.sprintf "left-kan entry inventory changed: %d" (List.length rows))
    (List.length rows = 203 && Global.StringMap.cardinal globals.entries = 203) in
  let members = ["recordFirst"; "recordSecond"; "LanCocone"; "LanFactor";
    "LanSolution"; "LanTail"; "LeftKanExtension"; "lanFunctor"; "lanTail";
    "lanUnit"; "lanSolve";
    "lanDesc"; "lanFac"; "lanUniq"; "desc_unique"] in
  let* () = List.fold_left (fun acc instance -> let* () = acc in
    List.fold_left (fun acc member -> let* () = acc in
      require ("missing left-kan member: " ^ instance ^ "_" ^ member)
        (Option.is_some (Global.find (instance ^ "_" ^ member) globals)))
      (Ok ()) members) (Ok ()) instances in
  let zero = Term.In (Shape.SMu ("Tiny", []), Term.ACtor "zero", []) in
  let one = Term.In (Shape.SMu ("Tiny", []), Term.ACtor "next", [zero]) in
  let two = Term.In (Shape.SMu ("Tiny", []), Term.ACtor "next", [one]) in
  let computations = ["lanUnitValue", one; "lanMapValue", two;
    "lanDescValue", one; "lanDescOther", two;
    "lanDescObject", zero; "lanDescSecond", one] in
  let* () = List.fold_left (fun acc (name, expected) -> let* () = acc in
    let* value = kernel (Eval.eval globals [] (Term.Global name)) in
    let* actual = kernel (Eval.quote globals 0 value) in
    require ("wrong computation: " ^ name ^ " = " ^ Pp.term [] actual)
      (actual = expected)) (Ok ()) computations in
  let negatives = ["missing-uniqueness"; "wrong-factor"; "wrong-uniqueness";
    "reversed-unit"; "erased-cocone"; "wrong-order"] in
  let* listed = Mechanism_import.Io.attempt
    (fun () -> Sys.readdir (Filename.concat root "test/neg/left-kan")) in
  let present = Array.to_list listed
    |> List.filter (fun name -> Filename.check_suffix name ".mech")
    |> List.map Filename.remove_extension |> List.sort String.compare in
  let* () = require "left-kan negative inventory changed"
    (present = List.sort String.compare negatives) in
  let* () = List.fold_left (fun acc name -> let* () = acc in
    let path = "test/neg/left-kan/" ^ name in
    let* text = read root (path ^ ".mech") in
    let* expected = read root (path ^ ".err") in
    Result.fold (Elab.check_in globals text)
      ~ok:(fun _ -> Error (name ^ ": expected refusal"))
      ~error:(fun error -> let actual = Error.to_string error ^ "\n" in
        require (name ^ ": " ^ actual) (actual = expected))) (Ok ()) negatives in
  let* () = Result.fold
    (Elab.check_in ~budget:(Budget.of_poll (fun () -> true)) Global.empty source)
    ~ok:(fun _ -> Error "left-kan budget: expected refusal")
    ~error:(fun error -> require "left-kan budget: wrong refusal"
      (error = Error.Budget_exhausted Check.budget_msg)) in
  Ok (List.length rows, List.length instances, List.length computations,
    List.length negatives + 1)

let () =
  let root = Array.to_list Sys.argv |> List.to_seq |> Seq.drop 1 |> Seq.uncons
    |> Option.map fst |> Option.value ~default:"." in
  Result.fold (suite root)
    ~ok:(fun (entries, instances, computations, negatives) ->
      Printf.printf "PRELUDE-LEFT-KAN-OK entries=%d instances=%d computations=%d negatives=%d\n"
        entries instances computations negatives)
    ~error:(fun message -> Printf.printf "PRELUDE-LEFT-KAN-FAIL %s\n" message; exit 1)
