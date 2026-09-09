open Mechanism_kernel

module Family_poly = Mechanism_surface.Family_poly

let ( let* ) = Result.bind
let require message condition = if condition then Ok () else Error message
let kernel result = Result.map_error Error.to_string result

let expect_refusal expected result =
  Result.fold result
    ~ok:(fun _value -> Error ("expected refusal: " ^ Error.to_string expected))
    ~error:(fun actual ->
      require ("wrong refusal: " ^ Error.to_string actual)
        (String.starts_with ~prefix:(Error.to_string expected) (Error.to_string actual)))

let variable index =
  Level.var index |> Option.to_result ~none:"nonnegative universe variable construction failed"

let level amount =
  Level.of_int amount |> Option.to_result ~none:"nonnegative universe construction failed"

let scope_error = Error.Universe "universe level is outside the global parameter scope"
let budget_error = Error.Budget_exhausted Check.budget_msg
let collision name = Error.Mismatch ("the name " ^ name ^ " is already declared")

let family globals name =
  let* fam = Global.find_family name globals
    |> Option.to_result ~none:("missing family: " ^ name) in
  let* () = require "family has no positivity verdict" fam.Positivity.f_positive in
  match fam.Positivity.f_status with
  | Positivity.Complete _ -> Ok fam
  | Positivity.Provisional | Positivity.Builtin -> Error "instance is not a completed family"

let member name params indices =
  Term.Lan (Shape.SMu (name, indices), Rules.diagram_of params)

let arrow quantity name domain codomain =
  Term.Ran (Shape.SPi (quantity, name, domain), codomain)

let lambda quantity name domain body =
  Term.Sec (Shape.SPi (quantity, name, domain),
    [{ Term.l_binders = [quantity, name]; l_body = body }])

let eq carrier : Check.family_decl * Check.ctor_decl list =
  { fam_name = "TemplateEq";
    fam_params = [Quantity.Zero, "A", Term.Univ carrier; Quantity.Zero, "x", Term.Var 0];
    fam_indices = [Quantity.Zero, "y", Term.Var 1]; fam_level = Level.zero },
  [{ ct_name = "templateRefl"; ct_args = [];
     ct_res_params = [Term.Var 1; Term.Var 0]; ct_res_idx = [Term.Var 0] }]

let sum left right : Check.family_decl * Check.ctor_decl list =
  let left = Level.succ left in
  let right = Level.succ right in
  { fam_name = "TemplateSum";
    fam_params = [Quantity.Zero, "A", Term.Univ left; Quantity.Zero, "B", Term.Univ right];
    fam_indices = []; fam_level = Level.max left right },
  [{ ct_name = "templateInl"; ct_args = [Quantity.Many, "left", Term.Var 1];
     ct_res_params = [Term.Var 2; Term.Var 1]; ct_res_idx = [] };
   { ct_name = "templateInr"; ct_args = [Quantity.Many, "right", Term.Var 0];
     ct_res_params = [Term.Var 2; Term.Var 1]; ct_res_idx = [] }]

let empty name sort : Check.family_decl =
  { fam_name = name; fam_params = []; fam_indices = []; fam_level = sort }

let declare ?(globals = Global.empty) ?(catalog = Family_poly.empty) ~arity (decl, ctors) =
  Family_poly.declare globals catalog ~arity decl ctors |> kernel

let instantiate ?(globals = Global.empty) catalog name levels as_name =
  Family_poly.instantiate globals catalog ~name ~levels ~as_name |> kernel

let check_refl globals name carrier =
  let result = member name [Term.Var 1; Term.Var 0] [Term.Var 0] in
  let declaration : Check.decl = {
    d_name = "reflClient"; d_kind = Check.Definition;
    d_ty = arrow Quantity.Zero "A" (Term.Univ carrier)
      (arrow Quantity.Zero "x" (Term.Var 0) result);
    d_body = Some (lambda Quantity.Zero "A" (Term.Univ carrier)
      (lambda Quantity.Zero "x" (Term.Var 0)
        (Term.In (Shape.SMu (name, [Term.Var 0]), Term.ACtor "templateRefl", [])))) } in
  let* _entry = kernel (Check.check_decl globals Budget.unlimited declaration) in
  Ok ()

