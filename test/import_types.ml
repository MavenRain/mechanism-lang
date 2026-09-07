open Mechanism_import
open Mechanism_kernel

let ( let* ) = Result.bind
let require message condition = if condition then Ok () else Error message
let int value = Ndjson.Number (Int.to_string value)
let str value = Ndjson.String value
let obj fields = Ndjson.Object fields
let array values = Ndjson.Array values
let ints values = array (List.map int values)
let node index key value = obj ["ie", int index; key, value]
let sort index level = node index "sort" (int level)
let bound index variable = node index "bvar" (int variable)
let app index head argument = node index "app" (obj ["fn", int head; "arg", int argument])
let constant index name universes = node index "const" (obj ["name", int name; "us", ints universes])
let binder tag index typ body = node index tag (obj [
  "name", int 3; "type", int typ; "body", int body; "binderInfo", str "default"])
let name index value = obj ["in", int index; "str", obj ["pre", int 0; "str", str value]]
let axiom name parameters typ = obj ["axiom", obj [
  "name", int name; "levelParams", ints parameters; "type", int typ; "isUnsafe", Ndjson.Bool false]]
let level index key value = obj ["il", int index; key, value]

let header = obj ["meta", obj [
  "exporter", obj ["name", str "lean4export"; "version", str "3.1.0"];
  "format", obj ["version", str "3.1.0"];
  "lean", obj ["githash", str "68218e876d2a38b1985b8590fff244a83c321783"; "version", str "4.31.0"]]]

let prefix = [header; name 1 "u"; name 2 "A"; name 3 "x"; name 4 "test";
  name 5 "F"; name 6 "v"; name 7 "Nat";
  level 1 "succ" (int 0); level 2 "param" (int 1); level 3 "param" (int 6);
  level 4 "imax" (ints [2; 3]); level 5 "succ" (int 4);
  level 6 "max" (ints [2; 3])]

let read lines =
  Export.read_string (String.concat "\n" (List.map Ndjson.to_string (prefix @ lines)) ^ "\n")
let parse lines = read lines |> Result.map_error (fun (e : Export.error) -> e.message)
let translate lines =
  let* source = parse lines in
  Translate.translate source |> Result.map_error (fun (e : Export.error) -> e.message)
let row table = List.find_opt (fun (row : Translate.row) -> row.name = "test") (Translate.rows table)
  |> Option.to_result ~none:"missing test declaration"
let missing ~name ~levels:_levels = Error ("no checked mapping for " ^ name)
let lower ?(resolve = missing) table =
  let* declaration = row table |> Result.map_error (fun message -> Translate.Unsupported message) in
  Translate.lower_type ~resolve Global.initial table declaration
let successful result = Result.map_error Translate.lowering_message result
let kernel result = Result.map_error Error.to_string result

let expect_semantic prefix lines =
  let* source = parse lines in
  Translate.translate source |> Result.fold
    ~ok:(fun _table -> Error ("expected semantic refusal: " ^ prefix))
    ~error:(fun (error : Export.error) ->
      require ("wrong semantic refusal: " ^ error.message)
        (error.line > 0 && String.starts_with ~prefix error.message))

let expect_lower kind result =
  result |> Result.fold ~ok:(fun _term -> Error ("expected " ^ kind))
    ~error:(fun error ->
      let actual = match error with
        | Translate.Deferred _message -> "deferred"
        | Translate.Unsupported _message -> "unsupported"
        | Translate.Kernel_error _error -> "kernel" in
      require ("wrong lowering refusal: " ^ Translate.lowering_message error) (actual = kind))

let equivalent expected actual =
  let context = Check.make Global.initial Budget.unlimited in
  let* left = kernel (Eval.eval Global.initial [] expected) in
  let* right = kernel (Eval.eval Global.initial [] actual) in
  let* same = kernel (Check.ensure context left right) in
  Ok same

