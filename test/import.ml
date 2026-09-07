open Mechanism_import

let ( let* ) = Result.bind

let require message condition = if condition then Ok () else Error message
let quote text = "\"" ^ text ^ "\""
let object_ fields =
  "{" ^ String.concat "," (List.map (fun (key, value) -> quote key ^ ":" ^ value) fields) ^ "}"
let array values = "[" ^ String.concat "," values ^ "]"
let change key value fields = (key, value) :: List.remove_assoc key fields

let metadata version = object_ ["meta", object_ [
  "exporter", object_ ["name", quote "lean4export"; "version", quote "3.1.0"];
  "format", object_ ["version", quote version];
  "lean", object_ ["githash", quote "fixture"; "version", quote "4.31.0"]]]

let name index text = object_ ["in", string_of_int index;
  "str", object_ ["pre", "0"; "str", quote text]]
let expression index kind body = object_ ["ie", string_of_int index; kind, body]
let base = [metadata "3.1.0"; name 1 "D"; name 2 "u"; name 3 "C";
  name 4 "R"; name 5 "T"; "{\"il\":1,\"succ\":0}";
  expression 0 "sort" "0"; expression 1 "bvar" "0"; expression 2 "sort" "1"]
let source rows = String.concat "\n" rows ^ "\n"
let read rows = Export.read_string (source rows)
let diagnostic (error : Export.error) =
  Printf.sprintf "line %d: %s" error.line error.message
let accepted rows = read rows |> Result.map (fun _table -> ()) |> Result.map_error diagnostic
let rejected_at line rows =
  read rows |> Result.fold
    ~ok:(fun _table -> Error "malformed input was accepted")
    ~error:(fun (error : Export.error) ->
      let* () = require ("wrong location: " ^ diagnostic error) (error.line = line) in
      require "empty diagnostic" (String.length error.message > 0))
let reject text = rejected_at (List.length base + 1) (base @ [text])
let negative name text = name, (fun () -> reject text)
let positive name text = name, (fun () -> accepted (base @ [text]))

let common = ["name", "1"; "levelParams", "[2]"; "type", "0"]
let axiom = common @ ["isUnsafe", "false"]
let definition = common @ ["value", "1"; "hints", quote "abbrev";
  "safety", quote "safe"; "all", "[1]"]
let theorem = common @ ["value", "1"; "all", "[1]"]
let opaque = common @ ["value", "1"; "isUnsafe", "false"; "all", "[1]"]
let quotient = common @ ["kind", quote "type"]
let inductive = common @ ["numParams", "0"; "numIndices", "0";
  "all", "[1]"; "ctors", "[3]"; "numNested", "0";
  "isRec", "false"; "isUnsafe", "false"; "isReflexive", "false"]
let constructor = change "name" "3" common @ ["induct", "1";
  "cidx", "0"; "numParams", "0"; "numFields", "0"; "isUnsafe", "false"]
let rule = ["ctor", "3"; "nfields", "0"; "rhs", "1"]
let recursor = change "name" "4" common @ ["all", "[1]";
  "numParams", "0"; "numIndices", "0"; "numMotives", "1"; "numMinors", "1";
  "rules", array [object_ rule]; "k", "false"; "isUnsafe", "false"]
let declaration kind fields = object_ [kind, object_ fields]
let group typ ctor rec_ = declaration "inductive"
  ["types", array [object_ typ]; "ctors", array [object_ ctor]; "recs", array [object_ rec_]]
let binder = ["name", "1"; "type", "0"; "body", "1"; "binderInfo", quote "default"]
let let_ = ["name", "1"; "type", "0"; "value", "1"; "body", "1"; "nondep", "false"]
let projection = ["typeName", "1"; "idx", "0"; "struct", "1"]

