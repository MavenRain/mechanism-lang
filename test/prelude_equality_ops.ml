open Mechanism_kernel

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let read path = Mechanism_import.Io.attempt
  (fun () -> In_channel.with_open_bin path In_channel.input_all)

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

(* Distinct bound endpoints make these contracts stronger than closed
   reflexivity examples. Every installed universe instance checks them. *)
let equality_contract name carrier motive =
  Printf.sprintf {|
def %s_reverse : (0 A : %s) -> (0 x : A) -> (0 y : A) -> %s A x y -> %s A y x :=
  fun (0 A : %s) (0 x : A) (0 y : A) (e : %s A x y) => %s_symm A x y e
def %s_compose : (0 A : %s) -> (0 x : A) -> (0 y : A) -> (0 z : A) ->
    %s A x y -> %s A y z -> %s A x z :=
  fun (0 A : %s) (0 x : A) (0 y : A) (0 z : A) (e : %s A x y) (h : %s A y z) =>
    %s_trans A x y z e h
def %s_map : (0 A : %s) -> (0 B : %s) -> (0 f : A -> B) ->
    (0 x : A) -> (0 y : A) -> %s A x y -> %s B (f x) (f y) :=
  fun (0 A : %s) (0 B : %s) (0 f : A -> B) (0 x : A) (0 y : A) (e : %s A x y) =>
    %s_congr A B f x y e
def %s_induct : (0 A : %s) -> (0 x : A) ->
    (0 P : (0 y : A) -> (0 e : %s A x y) -> %s) ->
    P x (%s_refl A x) -> (0 y : A) -> (e : %s A x y) -> P y e :=
  fun (0 A : %s) (0 x : A) (0 P : (0 y : A) -> (0 e : %s A x y) -> %s)
    (base : P x (%s_refl A x)) (0 y : A) (e : %s A x y) => %s_j A x P base y e
|} name carrier name name carrier name name
    name carrier name name name carrier name name name
    name carrier carrier name name carrier carrier name name
    name carrier name motive name name carrier name motive name name name

let cast_contract name level =
  Printf.sprintf {|
def %s_reverse : (0 A : %s) -> (0 B : %s) -> %s A B -> %s B A :=
  fun (0 A : %s) (0 B : %s) (e : %s A B) => %s_symm A B e
def %s_compose : (0 A : %s) -> (0 B : %s) -> (0 C : %s) ->
    %s A B -> %s B C -> %s A C :=
  fun (0 A : %s) (0 B : %s) (0 C : %s) (e : %s A B) (h : %s B C) =>
    %s_trans A B C e h
|} name level level name name level level name name
    name level level level name name name level level level name name name

let normalize globals name =
  let* value = kernel (Eval.eval globals [] (Term.Global name)) in
  kernel (Eval.quote globals 0 value)

let negatives : (string * string * string * string * (string -> string, unit, string) format) list = [
  "symm-endpoint", "mismatch: the term has type (Lan SMu OpsData", "e", "OpsData_refl MechNat x",
  {|def checkSymm : (0 x : MechNat) -> (0 y : MechNat) -> OpsData MechNat x y -> OpsData MechNat y x :=
    fun (0 x : MechNat) (0 y : MechNat) (e : OpsData MechNat x y) =>
      OpsData_symm MechNat x y (%s)|};
  "trans-middle", "mismatch: the term has type (Lan SMu OpsData", "h", "e",
  {|def checkTrans : (0 x : MechNat) -> (0 y : MechNat) -> (0 z : MechNat) ->
      OpsData MechNat x y -> OpsData MechNat y z -> OpsData MechNat x z :=
    fun (0 x : MechNat) (0 y : MechNat) (0 z : MechNat)
      (e : OpsData MechNat x y) (h : OpsData MechNat y z) =>
      OpsData_trans MechNat x y z e (%s)|};
  "congr-function", "mismatch: the term has type (Lan SMu OpsData",
  "fun (n : MechNat) => mechSucc n", "fun (n : MechNat) => n",
  {|def checkCongr : (0 x : MechNat) -> (0 y : MechNat) -> OpsData MechNat x y ->
      OpsData MechNat (mechSucc x) (mechSucc y) :=
    fun (0 x : MechNat) (0 y : MechNat) (e : OpsData MechNat x y) =>
      OpsData_congr MechNat MechNat (%s) x y e|};
  "j-base", "mismatch: the term has type (Lan SMu MechNat [] (Sec SColl 0 [])) "
    ^ "and the expected type is (Lan SMu OpsIsOne", "opsIsOne", "opsSymm",
  {|def checkJBase : OpsIsOne (mechSucc mechZero) :=
    OpsData_j MechNat (mechSucc mechZero)
      (fun (0 y : MechNat) (0 e : OpsData MechNat (mechSucc mechZero) y) => OpsIsOne y)
      (%s) (mechSucc mechZero) (OpsData_refl MechNat (mechSucc mechZero))|};
  "j-endpoint", "mismatch: the term has type (Lan SMu OpsData", "mechZero", "mechSucc mechZero",
  {|def checkJEndpoint : MechNat := OpsData_j MechNat mechZero
    (fun (0 y : MechNat) (0 e : OpsData MechNat mechZero y) => MechNat)
    mechZero (%s) (OpsData_refl MechNat mechZero)|};
  "cast-symm-endpoint", "mismatch: the term has type (Lan SMu OpsCast", "e", "OpsCast_refl A",
  {|def checkCastSymm : (0 A : Type 0) -> (0 B : Type 0) -> OpsCast A B -> OpsCast B A :=
    fun (0 A : Type 0) (0 B : Type 0) (e : OpsCast A B) => OpsCast_symm A B (%s)|};
  "cast-trans-middle", "mismatch: the term has type (Lan SMu OpsCast", "h", "e",
  {|def checkCastTrans : (0 A : Type 0) -> (0 B : Type 0) -> (0 C : Type 0) ->
      OpsCast A B -> OpsCast B C -> OpsCast A C :=
    fun (0 A : Type 0) (0 B : Type 0) (0 C : Type 0) (e : OpsCast A B) (h : OpsCast B C) =>
      OpsCast_trans A B C e (%s)|};
  "mixed-instance", "mismatch: the term has type (Lan SMu OpsHigher",
  "OpsData_refl MechNat mechZero", "OpsHigher_refl MechNat mechZero",
  {|def checkInstance : OpsData MechNat mechZero mechZero :=
    OpsData_symm MechNat mechZero mechZero (%s)|};
]

