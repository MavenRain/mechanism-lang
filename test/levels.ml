open Mechanism_kernel

module Poly = Mechanism_surface.Poly

let ( let* ) = Result.bind

let require (message : string) (condition : bool) : (unit, string) result =
  if condition then Ok () else Error message

(* Total halving, because plan section 11 forbids bare division. *)
let half (n : int) : int = Int.shift_right n 1

let kernel (result : ('a, Error.t) result) : ('a, string) result =
  Result.map_error Error.to_string result

let level (n : int) : (Level.t, string) result =
  Level.of_int n |> Option.to_result ~none:"nonnegative level construction failed"

let variable (n : int) : (Level.t, string) result =
  Level.var n |> Option.to_result ~none:"nonnegative variable construction failed"

let error_kind (error : Error.t) : string =
  match error with
  | Error.Not_yet _message -> "not-yet"
  | Error.Parse (_message, _line, _column) -> "parse"
  | Error.Carry _message -> "carry"
  | Error.Unbound _message -> "unbound"
  | Error.Mismatch _message -> "mismatch"
  | Error.Universe _message -> "universe"
  | Error.Quantity _message -> "quantity"
  | Error.Wrong_leg _message -> "wrong-leg"
  | Error.Missing_branch _message -> "missing-branch"
  | Error.Overflow _message -> "overflow"
  | Error.Cannot_infer _message -> "cannot-infer"
  | Error.Budget_exhausted _message -> "budget"
  | Error.Index_not_zero _message -> "index-not-zero"
  | Error.Index_above_universe _message -> "index-above-universe"
  | Error.Termination _name -> "termination"

let expect_error ?message (kind : string) (result : ('a, Error.t) result) :
    (unit, string) result =
  result
  |> Result.fold
       ~ok:(fun _value -> Error ("expected " ^ kind ^ " refusal"))
       ~error:(fun error ->
         let* () =
           require ("wrong refusal: " ^ Error.to_string error)
             (String.equal kind (error_kind error))
         in
         message
         |> Option.fold ~none:(Ok ()) ~some:(fun expected ->
                require ("wrong diagnostic: " ^ Error.to_string error)
                  (String.equal expected (Error.message error))))

let expect_scope (result : ('a, Error.t) result) : (unit, string) result =
  expect_error "universe" ~message:"universe level is outside the global parameter scope" result

let expect_collision (name : string) (result : ('a, Error.t) result) : (unit, string) result =
  expect_error "mismatch" ~message:("the name " ^ name ^ " is already declared") result

let same_level (expected : Level.t) (actual : Level.t) : (unit, string) result =
  require
    ("expected level " ^ Level.to_string expected ^ ", got " ^ Level.to_string actual)
    (Level.equal expected actual)

let universe (expected : Level.t) (value : Value.t) : (unit, string) result =
  let* actual = Value.as_univ value |> Option.to_result ~none:"expected a universe value" in
  same_level expected actual

let definition (name : string) (body_level : Level.t) : Check.decl =
  {
    Check.d_name = name;
    d_kind = Check.Definition;
    d_ty = Term.Univ (Level.succ body_level);
    d_body = Some (Term.Univ body_level);
  }

let distributed (name : string) : (Check.decl, string) result =
  let* u = variable 0 in
  let* v = variable 1 in
  let* w = variable 2 in
  Ok
    {
      Check.d_name = name;
      d_kind = Check.Definition;
      d_ty = Term.Univ (Level.succ (Level.max (Level.imax u v) (Level.imax u w)));
      d_body = Some (Term.Univ (Level.imax u (Level.max v w)));
    }

let stored_universe (globals : Global.t) (name : string) (body_level : Level.t)
    (reference : Term.t) : (unit, string) result =
  let* entry = Global.find_def name globals |> Option.to_result ~none:"instance was not installed" in
  let* () = require "instantiation did not return its global reference" (reference = Term.Global name) in
  let* ty = kernel (Eval.eval globals [] entry.Global.ty) in
  let* () = universe (Level.succ body_level) ty in
  let* body = kernel (Eval.eval globals [] entry.Global.def) in
  let* () = universe body_level body in
  let* inferred = kernel (Check.infer_term globals reference) in
  universe (Level.succ body_level) inferred

let primitive_count (globals : Global.t) : int =
  Global.StringMap.cardinal globals.Global.entries

let imax_relation (arity : int) : (Level.t * Level.t, string) result =
  let* variables =
    List.fold_left
      (fun result index ->
        let* levels = result in
        let* level = variable index in
        Ok (level :: levels))
      (Ok []) (List.init arity Fun.id)
  in
  match variables with
  | [] -> Error "an imax relation needs level variables"
  | u :: rest ->
      let left = Level.imax u (List.fold_left Level.max Level.zero rest) in
      let right = List.fold_left (fun acc v -> Level.max acc (Level.imax u v)) Level.zero rest in
      Ok (left, right)

let measure_polls (run : Budget.t -> ('a, Error.t) result) : ('a * int, string) result =
  let polls = ref 0 in
  let budget = Budget.of_poll (fun () -> incr polls; false) in
  let* value = kernel (run budget) in
  Ok (value, !polls)

let expect_late_exhaustion (run : Budget.t -> ('a, Error.t) result)
    ~(limit : int) ~(complete : int) : (unit, string) result =
  let polls = ref 0 in
  let budget = Budget.of_poll (fun () -> incr polls; !polls > limit) in
  let* () = expect_error "budget" ~message:"the check budget is exhausted" (run budget) in
  let* () = require "comparison returned before the late poll threshold" (!polls > limit) in
  require "comparison continued through all measured work after exhaustion" (!polls < complete)

let comparison_budget (compare : Budget.t -> Level.t -> Level.t -> (bool, Error.t) result) :
    (unit, string) result =
  let* left, right = imax_relation 12 in
  let run budget = compare budget left right in
  let* equivalent, complete = measure_polls run in
  let* () = require "unbounded symbolic comparison changed its result" equivalent in
  let* () = require "symbolic comparison did not poll during its work" (complete > 64) in
  expect_late_exhaustion run ~limit:(half complete) ~complete

let ignored_shape_checks (globals : Global.t) (good : Check.decl) (bad_body : Term.t) :
    (unit, string) result =
  let* _entry = kernel (Check.check_decl globals Budget.unlimited good) in
  let* expected = kernel (Eval.eval globals [] good.Check.d_ty) in
  let bad = { good with Check.d_body = Some bad_body } in
  let* () = expect_scope (Check.check_decl globals Budget.unlimited bad) in
  let* () = expect_scope (Check.check_term globals bad_body expected) in
  expect_scope (Check.check_scheme globals Budget.unlimited ~arity:0 bad)

let nat_shape (domain : Term.t) : Term.t Shape.t = Shape.SPi (Quantity.Many, "x", domain)

let nat_identity : Check.decl =
  {
    Check.d_name = "shapeIdentity";
    d_kind = Check.Definition;
    d_ty = Term.Ran (nat_shape Prim.nat_ty, Prim.nat_ty);
    d_body = Some (Term.Sec (nat_shape Prim.nat_ty,
      [{ Term.l_binders = [Quantity.Many, "x"]; l_body = Term.Var 0 }]));
  }

let algebra_cases : (string * (unit -> (unit, string) result)) list =
  [
    "imax-zero", (fun () ->
      let* u = variable 0 in
      same_level Level.zero (Level.imax u Level.zero));
    "imax-zero-left", (fun () ->
      let* u = variable 0 in
      same_level u (Level.imax Level.zero u));
    "imax-self", (fun () ->
      let* u = variable 0 in
      same_level u (Level.imax u u));
    "imax-distributes-over-max", (fun () ->
      let* u = variable 0 in
      let* v = variable 1 in
      let* w = variable 2 in
      same_level (Level.max (Level.imax u v) (Level.imax u w))
        (Level.imax u (Level.max v w)));
    "successor-distributes-over-max", (fun () ->
      let* u = variable 0 in
      let* v = variable 1 in
      same_level (Level.max (Level.succ u) (Level.succ v))
        (Level.succ (Level.max u v)));
    "successor-dominates-variable-and-one", (fun () ->
      let* u = variable 0 in
      same_level (Level.succ u) (Level.max (Level.succ u) (Level.max u Level.one)));
    "max-flattens-and-drops-duplicates", (fun () ->
      let* u = variable 0 in
      let* v = variable 1 in
      same_level (Level.max u v) (Level.max (Level.max v u) (Level.max u v)));
    "imax-positive-right-is-max", (fun () ->
      let* u = variable 0 in
      let* v = variable 1 in
      same_level (Level.max u (Level.succ v)) (Level.imax u (Level.succ v)));
    "imax-symbolic-right-is-not-max", (fun () ->
      let* u = variable 0 in
      let* v = variable 1 in
      require "zero branch of imax was discarded"
        (not (Level.equal (Level.imax u v) (Level.max u v))));
    "symbolic-zero-is-not-positive", (fun () ->
      let* u = variable 0 in
      let* () = require "variable was equated to zero" (not (Level.equal u Level.zero)) in
      let* () = require "variable was assumed always positive" (not (Level.always_positive u)) in
      require "successor lost positivity" (Level.always_positive (Level.succ u)));
    "imax-preserves-conditional-positivity", (fun () ->
      let* u = variable 0 in
      let* () = require "imax was positive when its right side can be zero"
          (not (Level.always_positive (Level.imax Level.one u))) in
      require "positive right side was not recognized"
        (Level.always_positive (Level.imax u Level.one)));
    "le-successor-domination", (fun () ->
      let* u = variable 0 in
      let* () = require "u <= succ u was refused" (Level.le u (Level.succ u)) in
      require "succ u <= u was accepted" (not (Level.le (Level.succ u) u)));
    "le-imax-case-split", (fun () ->
      let* u = variable 0 in
      let* v = variable 1 in
      let* () = require "imax <= max was refused" (Level.le (Level.imax u v) (Level.max u v)) in
      require "max <= imax ignored the zero branch"
        (not (Level.le (Level.max u v) (Level.imax u v))));
    "no-finite-sampling-of-unbounded-variables", (fun () ->
      let* eight = level 8 in
      let* u = variable 0 in
      let* () = require "max 8 u was equated with 8"
          (not (Level.equal (Level.max eight u) eight)) in
      require "unbounded variable was bounded by 8" (not (Level.le u eight)));
    "closed-levels-beyond-host-int", (fun () ->
      let* largest = level max_int in
      let next = Level.succ largest in
      let* () = require "successor wrapped at max_int" (Level.le largest next && not (Level.le next largest)) in
      let* () = require "successor became zero" (not (Level.equal next Level.zero)) in
      let expected = Int64.to_string (Int64.succ (Int64.of_int max_int)) in
      require "large closed level printed incorrectly" (String.equal expected (Level.to_string next)));
    "scope-and-constructor-boundaries", (fun () ->
      let* u = variable 0 in
      let* v = variable 1 in
      let* () = require "negative level was constructed" (Option.is_none (Level.of_int (-1))) in
      let* () = require "negative variable was constructed" (Option.is_none (Level.var (-1))) in
      let* () = require "negative scope was admitted" (not (Level.in_scope (-1) Level.zero)) in
      let* () = require "bound variable was refused" (Level.in_scope 1 u) in
      require "unbound variable was admitted"
        (not (Level.in_scope 0 u) && not (Level.in_scope 1 (Level.max u v))));
    "substitution-visits-imax-and-max", (fun () ->
      let* u = variable 0 in
      let* v = variable 1 in
      let* three = level 3 in
      let* actual = Level.subst [three; Level.zero] (Level.imax u (Level.max v Level.one))
          |> Option.to_result ~none:"closed substitution failed" in
      let* () = same_level three actual in
      require "missing substitution argument was accepted"
        (Option.is_none (Level.subst [three] (Level.max u v))));
    "equal-budget-expires-inside-symbolic-comparison", (fun () ->
      comparison_budget Level.equal_budget);
    "le-budget-expires-inside-symbolic-comparison", (fun () ->
      comparison_budget Level.le_budget);
  ]

let checker_cases : (string * (unit -> (unit, string) result)) list =
  [
    "scheme-checks-imax-distribution-symbolically", (fun () ->
      let* declaration = distributed "distribution" in
      kernel (Check.check_scheme Global.initial Budget.unlimited ~arity:3 declaration));
    "point-former-preserves-prop-impredicativity", (fun () ->
      let proposition = Term.Ran (Shape.SPi (Quantity.Zero, "P", Term.Univ Level.zero), Term.Var 0) in
      let* inferred = kernel (Check.infer_term Global.initial proposition) in
      universe Level.zero inferred);
    "symbolic-point-former-preserves-prop-impredicativity", (fun () ->
      let postulate : Check.decl = {
        d_name = "Proposition"; d_kind = Check.Postulate;
        d_ty = Term.Univ Level.zero; d_body = None } in
      let* entry = kernel (Check.check_decl Global.initial Budget.unlimited postulate) in
      let globals = Global.add postulate.Check.d_name entry Global.initial in
      let* u = variable 0 in
      let declaration : Check.decl = {
        d_name = "impredicative"; d_kind = Check.Definition; d_ty = Term.Univ Level.zero;
        d_body = Some (Term.Ran (Shape.SPi (Quantity.Many, "x", Term.Univ u),
          Term.Global postulate.Check.d_name)) } in
      kernel (Check.check_scheme globals Budget.unlimited ~arity:1 declaration));
    "scheme-refuses-finite-sampling-counterfeit", (fun () ->
      let* u = variable 0 in
      let* eight = level 8 in
      let declaration = { (definition "counterfeit" eight) with
          Check.d_body = Some (Term.Univ (Level.max eight u)) } in
      expect_error "mismatch" (Check.check_scheme Global.initial Budget.unlimited ~arity:1 declaration));
    "scheme-refuses-imax-with-zero-branch-deleted", (fun () ->
      let* u = variable 0 in
      let* v = variable 1 in
      let declaration = { (definition "badImMax" (Level.max u v)) with
          Check.d_body = Some (Term.Univ (Level.imax u v)) } in
      expect_error "mismatch" (Check.check_scheme Global.initial Budget.unlimited ~arity:2 declaration));
    "ordinary-inference-refuses-free-level", (fun () ->
      let* u = variable 0 in
      expect_scope (Check.infer_term Global.initial (Term.Univ u)));
    "ordinary-inference-refuses-erased-free-level", (fun () ->
      let* u = variable 0 in
      let hidden = Level.imax u Level.zero in
      let* () = same_level Level.zero hidden in
      expect_scope (Check.infer_term Global.initial (Term.Univ hidden)));
    "raw-inference-refuses-free-level", (fun () ->
      let* u = variable 0 in
      expect_scope
        (Check.infer_node (Check.make Global.initial Budget.unlimited) Quantity.Many (Term.Univ u)));
    "ordinary-check-refuses-free-level", (fun () ->
      let* u = variable 0 in
      expect_scope
        (Check.check_term Global.initial (Term.Univ u) (Value.VUniv (Level.succ u))));
    "ordinary-declaration-refuses-template", (fun () ->
      let* u = variable 0 in
      expect_scope
        (Check.check_decl Global.initial Budget.unlimited (definition "escaped" u)));
    "scheme-refuses-out-of-range-type-level", (fun () ->
      let* u = variable 1 in
      expect_scope
        (Check.check_scheme Global.initial Budget.unlimited ~arity:1 (definition "outside" u)));
    "scheme-refuses-hidden-annotation-level", (fun () ->
      let* outside = variable 1 in
      let declaration = { (definition "annotation" Level.zero) with
          Check.d_body = Some (Term.Ann (Term.Univ Level.zero, Term.Univ outside)) } in
      expect_scope (Check.check_scheme Global.initial Budget.unlimited ~arity:1 declaration));
    "scheme-checks-bound-shape-domain", (fun () ->
      let* u = variable 0 in
      let declaration = { (definition "shape" u) with
          Check.d_body = Some (Term.Ran (Shape.SPi (Quantity.Zero, "A", Term.Univ u), Term.Univ Level.zero)) } in
      kernel (Check.check_scheme Global.initial Budget.unlimited ~arity:1 declaration));
    "scheme-refuses-hidden-shape-domain-level", (fun () ->
      let* u = variable 0 in
      let* outside = variable 1 in
      let declaration = { (definition "shapeOutside" u) with
          Check.d_body = Some (Term.Ran (Shape.SPi (Quantity.Zero, "A", Term.Univ outside), Term.Univ Level.zero)) } in
      expect_scope (Check.check_scheme Global.initial Budget.unlimited ~arity:1 declaration));
    "ignored-section-shape-refuses-free-level", (fun () ->
      let* u = variable 0 in
      let bad_body = Term.Sec (nat_shape (Term.Univ u),
          [{ Term.l_binders = [Quantity.Many, "x"]; l_body = Term.Var 0 }]) in
      ignored_shape_checks Global.initial nat_identity bad_body);
    "ignored-injection-shape-refuses-free-level", (fun () ->
      let* u = variable 0 in
      let zero = Term.Lit (Literal.LInt Bignum.zero) in
      let body shape = Term.In (shape, Term.APt (Quantity.Many, zero), [zero]) in
      let declaration : Check.decl = {
        d_name = "shapePair"; d_kind = Check.Definition;
        d_ty = Term.Lan (nat_shape Prim.nat_ty, Prim.nat_ty);
        d_body = Some (body (nat_shape Prim.nat_ty)) } in
      ignored_shape_checks Global.initial declaration (body (nat_shape (Term.Univ u))));
    "ignored-application-shape-refuses-free-level", (fun () ->
      let* u = variable 0 in
      let* identity = kernel (Check.check_decl Global.initial Budget.unlimited nat_identity) in
      let globals = Global.add nat_identity.Check.d_name identity Global.initial in
      let zero = Term.Lit (Literal.LInt Bignum.zero) in
      let body shape = Term.Out (shape, Term.APt (Quantity.Many, zero), Term.Global nat_identity.Check.d_name) in
      let declaration : Check.decl = {
        d_name = "shapeApplication"; d_kind = Check.Definition; d_ty = Prim.nat_ty;
        d_body = Some (body (nat_shape Prim.nat_ty)) } in
      let bad_body = body (nat_shape (Term.Univ u)) in
      let* () = ignored_shape_checks globals declaration bad_body in
      expect_scope (Check.infer_term globals bad_body));
    "scheme-refuses-negative-arity", (fun () ->
      expect_error "universe" ~message:"a universe parameter arity must be nonnegative"
        (Check.check_scheme Global.initial Budget.unlimited ~arity:(-1) (definition "negative" Level.zero)));
    "closed-family-remains-supported", (fun () ->
      let declaration : Check.family_decl = {
        fam_name = "ClosedFamily"; fam_params = []; fam_indices = []; fam_level = Level.one } in
      let* globals = kernel (Check.declare_family Global.initial declaration) in
      require "closed family disappeared" (Option.is_some (Global.find_family "ClosedFamily" globals)));
    "family-refuses-open-universe", (fun () ->
      let* u = variable 0 in
      let declaration : Check.family_decl = {
        fam_name = "OpenFamily"; fam_params = []; fam_indices = []; fam_level = u } in
      expect_error "universe" ~message:"family universe levels must be closed at M0 Stage A"
        (Check.declare_family Global.initial declaration));
    "family-refuses-open-parameter-type", (fun () ->
      let* u = variable 0 in
      let declaration : Check.family_decl = {
        fam_name = "OpenParameter"; fam_params = [Quantity.Zero, "A", Term.Univ u];
        fam_indices = []; fam_level = Level.one } in
      expect_scope (Check.declare_family Global.initial declaration));
    "closed-high-universe-checks-without-overflow", (fun () ->
      let* largest = level max_int in
      let* inferred = kernel (Check.infer_term Global.initial (Term.Univ largest)) in
      universe (Level.succ largest) inferred);
    "scheme-honors-check-budget", (fun () ->
      let* declaration = distributed "budgeted" in
      expect_error "budget" ~message:Check.budget_msg
        (Check.check_scheme Global.initial (Budget.of_poll (fun () -> true)) ~arity:3 declaration));
    "scheme-budget-expires-during-level-comparison", (fun () ->
      let* left, right = imax_relation 12 in
      let declaration = { (definition "largeComparison" right) with Check.d_body = Some (Term.Univ left) } in
      let run budget = Check.check_scheme Global.initial budget ~arity:12 declaration in
      let* (), baseline = measure_polls (fun budget ->
          Check.check_scheme Global.initial budget ~arity:12 (definition "plainComparison" Level.zero)) in
      let* (), complete = measure_polls run in
      let* () = require "complex comparison did not add polls beyond the same declaration shape"
          (complete > baseline + 64) in
      let limit = Int.max (baseline + 1) (half complete) in
      expect_late_exhaustion run ~limit ~complete);
  ]

let poly_cases : (string * (unit -> (unit, string) result)) list =
  [
    "template-does-not-enter-monomorphic-globals", (fun () ->
      let* declaration = distributed "distribution" in
      let* _templates = kernel (Poly.declare Global.initial Poly.empty ~arity:3 declaration) in
      let* () = require "bare template entered globals" (Option.is_none (Global.find "distribution" Global.initial)) in
      expect_error "unbound" ~message:"distribution"
        (Check.infer_term Global.initial (Term.Global "distribution")));
    "closed-specializations-take-both-imax-branches", (fun () ->
      let* declaration = distributed "distribution" in
      let* templates = kernel (Poly.declare Global.initial Poly.empty ~arity:3 declaration) in
      let* two = level 2 in
      let* three = level 3 in
      let* globals_zero, ref_zero = kernel (Poly.instantiate Global.initial templates
          ~name:"distribution" ~levels:[two; Level.zero; Level.zero] ~as_name:"atZero") in
      let* () = stored_universe globals_zero "atZero" Level.zero ref_zero in
      let* globals_three, ref_three = kernel (Poly.instantiate globals_zero templates
          ~name:"distribution" ~levels:[two; Level.zero; three] ~as_name:"atThree") in
      let* () = stored_universe globals_three "atThree" three ref_three in
      let* () = stored_universe globals_three "atZero" Level.zero ref_zero in
      require "template entered instantiated globals" (Option.is_none (Global.find "distribution" globals_three)));
    "template-refuses-invalid-universal-body", (fun () ->
      let* u = variable 0 in
      let* eight = level 8 in
      let declaration = { (definition "counterfeit" eight) with
          Check.d_body = Some (Term.Univ (Level.max eight u)) } in
      expect_error "mismatch" (Poly.declare Global.initial Poly.empty ~arity:1 declaration));
    "template-refuses-duplicate-name", (fun () ->
      let* u = variable 0 in
      let declaration = definition "same" u in
      let* templates = kernel (Poly.declare Global.initial Poly.empty ~arity:1 declaration) in
      expect_collision "same" (Poly.declare Global.initial templates ~arity:1 declaration));
    "template-refuses-existing-global-name", (fun () ->
      let declaration = definition Prim.nat_name Level.zero in
      expect_collision Prim.nat_name (Poly.declare Global.initial Poly.empty ~arity:0 declaration));
    "instance-refuses-wrong-arity", (fun () ->
      let* declaration = distributed "distribution" in
      let* templates = kernel (Poly.declare Global.initial Poly.empty ~arity:3 declaration) in
      let* () = expect_error "universe" ~message:"the schema distribution expects 3 universe arguments, got 2"
        (Poly.instantiate Global.initial templates
          ~name:"distribution" ~levels:[Level.zero; Level.one] ~as_name:"tooFew") in
      expect_error "universe" ~message:"the schema distribution expects 3 universe arguments, got 4"
        (Poly.instantiate Global.initial templates
          ~name:"distribution" ~levels:[Level.zero; Level.one; Level.one; Level.one] ~as_name:"tooMany"));
    "instance-refuses-open-argument", (fun () ->
      let* u = variable 0 in
      let* templates = kernel (Poly.declare Global.initial Poly.empty ~arity:1 (definition "poly" u)) in
      expect_error "universe" ~message:"universe arguments must be closed" (Poly.instantiate Global.initial templates
          ~name:"poly" ~levels:[u] ~as_name:"open"));
    "instance-refuses-template-name", (fun () ->
      let* u = variable 0 in
      let* templates = kernel (Poly.declare Global.initial Poly.empty ~arity:1 (definition "poly" u)) in
      expect_collision "poly" (Poly.instantiate Global.initial templates
          ~name:"poly" ~levels:[Level.zero] ~as_name:"poly"));
    "instance-refuses-existing-global-name", (fun () ->
      let* u = variable 0 in
      let* templates = kernel (Poly.declare Global.initial Poly.empty ~arity:1 (definition "poly" u)) in
      expect_collision Prim.nat_name (Poly.instantiate Global.initial templates
          ~name:"poly" ~levels:[Level.zero] ~as_name:Prim.nat_name));
    "instance-refuses-unknown-template", (fun () ->
      expect_error "unbound" ~message:"unknown universe schema missing" (Poly.instantiate Global.initial Poly.empty
          ~name:"missing" ~levels:[] ~as_name:"unknown"));
    "failed-instances-preserve-environments", (fun () ->
      let* u = variable 0 in
      let* templates = kernel (Poly.declare Global.initial Poly.empty ~arity:1 (definition "poly" u)) in
      let original_count = primitive_count Global.initial in
      let* () = expect_error "universe" ~message:"the schema poly expects 1 universe arguments, got 0"
        (Poly.instantiate Global.initial templates
          ~name:"poly" ~levels:[] ~as_name:"retry") in
      let* () = require "failed instance changed original globals"
          (Int.equal original_count (primitive_count Global.initial)
           && Option.is_none (Global.find "retry" Global.initial)) in
      let* globals, reference = kernel (Poly.instantiate Global.initial templates
          ~name:"poly" ~levels:[Level.one] ~as_name:"retry") in
      let* () = require "the failed instance leaked an entry into the next environment"
          (Int.equal (primitive_count globals) (original_count + 1)) in
      let* () = stored_universe globals "retry" Level.one reference in
      let* () = expect_collision "retry" (Poly.instantiate globals templates
          ~name:"poly" ~levels:[Level.zero] ~as_name:"retry") in
      stored_universe globals "retry" Level.one reference);
    "instance-rechecks-with-caller-budget", (fun () ->
      let* u = variable 0 in
      let* templates = kernel (Poly.declare Global.initial Poly.empty ~arity:1 (definition "poly" u)) in
      expect_error "budget" ~message:Check.budget_msg
        (Poly.instantiate ~budget:(Budget.of_poll (fun () -> true)) Global.initial templates
           ~name:"poly" ~levels:[Level.one] ~as_name:"budgetInstance"));
  ]

(* Every case runs, so one run of a mutant names every case it breaks. *)
let run (cases : (string * (unit -> (unit, string) result)) list) : (unit, string) result =
  let failures =
    List.fold_left
      (fun failures (name, test) ->
        test ()
        |> Result.fold
             ~ok:(fun () ->
               Printf.printf "PASS levels %s\n%!" name;
               failures)
             ~error:(fun message ->
               Printf.printf "FAIL levels %s: %s\n%!" name message;
               (name ^ ": " ^ message) :: failures))
      [] cases
    |> List.rev
  in
  match failures with
  | [] -> Ok ()
  | first :: rest -> Error (String.concat "; " (first :: rest))

let () =
  run (algebra_cases @ checker_cases @ poly_cases)
  |> Result.fold
       ~ok:(fun () -> print_endline "LEVELS-OK")
       ~error:(fun message ->
         prerr_endline ("LEVELS-FAIL " ^ message);
         exit 1)
