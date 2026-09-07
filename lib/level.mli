(** Abstract universe levels.  Variables bind only at a global schema. *)
type t

val zero : t
val one : t
val succ : t -> t
val max : t -> t -> t
val imax : t -> t -> t
val equal : t -> t -> bool
val le : t -> t -> bool
val equal_budget : Budget.t -> t -> t -> (bool, Error.t) result
val le_budget : Budget.t -> t -> t -> (bool, Error.t) result
val of_int : int -> t option
val to_string : t -> string
val var : int -> t option
val in_scope : int -> t -> bool
val subst : t list -> t -> t option
val always_positive : t -> bool
val always_zero : t -> bool