let check_sum_constructor globals name left right ctor field_type =
  let left = Level.succ left in
  let right = Level.succ right in
  let result = member name [Term.Var 2; Term.Var 1] [] in
  let declaration : Check.decl = {
    d_name = "sumClient"; d_kind = Check.Definition;
    d_ty = arrow Quantity.Zero "A" (Term.Univ left)
      (arrow Quantity.Zero "B" (Term.Univ right)
        (arrow Quantity.Many "value" field_type result));
    d_body = Some (lambda Quantity.Zero "A" (Term.Univ left)
      (lambda Quantity.Zero "B" (Term.Univ right)
        (lambda Quantity.Many "value" field_type
          (Term.In (Shape.SMu (name, []), Term.ACtor ctor, [Term.Var 0]))))) } in
  let* _entry = kernel (Check.check_decl globals Budget.unlimited declaration) in
  Ok ()

(* Application inference ignores its raw shape domain. Keep a recursive
   family name and a motive name there to exercise specialization of stored
   metadata as well as the semantically checked recursive field below. *)
let hidden_metadata ?motive_name ?visible_carrier name carrier argument =
  let motive_name = Option.value ~default:name motive_name in
  let visible_carrier = Option.value ~default:carrier visible_carrier in
  let hidden = Term.Elim {
    e_shape = Shape.SMu (name, []); e_scrut = Term.Var 0; e_scrut_q = Quantity.Zero;
    e_motive = Some { m_ind = Some motive_name; m_idx = []; m_self = "self";
      m_body = Term.Univ carrier };
    e_branches = [Term.ACtor "templateNil", { l_binders = []; l_body = Term.Univ carrier }];
  } in
  let domain = Term.Univ visible_carrier in
  let identity = Term.Ann (lambda Quantity.Zero "T" domain (Term.Var 0),
    arrow Quantity.Zero "T" domain domain) in
  Term.Out (Shape.SPi (Quantity.Zero, "ignored", hidden),
    Term.APt (Quantity.Zero, argument), identity)

let recursive carrier : Check.family_decl * Check.ctor_decl list =
  let carrier = Level.succ carrier in
  let name = "TemplateList" in
  { fam_name = name; fam_params = [Quantity.Zero, "A", Term.Univ carrier];
    fam_indices = []; fam_level = carrier },
  [{ ct_name = "templateNil"; ct_args = []; ct_res_params = [Term.Var 0]; ct_res_idx = [] };
   { ct_name = "templateCons";
     ct_args = [Quantity.Many, "head", hidden_metadata name carrier (Term.Var 0);
                Quantity.Many, "tail", member name [Term.Var 1] []];
     ct_res_params = [Term.Var 2]; ct_res_idx = [] }]

let check_hidden_metadata expected term =
  let* shape = match term with
    | Term.Out (shape, _, _) -> Ok shape
    | Term.Var _ | Term.Univ _ | Term.Lan _ | Term.Ran _ | Term.In _ | Term.Elim _
    | Term.Sec _ | Term.Let _ | Term.Ann _ | Term.Global _ | Term.Lit _ | Term.Auto ->
        Error "stored metadata field lost its application" in
  let* hidden = Shape.point_dom shape |> Option.to_result ~none:"metadata domain disappeared" in
  match hidden with
  | Term.Elim elimination ->
      let* () = require "stored recursive shape was not renamed"
        (Shape.family elimination.Term.e_shape = Some expected) in
      let* motive = elimination.Term.e_motive |> Option.to_result ~none:"stored motive disappeared" in
      require "stored recursive motive name was not renamed" (motive.Term.m_ind = Some expected)
  | Term.Var _ | Term.Univ _ | Term.Lan _ | Term.Ran _ | Term.In _ | Term.Out _
  | Term.Sec _ | Term.Let _ | Term.Ann _ | Term.Global _ | Term.Lit _ | Term.Auto ->
      Error "stored metadata lost its elimination"

