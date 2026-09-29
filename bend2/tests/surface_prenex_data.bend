import Base

# Original source compositions and diagnostics, with provenance in dev/bend2/prenex-source-data.json.
type Negative is Data:
  Negative{name: String, kind: String, detail: String, good: Maybe<&2, String>, source: String}

type Positive is Data:
  Positive{source: String, rows: List<&2, String>}

type ParseCase is Data:
  ParseCase{name: String, source: String, expected: String}

def prenex_identity() -> String:
  "poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\n"

def prenex_tinySource() -> String:
  "mu Tiny : Type 0 with\n| tinyZero : Tiny\n"

def prenex_negatives() -> List<&2, Negative>:
  [Negative{"duplicate-binder", "parse", "duplicate universe u", Some{"poly (u, v) def p : Sort (succ u) := Sort u"}, "poly (u, u) def p : Sort (succ u) := Sort u"},
   Negative{"unbound-level", "parse", "unbound universe v", Some{"poly (u) def p : Sort (succ u) := Sort u"}, "poly (u) def p : Sort (succ u) := Sort v"},
   Negative{"scope-leak", "parse", "unbound universe u", Some{"poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\ndef p : Sort 1 := Prop"}, "poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\ndef p : Sort u := Prop"},
   Negative{"term-as-level", "parse", "unbound universe A", Some{"def p : (0 A : Type 0) -> Type 1 := fun (0 A : Type 0) => Type 0"}, "def p : (0 A : Type 0) -> Type 1 := fun (0 A : Type 0) => Sort A"},
   Negative{"open-instance", "parse", "unbound universe u", Some{"poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\nspecialize identity (1) as closed"}, "poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\nspecialize identity (u) as closed"},
   Negative{"wrong-arity", "universe", "the schema identity expects 1 universe arguments, got 2", Some{"poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\nspecialize identity (1) as closed"}, "poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\nspecialize identity (1, 2) as closed"},
   Negative{"unknown-template", "unbound", "unknown universe schema absent", Some{"poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\nspecialize identity (1) as closed"}, "poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\nspecialize absent (1) as closed"},
   Negative{"bare-template", "unbound", "identity", Some{"poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\nspecialize identity (1) as closed def p : (0 A : Type 0) -> A -> A := closed"}, "poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\ndef p : (0 A : Type 0) -> A -> A := identity"},
   Negative{"duplicate-template", "mismatch", "the name identity is already declared", Some{"poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\n"}, "poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\npoly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\n"},
   Negative{"global-collision", "mismatch", "the name closed is already declared", Some{"poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\nspecialize identity (1) as closed"}, "poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\ndef closed : Type 1 := Type 0 specialize identity (1) as closed"},
   Negative{"template-collision", "mismatch", "the name identity is already declared", Some{"poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\ndef closed : Type 1 := Type 0"}, "poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\ndef identity : Type 1 := Type 0"},
   Negative{"instance-template-collision", "mismatch", "the name identity is already declared", Some{"poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\nspecialize identity (1) as closed"}, "poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\nspecialize identity (1) as identity"},
   Negative{"family-collision", "mismatch", "the name identity is already declared", Some{"poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\nmu Fresh : Prop with | fresh : Fresh"}, "poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\nmu identity : Prop with | fresh : identity"},
   Negative{"universal-check", "mismatch", "the term has type Type 1 and the expected type is Type (u0 + 1)", Some{"poly (u) def p : Sort (succ u) := Sort u"}, "poly (u) def p : Sort (succ u) := Prop"},
   Negative{"no-poly-axiom", "parse", "expected a nonrecursive definition or single family after universe binders", Some{"poly (u) def p : Sort (succ u) := Sort u"}, "poly (u) axiom p : Sort u"},
   Negative{"no-poly-mutual", "parse", "expected a nonrecursive definition or single family after universe binders", Some{"poly (u) def p : Sort (succ u) := Sort u"}, "poly (u) mutual mu P : Prop with | p : P mu Q : Prop with | q : Q end"},
   Negative{"no-poly-recursion", "parse", "expected a nonrecursive definition or single family after universe binders", Some{"poly (u) def p : Sort (succ u) := Sort u"}, "poly (u) def rec p : Sort (succ u) := Sort u"},
   Negative{"reserved-universe-name", "parse", "reserved universe name max", Some{"poly (u) def p : Sort (succ u) := Sort u"}, "poly (max) def p : Sort (succ max) := Sort max"},
   Negative{"poly-shape", "parse", "expected '(' and a nonempty list of universe binders after 'poly'", Some{"poly (u) def p : Sort (succ u) := Sort u"}, "poly u def p : Sort (succ u) := Sort u"},
   Negative{"specialize-shape", "parse", "expected NAME, '(' with a nonempty list of universe levels", Some{"poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\nspecialize identity (1) as closed"}, "poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\nspecialize identity as later"},
   Negative{"constructor-template-collision", "mismatch", "the name tinyZero is already declared", Some{"mu Tiny : Type 0 with\n| tinyZero : Tiny\npoly (u) def fresh : Sort (succ u) := Sort u"}, "mu Tiny : Type 0 with\n| tinyZero : Tiny\npoly (u) def tinyZero : Sort (succ u) := Sort u"},
   Negative{"constructor-collision", "mismatch", "the name identity is already declared", Some{"poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\nmu Fresh : Prop with | fresh : Fresh"}, "poly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\nmu Fresh : Prop with | identity : Fresh"},
   Negative{"constructor-instance-collision", "mismatch", "the name tinyZero is already declared", Some{"mu Tiny : Type 0 with\n| tinyZero : Tiny\npoly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\nspecialize identity (1) as closed"}, "mu Tiny : Type 0 with\n| tinyZero : Tiny\npoly (u) def identity : (0 A : Sort u) -> A -> A :=\n  fun (0 A : Sort u) (x : A) => x\nspecialize identity (1) as tinyZero"}]

