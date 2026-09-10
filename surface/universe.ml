open Kanon_kernel

(** Surface levels keep their syntax for printing.  Variables are indices
    into the enclosing declaration's prenex binder list. *)
type t =
  | Nat of int
  | Var of int
  | Succ of t
  | Max of t * t
  | IMax of t * t

let ( let* ) = Result.bind

let rec lower = function
  | Nat n -> Level.of_int n
      |> Option.to_result ~none:(Error.Universe "a sort level must be nonnegative")
  | Var n -> Level.var n
      |> Option.to_result ~none:(Error.Universe "a universe index must be nonnegative")
  | Succ u -> Result.map Level.succ (lower u)
  | Max (u, v) -> let* u = lower u in let* v = lower v in Ok (Level.max u v)
  | IMax (u, v) -> let* u = lower u in let* v = lower v in Ok (Level.imax u v)

let rec text = function
  | Nat n -> string_of_int n
  | Var n -> "u" ^ string_of_int n
  | Succ u -> "(succ " ^ text u ^ ")"
  | Max (u, v) -> "(max " ^ text u ^ " " ^ text v ^ ")"
  | IMax (u, v) -> "(imax " ^ text u ^ " " ^ text v ^ ")"
