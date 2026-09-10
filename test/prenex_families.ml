open Mechanism_kernel
open Mechanism_surface
let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let box = "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A "
let identity = "poly (u) def identity : Sort (succ u) := Sort u "
let plain = "mu Tiny : Type 0 with | zero : Tiny "
let instance = box ^ "specialize Box (1) as DataBox "
(* Each row pins the error kind as well as the text.  Error.message
   erases the constructor (vendor/kanon/lib/error.ml:48-64), so a text
   alone leaves a producer free to move to another string arm without
   any suite motion.  The shape is the one of test/prenex.ml:92-107. *)
let negative_cases = [
  "scope", "parse", "unbound universe v", box,
    "poly (u) mu Box (0 A : Sort v) : Sort (succ u) with | box : A -> Box A";
  "scope-leak", "parse", "unbound universe u", instance,
    box ^ "specialize Box (u) as DataBox";
  "arity", "universe", "the family schema Box expects 1 universe arguments, got 2", instance,
    box ^ "specialize Box (1, 2) as DataBox";
  "unstable-universe", "universe",
    "a family schema must remain in Prop or Type for every universe substitution",
    "poly (u) mu Empty : Sort (succ u) with",
    "poly (u) mu Empty : Sort u with";
  "constructor-result", "mismatch", "a constructor of Box ends at another type", box,
    "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> A";
  "duplicate-constructor", "mismatch", "the constructor box is already declared in Box", box,
    box ^ "| box : (0 A : Sort u) -> Box A";
  "bare-family", "unbound", "Box", instance ^ "def T : Type 1 := DataBox (prod () : Type 0)",
    box ^ "def T : Type 1 := Box (prod () : Type 0)";
  "bare-constructor", "unbound", "box", plain ^ instance ^ "def x : DataBox Tiny := box zero",
    plain ^ box ^ "def x : Tiny := box zero";
  "duplicate-template", "mismatch", "the name Box is already declared", box, box ^ box;
  "definition-template-collision", "mismatch", "the name identity is already declared", identity ^ box,
    identity ^ "poly (u) mu identity : Prop with";
  "family-template-collision", "mismatch", "the name Box is already declared", box ^ identity,
    box ^ "poly (u) def Box : Sort (succ u) := Sort u";
  "ordinary-definition-collision", "mismatch", "the name Box is already declared",
    box ^ "def X : Type 0 := Prop",
    box ^ "def Box : Type 0 := Prop";
  "ordinary-family-collision", "mismatch", "the name Box is already declared", box ^ plain,
    box ^ "mu Box : Type 0 with";
  "ordinary-constructor-collision", "mismatch", "the name Box is already declared", box ^ plain,
    box ^ "mu Tiny : Type 0 with | Box : Tiny";
  "late-constructor-collision", "mismatch", "the name box is already declared", instance ^ identity,
    box ^ "poly (u) def box : Sort (succ u) := Sort u specialize Box (1) as DataBox";
  "template-constructor-collision", "mismatch", "the name identity is already declared", identity ^ box,
    identity ^ "poly (u) mu Box : Type 0 with | identity : Box specialize Box (1) as DataBox";
  "instance-template-collision", "mismatch", "the name Box is already declared", instance,
    box ^ "specialize Box (1) as Box";
  "instance-definition-template", "mismatch", "the name identity is already declared", identity ^ instance,
    identity ^ box ^ "specialize Box (1) as identity";
  "definition-instance-family-template", "mismatch", "the name Box is already declared",
    box ^ identity ^ "specialize identity (1) as X",
    box ^ identity ^ "specialize identity (1) as Box";
  "instance-family-collision", "mismatch", "the name Tiny is already declared", plain ^ instance,
    plain ^ box ^ "specialize Box (1) as Tiny";
  "instance-constructor-collision", "mismatch", "the name zero is already declared", plain ^ instance,
    plain ^ box ^ "specialize Box (1) as zero";
  "existing-constructor-template", "mismatch", "the name zero is already declared", plain ^ box,
    plain ^ "poly (u) mu zero : Type 0 with";
  "groups", "parse",
    "expected a nonrecursive definition or single family after universe binders, found 'poly'", box,
    "poly (u) mu P : Prop with and Q : Prop with";
  "late-family-template-collision", "mismatch", "the name box is already declared",
    instance ^ identity,
    box ^ "poly (u) mu box (0 A : Sort u) : Sort (succ u) with | mk : A -> box A "
      ^ "specialize Box (1) as DataBox";
  "instance-own-constructor", "mismatch", "the name box is already declared", instance,
    box ^ "specialize Box (1) as box";
  "self-named-constructor", "mismatch", "the constructor Box repeats the family name Box", box,
    "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | Box : A -> Box A";
]