def prenex_positives() -> List<&2, Positive>:
  []

def prenex_parsers() -> List<&2, ParseCase>:
  []

def prenex_families_box() -> String:
  "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A "

def prenex_families_identity() -> String:
  "poly (u) def identity : Sort (succ u) := Sort u "

def prenex_families_plain() -> String:
  "mu Tiny : Type 0 with | zero : Tiny "

def prenex_families_instance() -> String:
  "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A specialize Box (1) as DataBox "

def prenex_families_negatives() -> List<&2, Negative>:
  [Negative{"scope", "parse", "unbound universe v", Some{"poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A "}, "poly (u) mu Box (0 A : Sort v) : Sort (succ u) with | box : A -> Box A"},
   Negative{"scope-leak", "parse", "unbound universe u", Some{"poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A specialize Box (1) as DataBox "}, "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A specialize Box (u) as DataBox"},
   Negative{"arity", "universe", "the family schema Box expects 1 universe arguments, got 2", Some{"poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A specialize Box (1) as DataBox "}, "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A specialize Box (1, 2) as DataBox"},
   Negative{"unstable-universe", "universe", "a family schema must remain in Prop or Type for every universe substitution", Some{"poly (u) mu Empty : Sort (succ u) with"}, "poly (u) mu Empty : Sort u with"},
   Negative{"constructor-result", "mismatch", "a constructor of Box ends at another type", Some{"poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A "}, "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> A"},
   Negative{"duplicate-constructor", "mismatch", "the constructor box is already declared in Box", Some{"poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A "}, "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A | box : (0 A : Sort u) -> Box A"},
   Negative{"bare-family", "unbound", "Box", Some{"poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A specialize Box (1) as DataBox def T : Type 1 := DataBox (prod () : Type 0)"}, "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A def T : Type 1 := Box (prod () : Type 0)"},
   Negative{"bare-constructor", "unbound", "box", Some{"mu Tiny : Type 0 with | zero : Tiny poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A specialize Box (1) as DataBox def x : DataBox Tiny := box zero"}, "mu Tiny : Type 0 with | zero : Tiny poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A def x : Tiny := box zero"},
   Negative{"duplicate-template", "mismatch", "the name Box is already declared", Some{"poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A "}, "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A "},
   Negative{"definition-template-collision", "mismatch", "the name identity is already declared", Some{"poly (u) def identity : Sort (succ u) := Sort u poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A "}, "poly (u) def identity : Sort (succ u) := Sort u poly (u) mu identity : Prop with"},
   Negative{"family-template-collision", "mismatch", "the name Box is already declared", Some{"poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A poly (u) def identity : Sort (succ u) := Sort u "}, "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A poly (u) def Box : Sort (succ u) := Sort u"},
   Negative{"ordinary-definition-collision", "mismatch", "the name Box is already declared", Some{"poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A def X : Type 0 := Prop"}, "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A def Box : Type 0 := Prop"},
   Negative{"ordinary-family-collision", "mismatch", "the name Box is already declared", Some{"poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A mu Tiny : Type 0 with | zero : Tiny "}, "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A mu Box : Type 0 with"},
   Negative{"ordinary-constructor-collision", "mismatch", "the name Box is already declared", Some{"poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A mu Tiny : Type 0 with | zero : Tiny "}, "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A mu Tiny : Type 0 with | Box : Tiny"},
   Negative{"late-constructor-collision", "mismatch", "the name box is already declared", Some{"poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A specialize Box (1) as DataBox poly (u) def identity : Sort (succ u) := Sort u "}, "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A poly (u) def box : Sort (succ u) := Sort u specialize Box (1) as DataBox"},
   Negative{"template-constructor-collision", "mismatch", "the name identity is already declared", Some{"poly (u) def identity : Sort (succ u) := Sort u poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A "}, "poly (u) def identity : Sort (succ u) := Sort u poly (u) mu Box : Type 0 with | identity : Box specialize Box (1) as DataBox"},
   Negative{"instance-template-collision", "mismatch", "the name Box is already declared", Some{"poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A specialize Box (1) as DataBox "}, "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A specialize Box (1) as Box"},
   Negative{"instance-definition-template", "mismatch", "the name identity is already declared", Some{"poly (u) def identity : Sort (succ u) := Sort u poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A specialize Box (1) as DataBox "}, "poly (u) def identity : Sort (succ u) := Sort u poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A specialize Box (1) as identity"},
   Negative{"definition-instance-family-template", "mismatch", "the name Box is already declared", Some{"poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A poly (u) def identity : Sort (succ u) := Sort u specialize identity (1) as X"}, "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A poly (u) def identity : Sort (succ u) := Sort u specialize identity (1) as Box"},
   Negative{"instance-family-collision", "mismatch", "the name Tiny is already declared", Some{"mu Tiny : Type 0 with | zero : Tiny poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A specialize Box (1) as DataBox "}, "mu Tiny : Type 0 with | zero : Tiny poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A specialize Box (1) as Tiny"},
   Negative{"instance-constructor-collision", "mismatch", "the name zero is already declared", Some{"mu Tiny : Type 0 with | zero : Tiny poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A specialize Box (1) as DataBox "}, "mu Tiny : Type 0 with | zero : Tiny poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A specialize Box (1) as zero"},
   Negative{"existing-constructor-template", "mismatch", "the name zero is already declared", Some{"mu Tiny : Type 0 with | zero : Tiny poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A "}, "mu Tiny : Type 0 with | zero : Tiny poly (u) mu zero : Type 0 with"},
   Negative{"groups", "parse", "expected a nonrecursive definition or single family after universe binders, found 'poly'", Some{"poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A "}, "poly (u) mutual mu P : Prop with mu Q : Prop with end"},
   Negative{"late-family-template-collision", "mismatch", "the name box is already declared", Some{"poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A specialize Box (1) as DataBox poly (u) def identity : Sort (succ u) := Sort u "}, "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A poly (u) mu box (0 A : Sort u) : Sort (succ u) with | mk : A -> box A specialize Box (1) as DataBox"},
   Negative{"instance-own-constructor", "mismatch", "the name box is already declared", Some{"poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A specialize Box (1) as DataBox "}, "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A specialize Box (1) as box"},
   Negative{"self-named-constructor", "mismatch", "the constructor Box repeats the family name Box", Some{"poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A "}, "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | Box : A -> Box A"}]