let declaration_cases = [
  positive "axiom" (declaration "axiom" axiom);
  positive "definition" (declaration "def" definition);
  positive "theorem" (declaration "thm" theorem);
  positive "opaque" (declaration "opaque" opaque);
  positive "quotient" (declaration "quot" quotient);
  positive "inductive-group" (group inductive constructor recursor);
  negative "declaration-name-forward" (declaration "axiom" (change "name" "6" axiom));
  negative "declaration-type-forward" (declaration "axiom" (change "type" "3" axiom));
  negative "declaration-level-parameter-forward" (declaration "axiom" (change "levelParams" "[6]" axiom));
  negative "axiom-boolean-required" (declaration "axiom" (change "isUnsafe" (quote "false") axiom));
  negative "declaration-missing-field" (declaration "axiom" (List.remove_assoc "type" axiom));
  negative "declaration-extra-field" (declaration "axiom" (("extra", "0") :: axiom));
  negative "definition-value-forward" (declaration "def" (change "value" "3" definition));
  negative "definition-all-forward" (declaration "def" (change "all" "[6]" definition));
  negative "definition-invalid-safety" (declaration "def" (change "safety" (quote "maybe") definition));
  negative "definition-invalid-hints" (declaration "def" (change "hints" (quote "maybe") definition));
  negative "definition-negative-regular-hint" (declaration "def" (change "hints" "{\"regular\":-1}" definition));
  positive "definition-regular-hint" (declaration "def" (change "hints" "{\"regular\":2}" definition));
  negative "theorem-value-forward" (declaration "thm" (change "value" "3" theorem));
  negative "theorem-all-forward" (declaration "thm" (change "all" "[6]" theorem));
  negative "opaque-value-forward" (declaration "opaque" (change "value" "3" opaque));
  negative "opaque-all-forward" (declaration "opaque" (change "all" "[6]" opaque));
  negative "opaque-invalid-bool" (declaration "opaque" (change "isUnsafe" "1" opaque));
  negative "quotient-invalid-kind" (declaration "quot" (change "kind" (quote "elim") quotient));
  negative "inductive-all-forward" (group (change "all" "[6]" inductive) constructor recursor);
  negative "inductive-ctors-forward" (group (change "ctors" "[6]" inductive) constructor recursor);
  negative "inductive-negative-count" (group (change "numIndices" "-1" inductive) constructor recursor);
  negative "constructor-induct-forward" (group inductive (change "induct" "6" constructor) recursor);
  negative "constructor-type-forward" (group inductive (change "type" "3" constructor) recursor);
  negative "recursor-all-forward" (group inductive constructor (change "all" "[6]" recursor));
  negative "recursor-rule-ctor-forward" (group inductive constructor
    (change "rules" (array [object_ (change "ctor" "6" rule)]) recursor));
  negative "recursor-rule-rhs-forward" (group inductive constructor
    (change "rules" (array [object_ (change "rhs" "3" rule)]) recursor));
  negative "recursor-rule-negative-fields" (group inductive constructor
    (change "rules" (array [object_ (change "nfields" "-1" rule)]) recursor));
  negative "recursor-invalid-k" (group inductive constructor (change "k" (quote "false") recursor));
  negative "constructor-index-mismatch" (group inductive (change "cidx" "1" constructor) recursor);
  negative "constructor-outside-ctors-list" (group (change "ctors" "[1]" inductive) constructor recursor);
  negative "recursor-rule-field-mismatch" (group inductive constructor
    (change "rules" (array [object_ (change "nfields" "2" rule)]) recursor));
  negative "recursor-rule-names-non-constructor" (group inductive constructor
    (change "rules" (array [object_ (change "ctor" "4" rule)]) recursor));
  "duplicate-declaration-id", (fun () -> rejected_at (List.length base + 2)
    (base @ [declaration "axiom" axiom; declaration "axiom" axiom]));
  "duplicate-declaration-via-name-alias", (fun () -> rejected_at (List.length base + 3)
    (base @ [declaration "axiom" axiom; name 6 "D";
      declaration "axiom" (change "name" "6" axiom)]));
  "numeric-and-string-name-identity", (fun () ->
    let* table = read (base @ [name 6 "1";
      "{\"in\":7,\"num\":{\"pre\":0,\"i\":1}}";
      declaration "axiom" (change "name" "6" axiom);
      declaration "axiom" (change "name" "7" axiom)]) |> Result.map_error diagnostic in
    let* () = require "string name segment changed" (Export.name table 6 = Some (Names.Str (0, "1"))) in
    let* () = require "numeric name segment changed" (Export.name table 7 = Some (Names.Num (0, 1))) in
    require "distinct names lost a declaration" ((Export.counts table).declarations = 2));
  "group-counts-all-members", (fun () ->
    let* table = read (base @ [group inductive constructor recursor]) |> Result.map_error diagnostic in
    let* () = require "group members missing" ((Export.counts table).declarations = 3) in
    require "group identity lost" (List.length (Export.groups table) = 1));
  "dotted-component-and-name-path-identity", (fun () ->
    let* table = read (base @ [name 6 "x.y"; name 7 "x";
      "{\"in\":8,\"str\":{\"pre\":7,\"str\":\"y\"}}";
      declaration "axiom" (change "name" "6" axiom);
      declaration "axiom" (change "name" "8" axiom)]) |> Result.map_error diagnostic in
    let* () = require "dotted component structure changed"
      (Export.name table 6 = Some (Names.Str (0, "x.y"))) in
    let* () = require "nested name structure changed"
      (Export.name table 8 = Some (Names.Str (7, "y"))) in
    let* component = Export.name_string table 6 |> Option.to_result ~none:"missing dotted component" in
    let* nested = Export.name_string table 8 |> Option.to_result ~none:"missing nested name" in
    require "distinct name structures have the same display" (not (String.equal component nested)));
]

