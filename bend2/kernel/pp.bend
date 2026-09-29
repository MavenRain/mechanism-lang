import Base
import ./bignum.bend as Bignum
import ./common.bend as Common
import ./term.bend as Term
import ./shape.bend as Shape
import ./quantity.bend as Quantity
import ./level.bend as Level
import ./literal.bend as Literal
import ./bignum.bend as Bignum

law term:
  for +names: List<&2, String>
  for tm: Term.T
  String

def decimal_escape(+n: U32) -> String:
  "\\" ++ U32.show((n / 100 : U32)) ++ U32.show(((n / 10 : U32) % 10 : U32)) ++ U32.show((n % 10 : U32))

def escape_byte(+n: U32) -> String:
  Bool.pick(String, U32.is_eq(n, 34), "\\\"", Bool.pick(String, U32.is_eq(n, 92), "\\\\", Bool.pick(String, U32.is_eq(n, 10), "\\n", Bool.pick(String, U32.is_eq(n, 13), "\\r", Bool.pick(String, U32.is_eq(n, 9), "\\t", Bool.pick(String, U32.is_eq(n, 8), "\\b", Bool.pick(String, Bool.and(U32.is_ge(n, 32), U32.is_le(n, 126)), SCon{Char.from_u32(n), SNil{}}, decimal_escape(n))))))))

def escape_char(+n: U32) -> String:
  Bool.pick(String, U32.is_lt(n, 128), escape_byte(n), Bool.pick(String, U32.is_lt(n, 2048), escape_byte((192 + (n / 64 : U32) : U32)) ++ escape_byte((128 + (n % 64 : U32) : U32)), Bool.pick(String, U32.is_lt(n, 65536), escape_byte((224 + (n / 4096 : U32) : U32)) ++ escape_byte((128 + ((n / 64 : U32) % 64 : U32) : U32)) ++ escape_byte((128 + (n % 64 : U32) : U32)), escape_byte((240 + (n / 262144 : U32) : U32)) ++ escape_byte((128 + ((n / 4096 : U32) % 64 : U32) : U32)) ++ escape_byte((128 + ((n / 64 : U32) % 64 : U32) : U32)) ++ escape_byte((128 + (n % 64 : U32) : U32)))))

def escaped(s: String) -> String:
  match s:
    case SNil{}:
      ""
    case SCon{c, rest}:
      escape_char(Char.to_u32(c)) ++ escaped(rest)

def literal(lit: Literal.T) -> String:
  match lit:
    case Literal.LString{value}:
      "\"" ++ escaped(value) ++ "\""
    case Literal.LInt{value}:
      Bignum.to_string(value)

def join(xs: List<&2, String>, +sep: String) -> String:
  match xs:
    case Nil{}:
      ""
    case Con{x, Nil{}}:
      x
    case Con{x, Con{y, rest}}:
      x ++ sep ++ join(y <> rest, sep)

def binder_names(bs: List<&2, Common.Pair<Quantity.T, String>>) -> List<&2, String>:
  match bs:
    case Nil{}:
      []
    case Con{Common.Pair{q, name}, rest}:
      name <> binder_names(rest)

def push(names: List<&2, String>, added: List<&2, String>) -> List<&2, String>:
  List.append(&2, String, List.reverse(&2, String, added), names)

@unsafe def terms(xs: List<&2, Term.T>, +names: List<&2, String>) -> List<&2, String>:
  match xs:
    case Nil{}:
      []
    case Con{x, rest}:
      term(names, x) <> terms(rest, names)

@unsafe def shape(+names: List<&2, String>, s: Shape.T<Term.T>) -> String:
  match s:
    case Shape.SPi{q, x, dom}:
      "SPi " ++ Quantity.to_string(q) ++ " " ++ x ++ " " ++ term(names, dom)
    case Shape.SColl{count}:
      "SColl " ++ Bignum.to_string(count)
    case Shape.SPar{a, b}:
      "SPar " ++ term(names, a) ++ " " ++ term(names, b)
    case Shape.SMu{name, args}:
      "SMu " ++ name ++ " [" ++ join(terms(args, names), "; ") ++ "]"
    case Shape.SNu{name, args}:
      "SNu " ++ name ++ " [" ++ join(terms(args, names), "; ") ++ "]"
    case Shape.SZk{q, x, dom}:
      "SZk (" ++ Quantity.to_string(q) ++ " " ++ x ++ " : " ++ term(names, dom) ++ ")"
    case Shape.SFhc{level}:
      "SFhc " ++ term(names, level)
    case Shape.SMpc{parties, access}:
      "SMpc (" ++ term(names, parties) ++ ", " ++ term(names, access) ++ ")"

