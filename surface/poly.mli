open Kanon_kernel

(** Universally checked prenex templates.  Their names do not resolve in
    ordinary globals.  Family templates and references between templates
    are left to the importer and prelude stages. *)
type t

val empty : t
val arity : t -> string -> int option
val declare : ?budget:Budget.t -> Global.t -> t -> arity:int -> Check.decl ->
  (t, Error.t) result

(** Require exactly the declared number of closed universe arguments and a
    fresh output name.  Recheck the specialized type and body before adding
    an ordinary global.  Failure leaves both input values unchanged. *)
val instantiate : ?budget:Budget.t -> Global.t -> t -> name:string ->
  levels:Level.t list -> as_name:string -> ((Global.t * Term.t), Error.t) result