let document_cases = [
  "empty-input", (fun () -> Export.read_string "" |> Result.fold
    ~ok:(fun _table -> Error "zero-byte input was accepted")
    ~error:(fun (error : Export.error) ->
      require "zero-byte diagnostic lost its location" (error.line = 1 && String.length error.message > 0)));
  "newline-only-input", (fun () -> rejected_at 1 []);
  "blank-first-line", (fun () -> rejected_at 1 [""; metadata "3.1.0"]);
  "version-3.2.0", (fun () -> rejected_at 1 [metadata "3.2.0"]);
  "version-empty", (fun () -> rejected_at 1 [metadata ""]);
  "missing-metadata", (fun () -> rejected_at 1 [name 1 "D"]);
  "metadata-only", (fun () -> accepted [metadata "3.1.0"]);
  "duplicate-metadata", (fun () -> rejected_at 2 [metadata "3.1.0"; metadata "3.1.0"]);
  negative "blank-record" "";
  negative "non-object" "[]";
  negative "unknown-record" "{\"alien\":1}";
  negative "duplicate-json-key" "{\"in\":6,\"in\":7,\"str\":{\"pre\":0,\"str\":\"x\"}}";
  negative "duplicate-nested-key" "{\"in\":6,\"str\":{\"pre\":0,\"pre\":1,\"str\":\"x\"}}";
  negative "trailing-garbage" "{\"ie\":3,\"bvar\":0}x";
  negative "trailing-comma" "{\"ie\":3,\"bvar\":0,}";
  negative "invalid-escape" "{\"in\":6,\"str\":{\"pre\":0,\"str\":\"\\q\"}}";
  negative "lone-high-surrogate" "{\"in\":6,\"str\":{\"pre\":0,\"str\":\"\\uD800\"}}";
  negative "lone-low-surrogate" "{\"in\":6,\"str\":{\"pre\":0,\"str\":\"\\uDC00\"}}";
  negative "bad-surrogate-pair" "{\"in\":6,\"str\":{\"pre\":0,\"str\":\"\\uD800\\u0041\"}}";
  negative "raw-control-string" ("{\"in\":6,\"str\":{\"pre\":0,\"str\":\"" ^ "\001" ^ "\"}}");
  positive "valid-surrogate-pair" "{\"in\":6,\"str\":{\"pre\":0,\"str\":\"\\uD83D\\uDE00\"}}";
  positive "valid-json-escapes" "{\"ie\":3,\"strVal\":\"\\\"\\\\\\/\\b\\f\\n\\r\\t\\u0041\"}";
  negative "fractional-index" "{\"ie\":3.5,\"bvar\":0}";
  negative "boolean-index" "{\"ie\":true,\"bvar\":0}";
  negative "oversized-index" "{\"ie\":999999999999999999999999999999,\"bvar\":0}";
  negative "leading-zero-number" "{\"ie\":03,\"bvar\":0}";
  negative "negative-index" "{\"ie\":-1,\"bvar\":0}";
  negative "duplicate-name-id" (name 1 "other");
  positive "name-table-alias" (name 6 "D");
  negative "duplicate-level-id" "{\"il\":1,\"succ\":1}";
  negative "duplicate-expression-id" (expression 0 "sort" "0");
  negative "reserved-name-zero" (name 0 "zero");
  negative "reserved-level-zero" "{\"il\":0,\"succ\":0}";
  negative "name-forward-reference" "{\"in\":6,\"str\":{\"pre\":7,\"str\":\"x\"}}";
  negative "name-self-reference" "{\"in\":6,\"str\":{\"pre\":6,\"str\":\"x\"}}";
  negative "name-negative-number" "{\"in\":6,\"num\":{\"pre\":0,\"i\":-1}}";
  negative "name-two-forms" "{\"in\":6,\"str\":{\"pre\":0,\"str\":\"x\"},\"num\":{\"pre\":0,\"i\":3}}";
]

