import Base
import ../surface/elab.bend as Elab
import ../surface/parser.bend as Parser
import ../surface/syntax.bend as Syntax
import ../kernel/common.bend as Common
import ../kernel/global.bend as Global
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as Error

def ensure(ok: Bool, message: String) -> Result<&2, &2, Error.T, Unit>:
  Bool.pick(Result<&2, &2, Error.T, Unit>, ok, Done{Unit{}}, Fail{Error.Mismatch{message}})

def accepted(result: Result<&2, &2, Error.T, Elab.Rows>) -> Result<&2, &2, Error.T, Unit>:
  match result:
    case Done{rows}:
      Done{Unit{}}
    case Fail{error}:
      Fail{error}

def rejected(result: Result<&2, &2, Error.T, Elab.Rows>, source: String) -> Result<&2, &2, Error.T, Unit>:
  match result:
    case Done{rows}:
      Fail{Error.Mismatch{"unexpected program acceptance: " ++ source}}
    case Fail{error}:
      Done{Unit{}}

def accepts(source: String) -> Result<&2, &2, Error.T, Unit>:
  accepted(Elab.check_text(Global.initial(), Budget.unlimited(), source))

def rejects(+source: String) -> Result<&2, &2, Error.T, Unit>:
  rejected(Elab.check_text(Global.initial(), Budget.unlimited(), source), source)

def rejected_with(result: Result<&2, &2, Error.T, Elab.Rows>, +expected: String) -> Result<&2, &2, Error.T, Unit>:
  match result:
    case Done{rows}:
      Fail{Error.Mismatch{"expected rejection: " ++ expected}}
    case Fail{+error}:
      ensure(String.eq(expected, Error.to_string(error)), "expected " ++ expected ++ "; actual " ++ Error.to_string(error))

def rejects_with(source: String, message: String) -> Result<&2, &2, Error.T, Unit>:
  rejected_with(Elab.check_text(Global.initial(), Budget.unlimited(), source), Error.to_string(Error.Mismatch{message}))

def budget_error(result: Result<&2, &2, Error.T, Elab.Checked>) -> Bool:
  match result:
    case Fail{Error.Budget_exhausted{message}}:
      True{}
    case other:
      False{}

def expect_budget(result: Result<&2, &2, Error.T, Elab.Checked>, source: String, count: Nat) -> Result<&2, &2, Error.T, Unit>:
  match result:
    case Fail{Error.Budget_exhausted{message}}:
      Done{Unit{}}
    case Fail{error}:
      Fail{Error.Mismatch{"budget boundary " ++ Nat.show(count) ++ " returned " ++ Error.to_string(error) ++ " for " ++ source}}
    case Done{checked}:
      Fail{Error.Mismatch{"budget boundary " ++ Nat.show(count) ++ " accepted " ++ source}}

def measured(state: Common.Pair<Budget.T, Result<&2, &2, Error.T, Elab.Checked>>) -> Result<&2, &2, Error.T, Nat>:
  match state:
    case Common.Pair{Budget.Polls{remaining}, Done{checked}}:
      Done{Nat.sub(10000n, remaining)}
    case Common.Pair{budget, Fail{error}}:
      Fail{error}
    case other:
      Fail{Error.Mismatch{"measurement lost budget state"}}

def budget_boundary(+source: String) -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    +decls : List<&2, Syntax.Decl> <- Parser.program(source)
    action : Budget.Comp(Elab.Checked) = Elab.elab_program_comp(Global.initial(), decls)
    +used : Nat <- measured(Budget.run_state(Elab.Checked, action, Budget.of_polls(10000n)))
    checked : Unit <- ensure(Bool.not(Nat.is_eq(used, 0n)), "program consumed no trusted-check polls")
    checked : Elab.Checked <- Elab.elab_program_in(Global.initial(), Budget.of_polls(used), decls)
    expect_budget(Elab.elab_program_in(Global.initial(), Budget.of_polls(Nat.sub(used, 1n)), decls), source, used)

def box() -> String:
  "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A where def unbox : (0 A : Sort u) -> Box A -> A := fun (0 A : Sort u) (b : Box A) => match b as self in Box return A with | box (x : A) => x end\n"

def suite() -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    checked : Unit <- accepts("def x : Nat := 3 def y : Nat := natAdd x 4")
    checked : Unit <- accepts("mu Flag : Type 0 with | off : Flag | on : Flag def flag : Flag := on def choose : Flag -> Nat := fun (x : Flag) => case x as self in Flag return Nat with | off => 0 | on => 1")
    checked : Unit <- accepts("poly (u) def identity : (0 A : Sort u) -> A -> A := fun (0 A : Sort u) (x : A) => x specialize identity (1) as id def x : Nat := id Nat 9")
    checked : Unit <- accepts(box() ++ "specialize Box (1) as Small export (unbox := get) def x : Nat := get Nat (box 37)")
    checked : Unit <- accepts(box() ++ "poly (u) group Bundle where specialize Box (u) as Inner end specialize Bundle (1) as Run def x : Nat := Run_Inner_unbox Nat (box 37)")
    checked : Unit <- accepts(box() ++ "specialize Box (1) as First specialize Box (1) as Second with (Box := First) def x : Nat := Second_unbox Nat (box 37)")
    checked : Unit <- rejects_with("def x : Nat := 1 def x : Nat := 2", "the name x is already declared")
    checked : Unit <- accepts("def g : Nat := 1 poly (u) mu Box : Sort (succ u) with | box : Box where def m : Nat := g end specialize Box (0) as B")
    checked : Unit <- rejects_with("def g : Nat := 1 poly (u) mu Box : Sort (succ u) with | box : Box where def m : Nat := g end def g : Nat := 2 specialize Box (0) as B", "the name g is already declared")
    checked : Unit <- rejects_with("mu N : Type 0 with | z : N | s : N -> N def g : N := z def rec g : N -> N := fun (n : N) => match n as self in N return N with | z => z | s (p : N) => s (g p)", "the name g is already declared")
    checked : Unit <- accepts("mu N : Type 0 with | z : N | s : N -> N def rec id : N -> N := fun (n : N) => match n as self in N return N with | z => z | s (p : N) => s (id p) def result : N := id (s z)")
    checked : Unit <- rejects("def rec loop : Nat -> Nat := fun (n : Nat) => loop n")
    checked : Unit <- rejects("poly (u) def identity : (0 A : Sort u) -> A -> A := fun (0 A : Sort u) (x : A) => x specialize identity () as bad")
    checked : Unit <- rejects(box() ++ "specialize Box (1) as Small export ()")
    checked : Unit <- rejects(box() ++ "mu Wrong : Type 0 with | wrong : Wrong specialize Box (1) as Small with (Box := Wrong)")
    checked : Unit <- budget_boundary("def x : Nat := 3 def y : Nat := natAdd x 4")
    budget_boundary(box() ++ "specialize Box (1) as Small def x : Nat := Small_unbox Nat (box 37)")

def report(result: Result<&2, &2, Error.T, Unit>) -> IO(Unit):
  match result:
    case Done{unit}:
      IO.print("PASS surface declarations, templates, composition, reuse and shared budget")
    case Fail{error}:
      IO.die(Unit, 1, Error.to_string(error))

def main() -> IO(Unit):
  report(suite())
