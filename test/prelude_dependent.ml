open Mechanism_kernel

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let read path = Mechanism_import.Io.attempt
  (fun () -> In_channel.with_open_bin path In_channel.input_all)

let render bindings source = List.fold_left (fun text (marker, replacement) ->
  String.concat replacement (String.split_on_char marker text)) source bindings

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

let pi_contract = {|
def $sort : (0 A : @) -> (0 B : A -> %) -> ~ :=
  fun (0 A : @) (0 B : A -> %) => $ A B
def $contract : (0 A : @) -> (0 B : A -> %) -> ((x : A) -> B x) -> $ A B :=
  fun (0 A : @) (0 B : A -> %) (f : (x : A) -> B x) => f
def $apply : (0 A : @) -> (0 B : A -> %) -> $ A B -> (x : A) -> B x :=
  fun (0 A : @) (0 B : A -> %) (f : $ A B) (x : A) => f x
|}

(* The dependent fiber and motive stay abstract, so a constant fiber,
   a swapped projection, or an incorrect recursor index cannot pass. *)
let pair_contract = {|
def $sort : (0 A : @) -> (0 B : A -> %) -> # :=
  fun (0 A : @) (0 B : A -> %) => $Pair A B
def $construct : (0 A : @) -> (0 B : A -> %) -> (x : A) -> B x -> $Pair A B :=
  fun (0 A : @) (0 B : A -> %) (x : A) (y : B x) => $Mk A B x y
def $expand : (0 A : @) -> (0 B : A -> %) -> $Pair A B -> (x : A) * B x :=
  fun (0 A : @) (0 B : A -> %) (p : $Pair A B) => p
def $first : (0 A : @) -> (0 B : A -> %) -> $Pair A B -> A :=
  fun (0 A : @) (0 B : A -> %) (p : $Pair A B) => $Fst A B p
def $second : (0 A : @) -> (0 B : A -> %) -> (p : $Pair A B) -> B p.1 :=
  fun (0 A : @) (0 B : A -> %) (p : $Pair A B) => $Snd A B p
def $induct : (0 A : @) -> (0 B : A -> %) -> (0 P : $Pair A B -> ~) ->
    ((x : A) -> (y : B x) -> P (x, y)) -> (p : $Pair A B) -> P p :=
  fun (0 A : @) (0 B : A -> %) (0 P : $Pair A B -> ~)
    (step : (x : A) -> (y : B x) -> P (x, y)) (p : $Pair A B) => $Rec A B P step p
|}

let negatives = [
  "wrong-fiber", "mismatch: the term has type (Lan SMu MechUnit",
  "mechSucc mechZero", "unitValue",
  {|def misuse : SmallPair MechBool DepFiber := SmallMk MechBool DepFiber mechTrue ($)|};
  "wrong-domain", "mismatch: the term has type (Lan SMu MechNat [] (Sec SColl 0 [])) \
and the expected type is (Lan SMu MechBool",
  "mechTrue", "zeroValue",
  {|def misuse : SmallPair MechBool DepFiber := SmallMk MechBool DepFiber ($) (mechSucc mechZero)|};
  "wrong-projection", "mismatch: the term has type (Lan SMu MechBool",
  "SmallSnd", "SmallFst",
  {|def misuse : MechNat := $ MechBool DepFiber depPair|};
  "dependent-step", "mismatch: the term has type (Lan SMu MechNat [] (Sec SColl 0 [])) \
and the expected type is (Elim SMu MechBool [] x as self return Type 1 with \
| (ACtor mechFalse)",
  "y", "zeroValue",
  {|def misuse : MechNat := SmallRec MechBool DepFiber
      (fun (p : SmallPair MechBool DepFiber) => DepFiber p.1)
      (fun (x : MechBool) (y : DepFiber x) => $) depPair|};
  "wrong-pi-body", "unbound: mechTrue is not a constructor of MechNat",
  "n", "mechTrue",
  {|def misuse : SmallPi MechNat (fun (n : MechNat) => MechNat) :=
      fun (n : MechNat) => $|};
  "wrong-level", "mismatch: the term has type Type 2 and the expected type is Type 1",
  "UpMk", "SmallMk",
  {|def misuse : UpPair MechBool (fun (x : MechBool) => Type 0) :=
      $ MechBool (fun (x : MechBool) => Type 0) mechTrue MechNat|};
]

let budget_refusal = "budget: the check budget is exhausted"

