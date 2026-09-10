open Mechanism_kernel

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let read path = Mechanism_import.Io.attempt
  (fun () -> In_channel.with_open_bin path In_channel.input_all)

let contract name domain codomain = Printf.sprintf {|
def %s_map : (0 A : %s) -> (0 B : %s) -> (0 f : A -> B) ->
    (0 x : A) -> (0 y : A) -> %s A x y -> %s_Result B (f x) (f y) :=
  fun (0 A : %s) (0 B : %s) (0 f : A -> B) (0 x : A) (0 y : A)
    (e : %s A x y) => %s_congr A B f x y e
|} name domain codomain name name domain codomain name name

let audit (globals : Global.t) =
  let* () = Global.StringMap.fold (fun name entry acc ->
    let* () = acc in
    match entry with
    | Global.Def _ -> Ok ()
    | Global.Axiom _ | Global.Prim _ -> Error ("unexpected trusted entry: " ^ name))
    globals.entries (Ok ()) in
  Global.StringMap.fold (fun name (family : Positivity.family) acc ->
    let* () = acc in
    match family.f_status with
    | Positivity.Complete _ -> require ("unchecked family: " ^ name) family.f_positive
    | Positivity.Provisional | Positivity.Builtin -> Error ("unexpected family: " ^ name))
    globals.families (Ok ())

(* Every occurrence of the placeholder takes the good or the bad text. *)
let render source text = String.split_on_char '@' source |> String.concat text

(* Each pin holds the printed term and the printed expected type, so that
   no pin is a prefix of another pin.  Both halves are measured. *)
let mismatch term expected =
  "mismatch: the term has type " ^ term ^ " and the expected type is " ^ expected
let equality family index point = "(Lan SMu " ^ family ^ " [" ^ index
  ^ "] (Sec SColl 2 [ => (Lan SMu MechNat [] (Sec SColl 0 []));  => " ^ point ^ "]))"
let result_of = equality "Same_Result" "y" "x"

let negatives : (string * string * string * string * string) list = [
  "wrong-function",
  mismatch result_of "(Lan SMu Same_Result [(In SMu MechNat [] (ACtor mechSucc) [y])]",
  "fun (n : MechNat) => mechSucc n", "fun (n : MechNat) => n",
  {|def badFunction : (0 x : MechNat) -> (0 y : MechNat) ->
      Same MechNat x y -> Same_Result MechNat (mechSucc x) (mechSucc y) :=
    fun (0 x : MechNat) (0 y : MechNat) (e : Same MechNat x y) =>
      Same_congr MechNat MechNat (@) x y e|};
  "wrong-proof", mismatch (equality "Same" "x" "y") "(Lan SMu Same [y]",
  "e", "r",
  {|def badProof : (0 x : MechNat) -> (0 y : MechNat) ->
      Same MechNat x y -> Same MechNat y x -> Same_Result MechNat x y :=
    fun (0 x : MechNat) (0 y : MechNat)
      (e : Same MechNat x y) (r : Same MechNat y x) =>
      Same_congr MechNat MechNat (fun (n : MechNat) => n) x y (@)|};
  "wrong-family", mismatch result_of "(Lan SMu Same [y]",
  "Same_Result", "Same",
  {|def badFamily : (0 x : MechNat) -> (0 y : MechNat) ->
      Same MechNat x y -> @ MechNat x y :=
    fun (0 x : MechNat) (0 y : MechNat) (e : Same MechNat x y) =>
      Same_congr MechNat MechNat (fun (n : MechNat) => n) x y e|};
  "wrong-instance", mismatch result_of "(Lan SMu Again_Result [y]",
  "Same_Result", "Again_Result",
  {|def badInstance : (0 x : MechNat) -> (0 y : MechNat) ->
      Same MechNat x y -> @ MechNat x y :=
    fun (0 x : MechNat) (0 y : MechNat) (e : Same MechNat x y) =>
      Same_congr MechNat MechNat (fun (n : MechNat) => n) x y e|};
  "wrong-level", mismatch "Type 2" "Type 1",
  "Up", "Same",
  {|def lvRefl : @ MechNat mechZero mechZero := mechReflCtor
def lvLevel : @_Result (Type 0) MechNat MechNat :=
  @_congr MechNat (Type 0) (fun (n : MechNat) => MechNat)
    mechZero mechZero lvRefl|};
]

