open Mechanism_kernel

module Family_poly = Mechanism_surface.Family_poly

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let refusal expected result = Result.fold result
  ~ok:(fun _value -> Error ("expected refusal: " ^ Error.to_string expected))
  ~error:(fun actual -> require ("wrong refusal: " ^ Error.to_string actual)
    (String.starts_with ~prefix:(Error.to_string expected) (Error.to_string actual)))
let collision name = Error.Mismatch ("the name " ^ name ^ " is already declared")
let variable index = Level.var index |> Option.to_result ~none:"invalid universe variable"
let arrow q name domain body = Term.Ran (Shape.SPi (q, name, domain), body)
let lambda q name domain body = Term.Sec (Shape.SPi (q, name, domain),
  [{ Term.l_binders = [q, name]; l_body = body }])

let marker name : Check.family_decl = {
  fam_name = name; fam_params = []; fam_indices = []; fam_level = Level.one;
}
let constructor : Check.ctor_decl = {
  ct_name = "mark"; ct_args = []; ct_res_params = []; ct_res_idx = [];
}
let marker_type name = Term.Lan (Shape.SMu (name, []), Rules.diagram_of [])
let definition name ty body : Check.decl = {
  d_name = name; d_kind = Check.Definition; d_ty = ty; d_body = Some body;
}
let witness = definition "witness" (marker_type "Marker")
  (Term.In (Shape.SMu ("Marker", []), Term.ACtor "mark", []))
let alias = definition "alias" (marker_type "Marker") (Term.Global "witness")
let identity u = definition "identity"
  (arrow Quantity.Zero "A" (Term.Univ u) (arrow Quantity.Many "value" (Term.Var 0) (Term.Var 1)))
  (lambda Quantity.Zero "A" (Term.Univ u) (lambda Quantity.Many "value" (Term.Var 0) (Term.Var 0)))

let declare ?(globals = Global.empty) ?(catalog = Family_poly.empty)
    ?(budget = Budget.unlimited) members =
  Family_poly.declare ~budget ~members globals catalog ~arity:1 (marker "Marker") [constructor]
let instantiate ?(globals = Global.empty) ?(budget = Budget.unlimited) ?exports catalog as_name =
  Family_poly.instantiate ~budget ?exports globals catalog ~name:"Marker" ~levels:[Level.one] ~as_name

let positive () =
  let* u = variable 0 in
  let* catalog = kernel (declare [witness; alias; identity u]) in
  let* () = require "member inventory differs"
    (Family_poly.members catalog "Marker" = Some ["witness"; "alias"; "identity"]
      && Family_poly.members catalog "missing" = None) in
  let* globals = kernel (instantiate catalog "First") in
  let* globals = kernel (instantiate ~globals catalog "Second") in
  let* () = require "symbolic names escaped into ordinary globals"
    (Global.find "witness" globals = None && Global.find_family "Marker" globals = None) in
  let* _checked, _rows = kernel (Mechanism_surface.Elab.check_in globals
    "def firstClient : First := First_identity First First_alias\n\
     def secondClient : Second := Second_identity Second Second_alias\n") in
  let* entry = Global.find "First_alias" globals |> Option.to_result ~none:"alias missing" in
  let* () = match entry with
    | Global.Def d -> require "member reference was not renamed"
        (d.Global.def = Term.Global "First_witness" && d.ty = marker_type "First")
    | Global.Axiom _ | Global.Prim _ -> Error "member is not a definition" in
  refusal (Error.Mismatch "the term has type")
    (Mechanism_surface.Elab.check_in globals "def mixed : First := Second_alias\n")

let hidden_scope level =
  let domain = Term.Univ Level.one in
  let identity = Term.Ann (lambda Quantity.Many "A" domain (Term.Var 0),
    arrow Quantity.Many "A" domain domain) in
  definition "hidden" domain (Term.Out
    (Shape.SPi (Quantity.Many, "ignored", Term.Univ level),
     Term.APt (Quantity.Many, Term.Univ Level.zero), identity))

