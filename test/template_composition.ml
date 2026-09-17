open Mechanism_kernel
open Mechanism_surface

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let read root path = Mechanism_import.Io.attempt (fun () ->
  In_channel.with_open_bin (Filename.concat root path) In_channel.input_all)
let collision name = Error.Mismatch ("the name " ^ name ^ " is already declared")
let exact expected result = Result.fold result
  ~ok:(fun _value -> Error ("expected refusal: " ^ Error.to_string expected))
  ~error:(fun actual -> require ("wrong refusal: " ^ Error.to_string actual) (actual = expected))

let tiny = "mu Tiny : Type with | zero : Tiny "
let box = "poly (u) mu Box : Sort (succ u) with | box : Box "
let group body = "poly (u) group Group where " ^ body ^ " end "
let dependency = "specialize Box (u) as Local "

let positive source =
  let* parsed = kernel (Parser.parse source) in
  let* printed = kernel (Parser.parse (Syntax.print parsed)) in
  let* () = require "composition parse/print changed" (parsed = printed) in
  let* symbolic, rows = kernel (Elab.elab_program_in Global.empty parsed) in
  let* () = require "composition leaked symbolic globals"
    (Global.StringMap.is_empty symbolic.entries
      && Global.StringMap.is_empty symbolic.families && rows = []) in
  let client = tiny ^
    "specialize Pair (0, 1) as P specialize Nested (0) as N " ^
    "specialize Maximum (0, 2) as M specialize Impredicative (2, 0) as I " ^
    "def low : P_Left Tiny := box zero " ^
    "def high : P_Right Type := P_transfer Tiny Type (fun (x : Tiny) => Tiny) low " ^
    "def result : Type := P_Right_unbox Type high " ^
    "def nested : Type := N_project Type (box Tiny) " in
  let* globals, rows = kernel (Elab.check_in Global.empty (source ^ client)) in
  let expected_families = ["I_Item"; "M_Item"; "N_Inner_Left"; "N_Inner_Right";
    "P_Left"; "P_Right"; "Tiny"] in
  let* () = require "composition family inventory changed"
    (List.map fst (Global.StringMap.bindings globals.families) = expected_families) in
  let expected_rows = ["P_Left_unbox"; "P_Right_unbox"; "P_transfer";
    "N_Inner_Left_unbox"; "N_Inner_Right_unbox"; "N_Inner_transfer"; "N_project";
    "M_Item_unbox"; "I_Item_unbox"; "low"; "high"; "result"; "nested"] in
  let* () = require "composition definition order changed"
    (List.map fst rows = expected_rows
      && Global.StringMap.cardinal globals.entries = List.length expected_rows) in
  let* () = require "composition introduced axioms" (Elab.axiom_names rows = []) in
  let* () = Global.StringMap.fold (fun name entry acc -> let* () = acc in
    match entry with
    | Global.Def _ -> Ok ()
    | Global.Axiom _ | Global.Prim _ -> Error ("unchecked entry: " ^ name))
    globals.entries (Ok ()) in
  let* () = List.fold_left (fun acc (name, expected) -> let* () = acc in
    let* family = Global.find_family name globals |> Option.to_result
      ~none:("missing family: " ^ name) in
    require ("wrong universe: " ^ name) (Level.equal family.Positivity.f_level expected))
    (Ok ()) ["P_Left", Level.one; "P_Right", Level.succ Level.one;
      "N_Inner_Right", Level.succ Level.one; "M_Item", Level.succ (Level.succ Level.one);
      "I_Item", Level.one] in
  let* () = List.fold_left (fun acc name -> let* () = acc in
    let* value = kernel (Eval.eval globals [] (Term.Global name)) in
    let* actual = kernel (Eval.quote globals 0 value) in
    let expected = Term.Lan (Shape.SMu ("Tiny", []), Rules.diagram_of []) in
    require ("wrong composition computation: " ^ name) (actual = expected))
    (Ok ()) ["result"; "nested"] in
  Ok ()

