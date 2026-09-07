(** Exact comparison for all natural valuations.  Each branch fixes only
    zero or positive, then compares unbounded independent variables. *)
module Variables = Set.Make (Int)
module Arms = Map.Make (Int)
let ( let* ) = Result.bind

let poll (budget : Budget.t) : (unit, Error.t) result =
  if Budget.exhausted budget then
    Error (Error.Budget_exhausted "the check budget is exhausted")
  else Ok ()

let fold_arms budget step arms initial =
  let rec visit sequence state =
    let* () = poll budget in
    match sequence () with
    | Seq.Nil -> Ok state
    | Seq.Cons ((index, amount), rest) ->
        let* next = step index amount state in
        visit rest next
  in
  visit (Arms.to_seq arms) initial

type normal = { constant : Z.t; arms : Z.t Arms.t }

let constant (value : Z.t) : normal = { constant = value; arms = Arms.empty }

let maximum budget (left : normal) (right : normal) : (normal, Error.t) result =
  let* arms = fold_arms budget (fun index amount acc ->
    let other = Arms.find_opt index acc |> Option.value ~default:Z.zero in
    Ok (Arms.add index (Z.max amount other) acc)) left.arms right.arms in
  Ok { constant = Z.max left.constant right.constant; arms }

let shift budget (amount : Z.t) (value : normal) : (normal, Error.t) result =
  let* arms = fold_arms budget (fun index prior acc ->
    Ok (Arms.add index (Z.add amount prior) acc)) value.arms Arms.empty in
  Ok { constant = Z.add amount value.constant; arms }

let rec variables budget (level : Level_var.t) (found : Variables.t) :
    (Variables.t, Error.t) result =
  let* () = poll budget in
  match level with
  | Level_var.Zero -> Ok found
  | Level_var.Succ (amount, base) ->
      if Z.sign amount > 0 then variables budget base found
      else Error (Error.Universe "a universe successor offset must be positive")
  | Level_var.Max (left, right) | Level_var.IMax (left, right) ->
      let* found = variables budget left found in
      variables budget right found
  | Level_var.Var index ->
      if index >= 0 then Ok (Variables.add index found)
      else Error (Error.Universe "a universe variable index must be nonnegative")

let normal budget (positive : Variables.t) (level : Level_var.t) :
    (normal, Error.t) result =
  let rec visit (amount : Z.t) (node : Level_var.t) : (normal, Error.t) result =
    let* () = poll budget in
    match node with
    | Level_var.Zero -> Ok (constant amount)
    | Level_var.Succ (extra, base) -> visit (Z.add amount extra) base
    | Level_var.Var index ->
        if Variables.mem index positive then
          Ok { constant = amount; arms = Arms.singleton index amount }
        else Ok (constant amount)
    | Level_var.Max (left, right) ->
        let* lhs = visit amount left in
        let* rhs = visit amount right in
        maximum budget lhs rhs
    | Level_var.IMax (left, right) ->
        let* rhs = visit Z.zero right in
        match () with
        | () when Z.equal rhs.constant Z.zero && Arms.is_empty rhs.arms ->
            Ok (constant amount)
        | () ->
            let* lhs = visit amount left in
            let* rhs = shift budget amount rhs in
            maximum budget lhs rhs
  in
  visit Z.zero level

(** A positive variable has minimum one and no upper bound.  Thus a
    variable arm needs its own counterpart; constants need only the
    minimum of the right side. *)
let below budget (left : normal) (right : normal) : (bool, Error.t) result =
  let* minimum =
    fold_arms budget (fun _index amount acc -> Ok (Z.max acc (Z.succ amount)))
      right.arms right.constant
  in
  fold_arms budget
       (fun index amount same ->
         let matches = Arms.find_opt index right.arms
           |> Option.fold ~none:false ~some:(fun other -> Z.leq amount other) in
         Ok (same && matches))
       left.arms (Z.leq left.constant minimum)

let decide budget (relation : normal -> normal -> (bool, Error.t) result)
    (left : Level_var.t) (right : Level_var.t) : (bool, Error.t) result =
  let rec cases (remaining : int list) (positive : Variables.t) : (bool, Error.t) result =
    let* () = poll budget in
    match remaining with
    | [] ->
        let* lhs = normal budget positive left in
        let* rhs = normal budget positive right in
        relation lhs rhs
    | index :: rest ->
        let* holds = cases rest positive in
        match () with
        | () when holds -> cases rest (Variables.add index positive)
        | () -> Ok false
  in
  let* found = variables budget left Variables.empty in
  let* found = variables budget right found in
  if left == right then Ok true
  else cases (Variables.elements found) Variables.empty

let le_budget budget = decide budget (below budget)

let equal_budget budget =
  decide budget (fun left right ->
    let* same = below budget left right in
    if same then below budget right left else Ok false)

let le left right = le_budget Budget.unlimited left right |> Result.value ~default:false
let equal left right = equal_budget Budget.unlimited left right |> Result.value ~default:false

(** Zero and nonzero predicates are structural and exact for every valuation. *)
let rec always_zero (level : Level_var.t) : bool =
  match level with
  | Level_var.Zero -> true
  | Level_var.Succ (_, _) | Level_var.Var _ -> false
  | Level_var.Max (left, right) -> always_zero left && always_zero right
  | Level_var.IMax (_left, right) -> always_zero right

let rec always_positive (level : Level_var.t) : bool =
  match level with
  | Level_var.Zero | Level_var.Var _ -> false
  | Level_var.Succ (amount, _base) -> Z.sign amount > 0
  | Level_var.Max (left, right) -> always_positive left || always_positive right
  | Level_var.IMax (_left, right) -> always_positive right

let closed_value (level : Level_var.t) : Z.t option =
  if Level_var.in_scope 0 level then
    normal Budget.unlimited Variables.empty level |> Result.to_option
    |> Option.map (fun value -> value.constant)
  else None
