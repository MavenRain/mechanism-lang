import Base
import ./common.bend as Common
import ./error.bend as Error

# Finite state remains data. Callback state belongs to an affine runner.
type T is Data:
  Unlimited{}
  Polls{remaining: Nat}

def unlimited() -> T:
  Unlimited{}

def of_polls(remaining: Nat) -> T:
  Polls{remaining}

def poll(budget: T) -> Maybe<&2, T>:
  match budget:
    case Unlimited{}:
      Some{Unlimited{}}
    case Polls{0n}:
      None{}
    case Polls{1n+p}:
      Some{Polls{p}}

def exhausted(budget: T) -> Bool:
  match budget:
    case Polls{0n}:
      True{}
    case Unlimited{}:
      False{}
    case Polls{1n+p}:
      False{}

# A poll answer is true when exhausted, as in the original of_poll API.
# The continuation receives refusal too, so recover can observe tick failures.
type Step<-A: Data> is Type:
  Finished{result: Result<&2, &2, Error.T, A>}
  Poll{resume: Bool -> Step<A>}

# Compose result continuations once. Rewrapping every pending bind around
# each resumed poll makes deeply nested traversals quadratic.
def Continuation(-A: Data) -> Type:
  @-R: Data ->
  (Result<&2, &2, Error.T, A> -> Step<R>) -> Step<R>

type Computation<-A: Data> is Type:
  CompReady{value: A}
  CompError{error: Error.T}
  CompLater{action: Continuation(A)}

def Comp(-A: Data) -> Type:
  Computation<A>

def start_comp(-A: Data, -R: Data, action: Comp(A), done: Result<&2, &2, Error.T, A> -> Step<R>) -> Step<R>:
  match action:
    case CompReady{value}:
      done(Done{value})
    case CompError{error}:
      done(Fail{error})
    case CompLater{resume}:
      resume(R, done)

def pure(-A: Data, value: A) -> Comp(A):
  CompReady{value}

def failed(-A: Data, error: Error.T) -> Comp(A):
  CompError{error}

def bind_result(-A: Data, -B: Data, -R: Data, first: Result<&2, &2, Error.T, A>, next: A -> Comp(B), done: Result<&2, &2, Error.T, B> -> Step<R>) -> Step<R>:
  match first:
    case Fail{error}:
      done(Fail{error})
    case Done{value}:
      action = next(value)
      start_comp(B, R, action, done)

def bind(-A: Data, -B: Data, first: Comp(A), next: A -> Comp(B)) -> Comp(B):
  match first:
    case CompReady{value}:
      # A ready value needs no suspension. Only tick yields Poll, so the poll order is unchanged.
      next(value)
    case CompError{error}:
      CompError{error}
    case CompLater{action}:
      CompLater{R => done => action(R, result => bind_result(A, B, R, result, next, done))}

def tick_result(-R: Data, spent: Bool, message: String, done: Result<&2, &2, Error.T, Unit> -> Step<R>) -> Step<R>:
  match spent:
    case True{}:
      done(Fail{Error.Budget_exhausted{message}})
    case False{}:
      done(Done{Unit{}})

def tick_with(message: String) -> Comp(Unit):
  CompLater{R => done => Poll{spent => tick_result(R, spent, message, done)}}

def tick() -> Comp(Unit):
  tick_with("the check budget is exhausted")

def lift(-A: Data, result: Result<&2, &2, Error.T, A>) -> Comp(A):
  match result:
    case Done{value}:
      CompReady{value}
    case Fail{error}:
      CompError{error}

@unsafe def run_step(-A: Data, step: Step<A>, budget: T) -> Common.Pair<T, Result<&2, &2, Error.T, A>>:
  match step:
    case Finished{result}:
      Common.Pair{budget, result}
    case Poll{resume}:
      match budget:
        case Unlimited{}:
          run_step(A, resume(False{}), Unlimited{})
        case Polls{0n}:
          run_step(A, resume(True{}), Polls{0n})
        case Polls{1n+remaining}:
          run_step(A, resume(False{}), Polls{remaining})

def run_state(-A: Data, action: Comp(A), budget: T) -> Common.Pair<T, Result<&2, &2, Error.T, A>>:
  run_step(A, start_comp(A, A, action, result => Finished{result}), budget)

def discard(-A: Data, state: Common.Pair<T, Result<&2, &2, Error.T, A>>) -> Result<&2, &2, Error.T, A>:
  match state:
    case Common.Pair{budget, result}:
      result

def run(-A: Data, action: Comp(A), budget: T) -> Result<&2, &2, Error.T, A>:
  discard(A, run_state(A, action, budget))