def prenex_families_positives() -> List<&2, Positive>:
  []

def prenex_families_parsers() -> List<&2, ParseCase>:
  []

def prenex_groups_first() -> String:
  "poly (u) mu First : Sort (succ u) with | first : First "

def prenex_groups_second() -> String:
  "and Second : Sort (succ u) with | second : First -> Second "

def prenex_groups_members() -> String:
  "where def witness : Second := second first def alias : Second := witness end "

def prenex_groups_group() -> String:
  "poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end "

def prenex_groups_instance() -> String:
  "poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end specialize First (0) as One "

def prenex_groups_negatives() -> List<&2, Negative>:
  [Negative{"forward-family", "rendered", "unbound: Second", None{}, "poly (u) mu First : Sort (succ u) with | first : Second -> First and Second : Sort (succ u) with | second : First -> Second "},
   Negative{"forward-member", "rendered", "unbound: later", None{}, "poly (u) mu First : Sort (succ u) with | first : First where def witness : First := later def later : First := first end"},
   Negative{"recursive-member", "rendered", "unbound: witness", None{}, "poly (u) mu First : Sort (succ u) with | first : First where def witness : First := witness end"},
   Negative{"duplicate-family", "rendered", "mismatch: the family First is already declared", None{}, "poly (u) mu First : Sort (succ u) with | first : First and First : Sort (succ u) with"},
   Negative{"duplicate-member", "rendered", "mismatch: the name witness is already declared", None{}, "poly (u) mu First : Sort (succ u) with | first : First where def witness : First := first def witness : First := first end"},
   Negative{"member-family-name", "rendered", "mismatch: the name First is already declared", None{}, "poly (u) mu First : Sort (succ u) with | first : First where def First : First := first end"},
   Negative{"member-label-name", "rendered", "mismatch: the name first is already declared", None{}, "poly (u) mu First : Sort (succ u) with | first : First where def first : First := first end"},
   Negative{"companion-definition-template", "rendered", "mismatch: the name Second is already declared", None{}, "poly (u) def Second : Sort (succ u) := Sort u poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end "},
   Negative{"member-definition-template", "rendered", "mismatch: the name witness is already declared", None{}, "poly (u) def witness : Sort (succ u) := Sort u poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end "},
   Negative{"member-family-template", "rendered", "mismatch: the name witness is already declared", None{}, "poly (u) mu witness : Prop with poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end "},
   Negative{"unstable-companion", "rendered", "universe: a family schema must remain in Prop or Type for every universe substitution", None{}, "poly (u) mu First : Sort (succ u) with | first : First and Second : Sort u with"},
   Negative{"bad-member-type", "rendered", "mismatch: the term has type Type 1 and the expected type is Type 0", None{}, "poly (u) mu First : Sort (succ u) with | first : First where def bad : Sort 0 := Sort 0 end"},
   Negative{"symbolic-family-escape", "rendered", "unbound: Second", None{}, "poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end def escape : Type 0 := Second"},
   Negative{"symbolic-member-escape", "rendered", "unbound: witness", None{}, "poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end def escape : Type 0 := witness"},
   Negative{"wrong-arity", "rendered", "universe: the family schema First expects 1 universe arguments, got 2", None{}, "poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end specialize First (0, 1) as One"},
   Negative{"companion-template-specialization", "rendered", "unbound: unknown universe schema Second", None{}, "poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end specialize Second (0) as One"},
   Negative{"companion-template-target", "rendered", "mismatch: the name One_Second is already declared", None{}, "poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end poly (u) def One_Second : Sort (succ u) := Sort u specialize First (0) as One"},
   Negative{"member-template-target", "rendered", "mismatch: the name One_witness is already declared", None{}, "poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end poly (u) def One_witness : Sort (succ u) := Sort u specialize First (0) as One"},
   Negative{"member-family-template-target", "rendered", "mismatch: the name One_witness is already declared", None{}, "poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end poly (u) mu One_witness : Prop with specialize First (0) as One"},
   Negative{"companion-global-target", "rendered", "mismatch: the name One_Second is already declared", None{}, "poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end def One_Second : Type 0 := Prop specialize First (0) as One"},
   Negative{"member-global-target", "rendered", "mismatch: the name One_witness is already declared", None{}, "poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end def One_witness : Type 0 := Prop specialize First (0) as One"},
   Negative{"companion-constructor-target", "rendered", "mismatch: the name One_Second is already declared", None{}, "poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end mu Other : Type 0 with | One_Second : Other specialize First (0) as One"},
   Negative{"member-constructor-target", "rendered", "mismatch: the name One_witness is already declared", None{}, "poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end mu Other : Type 0 with | One_witness : Other specialize First (0) as One"},
   Negative{"companion-label-definition-template", "rendered", "mismatch: the name second is already declared", None{}, "poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end poly (u) def second : Sort (succ u) := Sort u specialize First (0) as One"},
   Negative{"companion-label-family-template", "rendered", "mismatch: the name second is already declared", None{}, "poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end poly (u) mu second : Prop with specialize First (0) as One"},
   Negative{"generated-companion-label", "rendered", "mismatch: the name One_Second is already declared", None{}, "poly (u) mu First : Sort (succ u) with | One_Second : First and Second : Sort (succ u) with | second : First -> Second specialize First (0) as One"},
   Negative{"generated-member-label", "rendered", "mismatch: the name One_witness is already declared", None{}, "poly (u) mu First : Sort (succ u) with | One_witness : First where def witness : First := One_witness end specialize First (0) as One"},
   Negative{"late-label-instance-name", "rendered", "mismatch: the name One is already declared", None{}, "poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end specialize First (0) as One poly (u) mu Holder : Sort (succ u) with | One : Holder specialize Holder (0) as Two"},
   Negative{"late-label-generated-member", "rendered", "mismatch: the name One_witness is already declared", None{}, "poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end specialize First (0) as One poly (u) mu Holder : Sort (succ u) with | One_witness : Holder specialize Holder (0) as Two"},
   Negative{"companion-constructor-name", "rendered", "mismatch: the name Second is already declared", None{}, "mu Other : Type 0 with | Second : Other poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second where def witness : Second := second first def alias : Second := witness end "}]

