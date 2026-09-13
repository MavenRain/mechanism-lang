open Mechanism_kernel

module Families = Mechanism_surface.Family_poly
let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let refusal expected result = Result.fold result
  ~ok:(fun _value -> Error ("expected refusal: " ^ Error.to_string expected))
  ~error:(fun actual -> require ("wrong refusal: " ^ Error.to_string actual)
    (String.starts_with ~prefix:(Error.to_string expected) (Error.to_string actual)))
let collision name = Error.Mismatch ("the name " ^ name ^ " is already declared")
let marker name : Check.family_decl = {
  fam_name = name; fam_params = []; fam_indices = []; fam_level = Level.one;
}
let ctor : Check.ctor_decl = {
  ct_name = "mark"; ct_args = []; ct_res_params = []; ct_res_idx = [];
}
let ty name = Term.Lan (Shape.SMu (name, []), Rules.diagram_of [])
let value name args = Term.In (Shape.SMu (name, []), Term.ACtor "mark", args)
let definition name typ body : Check.decl = {
  d_name = name; d_kind = Check.Definition; d_ty = typ; d_body = Some body;
}
let families = [marker "First", [ctor]; marker "Second",
  [{ ctor with ct_args = [Quantity.Many, "first", ty "First"] }]]
let witness = definition "witness" (ty "Second") (value "Second" [value "First" []])
let alias = definition "alias" (ty "Second") (Term.Global "witness")
let declare ?(globals = Global.empty) ?(catalog = Families.empty)
    ?(budget = Budget.unlimited) ?(members = [witness; alias]) group =
  Families.declare_group ~budget ~members globals catalog ~arity:1 group
let instantiate ?(globals = Global.empty) ?(budget = Budget.unlimited) catalog as_name =
  Families.instantiate ~budget globals catalog ~name:"First" ~levels:[Level.one] ~as_name

let positive () =
  let* catalog = kernel (declare families) in
  let* globals = kernel (instantiate catalog "One") in
  let* globals = kernel (instantiate ~globals catalog "Two") in
  let* globals, _rows = kernel (Mechanism_surface.Elab.check_in globals
    "def client : One := case One_alias as self in One_Second return One with\n\
     | mark (first : One) => first\n") in
  let* v = kernel (Eval.eval globals [] (Term.Global "client")) in
  let* actual = kernel (Eval.quote globals 0 v) in
  let* () = require "cross-family constructor reference was not specialized"
    (actual = value "One" []) in
  let* () = require "symbolic names escaped"
    (Global.find_family "First" globals = None && Global.find_family "Second" globals = None
      && Global.find "witness" globals = None) in
  refusal (Error.Mismatch "the term has type (Lan SMu Two_Second")
    (Mechanism_surface.Elab.check_in globals "def mixed : One_Second := Two_alias")

let exact_refusal expected result = Result.fold result
  ~ok:(fun _value -> Error ("expected refusal: " ^ Error.to_string expected))
  ~error:(fun actual -> require ("wrong refusal: " ^ Error.to_string actual)
    (actual = expected))

let elaborate ?(budget = Budget.unlimited) ?(catalog = Families.empty) members group =
  Families.declare_group_elaborated ~budget ~members Global.empty catalog ~arity:1 group