def Comp.pure(-A: Data, value: A) -> Comp(A):
  pure(A, value)

def Comp.bind(-A: Data, -B: Data, first: Comp(A), next: A -> Comp(B)) -> Comp(B):
  bind(A, B, first, next)

def recover_result(-A: Data, -R: Data, result: Result<&2, &2, Error.T, A>, handler: Error.T -> Comp(A), done: Result<&2, &2, Error.T, A> -> Step<R>) -> Step<R>:
  match result:
    case Done{value}:
      done(Done{value})
    case Fail{error}:
      action = handler(error)
      start_comp(A, R, action, done)

def recover(-A: Data, action: Comp(A), handler: Error.T -> Comp(A)) -> Comp(A):
  match action:
    case CompReady{value}:
      CompReady{value}
    case CompError{error}:
      handler(error)
    case CompLater{resume}:
      CompLater{R => done => resume(R, result => recover_result(A, R, result, handler, done))}

def attempt(-A: Data, action: Comp(A)) -> Comp(Result<&2, &2, Error.T, A>):
  match action:
    case CompReady{value}:
      CompReady{Done{value}}
    case CompError{error}:
      CompReady{Fail{error}}
    case CompLater{resume}:
      CompLater{R => done => resume(R, result => done(Done{result}))}

# Each callback returns its successor, allowing private state without copying
# an affine closure. Runners return that successor even when checking fails.
type Callback is Type:
  Callback{poll: Unit -> Callback & Bool}

def of_poll(poll: Unit -> Callback & Bool) -> Callback:
  Callback{poll}

law run_poll_step:
  for -A: Data
  for step: Step<A>
  for callback: Callback
  Callback & Result<&2, &2, Error.T, A>

@unsafe def resume_poll(-A: Data, resume: Bool -> Step<A>, response: Callback & Bool) -> Callback & Result<&2, &2, Error.T, A>:
  (next, spent) = response
  run_poll_step(A, resume(spent), next)

@unsafe def run_poll_step(A, step, callback):
  match step:
    case Finished{result}:
      (callback, result)
    case Poll{resume}:
      match callback:
        case Callback{poll}:
          resume_poll(A, resume, poll(Unit{}))

def run_poll_state(-A: Data, action: Comp(A), callback: Callback) -> Callback & Result<&2, &2, Error.T, A>:
  run_poll_step(A, start_comp(A, A, action, result => Finished{result}), callback)

def discard_poll(-A: Data, state: Callback & Result<&2, &2, Error.T, A>) -> Result<&2, &2, Error.T, A>:
  (next, result) = state
  result

def run_poll(-A: Data, action: Comp(A), callback: Callback) -> Result<&2, &2, Error.T, A>:
  discard_poll(A, run_poll_state(A, action, callback))

type IOCallback is Type:
  IOCallback{poll: Unit -> IO(IOCallback & Bool)}

def of_poll_io(poll: Unit -> IO(IOCallback & Bool)) -> IOCallback:
  IOCallback{poll}

law run_io_step:
  for -A: Data
  for step: Step<A>
  for callback: IOCallback
  IO(IOCallback & Result<&2, &2, Error.T, A>)

@unsafe def resume_io(-A: Data, resume: Bool -> Step<A>, response: IOCallback & Bool) -> IO(IOCallback & Result<&2, &2, Error.T, A>):
  (next, spent) = response
  run_io_step(A, resume(spent), next)

@unsafe def run_io_step(A, step, callback):
  match step:
    case Finished{result}:
      IO.pure(IOCallback & Result<&2, &2, Error.T, A>, (callback, result))
    case Poll{resume}:
      match callback:
        case IOCallback{poll}:
          IO.bind(IOCallback & Bool, IOCallback & Result<&2, &2, Error.T, A>, poll(Unit{}), response => resume_io(A, resume, response))

def run_io_state(-A: Data, action: Comp(A), callback: IOCallback) -> IO(IOCallback & Result<&2, &2, Error.T, A>):
  run_io_step(A, start_comp(A, A, action, result => Finished{result}), callback)

def discard_io(-A: Data, state: IOCallback & Result<&2, &2, Error.T, A>) -> Result<&2, &2, Error.T, A>:
  (callback, result) = state
  result

def run_io(-A: Data, action: Comp(A), callback: IOCallback) -> IO(Result<&2, &2, Error.T, A>):
  IO.bind(IOCallback & Result<&2, &2, Error.T, A>, Result<&2, &2, Error.T, A>, run_io_state(A, action, callback), state => IO.pure(Result<&2, &2, Error.T, A>, discard_io(A, state)))
