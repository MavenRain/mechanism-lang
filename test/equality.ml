open Mechanism_kernel

let ( let* ) = Result.bind

let require message condition = if condition then Ok () else Error message
let kernel result = Result.map_error Error.to_string result

let expect_refusal expected result =
  Result.fold result
    ~ok:(fun _value -> Error ("expected refusal: " ^ Error.to_string expected))
    ~error:(fun actual ->
      require ("wrong refusal: " ^ Error.to_string actual)
        (String.starts_with ~prefix:(Error.to_string expected) (Error.to_string actual)))

let level amount =
  Level.of_int amount |> Option.to_result ~none:"nonnegative level construction failed"

let variable index =
  Level.var index |> Option.to_result ~none:"nonnegative universe variable construction failed"

let anchored_family ?(quantity = Quantity.Zero) ~carrier ~sort () : Check.family_decl =
  {
    fam_name = "AnchoredEquality";
    fam_params = [ Quantity.Zero, "A", Term.Univ carrier; Quantity.Zero, "x", Term.Var 0 ];
    fam_indices = [ quantity, "y", Term.Var 1 ];
    fam_level = sort;
  }

let checked_equality carrier =
  let declaration = anchored_family ~carrier ~sort:Level.zero () in
  let* declared = kernel (Check.declare_family Global.empty declaration) in
  let constructor : Check.ctor_decl = {
    ct_name = "anchoredRefl"; ct_args = [];
    ct_res_params = [ Term.Var 1; Term.Var 0 ]; ct_res_idx = [ Term.Var 0 ];
  } in
  let* globals = kernel (Check.define_ctors declared ~group:[ declaration.fam_name ]
      ~name:declaration.fam_name [ constructor ]) in
  let* family = Global.find_family declaration.fam_name globals
    |> Option.to_result ~none:"checked equality family disappeared" in
  let* () = require "equality lost its positivity verdict" family.Positivity.f_positive in
  require "nullary equality lost large elimination" (Rules.mu_zero_eliminable family)

let simple_family index_type : Check.family_decl = {
  fam_name = "PropositionIndex"; fam_params = [];
  fam_indices = [ Quantity.Zero, "index", index_type ]; fam_level = Level.zero;
}

let hidden_data fam_level : Check.family_decl = {
  fam_name = "HiddenData";
  fam_params = [ Quantity.Zero, "A", Term.Univ Level.one ];
  fam_indices = []; fam_level;
}

(* One data family with one field of the parameter type.  The field lives at
   universe one, so it exceeds a Prop family and sits at the bound of a
   Type 0 family. *)
let hidden_ctors fam_level quantity : (Global.t, Error.t) result =
  let declaration = hidden_data fam_level in
  let* declared = Check.declare_family Global.empty declaration in
  let constructor : Check.ctor_decl = {
    ct_name = "hideData"; ct_args = [ quantity, "value", Term.Var 0 ];
    ct_res_params = [ Term.Var 1 ]; ct_res_idx = [];
  } in
  Check.define_ctors declared ~group:[ declaration.fam_name ]
    ~name:declaration.fam_name [ constructor ]

let field_bound quantity =
  expect_refusal (Error.Universe "a field of hideData exceeds its family universe")
    (hidden_ctors Level.zero quantity)

let field_at_bound quantity =
  let* globals = kernel (hidden_ctors Level.one quantity) in
  let* family = Global.find_family "HiddenData" globals
    |> Option.to_result ~none:"accepted data family disappeared" in
  require "accepted data family lost its positivity verdict" family.Positivity.f_positive

(* A poll that reports exhaustion only after [allowance] calls.  The second
   component reads back how many polls a checked path has spent. *)
let counting_budget allowance =
  let calls = ref 0 in
  let poll () = calls := !calls + 1; !calls > allowance in
  (Budget.of_poll poll, fun () -> !calls)