let refusal label kind detail result = Result.fold result
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
  let* source = Mechanism_import.Io.attempt (fun () ->
    In_channel.with_open_bin (Filename.concat root "test/fixtures/prelude/prenex-families.mech")
      In_channel.input_all) in
  let* tree = kernel (Parser.parse source) in
  let* roundtrip = kernel (Parser.parse (Syntax.print tree)) in
  let* () = require "parse/print changed family syntax" (tree = roundtrip) in
  let* globals, rows = kernel (Elab.check_in Global.empty source) in
  let expected_families = ["Tiny"; "Truth"; "DataBox"; "TypeBox"; "ProofBox";
    "AgainBox"; "TinyList"; "TypeList"; "Up"; "Down"; "DataEq"; "ProofEq"; "AnnotatedBox"] in
  let* () = require "family inventory or symbolic isolation changed"
    (List.map fst (Global.StringMap.bindings globals.families)
      = List.sort String.compare expected_families) in
  let expected_rows = ["dataBox"; "typeBox"; "proofBox"; "againBox"; "unbox";
    "dataValue"; "typeValue"; "items"; "types"; "head"; "listValue"; "up"; "down";
    "upType"; "dataRefl"; "proofRefl"; "boxIdentity"; "boxValue"] in
  let* () = require "entry inventory or row order changed"
    (List.map fst rows = expected_rows
     && Global.StringMap.cardinal globals.entries = List.length expected_rows) in
  let* () = Global.StringMap.fold (fun name entry acc -> let* () = acc in
    match entry with
    | Global.Def _ -> Ok ()
    | Global.Axiom _ | Global.Prim _ -> Error ("unexpected trusted entry: " ^ name))
    globals.entries (Ok ()) in
  let zero = Term.In (Shape.SMu ("Tiny", []), Term.ACtor "tinyZero", []) in
  let one = Term.In (Shape.SMu ("Tiny", []), Term.ACtor "tinySucc", [zero]) in
  let tiny = Term.Lan (Shape.SMu ("Tiny", []), Rules.diagram_of []) in
  let computations = ["dataValue", one; "typeValue", tiny; "listValue", one;
    "upType", tiny; "boxValue", one] in
  let* () = List.fold_left (fun acc (name, expected) -> let* () = acc in
    let* value = kernel (Eval.eval globals [] (Term.Global name)) in
    let* actual = kernel (Eval.quote globals 0 value) in
    require ("wrong computation: " ^ name ^ " = " ^ Pp.term [] actual) (actual = expected))
    (Ok ()) computations in
  let* () = List.fold_left (fun acc (label, kind, diagnostic, good, bad) -> let* () = acc in
    let* _accepted = kernel (Elab.check_in Global.empty good) in
    refusal label kind diagnostic (Elab.check_in Global.empty bad)) (Ok ()) negative_cases in
  let extras = [
    (fun () -> refusal "budget" "budget" Check.budget_msg
      (Elab.check_in ~budget:(Budget.of_poll (fun () -> true)) Global.empty box));
    (* The template text alone polls 43 times and the same text with the
       specialize polls 77 times, so a poll that fires after 60 calls can
       only fire inside Family_poly.instantiate.  This is the one check
       of the budget the specialize path threads. *)
    (fun () ->
      let polls = ref 0 in
      let poll () = incr polls; !polls > 60 in
      refusal "specialize-budget" "budget" Check.budget_msg
        (Elab.check_in ~budget:(Budget.of_poll poll) Global.empty instance));
    (fun () -> refusal "lifetime" "unbound" "unknown universe schema Box"
      (Elab.check_in globals "specialize Box (1) as Later"));
    (fun () -> refusal "template-ast-boundary" "mismatch"
      "a prenex declaration requires a program catalog"
      (Elab.elab_decl (Check.make Global.empty Budget.unlimited)
        (Syntax.DPolyMu (1, { Syntax.fm_name="Empty"; fm_params=[];
          fm_ty=Syntax.SType 0; fm_ctors=[] }))));
  ] in
  let* () = List.fold_left (fun acc test -> let* () = acc in test ()) (Ok ()) extras in
  Ok (Global.StringMap.cardinal globals.families, Global.StringMap.cardinal globals.entries,
      List.length computations, List.length negative_cases + List.length extras)

let () =
  let root = Array.to_list Sys.argv |> List.to_seq |> Seq.drop 1 |> Seq.uncons
    |> Option.map fst |> Option.value ~default:"." in
  Result.fold (suite root)
    ~ok:(fun (families, entries, computations, negatives) ->
      Printf.printf "PRENEX-FAMILIES-OK families=%d entries=%d computations=%d negatives=%d\n"
        families entries computations negatives)
    ~error:(fun message -> Printf.printf "PRENEX-FAMILIES-FAIL %s\n" message; exit 1)
