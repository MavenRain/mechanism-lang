import Base
import ../kernel/bignum.bend as Bignum
import ../surface/parser.bend as Parser
import ../surface/syntax.bend as Syntax
import ../surface/universe.bend as Universe
import ../kernel/error.bend as Error
import ../kernel/quantity.bend as Quantity
import ../kernel/bignum.bend as Bignum

def accepted(result: Result<&2, &2, Error.T, Syntax.T>) -> Bool:
  match result:
    case Done{term}:
      True{}
    case Fail{error}:
      False{}

def rejected(result: Result<&2, &2, Error.T, Syntax.T>) -> Bool:
  Bool.not(accepted(result))

def arrow(result: Result<&2, &2, Error.T, Syntax.T>) -> Bool:
  match result:
    case Done{Syntax.SArrow{Syntax.Binder{Quantity.QZero{}, x, Syntax.SType{Bignum.Big{False{}, Nil{}}}}, Syntax.SArrow{Syntax.Binder{Quantity.QZero{}, y, Syntax.SType{Bignum.Big{False{}, Nil{}}}}, Syntax.SVar{body}}}}:
      String.eq(x, "A") && String.eq(y, "B") && String.eq(body, "A")
    case other:
      False{}

def projection(result: Result<&2, &2, Error.T, Syntax.T>) -> Bool:
  match result:
    case Done{Syntax.SApp{Syntax.SVar{head}, Syntax.SProj{Syntax.SVar{arg}, Bignum.Big{False{}, Con{10, Nil{}}}}}}:
      String.eq(head, "f") && String.eq(arg, "x")
    case other:
      False{}

def big(result: Result<&2, &2, Error.T, Syntax.T>) -> Bool:
  match result:
    case Done{Syntax.SNat{number}}:
      String.eq(Bignum.to_string(number), "123456789012345678901234567890")
    case other:
      False{}

def declarations(result: Result<&2, &2, Error.T, List<&2, Syntax.Decl>>) -> Bool:
  match result:
    case Done{Con{Syntax.DMu{Con{Syntax.Fam{name, params, ty, Con{ctor, Nil{}}}, Nil{}}}, Con{Syntax.DDef{definition, fty, body}, Nil{}}}}:
      String.eq(name, "Box") && String.eq(definition, "identity")
    case other:
      False{}

def main() -> Bool:
  arrow(Parser.term("(0 A B : Type) -> A")) &&
  projection(Parser.term("f x.10")) &&
  big(Parser.term("123456789012345678901234567890")) &&
  accepted(Parser.term("fun (A : Type) (1 x : A) => let y : A := x in y")) &&
  accepted(Parser.term("case x as z return Type with | 0 (x : Type) => x | 1 => ()")) &&
  accepted(Parser.term("match x as z in Vec n return Type with | Cons (0 A : Type) n x xs => x")) &&
  accepted(Parser.term("mpc[P, A] (fhc level (zk (0 w : W) * R))")) &&
  accepted(Parser.term("mpc P f (share x) (input p y)")) &&
  accepted(Parser.term("open P 0 proof (eval f (enc pk x))")) &&
  accepted(Parser.term("tuple (sum (), prod (Type, Prop), b\"ok\")")) &&
  rejected(Parser.term("match x with | 0 => x")) &&
  rejected(Parser.term("nu")) &&
  rejected(Parser.term("x .")) &&
  rejected(Parser.term("x def")) &&
  declarations(Parser.program("mu Box (A : Type) : Type with | box (x : A) : Box A def identity (A : Type) (x : A) : A := x"))