let cases = [
  "prop-data-index", (fun () -> checked_equality Level.one);
  "prop-high-universe-index", (fun () ->
    let* highest = level max_int in
    checked_equality highest);
  "prop-type-index", (fun () ->
    let* _globals = kernel (Check.declare_family Global.empty
        (simple_family (Term.Univ Level.one))) in
    Ok ());
  "type-index-at-bound", (fun () ->
    let* _globals = kernel (Check.declare_family Global.empty
        (anchored_family ~carrier:Level.one ~sort:Level.one ())) in
    Ok ());
  "type-index-above-bound", (fun () ->
    expect_refusal (Error.Index_above_universe "the index y of AnchoredEquality lives at")
      (Check.declare_family Global.empty
         (anchored_family ~carrier:(Level.succ Level.one) ~sort:Level.one ())));
  "prop-runtime-index", (fun () ->
    expect_refusal (Error.Index_not_zero "the index y of AnchoredEquality is at quantity")
      (Check.declare_family Global.empty
         (anchored_family ~quantity:Quantity.Many ~carrier:Level.one ~sort:Level.zero ())));
  "prop-unbound-index-type", (fun () ->
    expect_refusal (Error.Unbound "de Bruijn index 0 is outside the context")
      (Check.declare_family Global.empty (simple_family (Term.Var 0))));
  "prop-index-must-be-type", (fun () ->
    let declaration = anchored_family ~carrier:Level.one ~sort:Level.zero () in
    let* context = kernel (Check.check_telescope
        (Check.make Global.empty Budget.unlimited) declaration.fam_params) in
    expect_refusal (Error.Universe "a term used as a type is not a universe:")
      (Check.index_rules context declaration.fam_name Level.zero
         (Quantity.Zero, "index", Term.Var 0)));
  "prop-erased-data-field", (fun () -> field_bound Quantity.Zero);
  "prop-runtime-data-field", (fun () -> field_bound Quantity.Many);
  "data-field-at-bound", (fun () -> field_at_bound Quantity.Many);
  "prop-index-refuses-free-universe", (fun () ->
    let* free = variable 0 in
    expect_refusal (Error.Universe "universe level is outside the global parameter scope")
      (Check.declare_family Global.empty (simple_family (Term.Univ free))));
  "family-refuses-free-universe", (fun () ->
    let* free = variable 0 in
    let declaration = anchored_family ~carrier:Level.one ~sort:free () in
    expect_refusal (Error.Universe "family universe levels must be closed at M0 Stage A")
      (Check.declare_family Global.empty declaration));
  "index-type-honors-budget", (fun () ->
    let budget = Budget.of_poll (fun () -> true) in
    expect_refusal (Error.Budget_exhausted Check.budget_msg)
      (Check.index_rules (Check.make Global.empty budget) "BudgetedIndex" Level.zero
         (Quantity.Zero, "index", Term.Univ Level.one)));
  "prop-index-skips-level-poll", (fun () ->
    let index = (Quantity.Zero, "index", Term.Univ Level.zero) in
    let patient, spent = counting_budget max_int in
    let* () = kernel (Check.index_rules (Check.make Global.empty patient)
        "BudgetedIndex" Level.zero index) in
    let allowance = spent () in
    let prop_budget, _prop_calls = counting_budget allowance in
    let* () = kernel (Check.index_rules (Check.make Global.empty prop_budget)
        "BudgetedIndex" Level.zero index) in
    let type_budget, _type_calls = counting_budget allowance in
    expect_refusal (Error.Budget_exhausted Check.budget_msg)
      (Check.index_rules (Check.make Global.empty type_budget)
         "BudgetedIndex" Level.one index));
]

let () =
  let failures = List.fold_left (fun failures (name, test) ->
      Result.fold (test ())
        ~ok:(fun () -> Printf.printf "PASS equality %s\n%!" name; failures)
        ~error:(fun message ->
          Printf.printf "FAIL equality %s: %s\n%!" name message;
          failures + 1)) 0 cases in
  if failures = 0 then Printf.printf "EQUALITY-OK cases=%d\n" (List.length cases)
  else exit 1
