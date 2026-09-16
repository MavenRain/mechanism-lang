open Mechanism_kernel
open Mechanism_surface

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let read root path = Mechanism_import.Io.attempt (fun () ->
  In_channel.with_open_bin (Filename.concat root path) In_channel.input_all)
let exact expected result = Result.fold result
  ~ok:(fun _value -> Error ("expected refusal: " ^ Error.to_string expected))
  ~error:(fun actual -> require ("wrong refusal: " ^ Error.to_string actual) (actual = expected))

let box = "poly (u) mu Box (0 A : Sort (succ u)) : Sort (succ u) with " ^
  "| box : A -> Box A where " ^
  "def unbox : (0 A : Sort (succ u)) -> Box A -> A := " ^
  "fun (0 A : Sort (succ u)) (b : Box A) => " ^
  "case b as self in Box return A with | box (a : A) => a end "
let pair = "poly (u, v) group Pair where " ^
  "specialize Box (u) as Left specialize Box (v) as Right end "
let base = box ^ pair ^ "specialize Box (0) as Existing "
let dependent_base =
  "poly (u) mu Marker : Sort (succ u) with | mark : Marker " ^
  "and Wrapper (0 x : Marker) : Sort (succ u) with | wrap : Wrapper x " ^
  "where def value : Wrapper mark := wrap end " ^
  "specialize Marker (0) as Old specialize Marker (0) as Other "

let mutual_base =
  "poly (u) mu P : Sort (succ u) with | pp : P and Q : Sort (succ u) with " ^
  "| qq : P -> Q where def mkp : P := pp end " ^
  "mu A : Sort (succ 0) with | pp : A and B : Sort (succ 0) with | qq : A -> B "
let level_base =
  "poly (u) mu R : Sort (succ u) with | rr : R " ^
  "mu E : Sort (succ 0) with | rr : E "

let param_level_base =
  "poly (u) mu R (0 A : Sort u) : Sort (succ u) with | rr : R A " ^
  "mu E (0 A : Sort 0) : Sort (succ 0) with | rr : E A "

(* SC-CR-M3, review round 3:  the reused family declares its OWN level as
   the non-normal (max (succ 0) (succ 0)), which elab_univ stores in
   fam_level as written (surface/elab.ml line 937) and check.ml line 461
   copies into f_level. *)
let decl_level_base =
  "poly (u) mu R (0 A : Sort u) : Sort (succ u) with | rr : R A " ^
  "mu E (0 A : Sort 0) : Sort (max (succ 0) (succ 0)) with | rr : E A "

let dependent () =
  let* original, _rows = kernel (Elab.check_in Global.empty dependent_base) in
  let* globals, _rows = kernel (Elab.check_in Global.empty (dependent_base ^
    "specialize Marker (0) as Shared with (Marker := Old, Wrapper := Old_Wrapper) " ^
    "def witness : Old_Wrapper mark := Shared_value")) in
  require "dependent reuse changed family identities" (globals.families = original.families)

(* SC-L2-2, review round 1:  one member of a mutual group is reused and
   the companion is fresh, an accepted path that no case covered. *)
let partial_dependent () =
  let* original, _rows = kernel (Elab.check_in Global.empty dependent_base) in
  let* globals, rows = kernel (Elab.check_in Global.empty (dependent_base ^
    "specialize Marker (0) as Sh with (Marker := Old)")) in
  let* () = require "partial reuse replaced the reused family"
    (Global.find_family "Old" globals = Global.find_family "Old" original) in
  let fresh = Global.StringMap.bindings globals.families
    |> List.filter_map (fun (name, _family) ->
      if Global.StringMap.mem name original.families then None else Some name) in
  let* () = require "wrong partial reuse family inventory" (fresh = ["Sh_Wrapper"]) in
  let* () = require "partial reuse introduced axioms" (Elab.axiom_names rows = []) in
  let* value = kernel (Eval.eval globals [] (Term.Global "Sh_value")) in
  let* _quoted = kernel (Eval.quote globals 0 value) in
  require "the fresh companion member is missing" (List.mem "Sh_value" (List.map fst rows))

