open Mechanism_kernel
open Mechanism_surface

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let exact expected result = Result.fold result
  ~ok:(fun _value -> Error ("expected refusal: " ^ Error.to_string expected))
  ~error:(fun actual -> require ("wrong refusal: " ^ Error.to_string actual) (actual = expected))
let read root path = Mechanism_import.Io.attempt (fun () ->
  In_channel.with_open_bin (Filename.concat root path) In_channel.input_all)

let box = "poly (u) mu Box (0 A : Sort (succ u)) : Sort (succ u) with " ^
  "| box : A -> Box A where " ^
  "def unbox : (0 A : Sort (succ u)) -> Box A -> A := " ^
  "fun (0 A : Sort (succ u)) (b : Box A) => " ^
  "case b as self in Box return A with | box (a : A) => a end "
let pair = "poly (u, v) group Pair where " ^
  "specialize Box (u) as Left specialize Box (v) as Right end "
let group body = "poly (u, v) group Shared where " ^ body ^ " end "
let first = "specialize Box (u) as Base "
let shared = group (first ^
  "specialize Pair (u, v) as Both with (Left := Base) " ^
  "specialize Box (u) as Again with (Box := Base)")
let nested = "poly (u, v) group Nested where specialize Shared (u, v) as Inner " ^
  "specialize Box (v) as Other with (Box := Inner_Both_Right) end "
let templates = box ^ pair ^ shared ^ nested
let mismatch name = Error.Mismatch ("the reused family " ^ name ^ " does not match the template")
let collision name = Error.Mismatch ("the name " ^ name ^ " is already declared")

let positive () =
  let* parsed = kernel (Parser.parse templates) in
  let* printed = kernel (Parser.parse (Syntax.print parsed)) in
  let* () = require "symbolic reuse round trip changed" (parsed = printed) in
  let* symbolic, rows = kernel (Elab.elab_program_in Global.empty parsed) in
  let* () = require "symbolic reuse leaked globals"
    (symbolic = Global.empty && rows = []) in
  let instances = templates ^ "specialize Nested (0, 0) as N " ^
    "specialize Nested (1, 2) as Higher " in
  let* original, _rows = kernel (Elab.check_in Global.initial instances) in
  let* globals, rows = kernel (Elab.check_in Global.initial (instances ^
    "specialize Nested (0, 0) as Reused " ^
    "with (Inner_Base := N_Inner_Base, Inner_Both_Right := N_Inner_Both_Right) " ^
    "def payload : N_Inner_Base Nat := box 37 " ^
    "def value : Nat := N_Inner_Both_Left_unbox Nat payload " ^
    "def again : Nat := N_Inner_Again_unbox Nat payload " ^
    "def reused : Nat := Reused_Inner_Again_unbox Nat payload " ^
    "def right : Nat := N_Other_unbox Nat (box 41 : N_Inner_Both_Right Nat)")) in
  let families = Global.StringMap.bindings globals.families
    |> List.filter_map (fun (name, _family) ->
      if Global.StringMap.mem name Global.initial.families then None else Some name) in
  let* () = require "wrong symbolic reuse family inventory"
    (families = ["Higher_Inner_Base"; "Higher_Inner_Both_Right";
      "N_Inner_Base"; "N_Inner_Both_Right"] && globals.families = original.families) in
  let expected_members prefix = List.map (fun name -> prefix ^ name)
    ["_Inner_Base_unbox"; "_Inner_Both_Left_unbox"; "_Inner_Both_Right_unbox";
     "_Inner_Again_unbox"; "_Other_unbox"] in
  let* () = require "wrong symbolic reuse member inventory"
    (List.map fst rows = expected_members "N" @ expected_members "Higher" @
      expected_members "Reused" @ ["payload"; "value"; "again"; "reused"; "right"]
      && Elab.axiom_names rows = []) in
  List.fold_left (fun acc (name, expected) -> let* () = acc in
    let* value = kernel (Eval.eval globals [] (Term.Global name)) in
    let* value = kernel (Eval.quote globals 0 value) in
    require ("wrong symbolic reuse computation: " ^ name)
      (value = Term.Lit (Literal.LInt (Bignum.of_int expected))))
    (Ok ()) ["value", 37; "again", 37; "reused", 37; "right", 41]

let marker = "poly (u) mu Marker : Sort (succ u) with | mark : Marker " ^
  "and Wrapper (0 x : Marker) : Sort (succ u) with | wrap : Wrapper x " ^
  "where def value : Wrapper mark := wrap end "