def prenex_groups_positives() -> List<&2, Positive>:
  [Positive{"poly (u) mu First : Sort (succ u) with | mark : First and Second : Sort (succ u) with | mark : First -> Second where def witness : Second := mark mark end specialize First (0) as One def value : One_Second := One_witness", []},
   Positive{"poly (u) mu P : Prop with | p : P and Q : Prop with | q : P -> Q where def witness : Q := q p end specialize P (0) as Proof def value : Proof_Q := Proof_witness", []},
   Positive{"poly (u) mu First : Sort (succ u) with | first : First and Second : Sort (succ u) with | second : First -> Second def ordinary : Type 0 := Prop specialize First (0) as One", []}]

def prenex_groups_parsers() -> List<&2, ParseCase>:
  [ParseCase{"empty-members", "poly (u) mu First : Sort (succ u) with | first : First where end", "expected a nonempty list of member definitions, found 'end'"},
   ParseCase{"postulated-member", "poly (u) mu First : Sort (succ u) with | first : First where axiom x : First end", "expected 'def' or 'end' in family members, found 'axiom'"},
   ParseCase{"recursive-syntax", "poly (u) mu First : Sort (succ u) with | first : First where def rec x : First := first end", "expected a nonrecursive member definition, found 'def'"},
   ParseCase{"missing-end", "poly (u) mu First : Sort (succ u) with | first : First where def x : First := first", "expected 'def' or 'end' in family members, found end of input"},
   ParseCase{"member-scope", "poly (u) mu First : Sort (succ u) with | first : First where def x : Sort (succ v) := Sort v end", "unbound universe v"}]

