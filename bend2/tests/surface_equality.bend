import Base
import ../kernel/check.bend as Check
import ../kernel/check_engine.bend as Engine
import ../kernel/global.bend as Global
import ../kernel/positivity.bend as P
import ../kernel/term.bend as Term
import ../kernel/quantity.bend as Q
import ../kernel/level.bend as Level
import ../kernel/level_var.bend as LevelVar
import ../kernel/bignum.bend as Bignum
import ../kernel/budget.bend as B
import ../kernel/common.bend as Common
import ../kernel/error.bend as Error

# The named assertions and refusal prefixes mirror test/equality.ml.
def require(ok: Bool, message: String) -> Result<&2, &2, Error.T, Unit>:
  Bool.pick(Result<&2, &2, Error.T, Unit>, ok, Done{Unit{}}, Fail{Error.Mismatch{message}})

def prefix(expected: String, actual: String) -> Bool:
  match expected actual:
    case SNil{} _:
      True{}
    case SCon{x, xs} SCon{y, ys}:
      Char.is_eq(x, y) && prefix(xs, ys)
    case _ _:
      False{}

def refusal(-A: Data, +expected: Error.T, result: Result<&2, &2, Error.T, A>) -> Result<&2, &2, Error.T, Unit>:
  match result:
    case Done{value}:
      Fail{Error.Mismatch{"expected refusal: " ++ Error.to_string(expected)}}
    case Fail{+actual}:
      require(prefix(Error.to_string(expected), Error.to_string(actual)), "wrong refusal: " ++ Error.to_string(actual))

def accepted(-A: Data, result: Result<&2, &2, Error.T, A>) -> Result<&2, &2, Error.T, Unit>:
  match result:
    case Done{value}: Done{Unit{}}
    case Fail{error}: Fail{error}

def params(carrier: Level.T) -> P.Telescope:
  [P.Binder{Q.QZero{}, "A", Term.Univ{carrier}}, P.Binder{Q.QZero{}, "x", Term.Var{0n}}]

def anchored(quantity: Q.T, carrier: Level.T, sort: Level.T) -> Check.FamilyDecl:
  Check.FamilyDecl{"AnchoredEquality", params(carrier), [P.Binder{quantity, "y", Term.Var{1n}}], sort}

def declared(family: Check.FamilyDecl) -> Result<&2, &2, Error.T, Global.T>:
  Check.declare_family(Global.empty(), B.unlimited(), family)

def family_positive(family: P.Family) -> Bool:
  match family:
    case P.Family{name, params, indices, level, status, ctors, positive}:
      positive

def equality_metadata(family: Maybe<&2, P.Family>) -> Result<&2, &2, Error.T, Unit>:
  match family:
    case None{}:
      Fail{Error.Mismatch{"checked equality family disappeared"}}
    case Some{+family}:
      do Result<&2, &2, Error.T, Unit>:
        checked : Unit <- require(family_positive(family), "equality lost its positivity verdict")
        require(Engine.mu_zero_eliminable(family), "nullary equality lost large elimination")

def checked_equality(carrier: Level.T) -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    globals : Global.T <- declared(anchored(Q.QZero{}, carrier, Level.zero()))
    globals : Global.T <- Check.define_ctors(globals, B.unlimited(), ["AnchoredEquality"], "AnchoredEquality",
      [Check.CtorDecl{"anchoredRefl", [], [Term.Var{1n}, Term.Var{0n}], [Term.Var{0n}]}])
    equality_metadata(Global.find_family("AnchoredEquality", globals))

def highest(value: Maybe<&2, Bignum.T>) -> Result<&2, &2, Error.T, Unit>:
  match value:
    case Some{number}:
      checked_equality(LevelVar.LSucc{number, Level.zero()})
    case None{}:
      Fail{Error.Mismatch{"nonnegative level construction failed"}}

def simple(index: Term.T) -> Check.FamilyDecl:
  Check.FamilyDecl{"PropositionIndex", [], [P.Binder{Q.QZero{}, "index", index}], Level.zero()}

def hidden_ctors(level: Level.T, quantity: Q.T) -> Result<&2, &2, Error.T, Global.T>:
  do Result<&2, &2, Error.T, Global.T>:
    globals : Global.T <- declared(Check.FamilyDecl{"HiddenData", [P.Binder{Q.QZero{}, "A", Term.Univ{Level.one()}}], [], level})
    Check.define_ctors(globals, B.unlimited(), ["HiddenData"], "HiddenData",
      [Check.CtorDecl{"hideData", [P.Binder{quantity, "value", Term.Var{0n}}], [Term.Var{1n}], []}])

def field_bound(quantity: Q.T) -> Result<&2, &2, Error.T, Unit>:
  refusal(Global.T, Error.Universe{"a field of hideData exceeds its family universe"}, hidden_ctors(Level.zero(), quantity))

def positive(family: Maybe<&2, P.Family>) -> Result<&2, &2, Error.T, Unit>:
  match family:
    case None{}: Fail{Error.Mismatch{"accepted data family disappeared"}}
    case Some{family}: require(family_positive(family), "accepted data family lost its positivity verdict")

def field_at_bound() -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    globals : Global.T <- hidden_ctors(Level.one(), Q.QMany{})
    positive(Global.find_family("HiddenData", globals))

def index_must_be_type() -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    context : Check.Ctx <- B.run(Check.Ctx, Check.check_telescope(params(Level.one()), Check.make(Global.empty(), B.unlimited(), 0n)), B.unlimited())
    refusal(Unit, Error.Universe{"a term used as a type is not a universe:"},
      B.run(Unit, Check.index_rules(context, "AnchoredEquality", Level.zero(), P.Binder{Q.QZero{}, "index", Term.Var{0n}}), B.unlimited()))

