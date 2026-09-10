(** Universally checked dependent function and pair definitions.
    MechPi takes domain and codomain Sort levels, including Prop, and
    returns Sort (imax u v). MechSigma, mechSigmaMk, mechSigmaFst and
    mechSigmaSnd take the two Type levels, before their successors.
    mechSigmaRec takes those Type levels and a motive Sort level.
    Its motive depends on the whole pair. Instantiate each definition
    through Poly under a fresh name. Templates contain no postulates,
    primitives or references to other catalog templates. *)
val catalog : ?budget:Mechanism_kernel.Budget.t -> Mechanism_kernel.Global.t ->
  (Mechanism_surface.Poly.t, Mechanism_kernel.Error.t) result
