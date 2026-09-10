open Mechanism_kernel

let ( let* ) = Result.bind

let variable index = Level.var index |> Option.to_result
  ~none:(Error.Universe "a prelude universe parameter must be nonnegative")
let arrow (q, name, domain) body = Term.Ran (Shape.SPi (q, name, domain), body)
let lambda (q, name, domain) body = Term.Sec (Shape.SPi (q, name, domain),
  [{ Term.l_binders = [q, name]; l_body = body }])
let eq name carrier left right =
  Term.Lan (Shape.SMu (name, [right]), Rules.diagram_of [carrier; left])
let apply domain head argument = Term.Out (Shape.SPi (Quantity.Many, "point", domain),
  Term.APt (Quantity.Many, argument), head)

let equality name level =
  let family : Check.family_decl = {
    fam_name = name;
    fam_params = [Quantity.Zero, "A", Term.Univ level; Quantity.Zero, "x", Term.Var 0];
    fam_indices = [Quantity.Zero, "y", Term.Var 1]; fam_level = Level.zero;
  } in
  let ctor : Check.ctor_decl = {
    ct_name = "mechReflCtor"; ct_args = [];
    ct_res_params = [Term.Var 1; Term.Var 0]; ct_res_idx = [Term.Var 0];
  } in
  family, [ctor]

let catalog ?(budget = Budget.unlimited) globals =
  let* u = variable 0 in
  let* v = variable 1 in
  let binders =
    [Quantity.Zero, "A", Term.Univ u; Quantity.Zero, "B", Term.Univ v;
     Quantity.Zero, "f", arrow (Quantity.Many, "point", Term.Var 1) (Term.Var 1);
     Quantity.Zero, "x", Term.Var 2; Quantity.Zero, "y", Term.Var 3;
     Quantity.Many, "proof", eq "MechCongr" (Term.Var 4) (Term.Var 1) (Term.Var 0)] in
  let result = eq "Result" (Term.Var 4)
    (apply (Term.Var 5) (Term.Var 3) (Term.Var 2))
    (apply (Term.Var 5) (Term.Var 3) (Term.Var 1)) in
  (* Body scope: proof, y, x, f, B, A. The motive adds self and right.
     Eliminate domain equality, producing equality in the codomain. *)
  let body = Term.Elim {
    e_shape = Shape.SMu ("MechCongr", [Term.Var 1]);
    e_scrut = Term.Var 0; e_scrut_q = Quantity.One;
    e_motive = Some { m_ind = Some "MechCongr"; m_idx = ["right"]; m_self = "self";
      m_body = eq "Result" (Term.Var 6)
        (apply (Term.Var 7) (Term.Var 5) (Term.Var 4))
        (apply (Term.Var 7) (Term.Var 5) (Term.Var 1)) };
    e_branches = [Term.ACtor "mechReflCtor", { l_binders = [];
      l_body = Term.In (Shape.SMu ("Result",
        [apply (Term.Var 5) (Term.Var 3) (Term.Var 2)]), Term.ACtor "mechReflCtor", []) }];
  } in
  let congr : Check.decl = {
    d_name = "congr"; d_kind = Check.Definition;
    d_ty = List.fold_right arrow binders result;
    d_body = Some (List.fold_right lambda binders body);
  } in
  Mechanism_surface.Family_poly.declare_group ~budget ~members:[congr]
    globals Mechanism_surface.Family_poly.empty ~arity:2
    [equality "MechCongr" u; equality "Result" v]