let dependent () =
  let* globals, rows = kernel (Elab.check_in Global.empty (marker ^
    "poly (u) group Dependent where specialize Marker (u) as Original " ^
    "specialize Marker (u) as Partial with (Marker := Original) " ^
    "specialize Marker (u) as Shared with " ^
    "(Marker := Original, Wrapper := Original_Wrapper) " ^
    "def witness : Original_Wrapper mark := Shared_value end " ^
    "specialize Dependent (1) as D")) in
  require "dependent symbolic reuse changed identities"
    (List.map fst (Global.StringMap.bindings globals.families) =
      ["D_Original"; "D_Original_Wrapper"; "D_Partial_Wrapper"]
      && List.map fst rows = ["D_Original_value"; "D_Partial_value";
        "D_Shared_value"; "D_witness"] && Elab.axiom_names rows = [])

let negative_cases = [
  "unknown-local", Error.Unbound "unknown template family Missing", box ^
    group (first ^ "specialize Box (u) as Copy with (Missing := Base)");
  "unknown-existing", Error.Unbound "unknown reused family Missing", box ^
    group (first ^ "specialize Box (u) as Copy with (Box := Missing)");
  "forward-binding", Error.Unbound "unknown reused family Later", box ^
    group (first ^ "specialize Box (u) as Copy with (Box := Later) " ^
      "specialize Box (u) as Later");
  "same-dependency", Error.Unbound "unknown reused family Copy_Right", box ^ pair ^
    group (first ^ "specialize Pair (u, u) as Copy with (Left := Copy_Right)");
  "duplicate-binding", Error.Mismatch "the family Box is bound more than once", box ^
    group (first ^ "specialize Box (u) as Copy with (Box := Base, Box := Base)");
  "independent-universes", mismatch "Base", box ^
    group (first ^ "specialize Box (v) as Copy with (Box := Base)");
  "member-as-local", Error.Unbound "unknown template family unbox", box ^
    group (first ^ "specialize Box (u) as Copy with (unbox := Base)");
  "member-as-existing", Error.Unbound "unknown reused family Base_unbox", box ^
    group (first ^ "specialize Box (u) as Copy with (Box := Base_unbox)");
  "prefix-collision", collision "Base", box ^
    group (first ^ "specialize Box (u) as Base with (Box := Base)");
  "member-collision", collision "Copy_unbox", box ^
    group (first ^ "specialize Box (u) as Copy with (Box := Base) " ^
      "def Copy_unbox : Nat := 0");
  "wrong-dependent-identity", mismatch "Original_Wrapper", marker ^
    group ("specialize Marker (u) as Original specialize Marker (u) as Copy " ^
      "with (Wrapper := Original_Wrapper)");
  "no-fresh-family", Error.Mismatch "a family schema group must be nonempty", box ^
    "specialize Box (0) as Existing " ^
    group "specialize Box (0) as Copy with (Box := Existing)";
]

let parser_cases = List.map (fun bindings -> box ^ group
  (first ^ "specialize Box (u) as Copy with " ^ bindings))
  ["()"; "( )"; "(Box Base)"; "(Box := Base,)"; "(Box := Base"; "Box := Base"]

let refusals () =
  let* () = List.fold_left (fun acc (name, expected, source) -> let* () = acc in
    Result.map_error (fun error -> name ^ ": " ^ error)
      (exact expected (Elab.check_in Global.initial source))) (Ok ()) negative_cases in
  List.fold_left (fun acc source -> let* () = acc in
    Result.fold (Parser.parse source) ~ok:(fun _parsed -> Error "parsed malformed reuse")
      ~error:(function Error.Parse _ -> Ok () | error -> Error (Error.to_string error)))
    (Ok ()) parser_cases

