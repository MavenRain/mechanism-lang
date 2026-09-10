open Mechanism_kernel
open Mechanism_surface

let ( let* ) = Result.bind
let kernel r = Result.map_error Error.to_string r
let require message yes = if yes then Ok () else Error message
let read path = Mechanism_import.Io.attempt
  (fun () -> In_channel.with_open_bin path In_channel.input_all)

let identity = {|poly (u) def identity : (0 A : Sort u) -> A -> A :=
  fun (0 A : Sort u) (x : A) => x
|}

let tinySource = {|mu Tiny : Type 0 with
| tinyZero : Tiny
|}

(* Each rejected source is paired with a nearby accepted source. *)
let negatives = [
  "duplicate-binder", "parse", "duplicate universe u",
  "poly (u, v) def p : Sort (succ u) := Sort u",
  "poly (u, u) def p : Sort (succ u) := Sort u";
  "unbound-level", "parse", "unbound universe v",
  "poly (u) def p : Sort (succ u) := Sort u",
  "poly (u) def p : Sort (succ u) := Sort v";
  "scope-leak", "parse", "unbound universe u",
  identity ^ "def p : Sort 1 := Prop",
  identity ^ "def p : Sort u := Prop";
  "term-as-level", "parse", "unbound universe A",
  "def p : (0 A : Type 0) -> Type 1 := fun (0 A : Type 0) => Type 0",
  "def p : (0 A : Type 0) -> Type 1 := fun (0 A : Type 0) => Sort A";
  "open-instance", "parse", "unbound universe u",
  identity ^ "specialize identity (1) as closed",
  identity ^ "specialize identity (u) as closed";
  "wrong-arity", "universe", "the schema identity expects 1 universe arguments, got 2",
  identity ^ "specialize identity (1) as closed",
  identity ^ "specialize identity (1, 2) as closed";
  "unknown-template", "unbound", "unknown universe schema absent",
  identity ^ "specialize identity (1) as closed",
  identity ^ "specialize absent (1) as closed";
  "bare-template", "unbound", "identity",
  identity ^ "specialize identity (1) as closed def p : (0 A : Type 0) -> A -> A := closed",
  identity ^ "def p : (0 A : Type 0) -> A -> A := identity";
  "duplicate-template", "mismatch", "the name identity is already declared",
  identity, identity ^ identity;
  "global-collision", "mismatch", "the name closed is already declared",
  identity ^ "specialize identity (1) as closed",
  identity ^ "def closed : Type 1 := Type 0 specialize identity (1) as closed";
  "template-collision", "mismatch", "the name identity is already declared",
  identity ^ "def closed : Type 1 := Type 0",
  identity ^ "def identity : Type 1 := Type 0";
  "instance-template-collision", "mismatch", "the name identity is already declared",
  identity ^ "specialize identity (1) as closed",
  identity ^ "specialize identity (1) as identity";
  "family-collision", "mismatch", "the name identity is already declared",
  identity ^ "mu Fresh : Prop with | fresh : Fresh",
  identity ^ "mu identity : Prop with | fresh : identity";
  "universal-check", "mismatch", "the term has type Type 1 and the expected type is Type (u0 + 1)",
  "poly (u) def p : Sort (succ u) := Sort u",
  "poly (u) def p : Sort (succ u) := Prop";
  "no-poly-axiom", "parse", "expected a nonrecursive definition after universe binders",
  "poly (u) def p : Sort (succ u) := Sort u",
  "poly (u) axiom p : Sort u";
  "no-poly-family", "parse", "expected a nonrecursive definition after universe binders",
  "poly (u) def p : Sort (succ u) := Sort u",
  "poly (u) mu P : Prop with | p : P";
  "no-poly-recursion", "parse", "expected a nonrecursive definition after universe binders",
  "poly (u) def p : Sort (succ u) := Sort u",
  "poly (u) def rec p : Sort (succ u) := Sort u";
  "reserved-universe-name", "parse", "reserved universe name max",
  "poly (u) def p : Sort (succ u) := Sort u",
  "poly (max) def p : Sort (succ max) := Sort max";
  "poly-shape", "parse",
  "expected '(' and a nonempty list of universe binders after 'poly'",
  "poly (u) def p : Sort (succ u) := Sort u",
  "poly u def p : Sort (succ u) := Sort u";
  "specialize-shape", "parse",
  "expected NAME, '(' with a nonempty list of universe levels",
  identity ^ "specialize identity (1) as closed",
  identity ^ "specialize identity as later";
  "constructor-template-collision", "mismatch", "the name tinyZero is already declared",
  tinySource ^ "poly (u) def fresh : Sort (succ u) := Sort u",
  tinySource ^ "poly (u) def tinyZero : Sort (succ u) := Sort u";
  "constructor-collision", "mismatch", "the name identity is already declared",
  identity ^ "mu Fresh : Prop with | fresh : Fresh",
  identity ^ "mu Fresh : Prop with | identity : Fresh";
  "constructor-instance-collision", "mismatch", "the name tinyZero is already declared",
  tinySource ^ identity ^ "specialize identity (1) as closed",
  tinySource ^ identity ^ "specialize identity (1) as tinyZero";
]

