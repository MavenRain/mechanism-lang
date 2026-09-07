type t = Zero | Succ of int | Max of int * int | Imax of int * int | Param of int
let ( let* ) = Result.bind

let pair value =
  let* indices = Ndjson.list Ndjson.nat value in
  match indices with
  | [left; right] -> Ok (left, right)
  | [] | [_] | _ :: _ :: _ :: _ -> Error "expected exactly two level indices"

let parse tag value =
  match () with
  | () when tag = "succ" -> let* index = Ndjson.nat value in Ok (Succ index)
  | () when tag = "max" -> let* left, right = pair value in Ok (Max (left, right))
  | () when tag = "imax" -> let* left, right = pair value in Ok (Imax (left, right))
  | () when tag = "param" -> let* index = Ndjson.nat value in Ok (Param index)
  | () -> Error ("unknown level tag " ^ tag)

let name_references = function
  | Param index -> [index]
  | Zero | Succ _ | Max _ | Imax _ -> []

let level_references = function
  | Zero | Param _ -> []
  | Succ index -> [index]
  | Max (left, right) | Imax (left, right) -> [left; right]