let raw () =
  let family = { Check.fam_name = "Seed"; fam_params = []; fam_indices = [];
    fam_level = Level.one } in
  let ctor = { Check.ct_name = "seed"; ct_args = []; ct_res_params = []; ct_res_idx = [] } in
  let* catalog = kernel (Family_poly.declare Global.empty Family_poly.empty ~arity:1 family [ctor]) in
  let* globals = kernel (Family_poly.instantiate Global.empty catalog
    ~name:"Seed" ~levels:[Level.zero] ~as_name:"Existing") in
  let* original = Global.find_family "Existing" globals |> Option.to_result ~none:"missing family" in
  let compose ?(budget = Budget.unlimited) ?(members = []) globals =
    Family_poly.compose ~budget ~members globals catalog ~arity:1 ~name:"Shared"
      ["Seed", [Level.zero], "Fresh", []; "Seed", [Level.zero], "Copy", ["Seed", "Existing"]] in
  let checks = [
    "preserve-caller", (fun () ->
      let* shared = kernel (compose globals) in
      let* installed = kernel (Family_poly.instantiate globals shared
        ~name:"Shared" ~levels:[Level.zero] ~as_name:"Instance") in
      require "caller certificate changed" (Global.find_family "Existing" installed = Some original
        && Family_poly.companions shared "Shared" = Some []));
    "cancelled", (fun () ->
      let reached = ref false in
      let corrupted = Global.add_family "Existing"
        { original with Positivity.f_status = Positivity.Provisional } globals in
      let run allowance =
        let polls = ref 0 in
        compose ~budget:(Budget.of_poll (fun () -> incr polls; !polls > allowance))
          ~members:[(fun _symbolic -> reached := true; Error (Error.Unbound "callback"))]
          corrupted in
      (* The smallest allowance that reports the certificate mismatch is the
         first poll of the reuse check itself, so one poll less must refuse. *)
      let rec frontier allowance = match () with
        | () when allowance > 512 -> Error "no certificate refusal below 512 polls"
        | () when run allowance = Error (mismatch "Existing") -> Ok allowance
        | () -> frontier (allowance + 1) in
      let* certificate_poll = frontier 1 in
      let* () = exact (Error.Budget_exhausted Check.budget_msg)
        (run (certificate_poll - 1)) in
      require "cancellation preempted no certificate poll or ran a member"
        (not !reached && certificate_poll > 3));
    "refuse-before-members", (fun () ->
      let reached = ref false in
      let corrupted = Global.add_family "Existing" { original with Positivity.f_positive = false } globals in
      let* () = exact (mismatch "Existing")
        (compose ~members:[(fun _symbolic -> reached := true; Error (Error.Unbound "callback"))] corrupted) in
      let* shared = kernel (compose globals) in
      require "failed reuse ran a member or hid the composed schema"
        (not !reached && Family_poly.arity shared "Shared" = Some 1));
    "provisional", (fun () -> exact (mismatch "Existing")
      (compose (Global.add_family "Existing" { original with Positivity.f_status = Positivity.Provisional } globals)));
    "constructors", (fun () -> exact (mismatch "Existing")
      (compose (Global.add_family "Existing" { original with Positivity.f_ctors = [] } globals)));
  ] in
  let* () = List.fold_left (fun acc (name, check) -> let* () = acc in
    Result.map_error (fun error -> name ^ ": " ^ error) (check ())) (Ok ()) checks in
  Ok (List.length checks)

let category root =
  let* source = List.fold_left (fun acc path -> let* source = acc in
    let* addition = read root path in Ok (source ^ "\n" ^ addition)) (Ok "")
    ["prelude/cat/category-core.mech"; "prelude/cat/heterogeneous-functor.mech";
     "prelude/cat/composable-functors.mech"; "prelude/cat/identity-functor.mech";
     "prelude/cat/shared-functor-chain.mech"; "test/fixtures/prelude/symbolic-reuse-contracts.mech"] in
  let* globals, rows = kernel (Elab.check_in Global.initial source) in
  let families = Global.StringMap.bindings globals.families |> List.filter_map (fun (name, _family) ->
    if Global.StringMap.mem name Global.initial.families then None else Some name) in
  require "shared category inventory or axioms changed"
    (families = ["Chain_Middle"; "Chain_Source"; "Chain_Target"] && Elab.axiom_names rows = [])

let suite root =
  let* () = positive () in
  let* () = dependent () in
  let* () = refusals () in
  let* count = raw () in
  let* () = category root in
  Ok count

let () =
  let root = match Array.to_list Sys.argv with
    | [_program; root] -> root
    | [] | [_] | _ :: _ :: _ :: _ -> "." in
  Result.fold (suite root)
    ~ok:(fun raw -> Printf.printf "TEMPLATE-SYMBOLIC-REUSE-OK negatives=%d parser=%d raw=%d\n"
      (List.length negative_cases) (List.length parser_cases) raw)
    ~error:(fun message -> Printf.printf "TEMPLATE-SYMBOLIC-REUSE-FAIL %s\n" message; exit 1)