let suite root =
  let* catalog = kernel (Mechanism_prelude.Equality.catalog Global.empty) in
  let* source = read (Filename.concat root "prelude/init.mech") in
  let* globals, _rows = kernel (Mechanism_surface.Elab.check_in Global.empty source) in
  let instances = [
    "MechEq", [Level.one; Level.one], "OpsData", equality_contract "OpsData" "Type 0" "Type 0";
    "MechEq", [Level.zero; Level.zero], "OpsProof", equality_contract "OpsProof" "Prop" "Prop";
    "MechEq", [Level.zero; Level.one], "OpsProofData", equality_contract "OpsProofData" "Prop" "Type 0";
    "MechEq", [Level.one; Level.zero], "OpsToProof", equality_contract "OpsToProof" "Type 0" "Prop";
    "MechEq", [Level.one; Level.succ Level.one], "OpsHigher", equality_contract "OpsHigher" "Type 0" "Type 1";
    "MechEq", [Level.succ Level.one; Level.succ Level.one], "OpsTypes", equality_contract "OpsTypes" "Type 1" "Type 1";
    "MechEq", [Level.succ (Level.succ Level.one); Level.one], "OpsTall", equality_contract "OpsTall" "Type 2" "Type 0";
    "MechTypeEq", [Level.zero], "OpsCast", cast_contract "OpsCast" "Type 0";
    "MechTypeEq", [Level.one], "OpsCastHigher", cast_contract "OpsCastHigher" "Type 1";
    "MechTypeEq", [Level.succ Level.one], "OpsCastTall", cast_contract "OpsCastTall" "Type 2";
  ] in
  let* installed = List.fold_left (fun acc (name, levels, as_name, contract) ->
    let* globals = acc in
    let* globals = kernel (Mechanism_surface.Family_poly.instantiate globals catalog
      ~name ~levels ~as_name) in
    let* globals, _rows = kernel (Mechanism_surface.Elab.check_in globals contract) in
    Ok globals) (Ok globals) instances in
  let* client = read (Filename.concat root "test/fixtures/prelude/equality-ops.mech") in
  let* checked, _rows = kernel (Mechanism_surface.Elab.check_in installed client) in
  let* () = audit checked in
  let zero = Term.In (Shape.SMu ("MechNat", []), Term.ACtor "mechZero", []) in
  let one = Term.In (Shape.SMu ("MechNat", []), Term.ACtor "mechSucc", [zero]) in
  let computations = List.map (fun name -> name, one)
    ["opsSymm"; "opsTrans"; "opsCongr"; "opsJ"; "opsProofDataJ";
     "opsCastSymm"; "opsCastTrans"; "opsHigherValue"; "opsTypesValue"; "opsCastHigherValue"]
    @ ["opsDependent", Term.In (Shape.SMu ("OpsFiber", [zero;
         Term.In (Shape.SMu ("OpsData", [zero]), Term.ACtor "mechReflCtor", [])]),
         Term.ACtor "opsFiber", [])] in
  let* () = List.fold_left (fun acc (name, expected) ->
    let* () = acc in
    let* actual = normalize checked name in
    require ("wrong computation: " ^ name ^ " = " ^ Pp.term [] actual) (actual = expected))
    (Ok ()) computations in
  let* () = List.fold_left (fun acc (name, prefix, good, bad, source) ->
    let* () = acc in
    let* _accepted = kernel (Mechanism_surface.Elab.check_in checked (Printf.sprintf source good)) in
    Result.fold (Mechanism_surface.Elab.check_in checked (Printf.sprintf source bad))
      ~ok:(fun _accepted -> Error (name ^ ": expected refusal: " ^ prefix))
      ~error:(fun error -> require (name ^ ": wrong refusal: " ^ Error.to_string error)
        (String.starts_with ~prefix (Error.to_string error)))) (Ok ()) negatives in
  Ok (List.length instances, List.length computations, List.length negatives)

let () =
  let root = Sys.argv |> Array.to_list |> fun args ->
    List.nth_opt args 1 |> Option.value ~default:"." in
  Result.fold (suite root)
    ~ok:(fun (instances, computations, negatives) ->
      Printf.printf "PRELUDE-EQUALITY-OPS-OK instances=%d computations=%d negatives=%d\n"
        instances computations negatives)
    ~error:(fun message -> Printf.printf "PRELUDE-EQUALITY-OPS-FAIL %s\n" message; exit 1)
