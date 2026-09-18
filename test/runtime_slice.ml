open Mechanism_kernel

module Names = Set.Make (String)
module Functions = Map.Make (String)
module E = Eterm

let ( let* ) = Result.bind

let rec references names = function
  | E.KVar _ | E.KLit _ | E.KErased -> names
  | E.KGlobal name -> Names.add name names
  | E.KLet (_, value, body) -> references (references names value) body
  | E.KClos (E.Fid name, _, captures) | E.KDelay (E.Fid name, captures) ->
      List.fold_left references (Names.add name names) captures
  | E.KApp (head, arguments) | E.KTail (head, arguments) ->
      List.fold_left references (references names head) arguments
  | E.KStruct (_, fields) | E.KTag (_, _, fields) ->
      List.fold_left references names fields
  | E.KProj (_, _, value) | E.KForce value -> references names value
  | E.KCase (_, scrutinee, branches) ->
      List.fold_left (fun names (branch : E.kbranch) -> references names branch.body)
        (references names scrutinee) branches

let select rows ~export =
  let* catalog = List.fold_left (fun result (_, entry) ->
    let* catalog = result in
    match entry with
    | Erase.Dropped | Erase.Postulate _ -> Ok catalog
    | Erase.Code declarations -> List.fold_left (fun result declaration ->
        let* catalog = result in
        match declaration with
        | E.KRec _ -> Ok catalog
        | E.KFun (E.Fid name, _, _, body) ->
            if Functions.mem name catalog then Error ("duplicate erased function: " ^ name)
            else Ok (Functions.add name (references Names.empty body) catalog))
        (Ok catalog) declarations) (Ok Functions.empty) rows in
  let rec visit needed = function
    | [] -> needed
    | name :: rest when Names.mem name needed -> visit needed rest
    | name :: rest ->
        let dependencies = Functions.find_opt name catalog
          |> Option.value ~default:Names.empty |> Names.elements in
        visit (Names.add name needed) (dependencies @ rest)
  in
  let needed = visit Names.empty [export] in
  (* Keep every type group and postulate. Only function bodies are selected. *)
  Ok (List.map (fun (name, entry) -> name, match entry with
    | Erase.Dropped -> Erase.Dropped
    | Erase.Postulate repr -> Erase.Postulate repr
    | Erase.Code declarations -> Erase.Code (List.filter (function
        | E.KRec _ -> true
        | E.KFun (E.Fid name, _, _, _) -> Names.mem name needed) declarations)) rows)

let self_test () =
  let require message condition = if condition then Ok () else Error message in
  let tid = E.Tid "slice-test" in
  let global name = E.KGlobal name in
  let body = E.KStruct (tid, [
    global "direct"; global "direct";
    E.KLet ("x", global "bound", global "body");
    E.KClos (E.Fid "closure", 1, [global "capture"]);
    E.KDelay (E.Fid "delay", [global "delayed-capture"]);
    E.KApp (global "call", [global "argument"]);
    E.KTail (global "tail", [global "tail-argument"]);
    E.KProj (tid, 0, global "projection");
    E.KTag (tid, 0, [global "payload"]);
    E.KCase (tid, global "scrutinee", [{ E.tag = 0; arity = 1; body = global "branch" }]);
    E.KForce (global "forced"); E.KVar 0; E.KErased;
    E.KLit (Literal.LInt (Bignum.of_int 0))]) in
  let expected = Names.of_list ["direct"; "bound"; "body"; "closure"; "capture";
    "delay"; "delayed-capture"; "call"; "argument"; "tail"; "tail-argument";
    "projection"; "payload"; "scrutinee"; "branch"; "forced"] in
  let* () = require "runtime reference traversal" (Names.equal (references Names.empty body) expected) in
  let fn name body = E.KFun (E.Fid name, [], E.RI31, body) in
  let root = fn "root" (E.KApp (global "middle", [])) in
  let middle = fn "middle" (E.KClos (E.Fid "lifted", 0, [])) in
  let lifted = fn "lifted" (global "root") in
  let dead = fn "dead" (global "unavailable") in
  let rows = ["entry", Erase.Code [E.KRec [tid]; root];
    "helpers", Erase.Code [middle; lifted; dead];
    "type", Erase.Dropped; "postulate", Erase.Postulate E.RI31] in
  let* selected = select rows ~export:"root" in
  let* () = require "runtime function closure or retained metadata"
    (selected = ["entry", Erase.Code [E.KRec [tid]; root];
      "helpers", Erase.Code [middle; lifted];
      "type", Erase.Dropped; "postulate", Erase.Postulate E.RI31]) in
  let* () = require "unknown references must remain visible"
    (Names.mem "unavailable" (references Names.empty (global "unavailable"))) in
  let* () = Result.fold (select ["duplicate", Erase.Code [root; root]] ~export:"root")
    ~ok:(fun _ -> Error "duplicate function accepted")
    ~error:(fun message -> require ("wrong duplicate refusal: " ^ message)
      (String.equal message "duplicate erased function: root")) in
  print_endline "RUNTIME-SLICE-OK";
  Ok ()