let cases = [
  "closed-dependent-pi", (fun () ->
    let* table = translate [sort 1 1; bound 2 0; binder "forallE" 3 1 2; axiom 4 [] 3] in
    let* term = successful (lower table) in
    equivalent (Rules.arrow Quantity.Many "x" (Term.Univ Level.one) (Term.Var 0)) term);
  "local-application-infers-domain", (fun () ->
    let* table = translate [sort 1 0; sort 2 1; bound 3 0;
      binder "forallE" 4 2 2; app 5 3 1; binder "forallE" 6 4 5; axiom 4 [] 6] in
    let* term = successful (lower table) in
    let* _ty = kernel (Check.infer_term Global.initial term) in Ok ());
  "lambda-result-type-and-application", (fun () ->
    let* table = translate [sort 1 1; bound 2 0; binder "lam" 3 1 2;
      app 4 3 2; binder "forallE" 5 1 4; axiom 4 [] 5] in
    let* term = successful (lower table) in
    equivalent (Rules.arrow Quantity.Many "x" (Term.Univ Level.one) (Term.Var 0)) term);
  "let-preserves-dependent-scope", (fun () ->
    let let_term = node 4 "letE" (obj ["name", int 3; "type", int 2;
      "value", int 1; "body", int 3; "nondep", Ndjson.Bool false]) in
    let* table = translate [sort 1 0; sort 2 1; bound 3 0; let_term; axiom 4 [] 4] in
    let* term = successful (lower table) in equivalent (Term.Univ Level.zero) term);
  "unbound-term-index", (fun () -> expect_semantic "type of test has 1 unbound"
    [bound 1 0; axiom 4 [] 1]);
  "binder-domain-is-outside-own-scope", (fun () -> expect_semantic "type of test has 1 unbound"
    [bound 1 0; sort 2 1; binder "forallE" 3 1 2; axiom 4 [] 3]);
  "shared-node-scope-is-per-root", (fun () -> expect_semantic "type of test has 1 unbound"
    [sort 1 1; bound 2 0; binder "forallE" 3 1 2; axiom 5 [] 3; axiom 4 [] 2]);
  "unbound-universe", (fun () -> expect_semantic "type of test has unbound universe"
    [sort 1 2; axiom 4 [] 1]);
  "universe-instantiation-arity", (fun () -> expect_semantic "constant F expects 1 universe"
    [sort 1 2; constant 2 5 []; axiom 5 [1] 1; axiom 4 [] 2]);
  "duplicate-universe-parameters", (fun () -> expect_semantic "duplicate universe parameters"
    [sort 1 2; axiom 4 [1; 1] 1]);
  "unknown-source-constant", (fun () -> expect_semantic "type references undeclared constant F"
    [constant 1 5 []; axiom 4 [] 1]);
  "mapping-is-explicit-even-for-nat", (fun () ->
    let* table = translate [sort 1 1; constant 2 7 []; axiom 7 [] 1; axiom 4 [] 2] in
    expect_lower "deferred" (lower table));
  "resolver-cannot-return-an-unknown-global", (fun () ->
    let* table = translate [sort 1 1; constant 2 5 []; axiom 5 [] 1; axiom 4 [] 2] in
    expect_lower "kernel" (lower ~resolve:(fun ~name:_name ~levels:_levels ->
      Ok (Term.Global "uninstalled")) table));
  "resolver-cannot-capture-a-local", (fun () ->
    let* table = translate [sort 1 1; constant 2 5 []; binder "forallE" 3 1 2;
      axiom 5 [] 1; axiom 4 [] 3] in
    expect_lower "kernel" (lower ~resolve:(fun ~name:_name ~levels:_levels -> Ok (Term.Var 0)) table));
  "non-type-root-is-refused", (fun () ->
    let* table = translate [node 1 "natVal" (str "13"); axiom 4 [] 1] in
    expect_lower "kernel" (lower table));
  "string-refusal-retains-node", (fun () ->
    let* table = translate [node 1 "strVal" (str "retained"); axiom 4 [] 1] in
    let* () = require "string node vanished" (List.mem (1, Translate.String "retained") (Translate.nodes table)) in
    expect_lower "unsupported" (lower table));
  "projection-needs-target-representation", (fun () ->
    let projection = node 2 "proj" (obj ["typeName", int 5; "idx", int 0; "struct", int 1]) in
    let* table = translate [sort 1 1; projection; axiom 5 [] 1; axiom 4 [] 2] in
    expect_lower "unsupported" (lower table));
]