let callback_order () =
  let guard message condition = if condition then Ok () else Error (Error.Mismatch message) in
  let first globals =
    let* () = guard "callback saw an incomplete family"
      (List.for_all (fun name -> Global.find_family name globals
        |> Option.fold ~none:false ~some:(fun family ->
          match family.Positivity.f_status with
          | Positivity.Complete _ -> family.f_positive
          | Positivity.Provisional | Positivity.Builtin -> false)) ["First"; "Second"]) in
    let* () = guard "callback saw a future member"
      (Global.find "witness" globals = None && Global.find "alias" globals = None) in
    Ok witness in
  let second globals =
    let* entry = Global.find "witness" globals |> Option.to_result
      ~none:(Error.Unbound "callback predecessor") in
    let* () = match entry with
      | Global.Def d -> guard "callback predecessor changed" (Some d.def = witness.d_body)
      | Global.Axiom _ | Global.Prim _ -> Error (Error.Mismatch "unchecked predecessor") in
    Ok alias in
  let* callback_catalog = kernel (elaborate [first; second] families) in
  let* raw_catalog = kernel (declare families) in
  let* actual = kernel (instantiate callback_catalog "One") in
  let* expected = kernel (instantiate raw_catalog "One") in
  require "callback declaration changed the installed program"
    (Global.StringMap.equal (=) actual.entries expected.entries
      && Global.StringMap.equal (=) actual.families expected.families
      && Families.members callback_catalog "First" = Some ["witness"; "alias"])

let callback_failure expected member =
  let reached = ref false in
  let* () = exact_refusal expected (elaborate
    [(fun _globals -> Ok member);
     (fun _globals -> reached := true; Ok alias)] families) in
  require "a callback ran after an invalid predecessor" (not !reached)

