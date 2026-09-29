import Base
import ../kernel/budget.bend as B
import ../kernel/common.bend as C
import ../kernel/error.bend as E

@unsafe def scripted(answers: List<&2, Bool>) -> B.Callback:
  match answers:
    case Nil{}:
      B.of_poll(unit => (scripted([]), True{}))
    case Con{answer, tail}:
      B.of_poll(unit => (scripted(tail), answer))

@unsafe def scripted_io(answers: List<&2, Bool>) -> B.IOCallback:
  match answers:
    case Nil{}:
      B.of_poll_io(unit => IO.pure(B.IOCallback & Bool, (scripted_io([]), True{})))
    case Con{answer, tail}:
      B.of_poll_io(unit => IO.pure(B.IOCallback & Bool, (scripted_io(tail), answer)))

# The driver owns clock reads. Cancellation is observed at a kernel poll;
# this does not preempt work performed between polls.
@unsafe def deadline(+expires: Nat) -> B.IOCallback:
  B.of_poll_io(unit => do IO<B.IOCallback & Bool>:
    now : Nat <- IO.now()
    return (deadline(expires), Nat.is_le(expires, now)))

@unsafe def ticks(count: Nat) -> B.Comp(Unit):
  match count:
    case 0n:
      B.pure(Unit, Unit{})
    case 1n+rest:
      do B.Comp<Unit>:
        tick : Unit <- B.tick()
        ticks(rest)

def success(-A: Data, result: Result<&2, &2, E.T, A>) -> Bool:
  match result:
    case Done{value}: True{}
    case Fail{error}: False{}

def refused(-A: Data, result: Result<&2, &2, E.T, A>) -> Bool:
  match result:
    case Fail{E.Budget_exhausted{message}}: True{}
    case other: False{}

def answer(response: B.Callback & Bool) -> Bool:
  (next, spent) = response
  spent

def next_answer(callback: B.Callback) -> Bool:
  match callback:
    case B.Callback{poll}: answer(poll(Unit{}))

def succeeded_at_next_poll(state: B.Callback & Result<&2, &2, E.T, Unit>) -> Bool:
  (callback, result) = state
  success(Unit, result) && next_answer(callback)

def refused_at_next_poll(state: B.Callback & Result<&2, &2, E.T, Unit>) -> Bool:
  (callback, result) = state
  refused(Unit, result) && next_answer(callback)

def remaining(state: C.Pair<B.T, Result<&2, &2, E.T, Unit>>) -> Bool:
  match state:
    case C.Pair{B.Polls{2n}, Done{Unit{}}}: True{}
    case other: False{}

def recovered() -> B.Comp(Unit):
  B.recover(Unit, B.tick(), error => B.tick())

def after_failure() -> B.Comp(Unit):
  do B.Comp<Unit>:
    tick : Unit <- B.tick()
    B.failed(Unit, E.Unbound{"must not run"})

def attempted(result: Result<&2, &2, E.T, Result<&2, &2, E.T, Unit>>) -> Bool:
  match result:
    case Done{inner}: refused(Unit, inner)
    case Fail{error}: False{}

def continue_run(state: B.Callback & Result<&2, &2, E.T, Unit>) -> Bool:
  (callback, result) = state
  success(Unit, result) && refused_at_next_poll(B.run_poll_state(Unit, ticks(2n), callback))

def recovered_error() -> B.Comp(Unit):
  B.recover(Unit, B.failed(Unit, E.Unbound{"probe"}), error => B.tick())

def pure_suite() -> Bool:
  remaining(B.run_state(Unit, ticks(3n), B.of_polls(5n))) &&
  success(Unit, B.run(Unit, ticks(3n), B.of_polls(3n))) &&
  refused(Unit, B.run(Unit, ticks(3n), B.of_polls(2n))) &&
  succeeded_at_next_poll(B.run_poll_state(Unit, ticks(2n), scripted([False{}, False{}, True{}, False{}]))) &&
  succeeded_at_next_poll(B.run_poll_state(Unit, B.pure(Unit, Unit{}), scripted([True{}, False{}]))) &&
  refused(Unit, B.run_poll(Unit, ticks(1000000n), scripted([True{}, False{}]))) &&
  refused(Unit, B.run_poll(Unit, after_failure(), scripted([True{}, False{}]))) &&
  succeeded_at_next_poll(B.run_poll_state(Unit, recovered(), scripted([True{}, False{}, True{}, False{}]))) &&
  refused(Unit, B.run(Unit, recovered(), B.of_polls(0n))) &&
  succeeded_at_next_poll(B.run_poll_state(Unit, recovered_error(), scripted([False{}, True{}, False{}]))) &&
  attempted(B.run_poll(Result<&2, &2, E.T, Unit>, B.attempt(Unit, B.tick()), scripted([True{}]))) &&
  continue_run(B.run_poll_state(Unit, ticks(2n), scripted([False{}, False{}, False{}, True{}, True{}, False{}])))

def io_continue(state: B.IOCallback & Result<&2, &2, E.T, Unit>) -> IO(Bool):
  (callback, result) = state
  do IO<Bool>:
    after : Result<&2, &2, E.T, Unit> <- B.run_io(Unit, B.tick(), callback)
    return success(Unit, result) && refused(Unit, after)

def verify(ok: Bool) -> IO(Unit):
  match ok:
    case True{}: IO.print("PASS budget callbacks, exact polls, recovery, laziness and clock deadline")
    case False{}: IO.die(Unit, 1, "FAIL budget callback regression")

def main() -> IO(Unit):
  do IO<Unit>:
    +pure_ok : Bool = pure_suite()
    state : B.IOCallback & Result<&2, &2, E.T, Unit> <- B.run_io_state(Unit, recovered(), scripted_io([True{}, False{}, True{}]))
    io_ok : Bool <- io_continue(state)
    expires : Nat <- IO.now()
    expired : Result<&2, &2, E.T, Unit> <- B.run_io(Unit, ticks(1000000n), deadline(expires))
    verify(pure_ok && io_ok && refused(Unit, expired))
