type binder_info = Default | Implicit | Strict_implicit | Inst_implicit
type binder = { name : int; typ : int; body : int; info : binder_info }
type t =
  | Bvar of int
  | Sort of int
  | Const of { name : int; universes : int list }
  | App of int * int
  | Lam of binder
  | Forall of binder
  | Let of { name : int; typ : int; value : int; body : int; nondep : bool }
  | Proj of { type_name : int; index : int; structure : int }
  | Nat of string
  | String of string
  | Mdata of { expr : int; data : (string * Ndjson.t) list }
val parse : string -> Ndjson.t -> (t, string) result
val name_references : t -> int list
val level_references : t -> int list
val expr_references : t -> int list