let checked_recursive_instance globals name carrier =
  let* fam = family globals name in
  let* cons = Positivity.ctor_of "templateCons" fam
    |> Option.to_result ~none:"recursive constructor disappeared" in
  let* () = require "recursive constructor lost its recursion flag" cons.Positivity.c_self_rec in
  let* () = match cons.Positivity.c_args with
    | [(_, _, head); (_, _, tail)] ->
        let* () = check_hidden_metadata name head in
        let* () = require "recursive tail lost its specialized family"
          (Term.exists_name ~include_families:true [name] tail) in
        require "recursive tail retained its template name"
          (not (Term.exists_name ~include_families:true ["TemplateList"] tail))
    | [] | [_] | _ :: _ :: _ :: _ -> Error "recursive constructor has the wrong field count" in
  let carrier = Level.succ carrier in
  let list = member name [Term.Var 0] [] in
  let declaration : Check.decl = {
    d_name = "listClient"; d_kind = Check.Definition;
    d_ty = arrow Quantity.Zero "A" (Term.Univ carrier)
      (arrow Quantity.Many "value" (Term.Var 0) (member name [Term.Var 1] []));
    d_body = Some (lambda Quantity.Zero "A" (Term.Univ carrier)
      (lambda Quantity.Many "value" (Term.Var 0)
        (Term.In (Shape.SMu (name, []), Term.ACtor "templateCons",
          [Term.Var 0; Term.In (Shape.SMu (name, []), Term.ACtor "templateNil", [])])))) } in
  let* _entry = kernel (Check.check_decl globals Budget.unlimited declaration) in
  let* _type = kernel (Check.infer_term globals
    (arrow Quantity.Zero "A" (Term.Univ carrier) list)) in
  Ok ()

let counting_budget allowance =
  let calls = ref 0 in
  let poll () = calls := !calls + 1; !calls > allowance in
  Budget.of_poll poll, (fun () -> !calls)

let named_carrier universe =
  let declaration : Check.decl = {
    d_name = "Carrier"; d_kind = Check.Postulate; d_ty = Term.Univ universe; d_body = None } in
  let* entry = kernel (Check.check_decl Global.empty Budget.unlimited declaration) in
  Ok (Global.add "Carrier" entry Global.empty)