let suite root =
  let* catalog = kernel (Mechanism_prelude.Dependent.catalog Global.empty) in
  let* () = Result.fold (Mechanism_prelude.Dependent.catalog
      ~budget:(Budget.of_poll (fun () -> true)) Global.empty)
    ~ok:(fun _catalog -> Error ("budget: expected refusal: " ^ budget_refusal))
    ~error:(fun error -> require ("budget: wrong refusal: " ^ Error.to_string error)
      (String.starts_with ~prefix:budget_refusal (Error.to_string error))) in
  let templates = ["MechPi", 2; "MechSigma", 2; "mechSigmaMk", 2;
    "mechSigmaFst", 2; "mechSigmaSnd", 2; "mechSigmaRec", 3] in
  let* () = List.fold_left (fun acc (name, arity) ->
    let* () = acc in
    require ("wrong template arity: " ^ name)
      (Mechanism_surface.Poly.arity catalog name = Some arity)) (Ok ()) templates in
  let* source = read (Filename.concat root "prelude/init.mech") in
  let* globals, _rows = kernel (Mechanism_surface.Elab.check_in Global.empty source) in
  let entry_count (globals : Global.t) = Global.StringMap.cardinal globals.entries in
  let install (globals, added) (name, levels, as_name) =
    let before = entry_count globals in
    let* globals, _term = kernel (Mechanism_surface.Poly.instantiate globals catalog
      ~name ~levels ~as_name) in
    Ok (globals, added + entry_count globals - before) in
  let groups = [
    "Small", Level.zero, Level.zero, Level.one, "Type 0", "Type 0", "Type 0";
    "Up", Level.zero, Level.one, Level.succ Level.one, "Type 0", "Type 1", "Type 1";
    "Down", Level.one, Level.zero, Level.zero, "Type 1", "Type 0", "Prop";
    "Tall", Level.one, Level.succ Level.one, Level.succ (Level.succ Level.one),
      "Type 1", "Type 2", "Type 2";
  ] in
  let* installed, instances = List.fold_left (fun acc (prefix, u, v, w, a, b, motive) ->
    let* globals, added = acc in
    let* globals, added = List.fold_left (fun acc instance ->
      let* state = acc in install state instance) (Ok (globals, added))
      ["MechPi", [Level.succ u; Level.succ v], prefix ^ "Pi";
       "MechSigma", [u; v], prefix ^ "Pair";
       "mechSigmaMk", [u; v], prefix ^ "Mk";
       "mechSigmaFst", [u; v], prefix ^ "Fst";
       "mechSigmaSnd", [u; v], prefix ^ "Snd";
       "mechSigmaRec", [u; v; w], prefix ^ "Rec"] in
    let result_sort = if Level.le u v then b else a in
    let contract = render ['$', prefix; '@', a; '%', b; '~', motive; '#', result_sort] pair_contract
      ^ render ['$', prefix ^ "Pi"; '@', a; '%', b; '~', result_sort] pi_contract in
    let* globals, _rows = kernel (Mechanism_surface.Elab.check_in globals contract) in
    Ok (globals, added)) (Ok (globals, 0)) groups in
  let pi_instances = [
    "ProofPi", Level.zero, Level.zero, "Prop", "Prop", "Prop";
    "ProofDataPi", Level.zero, Level.one, "Prop", "Type 0", "Type 0";
    "DataProofPi", Level.one, Level.zero, "Type 0", "Prop", "Prop";
    "HigherProofPi", Level.succ Level.one, Level.zero, "Type 1", "Prop", "Prop";
    "HigherPi", Level.succ Level.one, Level.one, "Type 1", "Type 0", "Type 1";
  ] in
  let* installed, instances = List.fold_left (fun acc (name, u, v, a, b, result_sort) ->
    let* globals, added = acc in
    let* globals, added = install (globals, added) ("MechPi", [u; v], name) in
    let* globals, _rows = kernel (Mechanism_surface.Elab.check_in globals
      (render ['$', name; '@', a; '%', b; '~', result_sort] pi_contract)) in
    Ok (globals, added)) (Ok (installed, instances)) pi_instances in
  let* () = require ("wrong instance count: " ^ string_of_int instances)
    (instances = 6 * List.length groups + List.length pi_instances) in
  let* client = read (Filename.concat root "test/fixtures/prelude/dependent.mech") in
  let* checked, _rows = kernel (Mechanism_surface.Elab.check_in installed client) in
  let* () = audit checked in
  let zero = Term.In (Shape.SMu ("MechNat", []), Term.ACtor "mechZero", []) in
  let one = Term.In (Shape.SMu ("MechNat", []), Term.ACtor "mechSucc", [zero]) in
  let two = Term.In (Shape.SMu ("MechNat", []), Term.ACtor "mechSucc", [one]) in
  let nat = Term.Lan (Shape.SMu ("MechNat", []), Rules.diagram_of []) in
  let computations = ["pairFirst", zero; "pairSecond", one; "pairRec", two;
    "depSecond", one; "depRec", one; "downFirst", nat; "downSecond", one;
    "upSecond", nat; "upRec", nat; "piValue", one;
    "downRec", Term.In (Shape.SMu ("MechTrue", []), Term.ACtor "mechTrivial", []);
    "proofPiValue", Term.In (Shape.SMu ("MechTrue", []), Term.ACtor "mechTrivial", [])] in
  let* () = List.fold_left (fun acc (name, expected) ->
    let* () = acc in
    let* value = kernel (Eval.eval checked [] (Term.Global name)) in
    let* actual = kernel (Eval.quote checked 0 value) in
    require ("wrong computation: " ^ name ^ " = " ^ Pp.term [] actual)
      (actual = expected)) (Ok ()) computations in
  let* () = List.fold_left (fun acc (name, prefix, good, bad, source) ->
    let* () = acc in
    let* _accepted = kernel (Mechanism_surface.Elab.check_in checked (render ['$', good] source)) in
    Result.fold (Mechanism_surface.Elab.check_in checked (render ['$', bad] source))
      ~ok:(fun _accepted -> Error (name ^ ": expected refusal: " ^ prefix))
      ~error:(fun error -> require (name ^ ": wrong refusal: " ^ Error.to_string error)
        (String.starts_with ~prefix (Error.to_string error)))) (Ok ()) negatives in
  Ok (List.length templates, instances,
    List.length computations, List.length negatives)

let () =
  let root = Sys.argv |> Array.to_list |> fun args ->
    List.nth_opt args 1 |> Option.value ~default:"." in
  Result.fold (suite root)
    ~ok:(fun (templates, instances, computations, negatives) ->
      Printf.printf "PRELUDE-DEPENDENT-OK templates=%d instances=%d computations=%d negatives=%d\n"
        templates instances computations negatives)
    ~error:(fun message -> Printf.printf "PRELUDE-DEPENDENT-FAIL %s\n" message; exit 1)
