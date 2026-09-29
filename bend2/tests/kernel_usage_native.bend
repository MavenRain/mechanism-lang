import Base
import ../kernel/quantity.bend as Q
import ../kernel/check_engine.bend as Engine

# Regression: Bend 2.0.25 native C miscomputed the former nested Bool.or/pick
# binder predicate for a dynamic linear identity. Keep close_valid's explicit
# cases unless this matrix and the real linear checker pass on both backends.

def quantity(value: Nat) -> Q.T:
  match value:
    case 0n: Q.QZero{}
    case 1n: Q.QOne{}
    case other: Q.QMany{}

def usage(kind: Nat, +level: Nat) -> Q.Usage:
  match kind:
    case 0n: Q.empty()
    case 1n: Q.occurrence(level,Q.QOne{})
    case 2n: Q.sequence(Q.occurrence(level,Q.QOne{}),Q.occurrence(level,Q.QOne{}))
    case 3n: Q.alternative(Q.empty(),Q.occurrence(level,Q.QOne{}))
    case 4n: Q.alternative(Q.occurrence(level,Q.QOne{}),Q.occurrence(level,Q.QMany{}))
    case other: Q.unreachable()

def expected_use(affine: Bool, kind: Nat) -> Bool:
  match affine kind:
    case _ 1n: True{}
    case _ 5n: True{}
    case True{} 0n: True{}
    case True{} 3n: True{}
    case _ _: False{}

def expected(outer: Bool, mode: Nat, binder: Nat, affine: Bool, kind: Nat) -> Bool:
  match outer mode binder:
    case True{} _ _: True{}
    case False{} 0n _: True{}
    case False{} _ 0n: True{}
    case False{} _ 2n: True{}
    case False{} _ _: expected_use(affine,kind)

def same(a: Bool, b: Bool) -> Bool:
  match a b:
    case True{} True{}: True{}
    case False{} False{}: True{}
    case _ _: False{}

def trial(+index: Nat, +level: Nat) -> Bool:
  +kind = Nat.mod(index,6n)
  +affine = Nat.is_eq(Nat.mod(Nat.div(index,6n),2n),1n)
  +binder = Nat.mod(Nat.div(index,12n),3n)
  +mode = Nat.mod(Nat.div(index,36n),3n)
  +outer = Nat.is_eq(Nat.div(index,108n),1n)
  same(Engine.close_valid(outer,quantity(mode),quantity(binder),affine,level,usage(kind,level)),expected(outer,mode,binder,affine,kind))

law selected:
  for valid: Bool
  for index: Nat
  for level: Nat
  Maybe<&2,Nat>

@unsafe def walk(+left: Nat, +level: Nat) -> Maybe<&2,Nat>:
  match left:
    case 0n: None{}
    case 1n+index:
      selected(trial(index,level),index,level)

@unsafe def selected(valid,index,level):
  match valid:
    case True{}: walk(index,level)
    case False{}: Some{index}

def finish(value: Maybe<&2,Nat>) -> IO(Unit):
  match value:
    case None{}: IO.print("KERNEL-USAGE-NATIVE-OK cases=216")
    case Some{index}: IO.die(Unit,1,"usage matrix failed case " ++ Nat.show(index))

def parsed(value: Maybe<&2,Nat>) -> IO(Unit):
  match value:
    case None{}: IO.die(Unit,2,"invalid dynamic level")
    case Some{level}: finish(walk(216n,level))

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Con{text,Nil{}}: parsed(Nat.read(text))
    case other: IO.die(Unit,2,"expected dynamic level")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
