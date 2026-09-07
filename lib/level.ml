(** Prenex universes use the semantic grammar Zero, Succ, Max, IMax and Var.
    The private representation compresses consecutive successors. *)
type t = Level_var.t

let zero : t = Level_var.Zero
let one : t = Level_var.offset Z.one zero
let succ (level : t) : t = Level_var.offset Z.one level
let max (left : t) (right : t) : t = Level_var.Max (left, right)
let imax (left : t) (right : t) : t = Level_var.IMax (left, right)
let equal : t -> t -> bool = Level_eq.equal
let le : t -> t -> bool = Level_eq.le
let equal_budget : Budget.t -> t -> t -> (bool, Error.t) result = Level_eq.equal_budget
let le_budget : Budget.t -> t -> t -> (bool, Error.t) result = Level_eq.le_budget
let always_positive : t -> bool = Level_eq.always_positive
let always_zero : t -> bool = Level_eq.always_zero
let in_scope : int -> t -> bool = Level_var.in_scope
let subst : t list -> t -> t option = Level_var.subst

let of_int (value : int) : t option =
  if value >= 0 then Some (Level_var.offset (Z.of_int value) zero) else None

let var (index : int) : t option =
  if index >= 0 then Some (Level_var.Var index) else None

let to_string (level : t) : string =
  let rec render (node : t) : string =
    match node with
    | Level_var.Zero -> "0"
    | Level_var.Succ (amount, base) ->
        "(" ^ render base ^ " + " ^ Z.to_string amount ^ ")"
    | Level_var.Max (left, right) -> "max(" ^ render left ^ ", " ^ render right ^ ")"
    | Level_var.IMax (left, right) -> "imax(" ^ render left ^ ", " ^ render right ^ ")"
    | Level_var.Var index -> "u" ^ string_of_int index
  in
  Option.fold ~none:(fun () -> render level)
    ~some:(fun value () -> Z.to_string value) (Level_eq.closed_value level) ()