let cases = [
  "ordered-families-members-and-renaming", positive;
  "empty", (fun () -> refusal (Error.Mismatch "a family schema group must be nonempty")
    (declare ~members:[] []));
  "duplicate-family", (fun () -> refusal (collision "First")
    (declare ~members:[] [marker "First", [ctor]; marker "First", [ctor]]));
  "forward-reference", (fun () -> refusal (Error.Unbound "the family First is not declared")
    (declare (List.rev families)));
  "duplicate-constructor", (fun () ->
    refusal (Error.Mismatch "the constructor mark is already declared in Second")
      (declare ~members:[] [marker "First", [ctor]; marker "Second", [ctor; ctor]]));
  "member-family-collision", (fun () -> refusal (collision "Second")
    (declare ~members:[{ witness with d_name = "Second" }] families));
  "member-template-reference", (fun () ->
    let* catalog = kernel (Families.declare Global.empty Families.empty ~arity:0
      (marker "Other") [ctor]) in
    refusal (Error.Not_yet "references between family schemas are not supported")
      (declare ~catalog ~members:[{ witness with d_ty = ty "Other" }] families));
  "member-body", (fun () -> refusal (Error.Mismatch "the term has type (Lan SMu First")
    (declare ~members:[{ witness with d_body =
      Some (Term.Ann (value "First" [], ty "First")) }] families));
  "member-forward-reference", (fun () -> refusal (Error.Unbound "witness")
    (declare ~members:[alias; witness] families));
  "postulate", (fun () -> refusal (Error.Not_yet "family schema members must be definitions")
    (declare ~members:[{ witness with d_kind = Check.Postulate; d_body = None }] families));
  "companion-level-scope", (fun () ->
    let* free = Level.var 1 |> Option.to_result ~none:"invalid universe variable" in
    refusal (Error.Universe "universe level is outside the global parameter scope")
      (declare ~members:[] [marker "First", [ctor];
        { (marker "Second") with fam_level = Level.succ free }, [ctor]]));
  "companion-level-specialization", (fun () ->
    let* u = Level.var 0 |> Option.to_result ~none:"invalid universe variable" in
    let* catalog = kernel (declare ~members:[] [marker "First", [ctor];
      { (marker "Second") with fam_level = Level.succ u },
      [{ ctor with ct_args = [Quantity.Many, "A", Term.Univ u] }]]) in
    let* globals = kernel (instantiate catalog "Levels") in
    let* family = Global.find_family "Levels_Second" globals
      |> Option.to_result ~none:"specialized companion missing" in
    let* () = require "companion universe was not specialized"
      (Level.equal family.Positivity.f_level (Level.succ Level.one)) in
    let* _checked = kernel (Mechanism_surface.Elab.check_in globals
      "def levelWitness : Levels_Second := mark Levels") in
    Ok ());
  "companion-global-collision", (fun () ->
    let globals = Global.add "Second" (Global.Axiom { ax_ty = Term.Univ Level.one }) Global.empty in
    refusal (collision "Second") (declare ~globals families));
  "companion-template-collision", (fun () ->
    let* catalog = kernel (Families.declare Global.empty Families.empty ~arity:0
      (marker "Second") [ctor]) in
    refusal (collision "Second") (declare ~catalog families));
  "target-companion-collision-atomic", (fun () ->
    let* catalog = kernel (declare families) in
    let globals = Global.add "One_Second" (Global.Axiom { ax_ty = Term.Univ Level.one }) Global.empty in
    let* () = refusal (collision "One_Second") (instantiate ~globals catalog "One") in
    let* fresh = kernel (instantiate ~globals catalog "Two") in
    require "the refused instance left the caller globals unusable"
      (Option.is_some (Global.find_family "Two_Second" fresh)
        && Option.is_some (Global.find "Two_witness" fresh)));
  "target-member-collision-atomic", (fun () ->
    let* catalog = kernel (declare families) in
    let globals = Global.add "One_alias" (Global.Axiom { ax_ty = Term.Univ Level.one }) Global.empty in
    let* () = refusal (collision "One_alias") (instantiate ~globals catalog "One") in
    let* fresh = kernel (instantiate ~globals catalog "Two") in
    require "the refused instance left the caller globals unusable"
      (Option.is_some (Global.find "Two_alias" fresh)
        && Option.is_some (Global.find "Two_witness" fresh)));
  "target-companion-template-collision", (fun () ->
    let* catalog = kernel (declare families) in
    let* catalog = kernel (Families.declare Global.empty catalog ~arity:0
      (marker "One_Second") [ctor]) in
    refusal (collision "One_Second") (instantiate catalog "One"));
  "closed-rechecking", (fun () ->
    let globals = Global.add "Seed" (Global.Axiom { ax_ty = Term.Univ Level.one }) Global.empty in
    let group = [marker "First", [ctor]; marker "Second",
      [{ ctor with ct_args = [Quantity.Many, "seed", Term.Global "Seed"] }]] in
    let* catalog = kernel (declare ~globals ~members:[] group) in
    let* _installed = kernel (instantiate ~globals catalog "Compatible") in
    refusal (Error.Unbound "Seed") (instantiate catalog "Missing"));
  "arity", (fun () ->
    let* catalog = kernel (declare families) in
    refusal (Error.Universe "the family schema First expects 1 universe arguments, got 0")
      (Families.instantiate Global.empty catalog ~name:"First" ~levels:[] ~as_name:"One"));
  "closed-levels", (fun () ->
    let* catalog = kernel (declare families) in
    let* u = Level.var 0 |> Option.to_result ~none:"invalid universe variable" in
    refusal (Error.Universe "universe arguments must be closed")
      (Families.instantiate Global.empty catalog ~name:"First" ~levels:[u] ~as_name:"One"));
  "declaration-budget", (fun () ->
    refusal (Error.Budget_exhausted Check.budget_msg)
      (declare ~budget:(Budget.of_poll (fun () -> true)) families));
  "specialization-budget", (fun () ->
    let* catalog = kernel (declare families) in
    refusal (Error.Budget_exhausted Check.budget_msg)
      (instantiate ~budget:(Budget.of_poll (fun () -> true)) catalog "One"));
  "callback-order-and-parity", callback_order;
  "callback-body-checked", (fun () -> callback_failure
    (Error.Unbound "de Bruijn index 0 is outside the context")
    { witness with d_body = Some (Term.Var 0) });
  "callback-postulate-refused", (fun () -> callback_failure
    (Error.Not_yet "family schema members must be definitions")
    { witness with d_kind = Check.Postulate; d_body = None });
  "callback-hidden-level-scope", (fun () ->
    let* free = Level.var 1 |> Option.to_result ~none:"invalid universe variable" in
    let domain = Term.Univ Level.one in
    let shape = Shape.SPi (Quantity.Many, "A", domain) in
    let identity = Term.Ann
      (Term.Sec (shape, [{ Term.l_binders = [Quantity.Many, "A"]; l_body = Term.Var 0 }]),
       Term.Ran (shape, domain)) in
    callback_failure (Error.Universe "universe level is outside the global parameter scope")
      (definition "hidden" domain (Term.Out
        (Shape.SPi (Quantity.Many, "ignored", Term.Univ free),
         Term.APt (Quantity.Many, Term.Univ Level.zero), identity))));
  "callback-error-stops", (fun () ->
    let reached = ref false in
    let expected = Error.Cannot_infer "callback failed" in
    let* () = exact_refusal expected (elaborate
      [(fun _globals -> Ok witness); (fun _globals -> Error expected);
       (fun _globals -> reached := true; Ok alias)] families) in
    require "a callback ran after an elaboration failure" (not !reached));
  "callback-invalid-family-stops", (fun () ->
    let reached = ref false in
    let* () = exact_refusal (Error.Mismatch "a family schema group must be nonempty")
      (elaborate [(fun _globals -> reached := true; Ok witness)] []) in
    let* () = exact_refusal (Error.Unbound "the family First is not declared")
      (elaborate [(fun _globals -> reached := true; Ok witness)] (List.rev families)) in
    require "a callback ran without checked families" (not !reached));
  "callback-budget-stops", (fun () ->
    let cancelled = ref false in
    let reached = ref false in
    let budget = Budget.of_poll (fun () -> !cancelled) in
    let* () = exact_refusal (Error.Budget_exhausted Check.budget_msg)
      (elaborate ~budget
        [(fun _globals -> cancelled := true; Ok witness);
         (fun _globals -> reached := true; Ok alias)] families) in
    require "a callback ran after cancellation" (not !reached));
  "callback-budget-stops-at-entry", (fun () ->
    (* The group without members measures the polls of the family phase alone,
       so the next poll is the one that guards the first callback. *)
    let counted = ref 0 in
    let counting = Budget.of_poll (fun () -> incr counted; false) in
    let* _catalog = kernel (elaborate ~budget:counting [] families) in
    let families_polls = !counted in
    let polled = ref 0 in
    let reached = ref false in
    let budget = Budget.of_poll (fun () -> incr polled; !polled > families_polls) in
    let* () = exact_refusal (Error.Budget_exhausted Check.budget_msg)
      (elaborate ~budget
        [(fun _globals -> reached := true; Ok witness);
         (fun _globals -> Ok alias)] families) in
    require "the first callback ran before the member poll" (not !reached));
  "member-refusal-precedence", (fun () ->
    (* The group checks each member before it traverses the next one, so the
       first failing member names the refusal.  The raw declaration traverses
       every member first, so a later traversal refusal comes first there. *)
    let* free = Level.var 1 |> Option.to_result ~none:"invalid universe variable" in
    let bad_body = definition "badBody" (ty "First") (Term.Var 0) in
    let bad_level = definition "badLevel" (Term.Univ free) (Term.Univ Level.zero) in
    let members = [bad_body; bad_level] in
    let* () = refusal (Error.Unbound "de Bruijn index 0") (declare ~members families) in
    refusal (Error.Universe "universe level is outside the global parameter scope")
      (Families.declare ~members Global.empty Families.empty ~arity:1
        (marker "First") [ctor]));
]

let () =
  let result = List.fold_left (fun acc (name, run) ->
    let* () = acc in Result.map_error (fun message -> name ^ ": " ^ message) (run ()))
    (Ok ()) cases in
  Result.fold result
    ~ok:(fun () -> Printf.printf "FAMILY-GROUPS-OK cases=%d\n" (List.length cases))
    ~error:(fun message -> Printf.printf "FAMILY-GROUPS-FAIL %s\n" message; exit 1)