def prenex_exports_group() -> String:
  "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end "

def prenex_exports_specialization() -> String:
  "specialize Box (0) as One "

def prenex_exports_mapping() -> String:
  "export (alias := chosenAlias, witness := chosenWitness) "

def prenex_exports_instance() -> String:
  "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One export (alias := chosenAlias, witness := chosenWitness) "

def prenex_exports_composed() -> String:
  "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local def pick : Local := Local_witness end "

def prenex_exports_composed_instance() -> String:
  "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local def pick : Local := Local_witness end specialize Pair (0) as Inst export (pick := chosenPick, Local_alias := chosenLocalAlias, Local_witness := chosenWitness) "

def prenex_exports_negatives() -> List<&2, Negative>:
  [Negative{"missing", "rendered", "mismatch: every family member needs an export name", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One export (witness := chosenWitness)"},
   Negative{"composed-own-members-only", "rendered", "mismatch: every family member needs an export name", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local def pick : Local := Local_witness end specialize Pair (0) as Inst export (pick := chosenPick)"},
   Negative{"empty", "rendered", "mismatch: every family member needs an export name", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One export ()"},
   Negative{"empty-spaced", "rendered", "mismatch: every family member needs an export name", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One export ( )"},
   Negative{"duplicate-source", "rendered", "mismatch: member exports must name each member once", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One export (witness := first, witness := second)"},
   Negative{"unknown-source", "rendered", "mismatch: member exports must name each member once", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One export (absent := first, alias := second)"},
   Negative{"duplicate-target", "rendered", "mismatch: member exports repeat the target same", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One export (witness := same, alias := same)"},
   Negative{"instance-name", "rendered", "mismatch: the name One is already declared", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One export (witness := One, alias := chosenAlias)"},
   Negative{"companion-name", "rendered", "mismatch: the name One_Other is already declared", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One export (witness := One_Other, alias := chosenAlias)"},
   Negative{"constructor", "rendered", "mismatch: the name box is already declared", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One export (witness := box, alias := chosenAlias)"},
   Negative{"companion-constructor", "rendered", "mismatch: the name other is already declared", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One export (witness := other, alias := chosenAlias)"},
   Negative{"ambient-global", "rendered", "mismatch: the name chosenWitness is already declared", None{}, "def chosenWitness : Type 0 := Prop poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One export (alias := chosenAlias, witness := chosenWitness) "},
   Negative{"ambient-family", "rendered", "mismatch: the name chosenWitness is already declared", None{}, "mu chosenWitness : Type 0 with poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One export (alias := chosenAlias, witness := chosenWitness) "},
   Negative{"ambient-constructor", "rendered", "mismatch: the name chosenWitness is already declared", None{}, "mu Ambient : Type 0 with | chosenWitness : Ambient poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One export (alias := chosenAlias, witness := chosenWitness) "},
   Negative{"definition-catalog", "rendered", "mismatch: the name chosenWitness is already declared", None{}, "poly (v) def chosenWitness : Sort (succ v) := Sort v poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One export (alias := chosenAlias, witness := chosenWitness) "},
   Negative{"family-catalog", "rendered", "mismatch: the name chosenWitness is already declared", None{}, "poly (v) mu chosenWitness : Sort (succ v) with poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One export (alias := chosenAlias, witness := chosenWitness) "},
   Negative{"reused-family", "rendered", "mismatch: the name Shared is already declared", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as Shared specialize Box (0) as One with (Box := Shared, Other := Shared_Other) export (witness := Shared, alias := chosenAlias)"},
   Negative{"reused-instance-name", "rendered", "mismatch: the name One is already declared", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as Shared specialize Box (0) as One with (Box := Shared, Other := Shared_Other) export (witness := One, alias := chosenAlias)"},
   Negative{"definition-template", "rendered", "not yet: member exports require a family template", None{}, "poly (u) def identity : Sort (succ u) := Sort u specialize identity (0) as concrete export ()"},
   Negative{"unknown-template", "rendered", "unbound: unknown universe schema Missing", None{}, "specialize Missing (0) as concrete export ()"},
   Negative{"default-name-absent", "rendered", "unbound: One_witness", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One export (alias := chosenAlias, witness := chosenWitness) def value : One := One_witness"}]

def prenex_exports_positives() -> List<&2, Positive>:
  [Positive{"poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One export (alias := chosenAlias, witness := chosenWitness) ", []},
   Positive{"def One_witness : Type 0 := Prop poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One export (alias := chosenAlias, witness := chosenWitness) ", []},
   Positive{"poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One ", []},
   Positive{"poly (u) mu Bare : Sort (succ u) with specialize Bare (0) as Empty export ()", []},
   Positive{"poly (u) mu Bare : Sort (succ u) with specialize Bare (0) as Empty export ( )", []},
   Positive{"def export : Type 0 := Prop def value : Type 0 := export", []},
   Positive{"poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as One export (witness := alias, alias := witness) def value : One := witness", []},
   Positive{"poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local def pick : Local := Local_witness end specialize Pair (0) as Inst export (pick := chosenPick, Local_alias := chosenLocalAlias, Local_witness := chosenWitness) ", []}]

def prenex_exports_parsers() -> List<&2, ParseCase>:
  [ParseCase{"missing-list", "specialize Box (0) as One export", "expected '(' after 'export'"},
   ParseCase{"missing-source", "specialize Box (0) as One export (:= target)", "expected a member export"},
   ParseCase{"missing-assignment", "specialize Box (0) as One export (witness target)", "expected a member export"},
   ParseCase{"missing-target", "specialize Box (0) as One export (witness :=)", "expected a global name after ':=', found ')'"},
   ParseCase{"keyword-target", "specialize Box (0) as One export (witness := def, alias := a)", "expected a global name after ':=', found 'def'"},
   ParseCase{"trailing-comma", "specialize Box (0) as One export (witness := target,)", "expected a member export"},
   ParseCase{"missing-close", "specialize Box (0) as One export (witness := target", "expected ',' or ')' after a member export"},
   ParseCase{"with-after-export", "specialize Box (0) as One export (witness := w, alias := a) with (Box := Shared)", "'with' must precede 'export' in a specialization"},
   ParseCase{"second-export", "specialize Box (0) as One export (witness := w) export (alias := a)", "one export clause per specialization"},
   ParseCase{"dependency-second-export", "specialize Box (0) as One poly (v) group Pair where specialize Box (v) as Inner export () export () end", "one export clause per specialization"}]

def prenex_dependency_exports_seed() -> String:
  "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end "

def prenex_dependency_exports_dependency() -> String:
  "specialize Box (v) as Local "

def prenex_dependency_exports_mapping() -> String:
  "export (alias := a, witness := w) "

def prenex_dependency_exports_complete() -> String:
  "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (alias := a, witness := w) def pick : Local := a end "

def prenex_dependency_exports_instance() -> String:
  "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (alias := a, witness := w) def pick : Local := a end specialize Pair (0) as Run "

def prenex_dependency_exports_negatives() -> List<&2, Negative>:
  [Negative{"missing", "rendered", "mismatch: every family member needs an export name", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (witness := w) end "},
   Negative{"empty", "rendered", "mismatch: every family member needs an export name", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export () end "},
   Negative{"duplicate-source", "rendered", "mismatch: member exports must name each member once", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (witness := w, witness := a) end "},
   Negative{"unknown-source", "rendered", "mismatch: member exports must name each member once", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (absent := w, alias := a) end "},
   Negative{"duplicate-target", "rendered", "mismatch: member exports repeat the target same", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (witness := same, alias := same) end "},
   Negative{"alias", "rendered", "mismatch: the name Local is already declared", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (witness := Local, alias := a) end "},
   Negative{"family", "rendered", "mismatch: the name Local_Other is already declared", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (witness := Local_Other, alias := a) end "},
   Negative{"constructor", "rendered", "mismatch: the name box is already declared", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (witness := box, alias := a) end "},
   Negative{"companion-constructor", "rendered", "mismatch: the name other is already declared", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (witness := other, alias := a) end "},
   Negative{"group", "rendered", "mismatch: the name Pair is already declared", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (witness := Pair, alias := a) end "},
   Negative{"own-member", "rendered", "mismatch: the name w is already declared", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (alias := a, witness := w) def w : Local := a end "},
   Negative{"poly-name", "rendered", "mismatch: the name w is already declared", None{}, "poly (u) def w : Sort (succ u) := Sort u poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (alias := a, witness := w)  end "},
   Negative{"existing-global", "rendered", "mismatch: the name w is already declared", None{}, "def w : Type := Nat poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (alias := a, witness := w)  end "},
   Negative{"earlier-alias", "rendered", "mismatch: the name First is already declared", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as First specialize Box (v) as Local export (witness := First, alias := a) end "},
   Negative{"earlier-composed-alias", "rendered", "mismatch: the name First is already declared", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (u) group Wrapper where specialize Box (u) as Wrapped end poly (v) group Pair where specialize Wrapper (v) as First specialize Box (v) as Local export (witness := First, alias := a) end "},
   Negative{"later-alias", "rendered", "mismatch: the name Later is already declared", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (witness := Later, alias := a) specialize Box (v) as Later end "},
   Negative{"earlier-member", "rendered", "mismatch: the name First_witness is already declared", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as First specialize Box (v) as Local export (witness := First_witness, alias := a) end "},
   Negative{"reused-family", "rendered", "mismatch: the name First is already declared", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as First specialize Box (v) as Second with (Box := First, Other := First_Other) export (witness := First, alias := First) end "},
   Negative{"own-composed-alias", "rendered", "mismatch: the name First is already declared", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (u) group Wrapper where specialize Box (u) as Wrapped end poly (v) group Pair where specialize Wrapper (v) as First export (Wrapped_witness := First, Wrapped_alias := a) end "},
   Negative{"template-name", "rendered", "mismatch: the name Box is already declared", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (witness := Box, alias := a) end "},
   Negative{"installed-family", "rendered", "mismatch: the name Shared is already declared", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Box (0) as Shared poly (v) group Pair where specialize Box (v) as Local export (witness := Shared, alias := a) end "},
   Negative{"installed-constructor", "rendered", "mismatch: the name empty is already declared", None{}, "poly (u) mu Empty : Sort (succ u) with | empty : Empty poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end specialize Empty (0) as E poly (v) group Pair where specialize Box (v) as Local export (witness := empty, alias := a) end "},
   Negative{"unbound-reuse-with-export", "rendered", "unbound: unknown reused family Nope", None{}, "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local with (Box := Nope) export (witness := Nope, alias := a) end "}]

def prenex_dependency_exports_positives() -> List<&2, Positive>:
  [Positive{"poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (alias := a, witness := w) def pick : Local := a end ", ["Closed_w", "Closed_a", "Closed_pick"]},
   Positive{"poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (witness := Local_alias, alias := Local_witness) end ", ["Closed_Local_alias", "Closed_Local_witness"]},
   Positive{"poly (u) mu Empty : Sort (succ u) with | empty : Empty poly (v) group Pair where specialize Empty (v) as Local export () end ", []},
   Positive{"poly (u) mu Empty : Sort (succ u) with | empty : Empty poly (v) group Pair where specialize Empty (v) as Local export ( ) end ", []},
   Positive{"poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (witness := export, alias := a) def pick : Local := export end ", ["Closed_export", "Closed_a", "Closed_pick"]},
   Positive{"poly (u) mu Box : Sort (succ u) with | box : Box where def Ty : Sort (succ u) := Box def value : Ty := box end poly (v) group Pair where specialize Box (v) as Local export (value := w, Ty := Carrier) def pick : Carrier := w end ", ["Closed_Carrier", "Closed_w", "Closed_pick"]},
   Positive{"poly (u) mu Box : Sort (succ u) with | box : Box where def Ty : Sort (succ u) := Box def value : Ty := box end poly (v) group Pair where specialize Box (v) as Local export (Ty := value, value := Ty) def pick : value := Ty end ", ["Closed_value", "Closed_Ty", "Closed_pick"]}]

def prenex_dependency_exports_parsers() -> List<&2, ParseCase>:
  [ParseCase{"missing-open", "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export w end ", "expected '(' after 'export'"},
   ParseCase{"missing-target", "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (witness :=) end ", "expected a global name after ':='"},
   ParseCase{"trailing-comma", "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export (witness := w,) end ", "expected a member export 'MEMBER := NAME'"},
   ParseCase{"second-export", "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export () export () end ", "one export clause per specialization"},
   ParseCase{"with-after-export", "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local export () with (Box := First) end ", "'with' must precede 'export'"},
   ParseCase{"second-with", "poly (u) mu Box : Sort (succ u) with | box : Box and Other : Sort (succ u) with | other : Other where def witness : Box := box def alias : Box := witness end poly (v) group Pair where specialize Box (v) as Local with (Box := First) with (Other := Second) end ", "one 'with' clause per specialization"}]