let extra_cases = [
  "resolver-receives-exact-prenex-instantiation", (fun () ->
    let* table = translate [sort 1 1; constant 2 5 [5; 6]; axiom 5 [1; 6] 1; axiom 4 [6; 1] 2] in
    let* u = Level.var 1 |> Option.to_result ~none:"missing universe variable" in
    let* v = Level.var 0 |> Option.to_result ~none:"missing universe variable" in
    let expected = [Level.succ (Level.imax u v); Level.max u v] in
    let calls = ref 0 in
    let resolve ~name ~levels =
      incr calls;
      let* () = require "wrong source reference or universe substitution"
        (name = "F" && List.equal Level.equal levels expected) in
      Ok (Term.Univ Level.zero) in
    let* term = successful (lower ~resolve table) in
    let* () = require "resolver did not run once" (!calls = 1) in
    equivalent (Term.Univ Level.zero) term);
  "aliased-universe-identity", (fun () ->
    let* table = translate [name 8 "u"; sort 1 2; axiom 4 [8] 1] in
    let* declaration = row table in
    let* () = require "parameter identity was not canonical" (declaration.parameters = [1, "u"]) in
    let* _term = successful (lower table) in Ok ());
  "aliased-duplicate-universe-identity", (fun () ->
    expect_semantic "duplicate universe parameters"
      [name 8 "u"; sort 1 2; axiom 4 [1; 8] 1]);
  "metadata-is-preserved-and-transparent", (fun () ->
    let data = ["opaque", obj ["array", array [str "x"; Ndjson.Bool true]]] in
    let metadata = node 2 "mdata" (obj ["expr", int 1; "data", obj data]) in
    let* table = translate [sort 1 0; metadata; axiom 4 [] 2] in
    let* () = require "metadata payload changed"
      (List.mem (2, Translate.Metadata { expr = 1; data }) (Translate.nodes table)) in
    let* term = successful (lower table) in equivalent (Term.Univ Level.zero) term);
  "natural-literal-in-dependent-type", (fun () ->
    let* table = translate [sort 1 1; constant 2 7 []; binder "forallE" 3 2 1;
      bound 4 0; node 5 "natVal" (str "123456789012345678901234567890");
      app 6 4 5; binder "forallE" 7 3 6; axiom 7 [] 1; axiom 4 [] 7] in
    let resolve ~name ~levels =
      let* () = require "unexpected Nat resolution" (name = "Nat" && levels = []) in
      Ok Prim.nat_ty in
    let* _term = successful (lower ~resolve table) in Ok ());
  "application-checks-argument-domain", (fun () ->
    let* table = translate [sort 1 1; binder "forallE" 2 1 1; bound 3 0;
      node 4 "natVal" (str "1"); app 5 3 4; binder "forallE" 6 2 5; axiom 4 [] 6] in
    expect_lower "kernel" (lower table));
  "shared-type-dag-is-not-expanded", (fun () ->
    let* table = translate [sort 1 1; binder "forallE" 2 1 1;
      binder "forallE" 3 2 2; axiom 5 [] 3; axiom 4 [] 3] in
    require "shared type nodes were copied or lost"
      (List.length (Translate.nodes table) = 3 && List.length (Translate.rows table) = 2));
  "expression-depth-is-an-explicit-refusal", (fun () ->
    let chain = List.init 1100 (fun offset ->
      let index = offset + 2 in node index "mdata" (obj ["expr", int (index - 1); "data", obj []])) in
    expect_semantic "type expression DAG exceeds the depth limit"
      ([sort 1 0] @ chain @ [axiom 4 [] 1101]));
  "level-depth-is-an-explicit-refusal", (fun () ->
    let chain = List.init 1100 (fun offset ->
      let index = offset + 7 in level index "succ" (int (index - 1))) in
    expect_semantic "type level DAG exceeds the depth limit"
      (chain @ [sort 1 1106; axiom 4 [1; 6] 1]));
  "lowering-observes-budget", (fun () ->
    let* table = translate [sort 1 0; axiom 4 [] 1] in
    let* declaration = row table in
    expect_lower "kernel" (Translate.lower_type ~budget:(Budget.of_poll (fun () -> true))
      ~resolve:missing Global.initial table declaration));
  "shared-pi-expansion-has-a-finite-budget", (fun () ->
    let chain = List.init 24 (fun offset ->
      let index = offset + 2 in binder "forallE" index (index - 1) (index - 1)) in
    let* table = translate ([sort 1 0] @ chain @ [axiom 4 [] 25]) in
    let* () = require "shared Pi input was expanded in the source table"
      (List.length (Translate.nodes table) = 25) in
    lower table |> Result.fold ~ok:(fun _term -> Error "shared Pi expansion escaped its budget")
      ~error:(fun failure -> match failure with
        | Translate.Kernel_error error -> require "shared Pi expansion failed without a budget error"
            (error = Error.Budget_exhausted (Error.message error))
        | Translate.Deferred message -> Error ("unexpected deferred type: " ^ message)
        | Translate.Unsupported message -> Error ("unexpected unsupported type: " ^ message)));
]

let run cases =
  List.fold_left (fun failures (name, test) ->
    test () |> Result.fold
      ~ok:(fun () -> Printf.printf "PASS import-types %s\n%!" name; failures)
      ~error:(fun message -> Printf.printf "FAIL import-types %s: %s\n%!" name message;
        (name ^ ": " ^ message) :: failures)) [] cases
  |> List.rev

let () =
  let failures = run (cases @ extra_cases) in
  if List.length failures = 0 then print_endline "IMPORT-TYPES-OK"
  else (prerr_endline ("IMPORT-TYPES-FAIL " ^ String.concat "; " failures); exit 1)