@unsafe def countdown(remaining: Nat) -> B.Callback:
  match remaining:
    case 0n:
      B.of_poll(unit => (countdown(0n), True{}))
    case 1n+rest:
      B.of_poll(unit => (countdown(rest), False{}))

def index_action(sort: Level.T, carrier: Level.T) -> B.Comp(Unit):
  Check.index_rules(Check.make(Global.empty(), B.unlimited(), 0n), "BudgetedIndex", sort,
    P.Binder{Q.QZero{}, "index", Term.Univ{carrier}})

def budget_error() -> Error.T:
  Error.Budget_exhausted{"the check budget is exhausted"}

def index_budget() -> Result<&2, &2, Error.T, Unit>:
  refusal(Unit, budget_error(), B.run_poll(Unit, index_action(Level.zero(), Level.one()), countdown(0n)))

def measured(state: Common.Pair<B.T, Result<&2, &2, Error.T, Unit>>) -> Result<&2, &2, Error.T, Nat>:
  match state:
    case Common.Pair{B.Polls{remaining}, Done{unit}}:
      Done{Nat.sub(100000n, remaining)}
    case Common.Pair{budget, Fail{error}}:
      Fail{error}
    case other:
      Fail{Error.Mismatch{"finite index poll measurement disappeared"}}

def poll_answer(response: B.Callback & Bool) -> Bool:
  (next, exhausted) = response
  exhausted

def next_poll(callback: B.Callback) -> Bool:
  match callback:
    case B.Callback{poll}:
      poll_answer(poll(Unit{}))

def exact_callback_boundary(state: B.Callback & Result<&2, &2, Error.T, Unit>) -> Result<&2, &2, Error.T, Unit>:
  (callback, result) = state
  do Result<&2, &2, Error.T, Unit>:
    checked : Unit <- result
    require(next_poll(callback), "Prop index used fewer callback polls than the measured path")

def prop_skips_level_poll() -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    +allowance : Nat <- measured(B.run_state(Unit, index_action(Level.zero(), Level.zero()), B.of_polls(100000n)))
    checked : Unit <- exact_callback_boundary(B.run_poll_state(Unit, index_action(Level.zero(), Level.zero()), countdown(allowance)))
    refusal(Unit, budget_error(), B.run_poll(Unit, index_action(Level.one(), Level.zero()), countdown(allowance)))

type Case is Type:
  Case{name: String, run: Unit -> Result<&2, &2, Error.T, Unit>}

def cases() -> List<&1, Case>:
  [Case{"prop-data-index", unit => checked_equality(Level.one())},
   Case{"prop-high-universe-index", unit => highest(Bignum.of_decimal("4611686018427387903"))},
   Case{"prop-type-index", unit => accepted(Global.T, declared(simple(Term.Univ{Level.one()})))},
   Case{"type-index-at-bound", unit => accepted(Global.T, declared(anchored(Q.QZero{}, Level.one(), Level.one())))},
   Case{"type-index-above-bound", unit => refusal(Global.T, Error.Index_above_universe{"the index y of AnchoredEquality lives at"}, declared(anchored(Q.QZero{}, Level.succ(Level.one()), Level.one())))},
   Case{"prop-runtime-index", unit => refusal(Global.T, Error.Index_not_zero{"the index y of AnchoredEquality is at quantity"}, declared(anchored(Q.QMany{}, Level.one(), Level.zero())))},
   Case{"prop-unbound-index-type", unit => refusal(Global.T, Error.Unbound{"de Bruijn index 0 is outside the context"}, declared(simple(Term.Var{0n})))},
   Case{"prop-index-must-be-type", unit => index_must_be_type()},
   Case{"prop-erased-data-field", unit => field_bound(Q.QZero{})},
   Case{"prop-runtime-data-field", unit => field_bound(Q.QMany{})},
   Case{"data-field-at-bound", unit => field_at_bound()},
   Case{"prop-index-refuses-free-universe", unit => refusal(Global.T, Error.Universe{"universe level is outside the global parameter scope"}, declared(simple(Term.Univ{Level.var(0n)})))},
   Case{"family-refuses-free-universe", unit => refusal(Global.T, Error.Universe{"family universe levels must be closed at M0 Stage A"}, declared(anchored(Q.QZero{}, Level.one(), Level.var(0n))))},
   Case{"index-type-honors-budget", unit => index_budget()},
   Case{"prop-index-skips-level-poll", unit => prop_skips_level_poll()}]

def report(name: String, result: Result<&2, &2, Error.T, Unit>) -> IO(Nat):
  match result:
    case Done{unit}:
      do IO<Nat>:
        IO.print("PASS equality " ++ name)
        return 0n
    case Fail{error}:
      do IO<Nat>:
        IO.print("FAIL equality " ++ name ++ ": " ++ Error.to_string(error))
        return 1n

def run(cases: List<&1, Case>, failures: Nat) -> IO(Unit):
  match cases:
    case Nil{}:
      match failures:
        case 0n: IO.print("EQUALITY-OK cases=15")
        case other: IO.die(Unit, 1, "equality assertions failed")
    case Con{Case{name, test}, tail}:
      do IO<Unit>:
        failed : Nat <- report(name, test(Unit{}))
        run(tail, (failures + failed : Nat))

def main() -> IO(Unit):
  run(cases(), 0n)
