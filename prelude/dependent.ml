open Mechanism_kernel

let ( let* ) = Result.bind

let variable index = Level.var index |> Option.to_result
  ~none:(Error.Universe "a prelude universe parameter must be nonnegative")

let arrow (q, name, domain) body = Term.Ran (Shape.SPi (q, name, domain), body)
let lambda (q, name, domain) body = Term.Sec (Shape.SPi (q, name, domain),
  [{ Term.l_binders = [q, name]; l_body = body }])
let definition name binders result body : Check.decl = {
  d_name = name; d_kind = Check.Definition;
  d_ty = List.fold_right arrow binders result;
  d_body = Some (List.fold_right lambda binders body);
}
let point domain = Shape.SPi (Quantity.Many, "point", domain)
let apply domain head argument = Term.Out (point domain,
  Term.APt (Quantity.Many, argument), head)

(* These helpers take the indices of A and B in the current scope.
   A pair's diagram adds its point binder before applying B. *)
let sigma a b = Term.Lan (point (Term.Var a),
  apply (Term.Var (a + 1)) (Term.Var (b + 1)) (Term.Var 0))
let pair a x y = Term.In (point (Term.Var a),
  Term.APt (Quantity.Many, x), [y])
let eliminate a scrut motive branches = Term.Elim {
  e_shape = point (Term.Var a); e_scrut = scrut; e_scrut_q = Quantity.One;
  e_motive = Some { m_ind = None; m_idx = []; m_self = "self"; m_body = motive };
  e_branches = branches;
}
let first a scrut = eliminate a scrut (Term.Var (a + 1))
  (Rules.proj_branch Quantity.Many 0)

let catalog ?(budget = Budget.unlimited) globals =
  let* u = variable 0 in
  let* v = variable 1 in
  let* w = variable 2 in
  let pi = definition "MechPi"
    [Quantity.Zero, "A", Term.Univ u;
     Quantity.Zero, "B", arrow (Quantity.Many, "x", Term.Var 0) (Term.Univ v)]
    (Term.Univ (Level.imax u v))
    (arrow (Quantity.Many, "x", Term.Var 1)
      (apply (Term.Var 2) (Term.Var 1) (Term.Var 0))) in
  let parameters =
    [Quantity.Zero, "A", Term.Univ (Level.succ u);
     Quantity.Zero, "B", arrow (Quantity.Many, "x", Term.Var 0)
       (Term.Univ (Level.succ v))] in
  let sigma_type = definition "MechSigma" parameters
    (Term.Univ (Level.max (Level.succ u) (Level.succ v))) (sigma 1 0) in
  let make = definition "mechSigmaMk"
    (parameters @ [Quantity.Many, "x", Term.Var 1;
      Quantity.Many, "y", apply (Term.Var 2) (Term.Var 1) (Term.Var 0)])
    (sigma 3 2) (pair 3 (Term.Var 1) (Term.Var 0)) in
  let projections = parameters @ [Quantity.Many, "pair", sigma 1 0] in
  let fst = definition "mechSigmaFst" projections (Term.Var 2)
    (first 2 (Term.Var 0)) in
  let snd = definition "mechSigmaSnd" projections
    (apply (Term.Var 2) (Term.Var 1) (first 2 (Term.Var 0)))
    (eliminate 2 (Term.Var 0)
      (apply (Term.Var 3) (Term.Var 2) (first 3 (Term.Var 0)))
      (Rules.proj_branch Quantity.Many 1)) in
  (* Body scope: pair, step, P, B, A. The branch adds x then y;
     the motive adds just self. P may return either proofs or data. *)
  let recursor = definition "mechSigmaRec"
    (parameters @
      [Quantity.Zero, "P", arrow (Quantity.Many, "pair", sigma 1 0) (Term.Univ w);
       Quantity.Many, "step", arrow (Quantity.Many, "x", Term.Var 2)
         (arrow (Quantity.Many, "y", apply (Term.Var 3) (Term.Var 2) (Term.Var 0))
           (apply (sigma 4 3) (Term.Var 2) (pair 4 (Term.Var 1) (Term.Var 0))));
       Quantity.Many, "pair", sigma 3 2])
    (apply (sigma 4 3) (Term.Var 2) (Term.Var 0))
    (eliminate 4 (Term.Var 0) (apply (sigma 5 4) (Term.Var 3) (Term.Var 0))
      [Term.ALeg 0, { Term.l_binders = [Quantity.Many, "x"; Quantity.Many, "y"];
        l_body = apply (apply (Term.Var 6) (Term.Var 5) (Term.Var 1))
          (apply (Term.Var 6) (Term.Var 3) (Term.Var 1)) (Term.Var 0) }]) in
  List.fold_left (fun acc (arity, decl) ->
    let* catalog = acc in
    Mechanism_surface.Poly.declare ~budget globals catalog ~arity decl)
    (Ok Mechanism_surface.Poly.empty)
    [2, pi; 2, sigma_type; 2, make; 2, fst; 2, snd; 3, recursor]