def under(s: Shape.T<Term.T>, names: List<&2, String>) -> List<&2, String>:
  match s:
    case Shape.SPi{q, name, dom}:
      name <> names
    case Shape.SZk{q, name, dom}:
      name <> names
    case _:
      names

@unsafe def addr(names: List<&2, String>, a: Term.Addr<Term.T>) -> String:
  match a:
    case Term.APt{q, arg}:
      "(APt " ++ Quantity.to_string(q) ++ " " ++ term(names, arg) ++ ")"
    case Term.ALeg{index}:
      "(ALeg " ++ Bignum.to_string(index) ++ ")"
    case Term.ACtor{name}:
      "(ACtor " ++ name ++ ")"

@unsafe def leg(names: List<&2, String>, item: Term.Leg<Term.T>) -> String:
  match item:
    case Term.Leg{binders, body}:
      +bnames = binder_names(binders)
      join(bnames, " ") ++ " => " ++ term(push(names, bnames), body)

@unsafe def legs(items: List<&2, Term.Leg<Term.T>>, +names: List<&2, String>) -> List<&2, String>:
  match items:
    case Nil{}:
      []
    case Con{x, rest}:
      leg(names, x) <> legs(rest, names)

@unsafe def motive(item: Maybe<&2, Term.Motive<Term.T>>, names: List<&2, String>) -> String:
  match item:
    case None{}:
      ""
    case Some{Term.Motive{ind, indices, +self, body}}:
      " as " ++ self ++ " return " ++ term(self <> push(names, indices), body)

@unsafe def branches(items: List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>, +names: List<&2, String>) -> String:
  match items:
    case Nil{}:
      ""
    case Con{Common.Pair{a, l}, rest}:
      " | " ++ addr(names, a) ++ " " ++ leg(names, l) ++ branches(rest, names)

def nth_name(result: Maybe<&2, String>, index: Nat) -> String:
  match result:
    case None{}:
      "#" ++ Nat.show(index)
    case Some{name}:
      name

@unsafe def term(names, tm):
  match tm:
    case Term.Var{+index}:
      nth_name(List.get(&2, String, names, index), index)
    case Term.Univ{level}:
      "Type " ++ Level.to_string(level)
    case Term.Lan{+s, d}:
      "(Lan " ++ shape(names, s) ++ " " ++ term(under(s, names), d) ++ ")"
    case Term.Ran{+s, d}:
      "(Ran " ++ shape(names, s) ++ " " ++ term(under(s, names), d) ++ ")"
    case Term.In{s, a, args}:
      "(In " ++ shape(names, s) ++ " " ++ addr(names, a) ++ " [" ++ join(terms(args, names), "; ") ++ "])"
    case Term.Elim{Term.ElimData{s, scrut, q, mo, bs}}:
      "(Elim " ++ shape(names, s) ++ " " ++ term(names, scrut) ++ motive(mo, names) ++ " with" ++ branches(bs, names) ++ ")"
    case Term.Sec{s, ls}:
      "(Sec " ++ shape(names, s) ++ " [" ++ join(legs(ls, names), "; ") ++ "])"
    case Term.Out{s, a, head}:
      "(Out " ++ shape(names, s) ++ " " ++ addr(names, a) ++ " " ++ term(names, head) ++ ")"
    case Term.Let{+name, ty, value, body}:
      "let " ++ name ++ " : " ++ term(names, ty) ++ " := " ++ term(names, value) ++ " in " ++ term(name <> names, body)
    case Term.Ann{body, ty}:
      "(" ++ term(names, body) ++ " : " ++ term(names, ty) ++ ")"
    case Term.Global{name}:
      name
    case Term.Lit{value}:
      literal(value)
    case Term.Auto{}:
      "auto"
