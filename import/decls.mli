type hints = Opaque_hint | Abbrev | Regular of int
type safety = Unsafe | Safe | Partial
type quot_kind = Quot_type | Quot_ctor | Quot_lift | Quot_ind
type rule = { ctor : int; nfields : int; rhs : int }
type inductive = {
  num_params : int; num_indices : int; all : int list; ctors : int list;
  num_nested : int; is_rec : bool; is_unsafe : bool; is_reflexive : bool;
}
type constructor = {
  induct : int; cidx : int; num_params : int; num_fields : int; is_unsafe : bool;
}
type recursor = {
  all : int list; num_params : int; num_indices : int; num_motives : int;
  num_minors : int; rules : rule list; k : bool; is_unsafe : bool;
}
type kind =
  | Axiom of bool
  | Definition of { value : int; hints : hints; safety : safety; all : int list }
  | Theorem of { value : int; all : int list }
  | Opaque of { value : int; is_unsafe : bool; all : int list }
  | Quotient of quot_kind
  | Inductive of inductive
  | Constructor of constructor
  | Recursor of recursor
type t = { name : int; level_params : int list; typ : int; kind : kind; line : int }
type group = { types : t list; ctors : t list; recs : t list; line : int }
val parse : line:int -> string -> Ndjson.t -> (t list * group option, string) result
val name_references : t -> int list
val expr_references : t -> int list
val kind_name : kind -> string