let cases = [
  "catalog-keeps-templates-out-of-globals", (fun () ->
    let* u = variable 0 in
    let* catalog = declare ~arity:1 (eq u) in
    let* () = require "template arity disappeared" (Family_poly.arity catalog "TemplateEq" = Some 1) in
    let* () = require "unknown template has an arity" (Family_poly.arity catalog "missing" = None) in
    let* () = require "template entered the family table"
      (Option.is_none (Global.find_family "TemplateEq" Global.empty)) in
    expect_refusal (Error.Unbound "the family TemplateEq is not declared")
      (Check.infer_term Global.empty (member "TemplateEq" [] [])));
  "equality-at-prop-and-high-carrier-universes", (fun () ->
    let* u = variable 0 in
    let* high = level max_int in
    let* catalog = declare ~arity:1 (eq u) in
    let* globals = instantiate catalog "TemplateEq" [Level.zero] "EqProp" in
    let* globals = instantiate ~globals catalog "TemplateEq" [Level.one] "EqData" in
    let* globals = instantiate ~globals catalog "TemplateEq" [high] "EqHigh" in
    let* () = check_refl globals "EqProp" Level.zero in
    let* () = check_refl globals "EqData" Level.one in
    let* () = check_refl globals "EqHigh" high in
    let* fam = family globals "EqHigh" in
    let* () = require "equality no longer lives in Prop" (Level.equal fam.f_level Level.zero) in
    let* () = require "equality lost its singleton elimination criterion" (Rules.mu_zero_eliminable fam) in
    require "specialization installed the original template"
      (Option.is_none (Global.find_family "TemplateEq" globals)));
  "sum-at-unequal-universes-in-both-directions", (fun () ->
    let* u = variable 0 in
    let* v = variable 1 in
    let* two = level 2 in
    let* catalog = declare ~arity:2 (sum u v) in
    let* globals = instantiate catalog "TemplateSum" [Level.zero; two] "SumLowHigh" in
    let* globals = instantiate ~globals catalog "TemplateSum" [two; Level.zero] "SumHighLow" in
    let* () = check_sum_constructor globals "SumLowHigh" Level.zero two "templateInl" (Term.Var 1) in
    let* () = check_sum_constructor globals "SumLowHigh" Level.zero two "templateInr" (Term.Var 0) in
    let* () = check_sum_constructor globals "SumHighLow" two Level.zero "templateInl" (Term.Var 1) in
    let* () = check_sum_constructor globals "SumHighLow" two Level.zero "templateInr" (Term.Var 0) in
    let* fam = family globals "SumLowHigh" in
    require "sum result universe did not specialize max" (Level.equal fam.f_level (Level.succ two)));
  "recursive-fields-and-motive-metadata-are-renamed", (fun () ->
    let* u = variable 0 in
    let* two = level 2 in
    let* catalog = declare ~arity:1 (recursive u) in
    let* globals = instantiate catalog "TemplateList" [Level.zero] "ListLow" in
    let* globals = instantiate ~globals catalog "TemplateList" [two] "ListHigh" in
    let* () = checked_recursive_instance globals "ListLow" Level.zero in
    checked_recursive_instance globals "ListHigh" two);
  "closed-sample-does-not-prove-universal-field-bound", (fun () ->
    let build carrier =
      { (empty "Counterfeit" Level.one) with
        Check.fam_params = [Quantity.Zero, "A", Term.Univ (Level.succ carrier)] },
      [{ Check.ct_name = "counterfeit"; ct_args = [Quantity.Many, "value", Term.Var 0];
         ct_res_params = [Term.Var 1]; ct_res_idx = [] }] in
    let sampled, sampled_ctors = build Level.zero in
    let* declared = kernel (Check.declare_family Global.empty sampled) in
    let* _closed = kernel (Check.define_ctors declared ~group:[sampled.fam_name]
      ~name:sampled.fam_name sampled_ctors) in
    let* u = variable 0 in
    let symbolic, symbolic_ctors = build u in
    expect_refusal (Error.Universe "a field of counterfeit exceeds its family universe")
      (Family_poly.declare Global.empty Family_poly.empty ~arity:1 symbolic symbolic_ctors));
  "mixed-prop-type-result-sort-is-refused", (fun () ->
    let* u = variable 0 in
    expect_refusal (Error.Universe "a family schema must remain in Prop or Type for every universe substitution")
      (Family_poly.declare Global.empty Family_poly.empty ~arity:1 (empty "Mixed" u) []));
  "negative-arity-is-refused", (fun () ->
    expect_refusal (Error.Universe "a universe parameter arity must be nonnegative")
      (Family_poly.declare Global.empty Family_poly.empty ~arity:(-1) (empty "Negative" Level.one) []));
  "free-header-level-is-refused", (fun () ->
    let* free = variable 1 in
    expect_refusal scope_error (Family_poly.declare Global.empty Family_poly.empty ~arity:1
      (empty "FreeHeader" (Level.succ free)) []));
  "free-parameter-level-is-refused", (fun () ->
    let* free = variable 1 in
    let decl, ctors = eq free in
    expect_refusal scope_error (Family_poly.declare Global.empty Family_poly.empty ~arity:1 decl ctors));
  "free-index-level-is-refused", (fun () ->
    let* free = variable 1 in
    let decl = { (empty "FreeIndex" Level.zero) with
      Check.fam_indices = [Quantity.Zero, "P", Term.Univ free] } in
    expect_refusal scope_error (Family_poly.declare Global.empty Family_poly.empty ~arity:1 decl []));
  "free-constructor-field-level-is-refused", (fun () ->
    let* free = variable 1 in
    let ctor : Check.ctor_decl = { ct_name = "freeField";
      ct_args = [Quantity.Zero, "P", Term.Univ free]; ct_res_params = []; ct_res_idx = [] } in
    expect_refusal scope_error (Family_poly.declare Global.empty Family_poly.empty ~arity:1
      (empty "FreeField" (Level.succ Level.one)) [ctor]));
  "free-level-in-result-parameter-annotation-is-refused", (fun () ->
    let* u = variable 0 in
    let* free = variable 1 in
    let decl, ctors = eq u in
    let ctors = List.map (fun (ctor : Check.ctor_decl) ->
      { ctor with ct_res_params = [Term.Ann (Term.Var 1, Term.Univ free); Term.Var 0] }) ctors in
    expect_refusal scope_error (Family_poly.declare Global.empty Family_poly.empty ~arity:1 decl ctors));
  "free-level-in-result-index-annotation-is-refused", (fun () ->
    let* u = variable 0 in
    let* free = variable 1 in
    let decl, ctors = eq u in
    let ctors = List.map (fun (ctor : Check.ctor_decl) ->
      { ctor with ct_res_idx = [Term.Ann (Term.Var 0, Term.Univ free)] }) ctors in
    expect_refusal scope_error (Family_poly.declare Global.empty Family_poly.empty ~arity:1 decl ctors));
  "hidden-free-level-in-ignored-shape-is-refused", (fun () ->
    let* u = variable 0 in
    let* free = variable 1 in
    let decl, ctors = recursive u in
    let ctors = List.map (fun (ctor : Check.ctor_decl) ->
      { ctor with ct_args = List.map (fun (quantity, name, ty) ->
          if String.equal name "head" then
            quantity, name,
              hidden_metadata ~visible_carrier:(Level.succ u) decl.fam_name free
                (Term.Var 0)
          else quantity, name, ty) ctor.ct_args }) ctors in
    expect_refusal scope_error (Family_poly.declare Global.empty Family_poly.empty ~arity:1 decl ctors));
  "negative-recursive-position-is-refused", (fun () ->
    let* u = variable 0 in
    let carrier = Level.succ u in
    let name = "NegativeRecursive" in
    let decl = { (empty name carrier) with
      Check.fam_params = [Quantity.Zero, "A", Term.Univ carrier] } in
    let ctor : Check.ctor_decl = {
      ct_name = "negative";
      ct_args = [Quantity.Many, "function", arrow Quantity.Many "self"
        (member name [Term.Var 0] []) (Term.Var 1)];
      ct_res_params = [Term.Var 1]; ct_res_idx = [] } in
    expect_refusal (Error.Not_yet Positivity.nonpositive_word)
      (Family_poly.declare Global.empty Family_poly.empty ~arity:1 decl [ctor]));
  "duplicate-template-name-is-refused", (fun () ->
    let* u = variable 0 in
    let decl, ctors = eq u in
    let* catalog = declare ~arity:1 (decl, ctors) in
    expect_refusal (collision decl.fam_name)
      (Family_poly.declare Global.empty catalog ~arity:1 decl ctors));
  "existing-global-template-name-is-refused", (fun () ->
    let* globals = named_carrier Level.one in
    expect_refusal (collision "Carrier") (Family_poly.declare globals Family_poly.empty ~arity:0
      (empty "Carrier" Level.one) []));
  "existing-family-template-name-is-refused", (fun () ->
    let decl = empty "Existing" Level.one in
    let* globals = kernel (Check.declare_family Global.empty decl) in
    expect_refusal (collision "Existing")
      (Family_poly.declare globals Family_poly.empty ~arity:0 decl []));
  "duplicate-constructor-name-is-refused", (fun () ->
    let* u = variable 0 in
    let decl, ctors = eq u in
    expect_refusal (Error.Mismatch "the constructor templateRefl is already declared in TemplateEq")
      (Family_poly.declare Global.empty Family_poly.empty ~arity:1 decl (ctors @ ctors)));
  "wrong-instance-arity-is-refused", (fun () ->
    let* u = variable 0 in
    let* catalog = declare ~arity:1 (eq u) in
    let* () = expect_refusal
      (Error.Universe "the family schema TemplateEq expects 1 universe arguments, got 0")
      (Family_poly.instantiate Global.empty catalog ~name:"TemplateEq" ~levels:[] ~as_name:"TooFew") in
    expect_refusal (Error.Universe "the family schema TemplateEq expects 1 universe arguments, got 2")
      (Family_poly.instantiate Global.empty catalog ~name:"TemplateEq"
        ~levels:[Level.zero; Level.one] ~as_name:"TooMany"));
  "free-instance-argument-is-refused", (fun () ->
    let* u = variable 0 in
    let* catalog = declare ~arity:1 (eq u) in
    expect_refusal (Error.Universe "universe arguments must be closed")
      (Family_poly.instantiate Global.empty catalog ~name:"TemplateEq" ~levels:[u] ~as_name:"Open"));
  "unknown-family-template-is-refused", (fun () ->
    expect_refusal (Error.Unbound "unknown family universe schema Missing")
      (Family_poly.instantiate Global.empty Family_poly.empty
        ~name:"Missing" ~levels:[] ~as_name:"Unknown"));
  "other-template-reference-in-motive-metadata-is-refused", (fun () ->
    let* u = variable 0 in
    let* catalog = declare ~arity:1 (eq u) in
    let decl, ctors = recursive u in
    let ctors = List.map (fun (ctor : Check.ctor_decl) ->
      { ctor with ct_args = List.map (fun (quantity, name, ty) ->
          if String.equal name "head" then
            quantity, name, hidden_metadata ~motive_name:"TemplateEq"
              decl.fam_name (Level.succ u) (Term.Var 0)
          else quantity, name, ty) ctor.ct_args }) ctors in
    expect_refusal (Error.Not_yet "references between family schemas are not supported")
      (Family_poly.declare Global.empty catalog ~arity:1 decl ctors));
  "instance-cannot-reuse-template-name", (fun () ->
    let* u = variable 0 in
    let* catalog = declare ~arity:1 (eq u) in
    expect_refusal (collision "TemplateEq") (Family_poly.instantiate Global.empty catalog
      ~name:"TemplateEq" ~levels:[Level.zero] ~as_name:"TemplateEq"));
  "instance-cannot-replace-existing-global", (fun () ->
    let* u = variable 0 in
    let* catalog = declare ~arity:1 (eq u) in
    let* globals = named_carrier Level.one in
    expect_refusal (collision "Carrier") (Family_poly.instantiate globals catalog
      ~name:"TemplateEq" ~levels:[Level.zero] ~as_name:"Carrier"));
  "empty-schema-honors-budget", (fun () ->
    expect_refusal budget_error (Check.check_family_scheme Global.empty
      (Budget.of_poll (fun () -> true)) ~arity:0 (empty "EmptyBudget" Level.one) []));
  "kernel-scheme-refuses-a-free-header-level", (fun () ->
    let* free = variable 3 in
    expect_refusal scope_error (Check.check_family_scheme Global.empty
      Budget.unlimited ~arity:1 (empty "KernelFreeHeader" (Level.succ free)) []));
  "empty-catalog-declaration-honors-budget", (fun () ->
    expect_refusal budget_error (Family_poly.declare ~budget:(Budget.of_poll (fun () -> true))
      Global.empty Family_poly.empty ~arity:0 (empty "EmptyBudget" Level.one) []));
  "empty-instance-honors-budget", (fun () ->
    let* catalog = declare ~arity:0 (empty "EmptyBudget" Level.one, []) in
    expect_refusal budget_error (Family_poly.instantiate ~budget:(Budget.of_poll (fun () -> true))
      Global.empty catalog ~name:"EmptyBudget" ~levels:[] ~as_name:"Budgeted"));
  "instance-budget-expires-after-entry", (fun () ->
    let* u = variable 0 in
    let* catalog = declare ~arity:1 (recursive u) in
    let budget, calls = counting_budget 4 in
    let* () = expect_refusal budget_error (Family_poly.instantiate ~budget Global.empty catalog
      ~name:"TemplateList" ~levels:[Level.zero] ~as_name:"Budgeted") in
    require "instance did not stop at its caller's budget" (calls () = 5));
  "raw-substitution-consumes-the-caller-budget", (fun () ->
    let* u = variable 0 in
    let annotated depth universe =
      let carrier = Level.succ universe in
      let rec wrap remaining term =
        if remaining = 0 then term
        else wrap (remaining - 1) (Term.Ann (term, Term.Univ (Level.succ carrier))) in
      { (empty "AnnotationBudget" Level.one) with Check.fam_params =
          [Quantity.Zero, "A", wrap depth (Term.Univ carrier)] } in
    let measure depth =
      let* catalog = declare ~arity:1 (annotated depth u, []) in
      let closed = annotated depth Level.zero in
      let direct_budget, direct_calls = counting_budget max_int in
      let* provisional = kernel (Check.declare_family ~budget:direct_budget Global.empty closed) in
      let* _checked = kernel (Check.define_ctors ~budget:direct_budget provisional
        ~group:[closed.fam_name] ~name:closed.fam_name []) in
      let complete_budget, complete_calls = counting_budget max_int in
      let* _instance = kernel (Family_poly.instantiate ~budget:complete_budget Global.empty catalog
        ~name:"AnnotationBudget" ~levels:[Level.zero] ~as_name:"Budgeted") in
      let* () = require "instance polling omitted part of closed kernel checking"
        (complete_calls () >= direct_calls ()) in
      Ok (catalog, direct_calls (), complete_calls () - direct_calls ()) in
    (* Only Term nodes grow between these inputs. Subtract closed checking
       separately for each depth, so shape, address, leg and catalog polls
       cannot stand in for a budgeted traversal of the added annotations. *)
    let* _shallow_catalog, _shallow_kernel, shallow_mapping = measure 0 in
    let* catalog, deep_kernel, deep_mapping = measure 64 in
    let* () = require "substituting more raw Term nodes consumed no additional caller budget"
      (deep_mapping > shallow_mapping) in
    let allowance = deep_kernel + shallow_mapping in
    let budget, calls = counting_budget allowance in
    let* () = expect_refusal budget_error (Family_poly.instantiate ~budget Global.empty catalog
      ~name:"AnnotationBudget" ~levels:[Level.zero] ~as_name:"Budgeted") in
    require "substitution and rechecking did not share one budget" (calls () = allowance + 1));
  "failed-instances-preserve-globals-and-catalog", (fun () ->
    let* u = variable 0 in
    let* catalog = declare ~arity:1 (eq u) in
    let* original = instantiate catalog "TemplateEq" [Level.zero] "First" in
    let* () = expect_refusal (Error.Universe "universe arguments must be closed")
      (Family_poly.instantiate original catalog ~name:"TemplateEq" ~levels:[u] ~as_name:"Retry") in
    let* () = require "failed instance changed the input family table"
      (Global.StringMap.cardinal original.families = 1
       && Option.is_none (Global.find_family "Retry" original)) in
    let* result = instantiate ~globals:original catalog "TemplateEq" [Level.one] "Retry" in
    let* () = require "successful retry installed an incorrect number of families"
      (Global.StringMap.cardinal result.families = 2) in
    let* () = expect_refusal (collision "Retry") (Family_poly.instantiate result catalog
      ~name:"TemplateEq" ~levels:[Level.zero] ~as_name:"Retry") in
    let* () = check_refl result "Retry" Level.one in
    check_refl original "First" Level.zero);
  "instance-rechecks-against-current-global-types", (fun () ->
    let* low = named_carrier Level.one in
    let* high = named_carrier (Level.succ Level.one) in
    let decl = empty "UsesCarrier" Level.one in
    let ctors : Check.ctor_decl list = [{ ct_name = "carry";
      ct_args = [Quantity.Many, "value", Term.Global "Carrier"];
      ct_res_params = []; ct_res_idx = [] }] in
    let* catalog = declare ~globals:low ~arity:0 (decl, ctors) in
    let* () = expect_refusal (Error.Universe "a field of carry exceeds its family universe")
      (Family_poly.instantiate high catalog ~name:"UsesCarrier" ~levels:[] ~as_name:"Retry") in
    let* () = require "failed recheck leaked its provisional family"
      (Global.StringMap.is_empty high.families) in
    let* result = instantiate ~globals:low catalog "UsesCarrier" [] "Retry" in
    let* _family = family result "Retry" in
    require "retry changed the original environment" (Global.StringMap.is_empty low.families));
  "instance-rechecks-missing-global-dependency", (fun () ->
    let* globals = named_carrier Level.one in
    let decl = { (empty "UsesCarrier" Level.one) with
      Check.fam_params = [Quantity.Zero, "value", Term.Global "Carrier"] } in
    let* catalog = declare ~globals ~arity:0 (decl, []) in
    expect_refusal (Error.Unbound "Carrier") (Family_poly.instantiate Global.empty catalog
      ~name:"UsesCarrier" ~levels:[] ~as_name:"MissingDependency"));
]

let () =
  let failures = List.fold_left (fun failures (name, test) ->
      Result.fold (test ())
        ~ok:(fun () -> Printf.printf "PASS family-poly %s\n%!" name; failures)
        ~error:(fun message ->
          Printf.printf "FAIL family-poly %s: %s\n%!" name message;
          failures + 1)) 0 cases in
  if failures = 0 then Printf.printf "FAMILY-POLY-OK cases=%d\n" (List.length cases)
  else exit 1
