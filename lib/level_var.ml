(** The five semantic forms.  [Succ (k, u)] stores [k] repeated successors,
    where [k] is positive.  Compression preserves large closed levels. *)
type t =
  | Zero
  | Succ of Z.t * t
  | Max of t * t
  | IMax of t * t
  | Var of int

let offset (amount : Z.t) (level : t) : t =
  if Z.sign amount <= 0 then level
  else
    match level with
    | Succ (prior, base) -> Succ (Z.add amount prior, base)
    | Zero | Max (_, _) | IMax (_, _) | Var _ -> Succ (amount, level)

let rec valid (variable : int -> bool) (level : t) : bool =
  match level with
  | Zero -> true
  | Succ (amount, base) -> Z.sign amount > 0 && valid variable base
  | Max (left, right) | IMax (left, right) ->
      valid variable left && valid variable right
  | Var index -> variable index

let well_formed : t -> bool = valid (fun index -> index >= 0)

let in_scope (arity : int) (level : t) : bool =
  arity >= 0 && valid (fun index -> index >= 0 && index < arity) level

let rec at (index : int) (values : t list) : t option =
  match values with
  | [] -> None
  | value :: rest ->
      match () with
      | () when index = 0 -> Some value
      | () when index > 0 -> at (index - 1) rest
      | () -> None

let subst (arguments : t list) (level : t) : t option =
  let rec visit (node : t) : t option =
    match node with
    | Zero -> Some Zero
    | Succ (amount, base) -> Option.map (offset amount) (visit base)
    | Var index -> at index arguments
    | Max (left, right) -> pair (fun a b -> Max (a, b)) left right
    | IMax (left, right) -> pair (fun a b -> IMax (a, b)) left right
  and pair (make : t -> t -> t) (left : t) (right : t) : t option =
    Option.bind (visit left) (fun a -> Option.map (make a) (visit right))
  in
  if in_scope (List.length arguments) level
     && List.for_all well_formed arguments
  then visit level
  else None