let suite root =
  let* catalog = kernel (Mechanism_prelude.Congruence.catalog Global.empty) in
  let* () = require "catalog inventory differs"
    (Mechanism_surface.Family_poly.arity catalog "MechCongr" = Some 2
      && Mechanism_surface.Family_poly.members catalog "MechCongr" = Some ["congr"]
      && Mechanism_surface.Family_poly.arity catalog "Result" = None) in
  let* () = Result.fold
    (Mechanism_prelude.Congruence.catalog ~budget:(Budget.of_poll (fun () -> true)) Global.empty)
    ~ok:(fun _catalog -> Error "budget: expected refusal")
    ~error:(fun error -> require ("budget: " ^ Error.to_string error)
      (String.starts_with ~prefix:(Error.to_string (Error.Budget_exhausted Check.budget_msg))
        (Error.to_string error))) in
  let* source = read (Filename.concat root "prelude/init.mech") in
  let* globals, _rows = kernel (Mechanism_surface.Elab.check_in Global.empty source) in
  let instances = [
    "Same", Level.one, Level.one, "Type 0", "Type 0";
    "Again", Level.one, Level.one, "Type 0", "Type 0";
    "Up", Level.one, Level.succ Level.one, "Type 0", "Type 1";
    "Down", Level.succ Level.one, Level.one, "Type 1", "Type 0";
    "Proof", Level.zero, Level.zero, "Prop", "Prop";
    "ProofData", Level.zero, Level.one, "Prop", "Type 0";
    "DataProof", Level.one, Level.zero, "Type 0", "Prop";
    "Tall", Level.succ (Level.succ Level.one), Level.zero, "Type 2", "Prop";
  ] in
  let* installed = List.fold_left (fun acc (name, u, v, domain, codomain) ->
    let* globals = acc in
    let* globals = kernel (Mechanism_surface.Family_poly.instantiate globals catalog
      ~name:"MechCongr" ~levels:[u; v] ~as_name:name) in
    let* globals, _rows = kernel (Mechanism_surface.Elab.check_in globals
      (contract name domain codomain)) in
    Ok globals) (Ok globals) instances in
  let* () = require "symbolic families escaped"
    (Global.find_family "MechCongr" installed = None
      && Global.find_family "Result" installed = None && Global.find "congr" installed = None) in
  let* client = read (Filename.concat root "test/fixtures/prelude/congruence.mech") in
  let* checked, _rows = kernel (Mechanism_surface.Elab.check_in installed client) in
  let* () = audit checked in
  let zero = Term.In (Shape.SMu ("MechNat", []), Term.ACtor "mechZero", []) in
  let one = Term.In (Shape.SMu ("MechNat", []), Term.ACtor "mechSucc", [zero]) in
  let computations = ["sameValue"; "upValue"; "downValue"; "proofDataValue"] in
  let* () = List.fold_left (fun acc name ->
    let* () = acc in
    let* value = kernel (Eval.eval checked [] (Term.Global name)) in
    let* actual = kernel (Eval.quote checked 0 value) in
    require ("wrong computation: " ^ name ^ " = " ^ Pp.term [] actual) (actual = one))
    (Ok ()) computations in
  let* () = List.fold_left (fun acc (name, prefix, good, bad, source) ->
    let* () = acc in
    let* _accepted = kernel (Mechanism_surface.Elab.check_in checked (render source good)) in
    Result.fold (Mechanism_surface.Elab.check_in checked (render source bad))
      ~ok:(fun _accepted -> Error (name ^ ": expected refusal: " ^ prefix))
      ~error:(fun error -> require (name ^ ": wrong refusal: " ^ Error.to_string error)
        (String.starts_with ~prefix (Error.to_string error)))) (Ok ()) negatives in
  Ok (List.length instances, List.length computations, List.length negatives)

let () =
  let root = Sys.argv |> Array.to_list |> fun args ->
    List.nth_opt args 1 |> Option.value ~default:"." in
  Result.fold (suite root)
    ~ok:(fun (instances, computations, negatives) ->
      Printf.printf "PRELUDE-CONGRUENCE-OK instances=%d computations=%d negatives=%d\n"
        instances computations negatives)
    ~error:(fun message -> Printf.printf "PRELUDE-CONGRUENCE-FAIL %s\n" message; exit 1)
