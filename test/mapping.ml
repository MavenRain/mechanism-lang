open Mechanism_import

let ( let* ) = Result.bind
let require message condition = if condition then Ok () else Error message
let int n = Ndjson.Number (Int.to_string n)
let str s = Ndjson.String s
let obj fields = Ndjson.Object fields
let name index pre text = obj ["in", int index; "str", obj ["pre", int pre; "str", str text]]
let constant index name = obj ["ie", int index; "const", obj ["name", int name; "us", Ndjson.Array []]]
let axiom name = obj ["axiom", obj ["name", int name; "levelParams", Ndjson.Array [];
    "type", int 0; "isUnsafe", Ndjson.Bool false]]
let header = obj ["meta", obj [
    "exporter", obj ["name", str "lean4export"; "version", str "3.1.0"];
    "format", obj ["version", str "3.1.0"];
    "lean", obj ["githash", str "test"; "version", str "4.31.0"]]]

let source () =
  let lines = [header;
    name 1 0 "Zeta"; name 2 0 "Alpha"; name 3 0 "Alpha";
    name 4 0 "Unused"; name 5 0 "UnifiedAggregation"; name 6 5 "inside";
    name 7 0 "UnifiedAggregationExtra"; name 8 7 "outside";
    name 9 0 "Lean"; name 10 9 "Omega"; name 11 10 "reflect";
    name 12 9 "OmegaExtra"; name 13 12 "ordinary";
    name 14 0 "_private"; name 15 14 "Init"; name 16 15 "Omega"; name 17 16 "helper";
    name 18 9 "Syntax"; name 19 0 "Array"; name 20 19 "push";
    name 21 0 "ArrowCat"; name 22 21 "inside";
    name 23 0 "CompCatTheory"; name 24 23 "inside";
    obj ["ie", int 0; "sort", int 0];
    constant 1 1; constant 2 2; constant 3 3; constant 4 6;
    constant 5 8; constant 6 11; constant 7 13; constant 8 17;
    constant 9 18; constant 10 20; constant 11 22; constant 12 24]
    @ List.map axiom [1; 2; 4; 6; 8; 11; 13; 17; 18; 20; 22; 24] in
  Export.read_string (String.concat "\n" (List.map Ndjson.to_string lines) ^ "\n")
  |> Result.map_error (fun (error : Export.error) -> error.message)

let inventory ?targets () =
  let* source = source () in Mapping.inventory ?targets source
let row name (inventory : Mapping.t) =
  List.find_opt (fun (row : Mapping.row) -> row.lean_name = name) inventory.rows
  |> Option.to_result ~none:("missing row " ^ name)
let refusal targets =
  let* source = source () in
  Mapping.inventory ~targets source |> Result.fold
    ~ok:(fun _ -> Error "expected target refusal") ~error:(fun _ -> Ok ())

let cases = [
  "canonical-identities-and-stable-order", (fun () ->
    let* inventory = inventory () in
    require "alias, namespace exclusion, sort, or reference denominator changed"
      (List.map (fun (row : Mapping.row) -> row.lean_name) inventory.rows =
        ["Alpha"; "Array.push"; "Lean.Omega.reflect"; "Lean.OmegaExtra.ordinary";
         "Lean.Syntax"; "UnifiedAggregationExtra.outside"; "Zeta"; "_private.Init.Omega.helper"]));
  "declared-and-referenced-counts-stay-distinct", (fun () ->
    let* inventory = inventory () in
    require "declaration-only name or aliases changed the denominator"
      (List.length inventory.rows = 8 && inventory.external_declared = 9 && inventory.const_names = 11));
  "only-exact-public-omega-namespace-is-never", (fun () ->
    let* inventory = inventory () in
    require "NEVER classification broadened"
      (inventory.never = [{ Mapping.lean_name = "Lean.Omega.reflect";
          ratification = "R-V4:omega-reflection"; milestone = "NEVER" }]));
  "candidate-never-claims-type-parity", (fun () ->
    let* inventory = inventory ~targets:["Alpha", "MechAlpha"] () in
    let* mapped = row "Alpha" inventory in
    let* missing = row "Zeta" inventory in
    require "candidate or missing target verdict changed"
      (mapped.target = Some "MechAlpha" && mapped.verdict = Mapping.Name_only
       && missing.target = None && missing.verdict = Mapping.Unmapped));
  "duplicate-source-target-refused", (fun () -> refusal ["Alpha", "One"; "Alpha", "Two"]);
  "unknown-source-target-refused", (fun () -> refusal ["Missing", "One"]);
  "unreferenced-source-target-refused", (fun () -> refusal ["Unused", "One"]);
  "in-house-source-target-refused", (fun () -> refusal ["UnifiedAggregation.inside", "One"]);
  "never-target-conflict-refused", (fun () -> refusal ["Lean.Omega.reflect", "One"]);
  "tab-target-refused", (fun () -> refusal ["Alpha", "bad\ttarget"]);
  "newline-target-refused", (fun () -> refusal ["Alpha", "bad\ntarget"]);
  "empty-target-refused", (fun () -> refusal ["Alpha", ""]);
  "sentinel-target-refused", (fun () -> refusal ["Alpha", "-"]);
  "tsv-schema-is-stable", (fun () ->
    let* first = inventory ~targets:["Zeta", "MechZeta"; "Alpha", "MechAlpha"] () in
    let* reordered = inventory ~targets:["Alpha", "MechAlpha"; "Zeta", "MechZeta"] () in
    require "output order, header, or NEVER link changed"
      (Mapping.map_tsv first = Mapping.map_tsv reordered
       && String.starts_with ~prefix:"lean_name\ttarget\tverdict\nAlpha\tMechAlpha\tNAME_ONLY\n"
            (Mapping.map_tsv first)
       && Mapping.never_tsv first = "lean_name\tratification\tmilestone\nLean.Omega.reflect\tR-V4:omega-reflection\tNEVER\n"));
]

let () =
  let failures = List.fold_left (fun failures (name, test) ->
      test () |> Result.fold
        ~ok:(fun () -> Printf.printf "PASS mapping %s\n%!" name; failures)
        ~error:(fun message -> Printf.printf "FAIL mapping %s: %s\n%!" name message;
          (name ^ ": " ^ message) :: failures)) [] cases in
  if List.length failures = 0 then print_endline "MAPPING-OK"
  else (prerr_endline ("MAPPING-FAIL " ^ String.concat "; " (List.rev failures)); exit 1)