let source_negatives = [
  "unknown-dependency", Error.Unbound "unknown family universe schema Missing",
    group "specialize Missing (u) as Local";
  "forward-dependency", Error.Unbound "unknown family universe schema Later",
    group "specialize Later (u) as Local" ^
    "poly (u) group Later where specialize Box (u) as Item end";
  "dependency-arity", Error.Universe "the family schema Box expects 1 universe arguments, got 2",
    group "specialize Box (u, u) as Local";
  "duplicate-prefix", collision "Local", group (dependency ^ dependency);
  "template-name-prefix", collision "Group", group "specialize Box (u) as Group";
  "template-name-member", collision "Group", group (dependency ^ "def Group : Local := box");
  "imported-family-collision", collision "Local", group (dependency ^ "def Local : Local := box");
  "duplicate-member", collision "member", group (dependency ^
    "def member : Local := box def member : Local := box");
  "imported-label-member", collision "box", group (dependency ^ "def box : Local := box");
  "imported-label-prefix", collision "box", group "specialize Box (u) as box";
  "body-unbound", Error.Unbound "missing", group (dependency ^ "def member : Local := missing");
  "member-forward", Error.Unbound "later", group (dependency ^
    "def member : Local := later def later : Local := box");
  "bare-import", Error.Unbound "Local", group dependency ^ "def bad : Type := Local";
  "bare-group", Error.Unbound "Group", group dependency ^ "def bad : Type := Group";
  "global-group-collision", collision "Group", "def Group : Type := Prop " ^ group dependency;
  "definition-template-dependency", Error.Unbound "unknown family universe schema Identity",
    "poly (u) def Identity : Sort (succ u) := Sort u " ^ group "specialize Identity (u) as Local";
  "prefix-definition-template", collision "Local",
    "poly (u) def Local : Sort (succ u) := Sort u " ^ group dependency;
  "label-late-template", collision "box", group dependency ^
    "poly (u) def box : Sort (succ u) := Sort u specialize Group (0) as G";
  "output-family-collision", collision "G_Local", group dependency ^
    "mu G_Local : Type with specialize Group (0) as G";
  "output-template-collision", collision "G_Local", group dependency ^
    "poly (u) def G_Local : Sort (succ u) := Sort u specialize Group (0) as G";
  "nominal-imports", Error.Mismatch
    "the term has type (Lan SMu Second_Local [] (Sec SColl 0 [])) and the expected type is (Lan SMu First_Local [] (Sec SColl 0 []))", group
    (dependency ^ "def witness : Local := box") ^
    "specialize Group (0) as First specialize Group (0) as Second " ^
    "def mixed : First_Local := Second_witness";
  "duplicate-disjoint-prefix", collision "Shared",
    "poly (u) group A where specialize Box (u) as One end " ^
    "poly (u) group B where specialize Box (u) as Two end " ^
    group "specialize A (u) as Shared specialize B (u) as Shared";
  "prefix-generated-family", collision "X_One",
    "poly (u) group A where specialize Box (u) as One end " ^
    "poly (u) group B where specialize Box (u) as Two end " ^
    group "specialize A (u) as X specialize B (u) as X_One";
  "prefix-generated-member", collision "X_item",
    "poly (u) group A where specialize Box (u) as One def item : One := box end " ^
    "poly (u) group B where specialize Box (u) as Two end " ^
    group "specialize A (u) as X specialize B (u) as X_item";
]

let negatives () = List.fold_left (fun acc (name, expected, source) -> let* () = acc in
  Result.map_error (fun message -> name ^ ": " ^ message)
    (exact expected (Elab.check_in Global.empty (box ^ source)))) (Ok ()) source_negatives

let parser_cases = [
  "empty", group "", "expected at least one template specialization in a group, found 'end'";
  "no-dependencies", group "def item : Type := Prop",
    "expected at least one template specialization in a group, found 'def'";
  "missing-name", "poly (u) group where end",
    "expected 'group NAME where' after universe binders, found 'group'";
  "open-level", group "specialize Box (v) as Local", "unbound universe v";
  "scope-leak", group dependency ^ "specialize Group (u) as Closed", "unbound universe u";
  "postulate", group (dependency ^ "axiom item : Local"),
    "expected 'def' or 'end' in family members, found 'axiom'";
  "recursive", group (dependency ^ "def rec item : Local := box"),
    "expected a nonrecursive member definition, found 'def'";
  "late-dependency", group (dependency ^ "def item : Local := box " ^ dependency),
    "expected 'def' or 'end' in family members, found 'specialize'";
  "missing-end", "poly (u) group Group where " ^ dependency,
    "expected 'def' or 'end' in family members, found end of input";
]

let parser_negatives () = List.fold_left (fun acc (name, source, expected) ->
  let* () = acc in
  Result.fold (Parser.parse source)
    ~ok:(fun _parsed -> Error (name ^ ": expected parser refusal"))
    ~error:(fun error -> match error with
      | Error.Parse (actual, _line, _column) ->
          require (name ^ ": wrong parser refusal: " ^ actual) (String.equal actual expected)
      | Error.Not_yet _ | Error.Carry _ | Error.Unbound _ | Error.Mismatch _
      | Error.Universe _ | Error.Quantity _ | Error.Wrong_leg _ | Error.Missing_branch _
      | Error.Overflow _ | Error.Cannot_infer _ | Error.Budget_exhausted _
      | Error.Index_not_zero _ | Error.Index_above_universe _ | Error.Termination _ ->
          Error (name ^ ": wrong refusal kind: " ^ Error.to_string error))) (Ok ()) parser_cases

