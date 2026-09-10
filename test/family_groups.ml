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
]

let () =
  let result = List.fold_left (fun acc (name, run) ->
    let* () = acc in Result.map_error (fun message -> name ^ ": " ^ message) (run ()))
    (Ok ()) cases in
  Result.fold result
    ~ok:(fun () -> Printf.printf "FAMILY-GROUPS-OK cases=%d\n" (List.length cases))
    ~error:(fun message -> Printf.printf "FAMILY-GROUPS-FAIL %s\n" message; exit 1)