(* SC-L1-2, review round 1:  the level argument (max 0 0) is legal and is
   not normal, and the reused family carries the level succ 0. *)
let level_reuse () =
  let* original, _rows = kernel (Elab.check_in Global.empty level_base) in
  let* globals, _rows = kernel (Elab.check_in Global.empty (level_base ^
    "specialize R ((max 0 0)) as Y with (R := E)")) in
  require "level reuse changed the reused family"
    (Global.find_family "E" globals = Global.find_family "E" original)

(* SC-L1-2, review round 2:  the level argument (max 0 0) is legal and is
   not normal, and it reaches the parameter type Sort u of the template
   and the constructor argument as well as the family level. *)
let param_level_reuse () =
  let* original, _rows = kernel (Elab.check_in Global.empty param_level_base) in
  let* globals, _rows = kernel (Elab.check_in Global.empty (param_level_base ^
    "specialize R ((max 0 0)) as Y with (R := E)")) in
  require "parameterized level reuse changed the reused family"
    (Global.find_family "E" globals = Global.find_family "E" original)

(* SC-CR-M3, review round 3:  the template instantiates to the level
   succ 0 and the reused family declares (max (succ 0) (succ 0)), so the
   certificate holds under the budgeted level test of Level.equal_budget
   and fails under structural equality of the two written levels. *)
let decl_level_reuse () =
  let* original, _rows = kernel (Elab.check_in Global.empty decl_level_base) in
  let* globals, _rows = kernel (Elab.check_in Global.empty (decl_level_base ^
    "specialize R (0) as Y with (R := E)")) in
  require "declared level reuse changed the reused family"
    (Global.find_family "E" globals = Global.find_family "E" original)

let positive () =
  let source = base ^
    "specialize Pair (0, 0) as Shared with (Left := Existing, Right := Existing) " ^
    "specialize Pair (0, 1) as Partial with (Left := Existing) " ^
    "poly (u) group Nested where specialize Pair (u, u) as Inner end " ^
    "specialize Nested (0) as N with (Inner_Left := Existing, Inner_Right := Existing) " ^
    "def payload : Existing Nat := box 37 " ^
    "def value : Nat := Shared_Right_unbox Nat payload " ^
    "def partial : Nat := Partial_Left_unbox Nat payload " ^
    "def nested : Nat := N_Inner_Right_unbox Nat payload " in
  let* parsed = kernel (Parser.parse source) in
  let* printed = kernel (Parser.parse (Syntax.print parsed)) in
  let* () = require "reuse round trip changed" (parsed = printed) in
  let* globals, rows = kernel (Elab.elab_program_in Global.initial parsed) in
  let* original, _rows = kernel (Elab.check_in Global.initial base) in
  let* () = require "reused family was replaced"
    (Global.find_family "Existing" globals = Global.find_family "Existing" original) in
  let extras = Global.StringMap.bindings globals.families |> List.filter_map (fun (name, _family) ->
    if Global.StringMap.mem name Global.initial.families then None else Some name) in
  let* () = require "wrong reuse family inventory" (extras = ["Existing"; "Partial_Right"]) in
  let* () = require "wrong definition inventory"
    (List.map fst rows = ["Existing_unbox"; "Shared_Left_unbox"; "Shared_Right_unbox";
      "Partial_Left_unbox"; "Partial_Right_unbox"; "N_Inner_Left_unbox";
      "N_Inner_Right_unbox"; "payload"; "value"; "partial"; "nested"]
      && Elab.axiom_names rows = []) in
  let* () = Global.StringMap.fold (fun name entry acc -> let* () = acc in
    if Global.StringMap.mem name Global.initial.entries then Ok () else
    match entry with
    | Global.Def _ -> Ok ()
    | Global.Prim _ | Global.Axiom _ -> Error ("unchecked entry: " ^ name)) globals.entries (Ok ()) in
  List.fold_left (fun acc name -> let* () = acc in
    let* value = kernel (Eval.eval globals [] (Term.Global name)) in
    let* value = kernel (Eval.quote globals 0 value) in
    require ("wrong reuse computation: " ^ name) (value = Term.Lit (Literal.LInt (Bignum.of_int 37))))
    (Ok ()) ["value"; "partial"; "nested"]