module Families = Family_poly
let marker : Check.family_decl = {
  fam_name = "Seed"; fam_params = []; fam_indices = []; fam_level = Level.one;
}
let constructor : Check.ctor_decl = {
  ct_name = "mark"; ct_args = []; ct_res_params = []; ct_res_idx = [];
}
let definition name ty body : Check.decl = {
  d_name = name; d_kind = Check.Definition; d_ty = ty; d_body = Some body;
}
let dependency_raw = ["Seed", [Level.zero], "Local"]
let compose ?(budget = Budget.unlimited) ?(globals = Global.empty) ?(arity = 1)
    ?(dependencies = dependency_raw) ?(members = []) catalog =
  Families.compose ~budget ~members globals catalog ~arity ~name:"Composed"
    (List.map (fun (source, levels, prefix) -> source, levels, prefix, []) dependencies)

let raw_cases catalog = [
  "empty-dependencies", (fun () -> exact (Error.Mismatch "a family schema group must be nonempty")
    (compose ~dependencies:[] catalog));
  "negative-arity", (fun () -> exact
    (Error.Universe "a universe parameter arity must be nonnegative") (compose ~arity:(-1) catalog));
  "raw-scope", (fun () ->
    let* free = Level.var 1 |> Option.to_result ~none:"invalid universe variable" in
    exact (Error.Universe "universe level is outside the global parameter scope")
      (compose ~dependencies:["Seed", [free], "Local"] catalog));
  "raw-member-body", (fun () ->
    let reached = ref false in
    let* () = exact (Error.Unbound "de Bruijn index 0 is outside the context")
      (compose ~members:[(fun _globals -> Ok (definition "bad" (Term.Univ Level.one) (Term.Var 0)));
        (fun _globals -> reached := true; Error (Error.Unbound "later"))] catalog) in
    require "a callback ran after a rejected body" (not !reached));
  "raw-group-name", (fun () -> exact (collision "Composed")
    (compose ~members:[(fun _globals ->
      Ok (definition "Composed" (Term.Univ Level.one) (Term.Univ Level.zero)))] catalog));
  "raw-postulate", (fun () -> exact
    (Error.Not_yet "family schema members must be definitions")
    (compose ~members:[(fun _globals -> Ok { Check.d_name = "postulated";
      d_kind = Check.Postulate; d_ty = Term.Univ Level.one; d_body = None })] catalog));
  "raw-hidden-scope", (fun () ->
    let* free = Level.var 1 |> Option.to_result ~none:"invalid universe variable" in
    let domain = Term.Univ Level.one in
    let shape = Shape.SPi (Quantity.Many, "A", domain) in
    let identity = Term.Ann
      (Term.Sec (shape, [{ Term.l_binders = [Quantity.Many, "A"]; l_body = Term.Var 0 }]),
       Term.Ran (shape, domain)) in
    let body = Term.Out (Shape.SPi (Quantity.Many, "ignored", Term.Univ free),
      Term.APt (Quantity.Many, Term.Univ Level.zero), identity) in
    exact (Error.Universe "universe level is outside the global parameter scope")
      (compose ~members:[(fun _globals -> Ok (definition "hidden" domain body))] catalog));
  "callback-error", (fun () ->
    let reached = ref false in
    let* () = exact (Error.Unbound "callback")
      (compose ~members:[(fun _globals -> Error (Error.Unbound "callback"));
        (fun _globals -> reached := true; Error (Error.Unbound "later"))] catalog) in
    require "a callback ran after an error" (not !reached));
  "cancelled-dependencies", (fun () ->
    let polls = ref 0 in
    let reached = ref false in
    let* () = exact (Error.Budget_exhausted Check.budget_msg)
      (compose ~budget:(Budget.of_poll (fun () -> incr polls; !polls > 1))
        ~dependencies:["Absent", [Level.zero], "Local"]
        ~members:[(fun _globals -> reached := true; Error (Error.Unbound "later"))] catalog) in
    let* () = require "the fold poll ran before the dependency schema lookup" (!polls = 2) in
    require "a callback ran after dependency cancellation" (not !reached));
  "cancelled-members", (fun () ->
    let cancelled = ref false in
    let reached = ref false in
    let* () = exact (Error.Budget_exhausted Check.budget_msg)
      (compose ~budget:(Budget.of_poll (fun () -> !cancelled))
        ~members:[(fun _globals -> cancelled := true;
          Ok (definition "first" (Term.Univ Level.one) (Term.Univ Level.zero)));
          (fun _globals -> reached := true; Error (Error.Unbound "later"))] catalog) in
    require "a callback ran after member cancellation" (not !reached));
]