let refusal label kind detail result =
  Result.fold result
    ~ok:(fun _value -> Error (label ^ ": expected refusal"))
    ~error:(fun error ->
      let actual_kind, actual_detail = match error with
        | Error.Parse (message, _line, _col) -> "parse", message
        | Error.Unbound message -> "unbound", message
        | Error.Universe message -> "universe", message
        | Error.Mismatch message -> "mismatch", message
        | Error.Budget_exhausted message -> "budget", message
        | Error.Not_yet _ | Error.Carry _ | Error.Quantity _ | Error.Wrong_leg _
        | Error.Missing_branch _ | Error.Overflow _ | Error.Cannot_infer _
        | Error.Index_not_zero _ | Error.Index_above_universe _ | Error.Termination _ ->
            "unexpected", Error.to_string error in
      require (label ^ ": wrong refusal: " ^ Error.to_string error)
        (String.equal actual_kind kind && String.starts_with ~prefix:detail actual_detail))

let suite root =
  let* source = read (Filename.concat root "test/fixtures/prelude/prenex.mech") in
  let* tree = kernel (Parser.parse source) in
  let* printed = kernel (Parser.parse (Syntax.print tree)) in
  let* () = require "parse/print changed the tree" (tree = printed) in
  let* globals, rows = kernel (Elab.check_in Global.empty source) in
  let* () = require "symbolic template escaped"
    (List.for_all (fun name -> Global.find name globals = None)
      ["identity"; "functionType"; "joinedSort"; "appliedIdentity"]) in
  let* () = Global.StringMap.fold (fun name entry acc ->
    let* () = acc in
    match entry with
    | Global.Def _ -> Ok ()
    | Global.Axiom _ | Global.Prim _ -> Error ("unexpected trusted entry: " ^ name))
    globals.entries (Ok ()) in
  let* () = require "row order changed"
    (List.map fst rows = ["proofIdentity"; "dataIdentity"; "typeIdentity"; "againIdentity";
      "dataValue"; "againValue"; "proofValue"; "typeValue"; "predicateType"; "higherType";
      "predicateWitness"; "higherWitness"; "joined"; "joinedWitness";
      "applied"; "appliedValue"]) in
  let zero = Term.In (Shape.SMu ("Tiny", []), Term.ACtor "tinyZero", []) in
  let succ n = Term.In (Shape.SMu ("Tiny", []), Term.ACtor "tinySucc", [n]) in
  let tiny = Term.Lan (Shape.SMu ("Tiny", []), Rules.diagram_of []) in
  (* joinedWitness holds no instance in its body, so its normal form
     repeats the one of typeValue and its row measures nothing.  The
     fixture keeps the definition, because its annotation forces the
     instance joined to a sort. *)
  let computations = ["dataValue", succ zero; "againValue", succ (succ zero);
    "typeValue", tiny; "appliedValue", succ zero] in
  let* () = List.fold_left (fun acc (name, expected) ->
    let* () = acc in
    let* value = kernel (Eval.eval globals [] (Term.Global name)) in
    let* actual = kernel (Eval.quote globals 0 value) in
    require ("wrong computation: " ^ name ^ " = " ^ Pp.term [] actual) (actual = expected))
    (Ok ()) computations in
  let* () = List.fold_left (fun acc (label, kind, detail, good, bad) ->
    let* () = acc in
    let* _accepted = kernel (Elab.check_in Global.empty good) in
    refusal label kind detail (Elab.check_in Global.empty bad)) (Ok ()) negatives in
  (* The refusals that need a state of their own stay in this list, so
     the printed count measures the checks that ran. *)
  let extra_negatives = [
    (fun () -> refusal "budget" "budget" Check.budget_msg
      (Elab.check_in ~budget:(Budget.of_poll (fun () -> true)) Global.empty identity));
    (fun () -> refusal "catalog-lifetime" "unbound" "unknown universe schema identity"
      (Elab.check_in globals "specialize identity (1) as later"));
  ] in
  let* () = List.fold_left (fun acc check -> let* () = acc in check ())
    (Ok ()) extra_negatives in
  (* A level above the literal digit limit keeps its composed syntax. *)
  let large = Printf.sprintf "def p : Sort (succ (succ %d)) := Sort (succ %d)"
    999999999999999999 999999999999999999 in
  let* large_tree = kernel (Parser.parse large) in
  let* large_again = kernel (Parser.parse (Syntax.print large_tree)) in
  let* () = require "large level round-trip changed" (large_tree = large_again) in
  let* _large = kernel (Elab.check_in Global.empty large) in
  Ok (Global.StringMap.cardinal globals.entries, List.length computations,
    List.length negatives + List.length extra_negatives)

let () =
  let root = Sys.argv |> Array.to_list |> fun args ->
    List.nth_opt args 1 |> Option.value ~default:"." in
  Result.fold (suite root)
    ~ok:(fun (entries, computations, negatives) ->
      Printf.printf "PRENEX-OK entries=%d computations=%d negatives=%d\n"
        entries computations negatives)
    ~error:(fun message -> Printf.printf "PRENEX-FAIL %s\n" message; exit 1)