let counting_budget allowance =
  let count = ref 0 in
  Budget.of_poll (fun () -> count := !count + 1; !count > allowance), (fun () -> !count)

(** Polls that one member adds to a members-free declaration, and polls that
    the same member adds to a members-free specialization.  Both numbers are
    measured against the members-free run in the case itself, so a dropped
    poll on the member path breaks the exact equality. *)
let declare_member_polls = 16
let specialize_member_polls = 13

(** Polls that a specialization makes before the first export binding.  A
    budget that stops on that poll refuses an invalid mapping with the budget
    message, so the export poll runs before the mapping is read. *)
let export_prefix_polls = 1

let chosen_exports = ["witness", "chosenWitness"; "alias", "chosenAlias"]

let missing_export = Error.Mismatch "every family member needs an export name"
let repeated_member = Error.Mismatch "member exports must name each member once"
let repeated_target target =
  Error.Mismatch ("member exports repeat the target " ^ target)

let bad_exports expected exports () =
  let* catalog = kernel (declare [witness; alias]) in
  let* () = require "invalid exports produced a name plan"
    (Family_poly.instance_names ~exports catalog ~name:"Marker" ~as_name:"Chosen" = None) in
  refusal expected (instantiate ~exports catalog "Chosen")

let cases = [
  "ordered-members-and-renaming", positive;
  "chosen-export-names-and-computation", (fun () ->
    let* catalog = kernel (declare [witness; alias]) in
    let exports = List.rev chosen_exports in
    let* () = require "export name plan differs"
      (Family_poly.instance_names ~exports catalog ~name:"Marker" ~as_name:"Chosen"
        = Some (["Chosen"], ["chosenWitness"; "chosenAlias"])) in
    let* globals = kernel (instantiate ~exports catalog "Chosen") in
    let* _ = kernel (Mechanism_surface.Elab.check_in globals
      "mu ExportWitness : (0 x : Chosen) -> Type 0 with\n\
       | exportWitness : ExportWitness chosenWitness\n\
       def exportedComputes : ExportWitness chosenAlias := exportWitness\n") in
    require "default names escaped an explicit export mapping"
      (Global.find "Chosen_witness" globals = None && Global.find "Chosen_alias" globals = None));
  "exports-empty-mapping", bad_exports missing_export [];
  "exports-missing-member", bad_exports missing_export ["witness", "chosenWitness"];
  "exports-unknown-member",
    bad_exports repeated_member (chosen_exports @ ["unknown", "extra"]);
  "exports-duplicate-source",
    bad_exports repeated_member ["witness", "first"; "witness", "second"];
  "exports-duplicate-target",
    bad_exports (repeated_target "same") ["witness", "same"; "alias", "same"];
  "exports-family-collision",
    bad_exports (collision "Chosen") ["witness", "Chosen"; "alias", "chosenAlias"];
  "exports-ambient-collision", (fun () ->
    let* catalog = kernel (declare [witness; alias]) in
    let globals = Global.add "chosenAlias" (Global.Axiom { ax_ty = Term.Univ Level.one }) Global.empty in
    refusal (collision "chosenAlias") (instantiate ~globals ~exports:chosen_exports catalog "Chosen"));
  "exports-template-collision", (fun () ->
    let* catalog = kernel (declare [witness; alias]) in
    refusal (collision "Marker")
      (instantiate ~exports:["witness", "Marker"; "alias", "chosenAlias"] catalog "Chosen"));
  "exports-companion-collision", (fun () ->
    let* catalog = kernel (Family_poly.declare_group ~members:[witness; alias]
      Global.empty Family_poly.empty ~arity:1
      [marker "Marker", [constructor]; marker "Companion", [constructor]]) in
    let exports = ["witness", "Chosen_Companion"; "alias", "chosenAlias"] in
    let* () = require "companion collision produced a name plan"
      (Family_poly.instance_names ~exports catalog ~name:"Marker" ~as_name:"Chosen" = None) in
    refusal (collision "Chosen_Companion") (instantiate ~exports catalog "Chosen"));
  "exports-constructor-collision", (fun () ->
    let* catalog = kernel (declare [witness; alias]) in
    let exports = ["witness", "mark"; "alias", "chosenAlias"] in
    let* () = require "constructor collision produced a name plan"
      (Family_poly.instance_names ~exports catalog ~name:"Marker" ~as_name:"Chosen" = None) in
    refusal (collision "mark") (instantiate ~exports catalog "Chosen"));
  "exports-ambient-constructor-collision", (fun () ->
    let* catalog = kernel (declare [witness; alias]) in
    let* ambient, _rows = kernel (Mechanism_surface.Elab.check_in Global.empty
      "mu Flag : Type 0 with\n| other : Flag\n") in
    let* _ = kernel (instantiate ~globals:ambient ~exports:chosen_exports catalog "Chosen") in
    refusal (collision "other") (instantiate ~globals:ambient
      ~exports:["witness", "other"; "alias", "chosenAlias"] catalog "Chosen"));
  "exports-as-name-under-reuse", (fun () ->
    let* catalog = kernel (declare [witness; alias]) in
    let* globals = kernel (instantiate catalog "First") in
    let reuse = ["Marker", "First"] in
    let exports = ["witness", "Chosen"; "alias", "chosenAlias"] in
    let* () = require "as_name collision under reuse produced a name plan"
      (Family_poly.instance_names ~reuse ~exports catalog ~name:"Marker" ~as_name:"Chosen" = None) in
    refusal (collision "Chosen") (Family_poly.instantiate ~reuse ~exports globals catalog
      ~name:"Marker" ~levels:[Level.one] ~as_name:"Chosen"));
  "exports-with-family-reuse", (fun () ->
    let* catalog = kernel (declare [witness; alias]) in
    let* globals = kernel (instantiate catalog "First") in
    let reuse = ["Marker", "First"] in
    let* () = require "reused family leaked into exported name plan"
      (Family_poly.instance_names ~reuse ~exports:chosen_exports catalog
        ~name:"Marker" ~as_name:"Chosen" = Some ([], ["chosenWitness"; "chosenAlias"])) in
    let* installed = kernel (Family_poly.instantiate ~reuse ~exports:chosen_exports
      globals catalog ~name:"Marker" ~levels:[Level.one] ~as_name:"Chosen") in
    let* _ = kernel (Mechanism_surface.Elab.check_in installed "def reused : First := chosenAlias") in
    let exports = ["witness", "First"; "alias", "otherAlias"] in
    refusal (collision "First") (Family_poly.instantiate ~reuse ~exports globals catalog
      ~name:"Marker" ~levels:[Level.one] ~as_name:"Chosen"));
  "export-validation-budget", (fun () ->
    let* catalog = kernel (declare [witness; alias]) in
    let budget, count = counting_budget max_int in
    let* _globals = kernel (instantiate ~budget catalog "Plain") in
    let plain = count () in
    let budget, count = counting_budget max_int in
    let* _globals = kernel
      (instantiate ~budget ~exports:chosen_exports catalog "Chosen") in
    let allowance = plain + List.length chosen_exports in
    let* () = require
      (Printf.sprintf "export polls %d, expected %d" (count ()) allowance)
      (count () = allowance) in
    let budget, _count = counting_budget plain in
    let* _globals = kernel (instantiate ~budget catalog "Fitted") in
    let budget, _count = counting_budget plain in
    let* () = refusal (Error.Budget_exhausted Check.budget_msg)
      (instantiate ~budget ~exports:chosen_exports catalog "Chosen") in
    let repeated = ["witness", "same"; "alias", "same"] in
    let budget, _count = counting_budget export_prefix_polls in
    refusal (Error.Budget_exhausted Check.budget_msg)
      (instantiate ~budget ~exports:repeated catalog "Chosen"));
  "exports-late-budget-rollback", (fun () ->
    let* catalog = kernel (declare [witness; alias]) in
    let prepared = Global.add "seed"
      (Global.Axiom { ax_ty = Term.Univ Level.one }) Global.empty in
    let exported globals budget =
      instantiate ~globals ~budget ~exports:chosen_exports catalog "Chosen" in
    let budget, calls = counting_budget max_int in
    let* _ = kernel (exported prepared budget) in
    let limited, _ = counting_budget (calls () - 1) in
    let* () = refusal (Error.Budget_exhausted Check.budget_msg)
      (exported prepared limited) in
    let* () = require "a refused run changed the input environment"
      (Global.find "chosenWitness" prepared = None
        && Global.find "chosenAlias" prepared = None
        && Global.find_family "Chosen" prepared = None) in
    let* installed = kernel (exported prepared Budget.unlimited) in
    require "the repeated run installed no exported member"
      (Option.is_some (Global.find "chosenWitness" installed)
        && Option.is_some (Global.find "chosenAlias" installed)
        && Option.is_some (Global.find_family "Chosen" installed)));
  "definition-must-check", (fun () ->
    refusal (Error.Unbound "de Bruijn index 0 is outside the context")
      (declare [{ witness with d_body = Some (Term.Var 0) }]));
  "postulate-refused", (fun () ->
    refusal (Error.Not_yet "family schema members must be definitions")
      (declare [{ witness with d_kind = Check.Postulate; d_body = None }]));
  "body-required", (fun () -> refusal (Check.missing_body "witness")
    (declare [{ witness with d_body = None }]));
  "forward-reference", (fun () -> refusal (Error.Unbound "witness") (declare [alias; witness]));
  "self-reference", (fun () -> refusal (Error.Unbound "witness")
    (declare [{ witness with d_body = Some (Term.Global "witness") }]));
  "duplicate-member", (fun () -> refusal (collision "witness") (declare [witness; witness]));
  "family-name-collision", (fun () -> refusal (collision "Marker")
    (declare [{ witness with d_name = "Marker" }]));
  "existing-global-collision", (fun () ->
    let globals = Global.add "witness" (Global.Axiom { ax_ty = Term.Univ Level.one }) Global.empty in
    refusal (collision "witness") (declare ~globals [witness]));
  "hidden-level-scope", (fun () ->
    let* u = variable 0 in
    let* _catalog = kernel (declare [hidden_scope u]) in
    let* free = variable 1 in
    refusal (Error.Universe "universe level is outside the global parameter scope")
      (declare [hidden_scope free]));
  "hidden-level-specialization", (fun () ->
    let* u = variable 0 in
    let* catalog = kernel (declare [hidden_scope u]) in
    let* globals = kernel (instantiate catalog "Hidden") in
    let* entry = Global.find "Hidden_hidden" globals |> Option.to_result ~none:"hidden member missing" in
    match entry with
    | Global.Def d -> require "a hidden universe argument was not specialized"
        (Some d.Global.def = (hidden_scope Level.one).Check.d_body)
    | Global.Axiom _ | Global.Prim _ -> Error "hidden member is not a definition");
  "member-template-collision", (fun () ->
    let* catalog = kernel (Family_poly.declare Global.empty Family_poly.empty ~arity:0
      (marker "Other") [constructor]) in
    refusal (collision "Other") (declare ~catalog [{ witness with d_name = "Other" }]));
  "cross-template-reference", (fun () ->
    let* catalog = kernel (Family_poly.declare Global.empty Family_poly.empty ~arity:0
      (marker "Other") [constructor]) in
    refusal (Error.Not_yet "references between family schemas are not supported")
      (declare ~catalog [definition "other" (marker_type "Other")
        (Term.In (Shape.SMu ("Other", []), Term.ACtor "mark", []))]));
  "target-global-collision", (fun () ->
    let* catalog = kernel (declare [witness; alias]) in
    let globals = Global.add "First_alias" (Global.Axiom { ax_ty = Term.Univ Level.one }) Global.empty in
    let* () = refusal (collision "First_alias") (instantiate ~globals catalog "First") in
    require "failed specialization modified input globals"
      (Global.find_family "First" globals = None && Global.find "First_witness" globals = None));
  "target-family-collision", (fun () ->
    let* catalog = kernel (declare [witness]) in
    let* globals = kernel (Check.declare_family Global.empty (marker "First_witness")) in
    refusal (collision "First_witness") (instantiate ~globals catalog "First"));
  "target-template-collision", (fun () ->
    let* catalog = kernel (declare [witness]) in
    let* catalog = kernel (Family_poly.declare Global.empty catalog ~arity:0
      (marker "First_witness") [constructor]) in
    refusal (collision "First_witness") (instantiate catalog "First"));
  "closed-rechecking", (fun () ->
    let globals = Global.add "seed" (Global.Axiom { ax_ty = Term.Univ Level.one }) Global.empty in
    let* catalog = kernel (declare ~globals
      [definition "seedUse" (Term.Univ Level.one) (Term.Global "seed")]) in
    let* _installed = kernel (instantiate ~globals catalog "Compatible") in
    let globals = Global.add "seed" (Global.Axiom { ax_ty = Term.Univ (Level.succ Level.one) })
      Global.empty in
    refusal (Error.Mismatch "the term has type") (instantiate ~globals catalog "Changed"));
  "declaration-member-budget", (fun () ->
    let budget, count = counting_budget max_int in
    let* _catalog = kernel (declare ~budget []) in
    let base = count () in
    let budget, count = counting_budget max_int in
    let* _catalog = kernel (declare ~budget [witness]) in
    let allowance = base + declare_member_polls in
    let* () = require
      (Printf.sprintf "member declaration polls %d, expected %d" (count ()) allowance)
      (count () = allowance) in
    let budget, _count = counting_budget (allowance - 1) in
    refusal (Error.Budget_exhausted Check.budget_msg) (declare ~budget [witness]));
  "specialization-member-budget", (fun () ->
    let* empty = kernel (declare []) in
    let budget, count = counting_budget max_int in
    let* _globals = kernel (instantiate ~budget empty "Empty") in
    let base = count () in
    let* catalog = kernel (declare [witness]) in
    let budget, count = counting_budget max_int in
    let* _globals = kernel (instantiate ~budget catalog "Limited") in
    let allowance = base + specialize_member_polls in
    let* () = require
      (Printf.sprintf "member specialization polls %d, expected %d" (count ()) allowance)
      (count () = allowance) in
    let budget, _count = counting_budget (allowance - 1) in
    refusal (Error.Budget_exhausted Check.budget_msg) (instantiate ~budget catalog "Limited"));
]

let () =
  let selected = Sys.argv |> Array.to_list |> fun args ->
    List.nth_opt args 1 |> Option.fold ~none:(Ok cases)
      ~some:(fun name -> List.assoc_opt name cases
        |> Option.to_result ~none:("unknown case: " ^ name)
        |> Result.map (fun run -> [name, run])) in
  let result =
    let* selected = selected in
    let* () = List.fold_left (fun acc (name, run) ->
      let* () = acc in Result.map_error (fun message -> name ^ ": " ^ message) (run ()))
      (Ok ()) selected in
    Ok (List.length selected) in
  Result.fold result
    ~ok:(fun count -> Printf.printf "FAMILY-MEMBERS-OK cases=%d\n" count)
    ~error:(fun message -> Printf.printf "FAMILY-MEMBERS-FAIL %s\n" message; exit 1)