let node_cases = [
  negative "level-succ-forward" "{\"il\":2,\"succ\":3}";
  negative "level-max-forward" "{\"il\":2,\"max\":[0,3]}";
  negative "level-imax-forward" "{\"il\":2,\"imax\":[3,0]}";
  negative "level-param-forward" "{\"il\":2,\"param\":6}";
  negative "level-max-arity" "{\"il\":2,\"max\":[0,1,1]}";
  negative "level-negative-reference" "{\"il\":2,\"succ\":-1}";
  positive "level-max" "{\"il\":2,\"max\":[0,1]}";
  positive "level-imax" "{\"il\":2,\"imax\":[1,0]}";
  positive "level-param" "{\"il\":2,\"param\":2}";
  negative "sort-forward-level" (expression 3 "sort" "2");
  negative "bvar-negative" (expression 3 "bvar" "-1");
  negative "const-forward-name" (expression 3 "const" "{\"name\":6,\"us\":[0]}");
  negative "const-forward-level" (expression 3 "const" "{\"name\":1,\"us\":[2]}");
  negative "app-forward-function" (expression 3 "app" "{\"fn\":4,\"arg\":1}");
  negative "app-forward-argument" (expression 3 "app" "{\"fn\":0,\"arg\":4}");
  negative "expression-self-reference" (expression 3 "app" "{\"fn\":3,\"arg\":1}");
  positive "const" (expression 3 "const" "{\"name\":1,\"us\":[0,1]}");
  positive "app" (expression 3 "app" "{\"fn\":0,\"arg\":1}");
  positive "lambda" (expression 3 "lam" (object_ binder));
  positive "forall" (expression 3 "forallE" (object_ binder));
  negative "lambda-invalid-binder" (expression 3 "lam" (object_ (change "binderInfo" (quote "unknown") binder)));
  negative "lambda-forward-name" (expression 3 "lam" (object_ (change "name" "6" binder)));
  negative "lambda-forward-type" (expression 3 "lam" (object_ (change "type" "4" binder)));
  negative "forall-forward-body" (expression 3 "forallE" (object_ (change "body" "4" binder)));
  positive "let" (expression 3 "letE" (object_ let_));
  negative "let-forward-value" (expression 3 "letE" (object_ (change "value" "4" let_)));
  negative "let-invalid-bool" (expression 3 "letE" (object_ (change "nondep" "0" let_)));
  positive "projection" (expression 3 "proj" (object_ projection));
  negative "projection-forward-name" (expression 3 "proj" (object_ (change "typeName" "6" projection)));
  negative "projection-forward-structure" (expression 3 "proj" (object_ (change "struct" "4" projection)));
  negative "projection-negative-field" (expression 3 "proj" (object_ (change "idx" "-1" projection)));
  positive "large-natural-literal" (expression 3 "natVal" (quote "9999999999999999999999999999999999999999"));
  negative "natural-number-not-string" (expression 3 "natVal" "1");
  negative "natural-negative" (expression 3 "natVal" (quote "-1"));
  negative "natural-empty" (expression 3 "natVal" (quote ""));
  positive "string-literal" (expression 3 "strVal" (quote "hello"));
  positive "metadata-expression" (expression 3 "mdata" "{\"expr\":0,\"data\":{\"x\":[null,1,true]}}");
  negative "metadata-expression-forward" (expression 3 "mdata" "{\"expr\":4,\"data\":{}}");
  negative "metadata-expression-duplicate-key" (expression 3 "mdata" "{\"expr\":0,\"data\":{\"x\":0,\"x\":1}}");
  negative "metadata-expression-bad-data" (expression 3 "mdata" "{\"expr\":0,\"data\":[]}");
]

