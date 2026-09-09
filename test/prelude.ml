open Mechanism_kernel

let ( let* ) = Result.bind
let require message condition = if condition then Ok () else Error message
let read path =
  Mechanism_import.Io.attempt (fun () -> In_channel.with_open_bin path In_channel.input_all)

let checked globals text =
  Mechanism_surface.Elab.check_in globals text
  |> Result.map_error Error.to_string

let no_postulates (globals : Global.t) =
  Global.StringMap.bindings globals.entries
  |> List.fold_left (fun result (name, entry) ->
      let* () = result in
      match entry with
      | Global.Def _ -> Ok ()
      | Global.Axiom _ -> Error ("prelude postulate: " ^ name)
      | Global.Prim _ -> Error ("prelude primitive: " ^ name)) (Ok ())

let audit text =
  let* globals, _rows = checked Global.empty text in
  let* () = no_postulates globals in
  Ok globals

let run_cases cases =
  List.fold_left (fun failures (name, test) ->
      test () |> Result.fold
        ~ok:(fun () -> Printf.printf "PASS prelude %s\n%!" name; failures)
        ~error:(fun message ->
          Printf.printf "FAIL prelude %s: %s\n%!" name message;
          failures + 1)) 0 cases

let suite root =
  let path relative = Filename.concat root relative in
  let* source = read (path "prelude/init.mech") in
  let* globals = audit source in
  let family name =
    let* family = Global.find_family name globals
      |> Option.to_result ~none:("missing checked family: " ^ name) in
    match family.Positivity.f_status with
    | Positivity.Complete _ -> require ("nonpositive family: " ^ name) family.f_positive
    | Positivity.Provisional | Positivity.Builtin ->
        Error ("family was not defined by the prelude: " ^ name)
  in
  let negative name =
    let base = path ("test/neg/prelude/" ^ name) in
    let* source = read (base ^ ".mech") in
    let* expected = read (base ^ ".err") in
    let expected = String.trim expected in
    let* () = require "empty expected diagnostic" (String.length expected > 0) in
    checked globals source |> Result.fold
      ~ok:(fun _checked -> Error "negative source was accepted")
      ~error:(fun message -> require ("wrong diagnostic: " ^ message)
          (String.starts_with ~prefix:expected message))
  in
  let name_only_targets text =
    String.split_on_char '\n' text
    |> List.filter_map (fun line ->
        match String.split_on_char '\t' line with
        | [ _lean_name; target; "NAME_ONLY" ] -> Some target
        | [] | _ :: _ -> None)
  in
  let declared name =
    Global.StringMap.mem name globals.entries
    || Global.StringMap.mem name globals.families
    || Global.StringMap.exists
         (fun _family_name family -> Option.is_some (Positivity.ctor_of name family))
         globals.families
  in
  let cases = [
    "empty-environment", (fun () ->
      let empty : Global.t = Global.empty in
      let* () = require "the initial environment holds an entry"
        (Global.StringMap.is_empty empty.entries) in
      require "the initial environment holds a family"
        (Global.StringMap.is_empty empty.families));
    "map-name-only-targets", (fun () ->
      let* text = read (path "map/prelude.map.tsv") in
      let targets = name_only_targets text in
      let* () = require "no NAME_ONLY row in the inventory" (targets <> []) in
      List.fold_left (fun result target ->
          let* () = result in
          require ("NAME_ONLY target is not a checked prelude name: " ^ target)
            (declared target)) (Ok ()) targets);
    "reject-postulate", (fun () ->
      audit (source ^ "\naxiom CounterfeitInit : Type 0\n")
      |> Result.fold ~ok:(fun _globals -> Error "a postulate escaped the audit")
        ~error:(fun message -> require "wrong postulate diagnostic"
            (message = "prelude postulate: CounterfeitInit")));
    "reject-hidden-builtin", (fun () ->
      checked Global.empty "def HiddenBuiltin : Type 0 := Nat\n"
      |> Result.fold ~ok:(fun _checked -> Error "an initial axiom was available")
        ~error:(fun message -> require ("wrong hidden builtin diagnostic: " ^ message)
            (message = "unbound: Nat")));
    "duplicate-constructor-name", (fun () ->
      let source = String.concat "\n"
        [ "mu AltSum (0 A : Type 0) : Type 0 with";
          "| mechInl : A -> AltSum A"; "";
          "def stillSum : MechSum MechNat MechUnit := mechInl mechZero";
          "def altUse : AltSum MechBool := mechInl mechTrue"; "" ] in
      checked globals source |> Result.fold ~ok:(fun _checked -> Ok ())
        ~error:(fun message ->
          Error ("a duplicate constructor name was refused: " ^ message)));
    "client", (fun () ->
      let* client = read (path "test/fixtures/prelude/client.mech") in
      let* final, _rows = checked globals client in
      no_postulates final);
    "equality-client", (fun () ->
      let* client = read (path "test/fixtures/prelude/equality.mech") in
      let* final, _rows = checked globals client in
      no_postulates final);
  ] @ List.map (fun name -> "family-" ^ name, fun () -> family name)
    [ "MechNat"; "MechBool"; "MechUnit"; "MechEmpty"; "MechFalse";
      "MechSum"; "MechDecidable"; "MechTrue"; "MechEq"; "MechProofEq" ]
    @ List.map (fun name -> "reject-" ^ name, fun () -> negative name)
      [ "eq-relevant-index"; "eq-erased-endpoint"; "eq-wrong-endpoint";
        "eq-multiple-large-elim"; "eq-constructor-field"; "eq-type-index";
        "nat-wrong-constructor";
        "nat-rec-wrong-step"; "proof-eq-large-elim"; "proof-eq-wrong-endpoint";
        "sum-missing-field"; "sum-wrong-parameter"; "sum-wrong-nested";
        "sum-wrong-family"; "decidable-wrong-proof"; "dependent-wrong-field";
        "indexed-wrong-result" ] in
  let failures = run_cases cases in
  let* () = require (Printf.sprintf "%d prelude cases failed" failures) (failures = 0) in
  Printf.printf "PRELUDE-OK families=%d definitions=%d axioms=0 primitives=0\n"
    (Global.StringMap.cardinal globals.families)
    (Global.StringMap.cardinal globals.entries);
  Ok ()

let () =
  let result = match Array.to_list Sys.argv with
    | [ _program; "--audit"; file ] ->
        let* text = read file in
        let* _globals = audit text in
        print_endline "PRELUDE-AXIOMS OK";
        Ok ()
    | [ _program; root ] -> suite root
    | [] | [ _ ] | _ :: _ :: _ :: _ ->
        Error "usage: prelude ROOT | prelude --audit FILE"
  in
  Result.iter_error (fun message -> prerr_endline message; exit 1) result
