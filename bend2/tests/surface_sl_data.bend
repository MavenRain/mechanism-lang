import Base

type Action is Data:
  RoundTrip{source: String}
  ParseRefusal{source: String, expected: String}
  SameTree{first: String, second: String}
  SameChecked{first: String, second: String}
  CheckedRefusal{source: String, expected: String}

type Case is Data:
  Case{name: String, action: Action}

def cases() -> List<&2, Case>:
  [
    Case{"constructor-sugar", SameTree{"mu N : Type 0 := | zero : N | succ (n : N) : N\n", "mu N : Type 0 with | zero : N | succ : (n : N) -> N\n"}},
    Case{"mutual-sugar", SameTree{"mutual mu A : Type 0 := | a (b : B) : A mu B : Type 0 := | b : B end", "mu A : Type 0 with | a : (b : B) -> A and B : Type 0 with | b : B"}},
    Case{"match-elaboration", SameChecked{"mu N : Type 0 := | zero : N | succ (n : N) : N\ndef pred : N -> N := fun (n : N) => match n as x in N return N with | zero => zero | succ (p : N) => p", "mu N : Type 0 with | zero : N | succ : (n : N) -> N\ndef pred : N -> N := fun (n : N) => case n as x in N return N with | zero => zero | succ p => p"}},
    Case{"mu-roundtrip", RoundTrip{"mu N : Type 0 := | zero : N | succ (n : N) : N\n"}},
    Case{"legacy-mu-roundtrip", RoundTrip{"mu N : Type 0 with | zero : N | succ : (n : N) -> N\n"}},
    Case{"mutual-roundtrip", RoundTrip{"mutual mu A : Type 0 := | a (b : B) : A mu B : Type 0 := | b : B end"}},
    Case{"legacy-mutual-roundtrip", RoundTrip{"mu A : Type 0 with | a : (b : B) -> A and B : Type 0 with | b : B"}},
    Case{"match-roundtrip", RoundTrip{"mu N : Type 0 := | zero : N | succ (n : N) : N\ndef pred : N -> N := fun (n : N) => match n as x in N return N with | zero => zero | succ (p : N) => p"}},
    Case{"case-roundtrip", RoundTrip{"mu N : Type 0 with | zero : N | succ : (n : N) -> N\ndef pred : N -> N := fun (n : N) => case n as x in N return N with | zero => zero | succ p => p"}},
    Case{"mixed-fields-roundtrip", RoundTrip{"def f : Nat := match n as x in N return Nat with | c (0 i : N) x (1 y : Nat) => y"}},
    Case{"nested-match-roundtrip", RoundTrip{"def f : Nat := match n as x in N return Nat with | z => 0 | s p => (match p as y in N return Nat with | z => 1 | s q => 2)"}},
    Case{"empty-match-roundtrip", RoundTrip{"mu Void : Prop := def exFalso : Void -> Type 0 := fun (v : Void) => match v as x in Void return Type 0 with"}},
    Case{"case-numeric-roundtrip", RoundTrip{"def f : Nat := case b with | 0 (x : Nat) => x | 1 (x : Nat) => x"}},
    Case{"match-numeric-refusal", ParseRefusal{"def f : Nat := match n with | 0 (x : Nat) => x", "a match branch keys a constructor, not a leg number"}},
    Case{"mutual-empty-refusal", ParseRefusal{"mutual end", "a mutual group needs at least two mu declarations"}},
    Case{"mutual-singleton-refusal", ParseRefusal{"mutual mu N : Type 0 := end", "a mutual group needs at least two mu declarations"}},
    Case{"mutual-unclosed-refusal", ParseRefusal{"mutual mu A : Type 0 := mu B : Type 0 :=", "expected 'mu' or 'end' in a mutual group, found end of input"}},
    Case{"mutual-and-refusal", ParseRefusal{"mutual mu A : Type 0 := and B : Type 0 := end", "expected 'mu' or 'end' in a mutual group, found 'and'"}},
    Case{"grouped-def", SameTree{"def f (0 A B : Type 0) (x y : Nat) (1 z : Nat) : Nat := natAdd x (natAdd y z)", "def f (0 A : Type 0) (0 B : Type 0) (x : Nat) (y : Nat) (1 z : Nat) : Nat := natAdd x (natAdd y z)"}},
    Case{"grouped-fun", SameTree{"def f : (1 x y : Nat) -> Nat := fun (1 x y : Nat) => natAdd x y", "def f : (1 x : Nat) -> (1 y : Nat) -> Nat := fun (1 x : Nat) (1 y : Nat) => natAdd x y"}},
    Case{"grouped-star", SameTree{"axiom p : (x y : Nat) * Nat", "axiom p : (x : Nat) * (y : Nat) * Nat"}},
    Case{"grouped-family", SameTree{"mu P (0 A B : Type 0) : Type 0 := | pair (x y : A) (z : B) : P A B", "mu P (0 A : Type 0) (0 B : Type 0) : Type 0 := | pair (x : A) (y : A) (z : B) : P A B"}},
    Case{"grouped-branch", SameTree{"def f : Nat := match p with | pair (1 x y : Nat) z => natAdd x (natAdd y z)", "def f : Nat := match p with | pair (1 x : Nat) (1 y : Nat) z => natAdd x (natAdd y z)"}},
    Case{"grouped-branch-annotation", CheckedRefusal{"mu Tag : Type 0 := | tag : Tag\nmu Box : Type 0 := | box (m : Nat) (t : Tag) : Box\ndef bad : Box -> Nat := fun (b : Box) => match b as self in Box return Nat with | box (m t : Nat) => 0", "the annotation of constructor field t differs from its declared type"}},
    Case{"grouped-case", SameTree{"def f : Nat := case p with | 0 (x y : Nat) => natSub x y", "def f : Nat := case p with | 0 (x : Nat) (y : Nat) => natSub x y"}},
    Case{"grouped-roundtrip", RoundTrip{"def f (0 A B : Type 0) (1 x y : Nat) : Nat := natAdd x y"}},
    Case{"grouped-dependent-check", SameChecked{"def f (0 A : Type 0) (x y : A) : A := x", "def f (0 A : Type 0) (x : A) (y : A) : A := x"}},
    Case{"grouped-linear-check", SameChecked{"def f (1 x y : Nat) : Nat := let used : Nat := x in y", "def f (1 x : Nat) (1 y : Nat) : Nat := let used : Nat := x in y"}},
    Case{"grouped-annotation-fallback", SameTree{"axiom f : Nat -> Nat def n : Nat := (f 3 : Nat)", "axiom f : Nat -> Nat def n : Nat := ((f 3) : Nat)"}},
    Case{"grouped-name-annotation-fallback", SameTree{"axiom f : Nat -> Nat axiom x : Nat def n : Nat := (f x : Nat)", "axiom f : Nat -> Nat axiom x : Nat def n : Nat := ((f x) : Nat)"}},
    Case{"grouped-empty-refusal", ParseRefusal{"def f (0 : Nat) : Nat := 0", "expected a binder name and ':', found ':'"}},
    Case{"grouped-missing-colon-refusal", ParseRefusal{"def f (x y) : Nat := 0", "expected ':', found ')'"}},
    Case{"grouped-missing-close-refusal", ParseRefusal{"def f (x y : Nat := 0", "expected ')', found ':='"}},
    Case{"grouped-zk-expansion", SameTree{"axiom P : zk (0 x y : Nat) * prod ()", "axiom P : zk (0 x : Nat) * (0 y : Nat) * prod ()"}},
    Case{"nu-term-refusal", ParseRefusal{"def f : Type 0 := nu", "nu arrives at M2"}},
    Case{"nu-declaration-refusal", ParseRefusal{"nu N : Type 0 :=", "nu arrives at M2"}}
  ]
