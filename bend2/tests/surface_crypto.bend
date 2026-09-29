import Base
import ../surface/elab.bend as Elab
import ../kernel/global.bend as Global
import ../kernel/common.bend as Common
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as Error

# Standalone source fixtures from vendor/veil/test, with comment lines removed.
def cases() -> List<&2, Common.Pair<String, String>>:
  [
    Common.Pair{"host/zk-pack.kan", "\ndef Bool : Type 0 := sum ((prod () : Type 0), (prod () : Type 0))\n\ndef isTrue : (b : Bool) -> Prop :=\n  fun (b : Bool) =>\n    case b as z return Prop with\n    | 0 (x : prod ()) => sum ()\n    | 1 (y : prod ()) => prod ()\n\ndef preimageR : (y : Nat) -> (w : Nat) -> Prop :=\n  fun (y : Nat) (w : Nat) => isTrue (natEq (natMul w w) y)\n\ndef sqProof : (y : Nat) -> (1 w : Nat) -> (r : preimageR y w) ->\n    zk (0 w : Nat) * preimageR y w :=\n  fun (y : Nat) (1 w : Nat) (r : preimageR y w) => prove y w r\n\ndef main : Nat := natMul 3 3"},
    Common.Pair{"host/fhc-pack.kan", "\ndef Pk : Type 0 := Nat\ndef Sk : Type 0 := Nat\ndef pk : Pk := 0\ndef sk : Sk := 0\ndef seal : (t : Nat) -> fhc 0 Nat := fun (t : Nat) => enc pk t\ndef addEnc : (c : fhc 0 Nat) -> fhc 1 Nat :=\n  fun (c : fhc 0 Nat) => eval ((fun (x : Nat) => natAdd x 1) : (y : Nat) -> Nat) c\ndef reveal : (1 k : Sk) -> (c : fhc 1 Nat) -> Nat :=\n  fun (1 k : Sk) (c : fhc 1 Nat) => dec k c\n\ndef main : Nat := reveal sk (addEnc (seal 7))"},
    Common.Pair{"host/mpc-pack.kan", "\ndef Bool : Type 0 := sum ((prod () : Type 0), (prod () : Type 0))\n\ndef isTrue : (b : Bool) -> Prop :=\n  fun (b : Bool) =>\n    case b as z return Prop with\n    | 0 (x : prod ()) => sum ()\n    | 1 (y : prod ()) => prod ()\n\ndef Party : Prop := sum (prod (), prod ())\ndef alice : Party := inj 0 of 2 ()\ndef bob : Party := inj 1 of 2 ()\ndef allParties : prod (Nat, Nat) := tuple (1, 1)\n\ndef both : (q : prod (Nat, Nat)) -> Prop :=\n  fun (q : prod (Nat, Nat)) => isTrue (natEq (natAdd q.0 q.1) 2)\n\ndef secretA : Nat := 1\ndef secretB : Nat := 1\n\ndef jointSum : (1 a : Nat) -> (1 b : Nat) -> mpc[Party, fun (0 q : prod (Nat, Nat)) => isTrue (natEq (natAdd q.0 q.1) 2)] Nat :=\n  fun (1 a : Nat) (1 b : Nat) =>\n    mpc allParties (natAdd : (x : Nat) -> (y : Nat) -> Nat)\n      ((input alice a) : mpc[Party, fun (0 q : prod (Nat, Nat)) => isTrue (natEq (natAdd q.0 q.1) 2)] Nat)\n      ((input bob b) : mpc[Party, fun (0 q : prod (Nat, Nat)) => isTrue (natEq (natAdd q.0 q.1) 2)] Nat)\n\ndef joint : mpc[Party, fun (0 q : prod (Nat, Nat)) => isTrue (natEq (natAdd q.0 q.1) 2)] Nat := jointSum secretA secretB\n\ndef reveal : (0 h : both allParties) -> (1 c : mpc[Party, fun (0 q : prod (Nat, Nat)) => isTrue (natEq (natAdd q.0 q.1) 2)] Nat) -> Nat :=\n  fun (0 h : both allParties) (1 c : mpc[Party, fun (0 q : prod (Nat, Nat)) => isTrue (natEq (natAdd q.0 q.1) 2)] Nat) =>\n    open allParties h c\n\ndef opened : Nat := reveal (tuple ()) joint\n\ndef main : Nat := opened"},
    Common.Pair{"shapes/zk-pack.kan", "\ndef Bool : Type 0 := sum ((prod () : Type 0), (prod () : Type 0))\n\ndef isTrue : (b : Bool) -> Prop :=\n  fun (b : Bool) =>\n    case b as z return Prop with\n    | 0 (x : prod ()) => sum ()\n    | 1 (y : prod ()) => prod ()\n\ndef preimageR : (y : Nat) -> (w : Nat) -> Prop :=\n  fun (y : Nat) (w : Nat) => isTrue (natEq (natMul w w) y)\n\ndef sqProof : (y : Nat) -> (1 w : Nat) -> (r : preimageR y w) ->\n    zk (0 w : Nat) * preimageR y w :=\n  fun (y : Nat) (1 w : Nat) (r : preimageR y w) => prove y w r\n\ndef stmt : (y : Nat) -> (p : zk (0 w : Nat) * preimageR y w) -> Prop :=\n  fun (y : Nat) (p : zk (0 w : Nat) * preimageR y w) => (h : verify y p) -> prod ()\n\ndef betaConv : (y : Nat) -> (w : Nat) -> (r : preimageR y w) ->\n    (h : verify y (prove y w r : zk (0 w : Nat) * preimageR y w)) -> preimageR y w :=\n  fun (y : Nat) (w : Nat) (r : preimageR y w)\n      (h : verify y (prove y w r : zk (0 w : Nat) * preimageR y w)) => h\n\ndef ok : (y : Nat) -> (p : zk (0 w : Nat) * preimageR y w) -> Prop :=\n  fun (y : Nat) => fun (p : zk (0 w : Nat) * preimageR y w) => verify y p"},
    Common.Pair{"shapes/fhc-pack.kan", "\naxiom Pk : Type\naxiom Sk : Type\naxiom pk : Pk\naxiom sk : Sk\ndef seal : (t : Nat) -> fhc 0 Nat := fun (t : Nat) => enc pk t\ndef addEnc : (c : fhc 2 Nat) -> fhc 3 Nat :=\n  fun (c : fhc 2 Nat) => eval (fun (x : Nat) => natAdd x 1) c\ndef reveal : (1 k : Sk) -> (c : fhc 3 Nat) -> Nat :=\n  fun (1 k : Sk) (c : fhc 3 Nat) => dec k c"},
    Common.Pair{"shapes/mpc-pack.kan", "\ndef Bool : Type 0 := sum ((prod () : Type 0), (prod () : Type 0))\n\ndef isTrue : (b : Bool) -> Prop :=\n  fun (b : Bool) =>\n    case b as z return Prop with\n    | 0 (x : prod ()) => sum ()\n    | 1 (y : prod ()) => prod ()\n\ndef Party : Prop := sum (prod (), prod ())\ndef alice : Party := inj 0 of 2 ()\ndef bob : Party := inj 1 of 2 ()\ndef allParties : prod (Nat, Nat) := tuple (1, 1)\n\ndef both : (0 q : prod (Nat, Nat)) -> Prop :=\n  fun (0 q : prod (Nat, Nat)) => isTrue (natEq (natAdd q.0 q.1) 2)\n\naxiom secretA : Nat\naxiom secretB : Nat\n\ndef jointSum : (1 a : Nat) -> (1 b : Nat) -> mpc[Party, both] Nat :=\n  fun (1 a : Nat) (1 b : Nat) =>\n    mpc allParties (natAdd : (x : Nat) -> (y : Nat) -> Nat)\n      ((input alice a) : mpc[Party, both] Nat)\n      ((input bob b) : mpc[Party, both] Nat)\n\ndef joint : mpc[Party, both] Nat := jointSum secretA secretB\n\ndef reveal : (0 h : both allParties) -> (1 c : mpc[Party, both] Nat) -> Nat :=\n  fun (0 h : both allParties) (1 c : mpc[Party, both] Nat) =>\n    open allParties h c\n\ndef opened : Nat := reveal (tuple ()) joint"},
    Common.Pair{"fixtures/circuit-fhc.kan", "mu FhcLeaf : Type 0 :=\n| fhcLeaf : FhcLeaf\n\ndef namedLeaf : FhcLeaf := fhcLeaf\ndef leafAlias : FhcLeaf := namedLeaf\n\ndef leafCircuit (n : Nat) : Nat :=\n  match leafAlias as x in FhcLeaf return Nat with\n  | fhcLeaf => natMul (natAdd n 1) 2\n\ndef lift (c : fhc 0 Nat) : fhc 2 Nat := eval leafCircuit c"}]

def checked(result: Result<&2, &2, Error.T, Elab.Rows>, name: String) -> Result<&2, &2, Error.T, Unit>:
  match result:
    case Done{rows}:
      Done{Unit{}}
    case Fail{error}:
      Fail{Error.Mismatch{name ++ ": " ++ Error.to_string(error)}}

def run(cases: List<&2, Common.Pair<String, String>>) -> Result<&2, &2, Error.T, Unit>:
  match cases:
    case Nil{}:
      Done{Unit{}}
    case Con{Common.Pair{name, source}, tail}:
      do Result<&2, &2, Error.T, Unit>:
        ok : Unit <- checked(Elab.check_text(Global.initial(), Budget.unlimited(), source), name)
        run(tail)

def report(result: Result<&2, &2, Error.T, Unit>) -> IO(Unit):
  match result:
    case Done{unit}:
      IO.print("PASS seven original ZK, FHC, MPC and circuit frontend fixtures")
    case Fail{error}:
      IO.die(Unit, 1, Error.to_string(error))

def main() -> IO(Unit):
  report(run(cases()))