module Name_set = Set.Make (String)

let name_of table id = Export.name_string table id |> Option.to_result ~none:"missing name after read"
let inhouse name = List.exists
  (fun prefix -> String.equal name prefix || String.starts_with ~prefix:(prefix ^ ".") name)
  ["UnifiedAggregation"; "ArrowCat"; "CompCatTheory"]

let corpus path =
  let* table = Export.read_file path |> Result.map_error diagnostic in
  let counts = Export.counts table in
  let* () = require "frozen table counts changed"
    (counts.lines = 219778 && counts.names = 17759 && counts.levels = 146
     && counts.exprs = 198927 && counts.declarations = 3202) in
  let declarations = Export.declarations table in
  let expected = ["axiom", 3; "def", 1176; "thm", 1649; "opaque", 1;
    "quot", 4; "inductive", 112; "constructor", 143; "recursor", 114] in
  let* () = List.fold_left (fun result (kind, count) ->
    let* () = result in
    let actual = List.fold_left (fun total (decl : Decls.t) ->
      if String.equal kind (Decls.kind_name decl.kind) then total + 1 else total) 0 declarations in
    require (Printf.sprintf "kind %s expected %d got %d" kind count actual) (actual = count)) (Ok ()) expected in
  let* external_declarations, axioms = List.fold_left
    (fun result (decl : Decls.t) ->
      let* external_count, axioms = result in
      let* name = name_of table decl.name in
      let external_count = if inhouse name then external_count else external_count + 1 in
      let axioms = if String.equal (Decls.kind_name decl.kind) "axiom" then name :: axioms else axioms in
      Ok (external_count, axioms)) (Ok (0, [])) declarations in
  let* () = require "external declaration denominator changed" (external_declarations = 2543) in
  let* () = require "axiom set changed"
    (List.sort String.compare axioms = ["Classical.choice"; "Quot.sound"; "propext"]) in
  let* constants = List.fold_left (fun result id ->
    let* constants = result in
    let* expr = Export.expr table id
      |> Option.to_result ~none:(Printf.sprintf "missing frozen expression %d" id) in
    match expr with
    | Exprs.Const {name; universes = _universes} ->
        let* name = name_of table name in Ok (Name_set.add name constants)
    | Exprs.Bvar _ | Exprs.Sort _ | Exprs.App _ | Exprs.Lam _
    | Exprs.Forall _ | Exprs.Let _ | Exprs.Proj _ | Exprs.Nat _
    | Exprs.String _ | Exprs.Mdata _ -> Ok constants)
    (Ok Name_set.empty) (List.init counts.exprs Fun.id) in
  let external_count = Name_set.cardinal (Name_set.filter (fun name -> not (inhouse name)) constants) in
  let* () = require "const-node denominator changed"
    (Name_set.cardinal constants = 3017 && external_count = 2477) in
  Printf.printf "CORPUS-IMPORT declarations=3202 external=2477 external_declared=2543 axioms=3\n%!";
  Ok ()

let run cases =
  let failures = List.fold_left (fun failures (name, test) ->
    test () |> Result.fold
      ~ok:(fun () -> Printf.printf "PASS import %s\n%!" name; failures)
      ~error:(fun message -> Printf.printf "FAIL import %s: %s\n%!" name message; failures + 1)) 0 cases in
  require (Printf.sprintf "%d of %d cases failed" failures (List.length cases)) (failures = 0)

let () =
  let cases = document_cases @ node_cases @ declaration_cases in
  let additional = match Array.to_list Sys.argv with
    | [_program] -> Ok []
    | [_program; "--corpus"; path] -> Ok ["frozen-corpus", (fun () -> corpus path)]
    | [] | _ :: _ -> Error "usage: import.exe [--corpus FILE]" in
  (let* additional = additional in run (cases @ additional))
  |> Result.fold
    ~ok:(fun () -> print_endline "IMPORT-OK")
    ~error:(fun message -> prerr_endline ("IMPORT-FAIL " ^ message); exit 1)