let mismatch name = Error.Mismatch ("the reused family " ^ name ^ " does not match the template")
let negative_cases = [
  "unknown-local", Error.Unbound "unknown template family Missing", base ^
    "specialize Pair (0, 0) as P with (Missing := Existing)";
  "member-is-not-family", Error.Unbound "unknown template family Left_unbox", base ^
    "specialize Pair (0, 0) as P with (Left_unbox := Existing)";
  "unknown-existing", Error.Unbound "unknown reused family Missing", base ^
    "specialize Pair (0, 0) as P with (Left := Missing)";
  "forward-existing", Error.Unbound "unknown reused family Later", base ^
    "specialize Pair (0, 0) as P with (Left := Later) specialize Box (0) as Later";
  "duplicate-local", Error.Mismatch "the family Left is bound more than once", base ^
    "specialize Pair (0, 0) as P with (Left := Existing, Left := Existing)";
  "wrong-level", mismatch "Existing", base ^
    "specialize Pair (1, 0) as P with (Left := Existing)";
  "wrong-constructor", mismatch "Other", base ^
    "mu Other (0 A : Type) : Type with | other : A -> Other A " ^
    "specialize Pair (0, 0) as P with (Left := Other)";
  "wrong-telescope", mismatch "Other", base ^
    "mu Other : Type with | other : Other " ^
    "specialize Pair (0, 0) as P with (Left := Other)";
  "root-reuse", Error.Mismatch "the name Existing is already declared", base ^
    "specialize Box (0) as Existing with (Box := Existing)";
  "fresh-name-collision", Error.Mismatch "the name P_Right is already declared", base ^
    "specialize Box (0) as P_Right " ^
    "specialize Pair (0, 0) as P with (Left := P_Right)";
  "member-collision", Error.Mismatch "the name P_Left_unbox is already declared", base ^
    "def P_Left_unbox : Nat := 0 " ^
    "specialize Pair (0, 0) as P with (Left := Existing)";
  "mutual-group-member", Error.Mismatch ("the reused family B is a member of a " ^
    "mutual group, which family reuse does not support"),
    mutual_base ^ "specialize P (0) as X with (P := A, Q := B)";
  "partial-reverse", mismatch "Old_Wrapper", dependent_base ^
    "specialize Marker (0) as Sh with (Wrapper := Old_Wrapper)";
  "unknown-template-name", Error.Unbound "unknown universe schema Typo", dependent_base ^
    "specialize Typo (0) as Sh with (Marker := Old)";
  "definition-template", Error.Not_yet "family reuse requires a family template", base ^
    "poly (u) def ident : (0 A : Sort (succ u)) -> A -> A := " ^
    "fun (0 A : Sort (succ u)) (x : A) => x " ^
    "specialize ident (0) as I with (Box := Existing)";
  "no-implicit-sharing", Error.Mismatch
    "the term has type (Lan SMu Existing [] (Sec SColl 1 [ => Nat])) and the expected type is (Lan SMu P_Right [] (Sec SColl 1 [ => Nat]))", base ^
    "specialize Pair (0, 0) as P with (Left := Existing) " ^
    "def wrong : P_Right Nat := (box 0 : Existing Nat)";
  "dependent-family", mismatch "Old_Wrapper", dependent_base ^
    "specialize Marker (0) as Shared with (Marker := Other, Wrapper := Old_Wrapper)";
]

let parser_cases = [
  "specialize Box (0) as P with ()";
  "specialize Box (0) as P with ( )";
  "specialize Box (0) as P with (Box Existing)";
  "specialize Box (0) as P with (Box := Existing,)";
  "specialize Box (0) as P with (Box := Existing";
  "poly (u) group P where specialize Box (u) as B with (Box := Existing) end";
]

let refusals () =
  let* () = List.fold_left (fun acc (name, expected, source) -> let* () = acc in
    let verdict = exact expected (Elab.check_in Global.initial source) in
    Result.map_error (fun message -> name ^ ": " ^ message) verdict) (Ok ()) negative_cases in
  List.fold_left (fun acc source -> let* () = acc in
    Result.fold (Parser.parse source) ~ok:(fun _ -> Error ("parsed: " ^ source))
      ~error:(function Error.Parse _ -> Ok () | error -> Error (Error.to_string error)))
    (Ok ()) parser_cases