let current_globals () =
  let globals = Global.add "External" (Global.Axiom { ax_ty = Term.Univ Level.one }) Global.empty in
  let constructor = { constructor with ct_args = [Quantity.Many, "item", Term.Global "External"] } in
  let* catalog = kernel (Families.declare globals Families.empty ~arity:1 marker [constructor]) in
  let* () = exact (Error.Unbound "External") (compose catalog) in
  let* catalog = kernel (compose ~globals catalog) in
  let* () = exact (Error.Unbound "External")
    (Families.instantiate Global.empty catalog ~name:"Composed" ~levels:[Level.zero] ~as_name:"Bad") in
  let* restored = kernel (Families.instantiate globals catalog ~name:"Composed"
    ~levels:[Level.zero] ~as_name:"Good") in
  require "failed specialization prevented a compatible instance"
    (Option.is_some (Global.find_family "Good_Local" restored))

let direct_parity () =
  let composed = box ^ group (dependency ^ "def member : Local := box") ^
    "specialize Group (0) as G" in
  let direct = box ^ "specialize Box (0) as G_Local def G_member : G_Local := box" in
  let* actual, rows = kernel (Elab.check_in Global.empty composed) in
  let* expected, expected_rows = kernel (Elab.check_in Global.empty direct) in
  require "composition differs from explicit closed declarations"
    (rows = expected_rows
      && Global.StringMap.equal (=) actual.entries expected.entries
      && Global.StringMap.equal (=) actual.families expected.families)

let raw () =
  let* catalog = kernel (Families.declare Global.empty Families.empty ~arity:1 marker [constructor]) in
  let tests = raw_cases catalog in
  let* () = List.fold_left (fun acc (name, test) -> let* () = acc in
    Result.map_error (fun message -> name ^ ": " ^ message) (test ())) (Ok ()) tests in
  Ok (List.length tests)

let category root =
  let* source = read root "prelude/cat/category.mech" in
  let* fixture = read root "test/fixtures/prelude/composition-category.mech" in
  let* parsed = kernel (Parser.parse source) in
  let rec prefix reversed = function
    | [] -> Error "category prefix has no assoc member"
    | member :: rest ->
        let reversed = member :: reversed in
        if String.equal member.Syntax.rd_name "assoc" then Ok (List.rev reversed)
        else prefix reversed rest in
  let* parsed = match parsed with
    | [Syntax.DPolyGroup (arity, family, companions, members)] ->
        let* members = prefix [] members in
        Ok [Syntax.DPolyGroup (arity, family, companions, members)]
    | [] | _ :: _ -> Error "category prelude is not one family group" in
  let* client = kernel (Parser.parse fixture) in
  let* roundtrip = kernel (Parser.parse (Syntax.print client)) in
  let* () = require "category composition round trip changed" (client = roundtrip) in
  let* globals, rows = kernel (Elab.elab_program_in Global.empty (parsed @ client)) in
  let* () = require "mixed-universe category family inventory changed"
    (List.map fst (Global.StringMap.bindings globals.families)
      = ["First_Source"; "First_Target"; "Second_Source"; "Second_Target"]) in
  require "mixed-universe category entry inventory changed"
    (List.length rows = 40 && Global.StringMap.cardinal globals.entries = 40
      && Elab.axiom_names rows = [])

let suite root =
  let* source = read root "test/fixtures/prelude/template-composition.mech" in
  let* () = positive source in
  let* () = negatives () in
  let* () = parser_negatives () in
  let* () = direct_parity () in
  let* () = current_globals () in
  let* () = category root in
  raw ()

let () =
  let root = match Array.to_list Sys.argv with
    | [_program; root] -> root
    | [] | [_] | _ :: _ :: _ :: _ -> "." in
  Result.fold (suite root)
    ~ok:(fun raw_cases -> Printf.printf "TEMPLATE-COMPOSITION-OK negatives=%d parser=%d raw=%d\n"
      (List.length source_negatives) (List.length parser_cases) raw_cases)
    ~error:(fun message -> Printf.printf "TEMPLATE-COMPOSITION-FAIL %s\n" message; exit 1)
