import Base
import ../kernel/check_engine.bend as Engine
import ../kernel/context.bend as C
import ../kernel/global.bend as G
import ../kernel/value.bend as V
import ../kernel/level_var.bend as L
import ../kernel/quantity.bend as Q
import ../kernel/budget.bend as B
import ../kernel/common.bend as Pair
import ../kernel/error.bend as E

type Expected is Data:
  Missing{}
  UniverseVariable{index: Nat}

type Fixture is Data:
  Fixture{name: String, context: C.Ctx, level: Nat, expected: Expected}

def typed(index: Nat) -> V.T:
  V.VUniv{L.LVar{index}}

def empty() -> C.Ctx:
  C.make(G.empty(), B.unlimited(), 3n)

def one() -> C.Ctx:
  C.bind("oldest", Q.QMany{}, typed(0n), empty())

def three() -> C.Ctx:
  C.bind("newest", Q.QMany{}, typed(2n), C.bind("middle", Q.QMany{}, typed(1n), one()))

# Raw inconsistent contexts preserve the old missing-list behavior separately.
def raw(size: Nat, locals: List<&2, C.Local>) -> C.Ctx:
  C.Ctx{G.empty(), [], C.from_list(locals), size, B.unlimited(), 3n, False{}}

def cases() -> List<&2, Fixture>:
  [Fixture{"size1-equal", one(), 1n, Missing{}},
   Fixture{"size1-greater", one(), 2n, Missing{}},
   Fixture{"size1-max-level", one(), Nat.read.max(), Missing{}},
   Fixture{"size0-empty", empty(), 0n, Missing{}},
   Fixture{"size0-extra-local", raw(0n, [C.Local{"extra", Q.QMany{}, typed(2n)}]), 0n, Missing{}},
   Fixture{"size0-extra-local-greater", raw(0n, [C.Local{"extra", Q.QMany{}, typed(2n)}]), 1n, Missing{}},
   Fixture{"size3-equal", three(), 3n, Missing{}},
   Fixture{"size3-greater", three(), 4n, Missing{}},
   Fixture{"size1-valid", one(), 0n, UniverseVariable{0n}},
   Fixture{"size3-oldest", three(), 0n, UniverseVariable{0n}},
   Fixture{"size3-middle", three(), 1n, UniverseVariable{1n}},
   Fixture{"size3-newest", three(), 2n, UniverseVariable{2n}},
   Fixture{"size3-missing-list", raw(3n, []), 0n, Missing{}},
   Fixture{"size3-short-list-present", raw(3n, [C.Local{"newest", Q.QMany{}, typed(2n)}]), 2n, UniverseVariable{2n}}]

def result_matches(expected: Expected, result: Result<&2, &2, E.T, V.T>) -> Bool:
  match expected result:
    case Missing{} Fail{E.Unbound{message}}:
      String.eq(message, "a local level is outside the context")
    case UniverseVariable{index} Done{V.VUniv{L.LVar{actual}}}:
      Nat.is_eq(index, actual)
    case _ _:
      False{}

def finite_result_matches(same_fuel: Bool, expected: Expected, result: Result<&2, &2, E.T, V.T>) -> Bool:
  match same_fuel:
    case True{}: result_matches(expected, result)
    case False{}: False{}

def finite_matches(fuel: Nat, expected: Expected, state: Pair.Pair<B.T, Result<&2, &2, E.T, V.T>>) -> Bool:
  match state:
    case Pair.Pair{B.Polls{remaining}, result}:
      finite_result_matches(Nat.is_eq(fuel, remaining), expected, result)
    case _:
      False{}

@unsafe def scripted(answers: List<&2, Bool>) -> B.Callback:
  match answers:
    case Nil{}: B.of_poll(unit => (scripted([]), True{}))
    case Con{answer, rest}: B.of_poll(unit => (scripted(rest), answer))

def polled_answer(state: B.Callback & Bool) -> Bool:
  (next, answer) = state
  answer

def next_answer(callback: B.Callback) -> Bool:
  match callback:
    case B.Callback{poll}:
      polled_answer(poll(Unit{}))

def callback_result_matches(same_result: Bool, callback: B.Callback) -> Bool:
  match same_result:
    case True{}: next_answer(callback)
    case False{}: False{}

def callback_matches(expected: Expected, state: B.Callback & Result<&2, &2, E.T, V.T>) -> Bool:
  (callback, result) = state
  callback_result_matches(result_matches(expected, result), callback)

def require(ok: Bool, name: String) -> IO(Unit):
  match ok:
    case True{}: IO.pure(Unit, Unit{})
    case False{}: IO.die(Unit, 1, String.append("HEAD-TY-INVALID-LEVEL FAIL ", name))

@unsafe def run_case(fixture: Fixture) -> IO(Unit):
  match fixture:
    case Fixture{+name, +context, +level, +expected}:
      do IO<Unit>:
        unlimited : Unit <- require(result_matches(expected, B.run(V.T, Engine.head_ty(context, V.HLocal{level}), B.unlimited())), String.append(name, "/unlimited"))
        zero : Unit <- require(finite_matches(0n, expected, B.run_state(V.T, Engine.head_ty(context, V.HLocal{level}), B.Polls{0n})), String.append(name, "/fuel0"))
        one : Unit <- require(finite_matches(1n, expected, B.run_state(V.T, Engine.head_ty(context, V.HLocal{level}), B.Polls{1n})), String.append(name, "/fuel1"))
        require(callback_matches(expected, B.run_poll_state(V.T, Engine.head_ty(context, V.HLocal{level}), scripted([True{}, False{}]))), String.append(name, "/callback"))

@unsafe def run_cases(fixtures: List<&2, Fixture>) -> IO(Unit):
  match fixtures:
    case Nil{}: IO.print("HEAD-TY-INVALID-LEVEL OK cases=14 observations=56")
    case Con{fixture, rest}:
      do IO<Unit>:
        checked : Unit <- run_case(fixture)
        run_cases(rest)

@unsafe def main() -> IO(Unit):
  run_cases(cases())