let raw () =
  let family = { Check.fam_name = "Marker"; fam_params = []; fam_indices = [];
    fam_level = Level.one } in
  let ctor = { Check.ct_name = "marker"; ct_args = []; ct_res_params = []; ct_res_idx = [] } in
  let* catalog = kernel (Family_poly.declare Global.empty Family_poly.empty ~arity:1 family [ctor]) in
  let* globals = kernel (Family_poly.instantiate Global.empty catalog
    ~name:"Marker" ~levels:[Level.zero] ~as_name:"Existing") in
  let instantiate ?(budget = Budget.unlimited) globals =
    Family_poly.instantiate ~budget ~reuse:["Marker", "Existing"] globals catalog
      ~name:"Marker" ~levels:[Level.zero] ~as_name:"Alias" in
  let* reused = kernel (instantiate globals) in
  let* original = Global.find_family "Existing" globals |> Option.to_result ~none:"missing raw family" in
  let replacements = [{ original with Positivity.f_positive = false };
    { original with Positivity.f_status = Positivity.Provisional };
    { original with Positivity.f_ctors = [] }] in
  let weakened replacement () =
    exact (mismatch "Existing") (instantiate (Global.add_family "Existing" replacement globals)) in
  (* SC-L1-3, review round 2:  every raw check is one row of this list and
     the printed count is its length, so the deletion of any single check
     lowers the count and the pinned gate line fails. *)
  let checks =
    ("identity reuse keeps globals",
       fun () -> require "root family reuse changed globals" (globals = reused)) ::
    ("an exhausted budget refuses the reuse",
       fun () -> exact (Error.Budget_exhausted "the check budget is exhausted")
         (instantiate ~budget:(Budget.of_poll (fun () -> true)) globals)) ::
    List.map (fun replacement -> ("a weakened certificate refuses the reuse", weakened replacement))
      replacements
    @ [("a failed reuse keeps the original family",
         fun () -> require "failed reuse changed original family"
           (Global.find_family "Existing" globals = Some original))] in
  let* () = List.fold_left (fun acc (_label, check) -> let* () = acc in check ())
    (Ok ()) checks in
  Ok (List.length checks)

let suite root =
  let* () = positive () in
  let* () = dependent () in
  let* () = partial_dependent () in
  let* () = level_reuse () in
  let* () = param_level_reuse () in
  let* () = decl_level_reuse () in
  let* () = refusals () in
  let* raw_cases = raw () in
  let* sources = List.fold_left (fun acc path -> let* source = acc in
    let* addition = read root path in Ok (source ^ "\n" ^ addition)) (Ok "")
    ["prelude/cat/category-core.mech"; "prelude/cat/heterogeneous-functor.mech";
     "prelude/cat/composable-functors.mech"; "prelude/cat/identity-functor.mech";
     "test/fixtures/prelude/reuse-contracts.mech"] in
  let* globals, rows = kernel (Elab.check_in Global.initial sources) in
  let* () = require "category reuse introduced axioms" (Elab.axiom_names rows = []) in
  let fresh = Global.StringMap.bindings globals.families |> List.filter_map (fun (name, _family) ->
    if Global.StringMap.mem name Global.initial.families then None else Some name) in
  let* () = require "category specializations allocated fresh families"
    (fresh = ["Middle"; "Source"; "Target"]) in
  Ok raw_cases

let () =
  let root = match Array.to_list Sys.argv with
    | [_program; root] -> root
    | [] | [_] | _ :: _ :: _ :: _ -> "." in
  Result.fold (suite root)
    ~ok:(fun raw_cases -> Printf.printf "TEMPLATE-REUSE-OK negatives=%d parser=%d raw=%d\n"
      (List.length negative_cases) (List.length parser_cases) raw_cases)
    ~error:(fun message -> Printf.printf "TEMPLATE-REUSE-FAIL %s\n" message; exit 1)
