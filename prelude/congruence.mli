(** Congruence with independent domain and codomain Sort levels, including
    Prop. Instantiate MechCongr through Family_poly with [u; v]. Instance N
    provides domain equality N, codomain equality N_Result and N_congr.
    These families are distinct from other equality catalog instances.
    All families and the definition check universally and again when closed. *)
val catalog : ?budget:Mechanism_kernel.Budget.t -> Mechanism_kernel.Global.t ->
  (Mechanism_surface.Family_poly.t, Mechanism_kernel.Error.t) result
