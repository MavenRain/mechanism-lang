import Base
import ./token.bend as Token
import ./lexer.bend as Lexer
import ./syntax.bend as Syntax
import ./universe.bend as Universe
import ../kernel/error.bend as Error
import ../kernel/bignum.bend as Bignum
import ../kernel/quantity.bend as Quantity
import ../kernel/common.bend as Common

type Parsed<-A: Data> is Data:
  Parsed{value: A, rest: List<&2, Token.T>}

def value(-A: Data, parsed: Parsed<A>) -> A:
  match parsed:
    case Parsed{x, rest}:
      x

def rest(-A: Data, parsed: Parsed<A>) -> List<&2, Token.T>:
  match parsed:
    case Parsed{x, rest}:
      rest

def error(loc: Token.Loc, message: String) -> Error.T:
  match loc:
    case Token.Loc{line, col}:
      Error.Parse{message, line, col}

def expected(-A: Data, what: String, tokens: List<&2, Token.T>) -> Result<&2, &2, Error.T, A>:
  match tokens:
    case Con{Token.Tok{kind, loc}, tail}:
      Fail{error(loc, "expected " ++ what ++ ", found " ++ Token.describe(kind))}
    case Nil{}:
      Fail{error(Token.start(), "unexpected end of input")}

def expect(+kind: Token.KindT, tokens: List<&2, Token.T>) -> Result<&2, &2, Error.T, List<&2, Token.T>>:
  match tokens:
    case Con{Token.Tok{+found, +loc}, +tail}:
      Bool.pick(Result<&2, &2, Error.T, List<&2, Token.T>>, U32.is_eq(Token.tag(kind), Token.tag(found)),
        Done{tail}, expected(List<&2, Token.T>, Token.describe(kind), Token.Tok{found, loc} <> tail))
    case Nil{}:
      expected(List<&2, Token.T>, Token.describe(kind), [])

def bounded_number_result(value: Maybe<&2, Bignum.T>, loc: Token.Loc) -> Result<&2, &2, Error.T, Bignum.T>:
  match value:
    case Some{number}: Done{number}
    case None{}: Fail{error(loc, "invalid natural literal")}

def bounded_number(+digits: String, +loc: Token.Loc) -> Result<&2, &2, Error.T, Bignum.T>:
  next = Bool.pick(Unit -> Result<&2, &2, Error.T, Bignum.T>, Nat.is_le(String.length(digits), 18n),
    unit => bounded_number_result(Bignum.of_decimal(digits), loc), unit => Fail{error(loc, "numeric literal too long")})
  next(Unit{})

def natural(parsed: Maybe<&2, Bignum.T>, loc: Token.Loc) -> Result<&2, &2, Error.T, Syntax.T>:
  match parsed:
    case Some{n}:
      Done{Syntax.SNat{n}}
    case None{}:
      Fail{error(loc, "invalid natural literal")}

def bytes_term(bytes: List<&2, U32>) -> Syntax.T:
  match bytes:
    case Nil{}:
      Syntax.SVar{"bytesNil"}
    case Con{byte, tail}:
      Syntax.SApp{Syntax.SApp{Syntax.SVar{"bytesCons"}, Syntax.SNat{Bignum.of_u32(byte)}}, bytes_term(tail)}

def names(tokens: List<&2, Token.T>, acc: List<&2, String>) -> Parsed<List<&2, String>>:
  match tokens:
    case Con{Token.Tok{Token.Ident{x}, loc}, tail}:
      names(tail, x <> acc)
    case other:
      Parsed{List.reverse(&2, String, acc), other}

def mark(tokens: List<&2, Token.T>) -> Parsed<Quantity.T>:
  match tokens:
    case Con{Token.Tok{Token.NatTok{+n}, +loc}, +tail}:
      Bool.pick(Parsed<Quantity.T>, String.eq(n, "0"), Parsed{Quantity.QZero{}, tail},
        Bool.pick(Parsed<Quantity.T>, String.eq(n, "1"), Parsed{Quantity.QOne{}, tail},
          Parsed{Quantity.QMany{}, Token.Tok{Token.NatTok{n}, loc} <> tail}))
    case other:
      Parsed{Quantity.QMany{}, other}

def proof_mark(tokens: List<&2, Token.T>) -> Parsed<Maybe<&2, Quantity.T>>:
  match tokens:
    case Con{Token.Tok{Token.NatTok{+n}, +loc}, +tail}:
      Bool.pick(Parsed<Maybe<&2, Quantity.T>>, String.eq(n, "0"), Parsed{Some{Quantity.QZero{}}, tail},
        Bool.pick(Parsed<Maybe<&2, Quantity.T>>, String.eq(n, "1"), Parsed{Some{Quantity.QOne{}}, tail},
          Parsed{None{}, Token.Tok{Token.NatTok{n}, loc} <> tail}))
    case other:
      Parsed{None{}, other}

def make_binders(names: List<&2, String>, +q: Quantity.T, +ty: Syntax.T) -> List<&2, Syntax.Binder<Syntax.T>>:
  match names:
    case Nil{}:
      []
    case Con{name, tail}:
      Syntax.Binder{q, name, ty} <> make_binders(tail, q, ty)

def arrows(binders: List<&2, Syntax.Binder<Syntax.T>>, body: Syntax.T, +star: Bool) -> Syntax.T:
  match binders:
    case Nil{}:
      body
    case Con{+binder, tail}:
      +body = arrows(tail, body, star)
      Bool.pick(Syntax.T, star, Syntax.SStar{binder, body}, Syntax.SArrow{binder, body})

def fields(binders: List<&2, Syntax.Binder<Syntax.T>>) -> List<&2, Syntax.Field<Syntax.T>>:
  match binders:
    case Nil{}:
      []
    case Con{Syntax.Binder{q, name, ty}, tail}:
      Syntax.Field{q, name, Some{ty}} <> fields(tail)

def zk(body: Syntax.T, loc: Token.Loc) -> Result<&2, &2, Error.T, Syntax.T>:
  match body:
    case Syntax.SStar{binder, body}:
      Done{Syntax.SZkTy{binder, body}}
    case other:
      Fail{error(loc, "expected '(q w : W) * R' after 'zk'")}

def index_of(names: List<&2, String>, +name: String, +index: Nat) -> Maybe<&2, Nat>:
  match names:
    case Nil{}:
      None{}
    case Con{head, tail}:
      Bool.pick(Maybe<&2, Nat>, String.eq(head, name), Some{index}, index_of(tail, name, 1n+index))

def universe_var(index: Maybe<&2, Nat>, name: String, loc: Token.Loc) -> Result<&2, &2, Error.T, Universe.T>:
  match index:
    case Some{n}:
      Done{Universe.VarLevel{n}}
    case None{}:
      Fail{error(loc, "unbound universe " ++ name)}

def starts_atom(tokens: List<&2, Token.T>) -> Bool:
  match tokens:
    case Con{Token.Tok{Token.Ident{name}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.NatTok{digits}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.Bytes{bytes}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.LParen{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.UnitTok{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KProp{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KType{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KSort{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KAuto{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KTuple{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KSum{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KProd{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KNatAdd{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KNatSub{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KNatMul{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KNatEq{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KNatLt{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KNu{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KProve{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KVerify{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KFhc{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KEnc{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KEval{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KDec{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KMpc{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KInput{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KShare{}, loc}, tail}:
      True{}
    case Con{Token.Tok{Token.KOpen{}, loc}, tail}:
      True{}
    case other:
      False{}

type Value is Data:
  TermValue{value: Syntax.T, rest: List<&2, Token.T>}
  BindersValue{value: List<&2, Syntax.Binder<Syntax.T>>, rest: List<&2, Token.T>}
  FieldsValue{value: List<&2, Syntax.Field<Syntax.T>>, rest: List<&2, Token.T>}
  BranchesValue{value: List<&2, Syntax.Branch<Syntax.T>>, rest: List<&2, Token.T>}
  LevelValue{value: Universe.T, rest: List<&2, Token.T>}
  TermsValue{value: List<&2, Syntax.T>, rest: List<&2, Token.T>}
  DeclValue{value: Syntax.Decl, rest: List<&2, Token.T>}
  DeclsValue{value: List<&2, Syntax.Decl>, rest: List<&2, Token.T>}
  FamilyValue{value: Syntax.Fam<Syntax.T>, rest: List<&2, Token.T>}
  FamiliesValue{value: List<&2, Syntax.Fam<Syntax.T>>, rest: List<&2, Token.T>}
  CtorsValue{value: List<&2, Syntax.FamCtor<Syntax.T>>, rest: List<&2, Token.T>}
  RecValue{value: Syntax.RecDef<Syntax.T>, rest: List<&2, Token.T>}
  RecsValue{value: List<&2, Syntax.RecDef<Syntax.T>>, rest: List<&2, Token.T>}

def read_term(result: Result<&2, &2, Error.T, Value>) -> Result<&2, &2, Error.T, Parsed<Syntax.T>>:
  match result:
    case Done{TermValue{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{error}:
      Fail{error}
    case Done{other}:
      Fail{Error.Parse{"internal parser result mismatch: Term", 1n, 1n}}

def read_binders(result: Result<&2, &2, Error.T, Value>) -> Result<&2, &2, Error.T, Parsed<List<&2, Syntax.Binder<Syntax.T>>>>:
  match result:
    case Done{BindersValue{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{error}:
      Fail{error}
    case Done{other}:
      Fail{Error.Parse{"internal parser result mismatch: Binders", 1n, 1n}}

def read_fields(result: Result<&2, &2, Error.T, Value>) -> Result<&2, &2, Error.T, Parsed<List<&2, Syntax.Field<Syntax.T>>>>:
  match result:
    case Done{FieldsValue{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{error}:
      Fail{error}
    case Done{other}:
      Fail{Error.Parse{"internal parser result mismatch: Fields", 1n, 1n}}

def read_branches(result: Result<&2, &2, Error.T, Value>) -> Result<&2, &2, Error.T, Parsed<List<&2, Syntax.Branch<Syntax.T>>>>:
  match result:
    case Done{BranchesValue{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{error}:
      Fail{error}
    case Done{other}:
      Fail{Error.Parse{"internal parser result mismatch: Branches", 1n, 1n}}

def read_level(result: Result<&2, &2, Error.T, Value>) -> Result<&2, &2, Error.T, Parsed<Universe.T>>:
  match result:
    case Done{LevelValue{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{error}:
      Fail{error}
    case Done{other}:
      Fail{Error.Parse{"internal parser result mismatch: Level", 1n, 1n}}

def read_terms(result: Result<&2, &2, Error.T, Value>) -> Result<&2, &2, Error.T, Parsed<List<&2, Syntax.T>>>:
  match result:
    case Done{TermsValue{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{error}:
      Fail{error}
    case Done{other}:
      Fail{Error.Parse{"internal parser result mismatch: Terms", 1n, 1n}}

def read_decl(result: Result<&2, &2, Error.T, Value>) -> Result<&2, &2, Error.T, Parsed<Syntax.Decl>>:
  match result:
    case Done{DeclValue{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{error}:
      Fail{error}
    case Done{other}:
      Fail{Error.Parse{"internal parser result mismatch: Decl", 1n, 1n}}

def read_decls(result: Result<&2, &2, Error.T, Value>) -> Result<&2, &2, Error.T, Parsed<List<&2, Syntax.Decl>>>:
  match result:
    case Done{DeclsValue{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{error}:
      Fail{error}
    case Done{other}:
      Fail{Error.Parse{"internal parser result mismatch: Decls", 1n, 1n}}

def read_family(result: Result<&2, &2, Error.T, Value>) -> Result<&2, &2, Error.T, Parsed<Syntax.Fam<Syntax.T>>>:
  match result:
    case Done{FamilyValue{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{error}:
      Fail{error}
    case Done{other}:
      Fail{Error.Parse{"internal parser result mismatch: Family", 1n, 1n}}

def read_families(result: Result<&2, &2, Error.T, Value>) -> Result<&2, &2, Error.T, Parsed<List<&2, Syntax.Fam<Syntax.T>>>>:
  match result:
    case Done{FamiliesValue{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{error}:
      Fail{error}
    case Done{other}:
      Fail{Error.Parse{"internal parser result mismatch: Families", 1n, 1n}}

def read_ctors(result: Result<&2, &2, Error.T, Value>) -> Result<&2, &2, Error.T, Parsed<List<&2, Syntax.FamCtor<Syntax.T>>>>:
  match result:
    case Done{CtorsValue{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{error}:
      Fail{error}
    case Done{other}:
      Fail{Error.Parse{"internal parser result mismatch: Ctors", 1n, 1n}}

def read_rec(result: Result<&2, &2, Error.T, Value>) -> Result<&2, &2, Error.T, Parsed<Syntax.RecDef<Syntax.T>>>:
  match result:
    case Done{RecValue{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{error}:
      Fail{error}
    case Done{other}:
      Fail{Error.Parse{"internal parser result mismatch: Rec", 1n, 1n}}

def read_recs(result: Result<&2, &2, Error.T, Value>) -> Result<&2, &2, Error.T, Parsed<List<&2, Syntax.RecDef<Syntax.T>>>>:
  match result:
    case Done{RecsValue{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{error}:
      Fail{error}
    case Done{other}:
      Fail{Error.Parse{"internal parser result mismatch: Recs", 1n, 1n}}

type Mode is Data:
  PTerm{}
  PArrow{}
  PArrowTry{attempt: Result<&2, &2, Error.T, Value>}
  PArrowPlain{}
  PArrowRest{head: Syntax.T}
  PApp{}
  PAppRest{head: Syntax.T}
  PAppNext{head: Syntax.T}
  PAtom{}
  PHead{}
  PPostfix{head: Syntax.T}
  PReturn{head: Syntax.T}
  PParenRest{head: Syntax.T}
  PBinderGroup{}
  PBinderNames{q: Quantity.T}
  PBinders{acc: List<&2, Syntax.Binder<Syntax.T>>}
  PFun{}
  PLet{}
  PCase{fibered: Bool}
  PCaseMotive{fibered: Bool, scrut: Syntax.T}
  PCaseReturn{fibered: Bool, scrut: Syntax.T, name: String, ind: Maybe<&2, String>, indices: List<&2, String>}
  PBranches{fibered: Bool, acc: List<&2, Syntax.Branch<Syntax.T>>}
  PFields{acc: List<&2, Syntax.Field<Syntax.T>>}
  PBareField{acc: List<&2, Syntax.Field<Syntax.T>>, original: List<&2, Token.T>, q: Quantity.T}
  PLevel{}
  PLevelParen{}
  PItems{kind: Nat}
  PItemsBody{kind: Nat}
  PItemsNext{kind: Nat, acc: List<&2, Syntax.T>}
  PItemsRest{kind: Nat, acc: List<&2, Syntax.T>}
  PAtoms{acc: List<&2, Syntax.T>}
  PAtomsNext{acc: List<&2, Syntax.T>}
  PAtomsDone{acc: List<&2, Syntax.T>}
  PDecl{}
  PDecls{acc: List<&2, Syntax.Decl>}
  PDef{rec: Bool}
  PRecGroup{acc: List<&2, Syntax.RecDef<Syntax.T>>}
  PRecRest{acc: List<&2, Syntax.RecDef<Syntax.T>>}
  PFamily{}
  PFamilyCtors{acc: List<&2, Syntax.FamCtor<Syntax.T>>}
  PFamilyGroup{acc: List<&2, Syntax.Fam<Syntax.T>>}
  PFamilyRest{acc: List<&2, Syntax.Fam<Syntax.T>>}
  PMutual{acc: List<&2, Syntax.Fam<Syntax.T>>}

def collection(kind: Nat, items: List<&2, Syntax.T>) -> Syntax.T:
  match kind:
    case 0n:
      Syntax.STuple{items}
    case 1n:
      Syntax.SSum{items}
    case other:
      Syntax.SProd{items}

def elimination(+fibered: Bool, +scrut: Syntax.T, +motive: Maybe<&2, Syntax.Motive<Syntax.T>>,
    +branches: List<&2, Syntax.Branch<Syntax.T>>) -> Syntax.T:
  Bool.pick(Syntax.T, fibered, Syntax.SMatch{scrut, motive, branches}, Syntax.SCase{scrut, motive, branches})

def numeric_branch(fibered: Bool, loc: Token.Loc) -> Result<&2, &2, Error.T, Unit>:
  match fibered:
    case True{}:
      Fail{error(loc, "a match branch keys a constructor, not a leg number")}
    case False{}:
      Done{Unit{}}

def mutual_count(+families: List<&2, Syntax.Fam<Syntax.T>>, loc: Token.Loc) -> Result<&2, &2, Error.T, List<&2, Syntax.Fam<Syntax.T>>>:
  match families:
    case Con{first, Con{second, rest}}:
      Done{List.reverse(&2, Syntax.Fam<Syntax.T>, first <> second <> rest)}
    case other:
      Fail{error(loc, "a mutual group needs at least two mu declarations")}

def make_def(name: String, binders: List<&2, Syntax.Binder<Syntax.T>>, ty: Syntax.T, body: Syntax.T) -> Syntax.Decl:
  match binders:
    case Nil{}:
      Syntax.DDef{name, ty, body}
    case Con{+first, +tail}:
      Syntax.DDef{name, arrows(first <> tail, ty, False{}), Syntax.SFun{first <> tail, body}}

def family_separator(tokens: List<&2, Token.T>) -> Result<&2, &2, Error.T, List<&2, Token.T>>:
  match tokens:
    case Con{Token.Tok{Token.KWith{}, loc}, tail}:
      Done{tail}
    case Con{Token.Tok{Token.ColonEq{}, loc}, tail}:
      Done{tail}
    case other:
      expected(List<&2, Token.T>, "':=' or 'with'", other)

law extended_decl:
  for tokens: List<&2, Token.T>
  for scope: List<&2, String>
  Result<&2, &2, Error.T, Value>

@unsafe
def parse(mode: Mode, +scope: List<&2, String>, +tokens: List<&2, Token.T>) -> Result<&2, &2, Error.T, Value>:
  match mode tokens:
    case PTerm{} Con{Token.Tok{Token.KFun{}, loc}, tail}:
      parse(PFun{}, scope, tail)
    case PTerm{} Con{Token.Tok{Token.KLet{}, loc}, tail}:
      parse(PLet{}, scope, tail)
    case PTerm{} Con{Token.Tok{Token.KCase{}, loc}, tail}:
      parse(PCase{False{}}, scope, tail)
    case PTerm{} Con{Token.Tok{Token.KMatch{}, loc}, tail}:
      parse(PCase{True{}}, scope, tail)
    case PTerm{} Con{Token.Tok{Token.KZk{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PArrow{}, scope, tail))
        x : Syntax.T <- zk(value(Syntax.T, a), loc)
        return TermValue{x, rest(Syntax.T, a)}
    case PTerm{} tokens:
      parse(PArrow{}, scope, tokens)
    case PArrow{} Con{Token.Tok{Token.LParen{}, loc}, tail}:
      parse(PArrowTry{parse(PBinderGroup{}, scope, Con{Token.Tok{Token.LParen{}, loc}, tail})}, scope, Con{Token.Tok{Token.LParen{}, loc}, tail})
    case PArrow{} tokens:
      parse(PArrowPlain{}, scope, tokens)
    case PArrowTry{Done{BindersValue{bs, Con{Token.Tok{Token.Arrow{}, loc}, tail}}}} original:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        return TermValue{arrows(bs, value(Syntax.T, a), False{}), rest(Syntax.T, a)}
    case PArrowTry{Done{BindersValue{bs, Con{Token.Tok{Token.Star{}, loc}, tail}}}} original:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        return TermValue{arrows(bs, value(Syntax.T, a), True{}), rest(Syntax.T, a)}
    case PArrowTry{attempt} tokens:
      parse(PArrowPlain{}, scope, tokens)
    case PArrowPlain{} tokens:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PApp{}, scope, tokens))
        parse(PArrowRest{value(Syntax.T, a)}, scope, rest(Syntax.T, a))
    case PArrowRest{head} Con{Token.Tok{Token.Arrow{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        return TermValue{Syntax.SArrow{Syntax.Binder{Quantity.QMany{}, "_", head}, value(Syntax.T, a)}, rest(Syntax.T, a)}
    case PArrowRest{head} Con{Token.Tok{Token.Star{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        return TermValue{Syntax.SStar{Syntax.Binder{Quantity.QMany{}, "_", head}, value(Syntax.T, a)}, rest(Syntax.T, a)}
    case PArrowRest{head} tokens:
      Done{TermValue{head, tokens}}
    case PReturn{head} tokens:
      Done{TermValue{head, tokens}}
    case PApp{} Con{Token.Tok{Token.KAbsurd{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PApp{}, scope, tail))
        return TermValue{Syntax.SAbsurd{value(Syntax.T, a)}, rest(Syntax.T, a)}
    case PApp{} Con{Token.Tok{Token.KInj{}, loc}, Con{Token.Tok{Token.NatTok{k}, kloc}, Con{Token.Tok{Token.KOf{}, loc}, Con{Token.Tok{Token.NatTok{n}, nloc}, tail}}}}:
      do Result<&2, &2, Error.T, Value>:
        k : Bignum.T <- bounded_number(k, kloc)
        n : Bignum.T <- bounded_number(n, nloc)
        +a : Parsed<Syntax.T> <- read_term(parse(PApp{}, scope, tail))
        return TermValue{Syntax.SInj{k, n, value(Syntax.T, a)}, rest(Syntax.T, a)}
    case PApp{} Con{Token.Tok{Token.KInj{}, loc}, tail}:
      expected(Value, "'K of N' after 'inj'", tail)
    case PApp{} tokens:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, tokens))
        parse(PAppRest{value(Syntax.T, a)}, scope, rest(Syntax.T, a))
    case PAppRest{+head} tokens:
      parse(Bool.pick(Mode, starts_atom(tokens), PAppNext{head}, PReturn{head}), scope, tokens)
    case PAppNext{head} tokens:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, tokens))
        parse(PAppRest{Syntax.SApp{head, value(Syntax.T, a)}}, scope, rest(Syntax.T, a))
    case PAtom{} tokens:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PHead{}, scope, tokens))
        parse(PPostfix{value(Syntax.T, a)}, scope, rest(Syntax.T, a))
    case PPostfix{head} Con{Token.Tok{Token.Dot1{}, loc}, tail}:
      parse(PPostfix{Syntax.SProj{head, Bignum.one()}}, scope, tail)
    case PPostfix{head} Con{Token.Tok{Token.Dot2{}, loc}, tail}:
      parse(PPostfix{Syntax.SProj{head, Bignum.of_u32(2)}}, scope, tail)
    case PPostfix{head} Con{Token.Tok{Token.Dot{}, loc}, Con{Token.Tok{Token.NatTok{k}, nloc}, tail}}:
      do Result<&2, &2, Error.T, Value>:
        k : Bignum.T <- bounded_number(k, nloc)
        parse(PPostfix{Syntax.SProj{head, k}}, scope, tail)
    case PPostfix{head} Con{Token.Tok{Token.Dot{}, loc}, tail}:
      Fail{error(loc, "expected a leg number after '.'")}
    case PPostfix{head} tokens:
      Done{TermValue{head, tokens}}
    case PHead{} Con{Token.Tok{Token.Ident{x}, loc}, tail}:
      Done{TermValue{Syntax.SVar{x}, tail}}
    case PHead{} Con{Token.Tok{Token.Bytes{bytes}, loc}, tail}:
      Done{TermValue{bytes_term(bytes), tail}}
    case PHead{} Con{Token.Tok{Token.KProp{}, loc}, tail}:
      Done{TermValue{Syntax.SProp{}, tail}}
    case PHead{} Con{Token.Tok{Token.KAuto{}, loc}, tail}:
      Done{TermValue{Syntax.SAuto{}, tail}}
    case PHead{} Con{Token.Tok{Token.UnitTok{}, loc}, tail}:
      Done{TermValue{Syntax.SUnit{}, tail}}
    case PHead{} Con{Token.Tok{Token.NatTok{n}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        n : Syntax.T <- natural(Bignum.of_decimal(n), loc)
        return TermValue{n, tail}
    case PHead{} Con{Token.Tok{Token.KSort{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Universe.T> <- read_level(parse(PLevel{}, scope, tail))
        return TermValue{Syntax.SSort{value(Universe.T, a)}, rest(Universe.T, a)}
    case PHead{} Con{Token.Tok{Token.KType{}, loc}, Con{Token.Tok{Token.NatTok{n}, nloc}, tail}}:
      do Result<&2, &2, Error.T, Value>:
        n : Bignum.T <- bounded_number(n, nloc)
        return TermValue{Syntax.SType{n}, tail}
    case PHead{} Con{Token.Tok{Token.KType{}, loc}, tail}:
      Done{TermValue{Syntax.SType{Bignum.zero()}, tail}}
    case PHead{} Con{Token.Tok{Token.KNatAdd{}, loc}, tail}:
      Done{TermValue{Syntax.SPrim{Syntax.PAdd{}}, tail}}
    case PHead{} Con{Token.Tok{Token.KNatSub{}, loc}, tail}:
      Done{TermValue{Syntax.SPrim{Syntax.PSub{}}, tail}}
    case PHead{} Con{Token.Tok{Token.KNatMul{}, loc}, tail}:
      Done{TermValue{Syntax.SPrim{Syntax.PMul{}}, tail}}
    case PHead{} Con{Token.Tok{Token.KNatEq{}, loc}, tail}:
      Done{TermValue{Syntax.SPrim{Syntax.PEq{}}, tail}}
    case PHead{} Con{Token.Tok{Token.KNatLt{}, loc}, tail}:
      Done{TermValue{Syntax.SPrim{Syntax.PLt{}}, tail}}
    case PHead{} Con{Token.Tok{Token.KTuple{}, loc}, tail}:
      parse(PItems{0n}, scope, tail)
    case PHead{} Con{Token.Tok{Token.KSum{}, loc}, tail}:
      parse(PItems{1n}, scope, tail)
    case PHead{} Con{Token.Tok{Token.KProd{}, loc}, tail}:
      parse(PItems{2n}, scope, tail)
    case PHead{} Con{Token.Tok{Token.KMu{}, loc}, tail}:
      Fail{error(loc, "a mu group is a declaration and not a term")}
    case PHead{} Con{Token.Tok{Token.KAnd{}, loc}, tail}:
      Fail{error(loc, "'and' joins two members of a mu group and is not a term")}
    case PHead{} Con{Token.Tok{Token.KNu{}, loc}, tail}:
      Fail{error(loc, "nu arrives at M2")}
    case PHead{} Con{Token.Tok{Token.KZk{}, loc}, tail}:
      Fail{error(loc, "a zk type is not an atom")}
    case PHead{} Con{Token.Tok{Token.LParen{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        parse(PParenRest{value(Syntax.T, a)}, scope, rest(Syntax.T, a))
    case PHead{} Con{Token.Tok{Token.KProve{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, tail))
        +b : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, rest(Syntax.T, a)))
        +c : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, rest(Syntax.T, b)))
        return TermValue{Syntax.SProve{value(Syntax.T, a), value(Syntax.T, b), value(Syntax.T, c)}, rest(Syntax.T, c)}
    case PHead{} Con{Token.Tok{Token.KVerify{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, tail))
        +b : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, rest(Syntax.T, a)))
        return TermValue{Syntax.SVerify{value(Syntax.T, a), value(Syntax.T, b)}, rest(Syntax.T, b)}
    case PHead{} Con{Token.Tok{Token.KFhc{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, tail))
        +b : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, rest(Syntax.T, a)))
        return TermValue{Syntax.SFhcTy{value(Syntax.T, a), value(Syntax.T, b)}, rest(Syntax.T, b)}
    case PHead{} Con{Token.Tok{Token.KEnc{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, tail))
        +b : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, rest(Syntax.T, a)))
        return TermValue{Syntax.SEnc{value(Syntax.T, a), value(Syntax.T, b)}, rest(Syntax.T, b)}
    case PHead{} Con{Token.Tok{Token.KEval{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, tail))
        +b : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, rest(Syntax.T, a)))
        return TermValue{Syntax.SEval{value(Syntax.T, a), value(Syntax.T, b)}, rest(Syntax.T, b)}
    case PHead{} Con{Token.Tok{Token.KDec{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, tail))
        +b : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, rest(Syntax.T, a)))
        return TermValue{Syntax.SDec{value(Syntax.T, a), value(Syntax.T, b)}, rest(Syntax.T, b)}
    case PHead{} Con{Token.Tok{Token.KShare{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, tail))
        return TermValue{Syntax.SShare{value(Syntax.T, a)}, rest(Syntax.T, a)}
    case PHead{} Con{Token.Tok{Token.KInput{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, tail))
        +b : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, rest(Syntax.T, a)))
        return TermValue{Syntax.SInput{value(Syntax.T, a), value(Syntax.T, b)}, rest(Syntax.T, b)}
    case PHead{} Con{Token.Tok{Token.KMpc{}, loc}, Con{Token.Tok{Token.LBracket{}, loc}, tail}}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        tail : List<&2, Token.T> <- expect(Token.Comma{}, rest(Syntax.T, a))
        +b : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        tail : List<&2, Token.T> <- expect(Token.RBracket{}, rest(Syntax.T, b))
        +c : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, tail))
        return TermValue{Syntax.SMpcTy{value(Syntax.T, a), value(Syntax.T, b), value(Syntax.T, c)}, rest(Syntax.T, c)}
    case PHead{} Con{Token.Tok{Token.KMpc{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, tail))
        +b : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, rest(Syntax.T, a)))
        +c : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, rest(Syntax.T, b)))
        +cs : Parsed<List<&2, Syntax.T>> <- read_terms(parse(PAtoms{[]}, scope, rest(Syntax.T, c)))
        return TermValue{Syntax.SJoin{value(Syntax.T, a), value(Syntax.T, b), value(Syntax.T, c) <> value(List<&2, Syntax.T>, cs)}, rest(List<&2, Syntax.T>, cs)}
    case PHead{} Con{Token.Tok{Token.KOpen{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, tail))
        +q : Parsed<Maybe<&2, Quantity.T>> = proof_mark(rest(Syntax.T, a))
        +b : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, rest(Maybe<&2, Quantity.T>, q)))
        +c : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, rest(Syntax.T, b)))
        return TermValue{Syntax.SOpen{value(Maybe<&2, Quantity.T>, q), value(Syntax.T, a), value(Syntax.T, b), value(Syntax.T, c)}, rest(Syntax.T, c)}
    case PHead{} tokens:
      expected(Value, "a term", tokens)
    case PParenRest{head} Con{Token.Tok{Token.RParen{}, loc}, tail}:
      Done{TermValue{head, tail}}
    case PParenRest{head} Con{Token.Tok{Token.Comma{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        tail : List<&2, Token.T> <- expect(Token.RParen{}, rest(Syntax.T, a))
        return TermValue{Syntax.SPair{head, value(Syntax.T, a)}, tail}
    case PParenRest{head} Con{Token.Tok{Token.Colon{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        tail : List<&2, Token.T> <- expect(Token.RParen{}, rest(Syntax.T, a))
        return TermValue{Syntax.SAnn{head, value(Syntax.T, a)}, tail}
    case PParenRest{head} tokens:
      expected(Value, "')', ',' or ':'", tokens)
    case PBinderGroup{} Con{Token.Tok{Token.LParen{}, loc}, tail}:
      +q = mark(tail)
      parse(PBinderNames{value(Quantity.T, q)}, scope, rest(Quantity.T, q))
    case PBinderGroup{} tokens:
      expected(Value, "'('", tokens)
    case PBinderNames{+q} Con{Token.Tok{Token.Ident{name}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +ns : Parsed<List<&2, String>> = names(tail, [name])
        tail : List<&2, Token.T> <- expect(Token.Colon{}, rest(List<&2, String>, ns))
        +a : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        tail : List<&2, Token.T> <- expect(Token.RParen{}, rest(Syntax.T, a))
        return BindersValue{make_binders(value(List<&2, String>, ns), q, value(Syntax.T, a)), tail}
    case PBinderNames{q} tokens:
      expected(Value, "a binder name and ':'", tokens)
    case PBinders{acc} Con{Token.Tok{Token.LParen{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<List<&2, Syntax.Binder<Syntax.T>>> <- read_binders(parse(PBinderGroup{}, scope, Con{Token.Tok{Token.LParen{}, loc}, tail}))
        parse(PBinders{List.append(&2, Syntax.Binder<Syntax.T>, List.reverse(&2, Syntax.Binder<Syntax.T>, value(List<&2, Syntax.Binder<Syntax.T>>, a)), acc)}, scope, rest(List<&2, Syntax.Binder<Syntax.T>>, a))
    case PBinders{acc} tokens:
      Done{BindersValue{List.reverse(&2, Syntax.Binder<Syntax.T>, acc), tokens}}
    case PFun{} tokens:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<List<&2, Syntax.Binder<Syntax.T>>> <- read_binders(parse(PBinderGroup{}, scope, tokens))
        +b : Parsed<List<&2, Syntax.Binder<Syntax.T>>> <- read_binders(parse(PBinders{List.reverse(&2, Syntax.Binder<Syntax.T>, value(List<&2, Syntax.Binder<Syntax.T>>, a))}, scope, rest(List<&2, Syntax.Binder<Syntax.T>>, a)))
        tail : List<&2, Token.T> <- expect(Token.DArrow{}, rest(List<&2, Syntax.Binder<Syntax.T>>, b))
        +c : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        return TermValue{Syntax.SFun{value(List<&2, Syntax.Binder<Syntax.T>>, b), value(Syntax.T, c)}, rest(Syntax.T, c)}
    case PLet{} Con{Token.Tok{Token.Ident{name}, loc}, Con{Token.Tok{Token.Colon{}, loc}, tail}}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        tail : List<&2, Token.T> <- expect(Token.ColonEq{}, rest(Syntax.T, a))
        +b : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        tail : List<&2, Token.T> <- expect(Token.KIn{}, rest(Syntax.T, b))
        +c : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        return TermValue{Syntax.SLet{name, value(Syntax.T, a), value(Syntax.T, b), value(Syntax.T, c)}, rest(Syntax.T, c)}
    case PLet{} tokens:
      expected(Value, "a name and ':' after 'let'", tokens)
    case PItems{kind} Con{Token.Tok{Token.UnitTok{}, loc}, tail}:
      Done{TermValue{collection(kind, []), tail}}
    case PItems{kind} Con{Token.Tok{Token.LParen{}, loc}, tail}:
      parse(PItemsBody{kind}, scope, tail)
    case PItems{kind} tokens:
      expected(Value, "'(' after collection keyword", tokens)
    case PItemsBody{kind} Con{Token.Tok{Token.RParen{}, loc}, tail}:
      Done{TermValue{collection(kind, []), tail}}
    case PItemsBody{kind} tokens:
      parse(PItemsNext{kind, []}, scope, tokens)
    case PItemsNext{kind, acc} tokens:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tokens))
        parse(PItemsRest{kind, value(Syntax.T, a) <> acc}, scope, rest(Syntax.T, a))
    case PItemsRest{kind, acc} Con{Token.Tok{Token.Comma{}, loc}, tail}:
      parse(PItemsNext{kind, acc}, scope, tail)
    case PItemsRest{kind, acc} Con{Token.Tok{Token.RParen{}, loc}, tail}:
      Done{TermValue{collection(kind, List.reverse(&2, Syntax.T, acc)), tail}}
    case PItemsRest{kind, acc} tokens:
      expected(Value, "',' or ')'", tokens)
    case PAtoms{+acc} tokens:
      parse(Bool.pick(Mode, starts_atom(tokens), PAtomsNext{acc}, PAtomsDone{acc}), scope, tokens)
    case PAtomsNext{acc} tokens:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PAtom{}, scope, tokens))
        parse(PAtoms{value(Syntax.T, a) <> acc}, scope, rest(Syntax.T, a))
    case PAtomsDone{acc} tokens:
      Done{TermsValue{List.reverse(&2, Syntax.T, acc), tokens}}
    case PLevel{} Con{Token.Tok{Token.NatTok{n}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        n : Bignum.T <- bounded_number(n, loc)
        return LevelValue{Universe.NatLevel{n}, tail}
    case PLevel{} Con{Token.Tok{Token.Ident{+name}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        u : Universe.T <- universe_var(index_of(scope, name, 0n), name, loc)
        return LevelValue{u, tail}
    case PLevel{} Con{Token.Tok{Token.LParen{}, loc}, tail}:
      parse(PLevelParen{}, scope, tail)
    case PLevel{} tokens:
      expected(Value, "a universe level", tokens)
    case PLevelParen{} Con{Token.Tok{Token.Ident{SCon{'s', SCon{'u', SCon{'c', SCon{'c', SNil{}}}}}}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Universe.T> <- read_level(parse(PLevel{}, scope, tail))
        tail : List<&2, Token.T> <- expect(Token.RParen{}, rest(Universe.T, a))
        return LevelValue{Universe.SuccLevel{value(Universe.T, a)}, tail}
    case PLevelParen{} Con{Token.Tok{Token.Ident{SCon{'m', SCon{'a', SCon{'x', SNil{}}}}}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Universe.T> <- read_level(parse(PLevel{}, scope, tail))
        +b : Parsed<Universe.T> <- read_level(parse(PLevel{}, scope, rest(Universe.T, a)))
        tail : List<&2, Token.T> <- expect(Token.RParen{}, rest(Universe.T, b))
        return LevelValue{Universe.MaxLevel{value(Universe.T, a), value(Universe.T, b)}, tail}
    case PLevelParen{} Con{Token.Tok{Token.Ident{SCon{'i', SCon{'m', SCon{'a', SCon{'x', SNil{}}}}}}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Universe.T> <- read_level(parse(PLevel{}, scope, tail))
        +b : Parsed<Universe.T> <- read_level(parse(PLevel{}, scope, rest(Universe.T, a)))
        tail : List<&2, Token.T> <- expect(Token.RParen{}, rest(Universe.T, b))
        return LevelValue{Universe.IMaxLevel{value(Universe.T, a), value(Universe.T, b)}, tail}
    case PLevelParen{} tokens:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Universe.T> <- read_level(parse(PLevel{}, scope, tokens))
        tail : List<&2, Token.T> <- expect(Token.RParen{}, rest(Universe.T, a))
        return LevelValue{value(Universe.T, a), tail}
    case PCase{+fibered} tokens:
      do Result<&2, &2, Error.T, Value>:
        +a : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tokens))
        parse(PCaseMotive{fibered, value(Syntax.T, a)}, scope, rest(Syntax.T, a))
    case PCaseMotive{+fibered, scrut} Con{Token.Tok{Token.KWith{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +bs : Parsed<List<&2, Syntax.Branch<Syntax.T>>> <- read_branches(parse(PBranches{fibered, []}, scope, tail))
        return TermValue{elimination(fibered, scrut, None{}, value(List<&2, Syntax.Branch<Syntax.T>>, bs)), rest(List<&2, Syntax.Branch<Syntax.T>>, bs)}
    case PCaseMotive{fibered, scrut} Con{Token.Tok{Token.KAs{}, loc}, Con{Token.Tok{Token.Ident{name}, loc}, Con{Token.Tok{Token.KIn{}, loc}, Con{Token.Tok{Token.Ident{ind}, loc}, tail}}}}:
      +ns = names(tail, [])
      parse(PCaseReturn{fibered, scrut, name, Some{ind}, value(List<&2, String>, ns)}, scope, rest(List<&2, String>, ns))
    case PCaseMotive{fibered, scrut} Con{Token.Tok{Token.KAs{}, loc}, Con{Token.Tok{Token.Ident{name}, loc}, tail}}:
      parse(PCaseReturn{fibered, scrut, name, None{}, []}, scope, tail)
    case PCaseMotive{fibered, scrut} Con{Token.Tok{Token.KAs{}, loc}, tail}:
      Fail{error(loc, "expected 'NAME [in FAMILY IDX..] return TYPE' after 'as'")}
    case PCaseMotive{fibered, scrut} tokens:
      expected(Value, "'as' or 'with'", tokens)
    case PCaseReturn{+fibered, scrut, name, ind, indices} Con{Token.Tok{Token.KReturn{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +body : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        tail : List<&2, Token.T> <- expect(Token.KWith{}, rest(Syntax.T, body))
        +bs : Parsed<List<&2, Syntax.Branch<Syntax.T>>> <- read_branches(parse(PBranches{fibered, []}, scope, tail))
        return TermValue{elimination(fibered, scrut, Some{Syntax.Motive{name, ind, indices, value(Syntax.T, body)}}, value(List<&2, Syntax.Branch<Syntax.T>>, bs)), rest(List<&2, Syntax.Branch<Syntax.T>>, bs)}
    case PCaseReturn{fibered, scrut, name, ind, indices} tokens:
      expected(Value, "'return'", tokens)
    case PBranches{+fibered, acc} Con{Token.Tok{Token.Pipe{}, loc}, Con{Token.Tok{Token.NatTok{k}, kloc}, tail}}:
      do Result<&2, &2, Error.T, Value>:
        checked : Unit <- numeric_branch(fibered, kloc)
        k : Bignum.T <- bounded_number(k, kloc)
        +bs : Parsed<List<&2, Syntax.Binder<Syntax.T>>> <- read_binders(parse(PBinders{[]}, scope, tail))
        tail : List<&2, Token.T> <- expect(Token.DArrow{}, rest(List<&2, Syntax.Binder<Syntax.T>>, bs))
        +body : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        parse(PBranches{fibered, Syntax.BrLeg{k, value(List<&2, Syntax.Binder<Syntax.T>>, bs), value(Syntax.T, body)} <> acc}, scope, rest(Syntax.T, body))
    case PBranches{fibered, acc} Con{Token.Tok{Token.Pipe{}, loc}, Con{Token.Tok{Token.Ident{name}, loc}, tail}}:
      do Result<&2, &2, Error.T, Value>:
        +fs : Parsed<List<&2, Syntax.Field<Syntax.T>>> <- read_fields(parse(PFields{[]}, scope, tail))
        tail : List<&2, Token.T> <- expect(Token.DArrow{}, rest(List<&2, Syntax.Field<Syntax.T>>, fs))
        +body : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        parse(PBranches{fibered, Syntax.BrCtor{name, value(List<&2, Syntax.Field<Syntax.T>>, fs), value(Syntax.T, body)} <> acc}, scope, rest(Syntax.T, body))
    case PBranches{fibered, acc} Con{Token.Tok{Token.Pipe{}, loc}, tail}:
      Fail{error(loc, "expected a leg number or a constructor name after '|'")}
    case PBranches{fibered, acc} tokens:
      Done{BranchesValue{List.reverse(&2, Syntax.Branch<Syntax.T>, acc), tokens}}
    case PFields{acc} Con{Token.Tok{Token.LParen{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +bs : Parsed<List<&2, Syntax.Binder<Syntax.T>>> <- read_binders(parse(PBinderGroup{}, scope, Con{Token.Tok{Token.LParen{}, loc}, tail}))
        parse(PFields{List.append(&2, Syntax.Field<Syntax.T>, List.reverse(&2, Syntax.Field<Syntax.T>, fields(value(List<&2, Syntax.Binder<Syntax.T>>, bs))), acc)}, scope, rest(List<&2, Syntax.Binder<Syntax.T>>, bs))
    case PFields{acc} tokens:
      +q = mark(tokens)
      parse(PBareField{acc, tokens, value(Quantity.T, q)}, scope, rest(Quantity.T, q))
    case PBareField{acc, original, q} Con{Token.Tok{Token.Ident{name}, loc}, tail}:
      parse(PFields{Syntax.Field{q, name, None{}} <> acc}, scope, tail)
    case PBareField{acc, original, q} tokens:
      Done{FieldsValue{List.reverse(&2, Syntax.Field<Syntax.T>, acc), original}}
    case PDecls{acc} Con{Token.Tok{Token.Eof{}, loc}, tail}:
      Done{DeclsValue{List.reverse(&2, Syntax.Decl, acc), []}}
    case PDecls{acc} tokens:
      do Result<&2, &2, Error.T, Value>:
        +d : Parsed<Syntax.Decl> <- read_decl(parse(PDecl{}, scope, tokens))
        parse(PDecls{value(Syntax.Decl, d) <> acc}, scope, rest(Syntax.Decl, d))
    case PDecl{} Con{Token.Tok{Token.KDef{}, loc}, Con{Token.Tok{Token.KRec{}, loc}, tail}}:
      do Result<&2, &2, Error.T, Value>:
        +ds : Parsed<List<&2, Syntax.RecDef<Syntax.T>>> <- read_recs(parse(PRecGroup{[]}, scope, tail))
        return DeclValue{Syntax.DRec{value(List<&2, Syntax.RecDef<Syntax.T>>, ds)}, rest(List<&2, Syntax.RecDef<Syntax.T>>, ds)}
    case PDecl{} Con{Token.Tok{Token.KDef{}, loc}, Con{Token.Tok{Token.Ident{name}, nloc}, Con{Token.Tok{Token.Colon{}, cloc}, tail}}}:
      parse(PDef{False{}}, scope, Token.Tok{Token.Ident{name}, nloc} <> Token.Tok{Token.Colon{}, cloc} <> tail)
    case PDecl{} Con{Token.Tok{Token.KDef{}, loc}, Con{Token.Tok{Token.Ident{name}, nloc}, Con{Token.Tok{Token.LParen{}, ploc}, tail}}}:
      parse(PDef{False{}}, scope, Token.Tok{Token.Ident{name}, nloc} <> Token.Tok{Token.LParen{}, ploc} <> tail)
    case PDecl{} Con{Token.Tok{Token.KAxiom{}, loc}, Con{Token.Tok{Token.Ident{name}, loc}, Con{Token.Tok{Token.Colon{}, loc}, tail}}}:
      do Result<&2, &2, Error.T, Value>:
        +ty : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        return DeclValue{Syntax.DAxiom{name, value(Syntax.T, ty)}, rest(Syntax.T, ty)}
    case PDecl{} Con{Token.Tok{Token.KMu{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +fs : Parsed<List<&2, Syntax.Fam<Syntax.T>>> <- read_families(parse(PFamilyGroup{[]}, scope, tail))
        return DeclValue{Syntax.DMu{value(List<&2, Syntax.Fam<Syntax.T>>, fs)}, rest(List<&2, Syntax.Fam<Syntax.T>>, fs)}
    case PDecl{} Con{Token.Tok{Token.KMutual{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +fs : Parsed<List<&2, Syntax.Fam<Syntax.T>>> <- read_families(parse(PMutual{[]}, scope, tail))
        return DeclValue{Syntax.DMu{value(List<&2, Syntax.Fam<Syntax.T>>, fs)}, rest(List<&2, Syntax.Fam<Syntax.T>>, fs)}
    case PDecl{} Con{Token.Tok{Token.KNu{}, loc}, tail}:
      Fail{error(loc, "nu arrives at M2")}
    case PDecl{} Con{Token.Tok{Token.KPoly{}, loc}, tail}:
      extended_decl(Token.Tok{Token.KPoly{}, loc} <> tail, scope)
    case PDecl{} Con{Token.Tok{Token.KSpecialize{}, loc}, tail}:
      extended_decl(Token.Tok{Token.KSpecialize{}, loc} <> tail, scope)
    case PDecl{} tokens:
      expected(Value, "'def NAME :', 'def rec NAME :', 'axiom NAME :', 'mu NAME' or 'mutual'", tokens)
    case PDef{True{}} Con{Token.Tok{Token.Ident{name}, loc}, Con{Token.Tok{Token.Colon{}, loc}, tail}}:
      do Result<&2, &2, Error.T, Value>:
        +ty : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        tail : List<&2, Token.T> <- expect(Token.ColonEq{}, rest(Syntax.T, ty))
        +body : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        return RecValue{Syntax.RecDef{name, value(Syntax.T, ty), value(Syntax.T, body)}, rest(Syntax.T, body)}
    case PDef{False{}} Con{Token.Tok{Token.Ident{name}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +bs : Parsed<List<&2, Syntax.Binder<Syntax.T>>> <- read_binders(parse(PBinders{[]}, scope, tail))
        tail : List<&2, Token.T> <- expect(Token.Colon{}, rest(List<&2, Syntax.Binder<Syntax.T>>, bs))
        +ty : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        tail : List<&2, Token.T> <- expect(Token.ColonEq{}, rest(Syntax.T, ty))
        +body : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        return DeclValue{make_def(name, value(List<&2, Syntax.Binder<Syntax.T>>, bs), value(Syntax.T, ty), value(Syntax.T, body)), rest(Syntax.T, body)}
    case PDef{rec} tokens:
      expected(Value, "a definition name and ':'", tokens)
    case PRecGroup{acc} tokens:
      do Result<&2, &2, Error.T, Value>:
        +d : Parsed<Syntax.RecDef<Syntax.T>> <- read_rec(parse(PDef{True{}}, scope, tokens))
        parse(PRecRest{value(Syntax.RecDef<Syntax.T>, d) <> acc}, scope, rest(Syntax.RecDef<Syntax.T>, d))
    case PRecRest{acc} Con{Token.Tok{Token.KAnd{}, loc}, tail}:
      parse(PRecGroup{acc}, scope, tail)
    case PRecRest{acc} tokens:
      Done{RecsValue{List.reverse(&2, Syntax.RecDef<Syntax.T>, acc), tokens}}
    case PFamily{} Con{Token.Tok{Token.Ident{name}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +bs : Parsed<List<&2, Syntax.Binder<Syntax.T>>> <- read_binders(parse(PBinders{[]}, scope, tail))
        tail : List<&2, Token.T> <- expect(Token.Colon{}, rest(List<&2, Syntax.Binder<Syntax.T>>, bs))
        +ty : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        tail : List<&2, Token.T> <- family_separator(rest(Syntax.T, ty))
        +cs : Parsed<List<&2, Syntax.FamCtor<Syntax.T>>> <- read_ctors(parse(PFamilyCtors{[]}, scope, tail))
        return FamilyValue{Syntax.Fam{name, value(List<&2, Syntax.Binder<Syntax.T>>, bs), value(Syntax.T, ty), value(List<&2, Syntax.FamCtor<Syntax.T>>, cs)}, rest(List<&2, Syntax.FamCtor<Syntax.T>>, cs)}
    case PFamily{} tokens:
      expected(Value, "a family name", tokens)
    case PFamilyCtors{acc} Con{Token.Tok{Token.Pipe{}, loc}, Con{Token.Tok{Token.Ident{name}, loc}, tail}}:
      do Result<&2, &2, Error.T, Value>:
        +bs : Parsed<List<&2, Syntax.Binder<Syntax.T>>> <- read_binders(parse(PBinders{[]}, scope, tail))
        tail : List<&2, Token.T> <- expect(Token.Colon{}, rest(List<&2, Syntax.Binder<Syntax.T>>, bs))
        +ty : Parsed<Syntax.T> <- read_term(parse(PTerm{}, scope, tail))
        parse(PFamilyCtors{Syntax.FamCtor{name, arrows(value(List<&2, Syntax.Binder<Syntax.T>>, bs), value(Syntax.T, ty), False{})} <> acc}, scope, rest(Syntax.T, ty))
    case PFamilyCtors{acc} Con{Token.Tok{Token.Pipe{}, loc}, tail}:
      Fail{error(loc, "expected a constructor name and ':' after '|'")}
    case PFamilyCtors{acc} tokens:
      Done{CtorsValue{List.reverse(&2, Syntax.FamCtor<Syntax.T>, acc), tokens}}
    case PFamilyGroup{acc} tokens:
      do Result<&2, &2, Error.T, Value>:
        +f : Parsed<Syntax.Fam<Syntax.T>> <- read_family(parse(PFamily{}, scope, tokens))
        parse(PFamilyRest{value(Syntax.Fam<Syntax.T>, f) <> acc}, scope, rest(Syntax.Fam<Syntax.T>, f))
    case PFamilyRest{acc} Con{Token.Tok{Token.KAnd{}, loc}, tail}:
      parse(PFamilyGroup{acc}, scope, tail)
    case PFamilyRest{acc} tokens:
      Done{FamiliesValue{List.reverse(&2, Syntax.Fam<Syntax.T>, acc), tokens}}
    case PMutual{acc} Con{Token.Tok{Token.KMu{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +f : Parsed<Syntax.Fam<Syntax.T>> <- read_family(parse(PFamily{}, scope, tail))
        parse(PMutual{value(Syntax.Fam<Syntax.T>, f) <> acc}, scope, rest(Syntax.Fam<Syntax.T>, f))
    case PMutual{acc} Con{Token.Tok{Token.KEnd{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        fs : List<&2, Syntax.Fam<Syntax.T>> <- mutual_count(acc, loc)
        return FamiliesValue{fs, tail}
    case PMutual{acc} tokens:
      expected(Value, "'mu' or 'end' in a mutual group", tokens)

def finish_term(parsed: Parsed<Syntax.T>) -> Result<&2, &2, Error.T, Syntax.T>:
  match parsed:
    case Parsed{term, Con{Token.Tok{Token.Eof{}, loc}, Nil{}}}:
      Done{term}
    case Parsed{term, rest}:
      expected(Syntax.T, "end of input", rest)

def term(source: String) -> Result<&2, &2, Error.T, Syntax.T>:
  do Result<&2, &2, Error.T, Syntax.T>:
    tokens : List<&2, Token.T> <- Lexer.lex(source)
    parsed : Parsed<Syntax.T> <- read_term(parse(PTerm{}, [], tokens))
    finish_term(parsed)

def program(source: String) -> Result<&2, &2, Error.T, List<&2, Syntax.Decl>>:
  do Result<&2, &2, Error.T, List<&2, Syntax.Decl>>:
    tokens : List<&2, Token.T> <- Lexer.lex(source)
    parsed : Parsed<List<&2, Syntax.Decl>> <- read_decls(parse(PDecls{[]}, [], tokens))
    return value(List<&2, Syntax.Decl>, parsed)

def duplicate_universe(index: Maybe<&2, Nat>, name: String, loc: Token.Loc) -> Result<&2, &2, Error.T, Unit>:
  match index:
    case Some{n}:
      Fail{error(loc, "duplicate universe " ++ name)}
    case None{}:
      Done{Unit{}}

def check_universe_name(+name: String, +acc: List<&2, String>, +loc: Token.Loc) -> Result<&2, &2, Error.T, Unit>:
  Bool.pick(Result<&2, &2, Error.T, Unit>, String.eq(name, "succ") || String.eq(name, "max") || String.eq(name, "imax"),
    Fail{error(loc, "reserved universe name " ++ name)},
    duplicate_universe(index_of(acc, name, 0n), name, loc))

@unsafe
def universe_names(tokens: List<&2, Token.T>, +acc: List<&2, String>, entry: Bool) -> Result<&2, &2, Error.T, Parsed<List<&2, String>>>:
  match tokens entry:
    case Con{Token.Tok{Token.Ident{+name}, loc}, tail} True{}:
      do Result<&2, &2, Error.T, Parsed<List<&2, String>>>:
        checked : Unit <- check_universe_name(name, acc, loc)
        universe_names(tail, name <> acc, False{})
    case Con{Token.Tok{Token.Comma{}, loc}, tail} False{}:
      universe_names(tail, acc, True{})
    case Con{Token.Tok{Token.RParen{}, loc}, tail} False{}:
      Done{Parsed{List.reverse(&2, String, acc), tail}}
    case other True{}:
      expected(Parsed<List<&2, String>>, "a universe binder", other)
    case other False{}:
      expected(Parsed<List<&2, String>>, "',' or ')' after a universe binder", other)

@unsafe
def universe_arguments(entry: Bool, +tokens: List<&2, Token.T>, +scope: List<&2, String>, acc: List<&2, Universe.T>) -> Result<&2, &2, Error.T, Parsed<List<&2, Universe.T>>>:
  match entry tokens:
    case False{} Con{Token.Tok{Token.Comma{}, loc}, tail}:
      universe_arguments(True{}, tail, scope, acc)
    case False{} Con{Token.Tok{Token.RParen{}, loc}, tail}:
      Done{Parsed{List.reverse(&2, Universe.T, acc), tail}}
    case False{} other:
      expected(Parsed<List<&2, Universe.T>>, "',' or ')' after a universe argument", other)
    case True{} tokens:
      do Result<&2, &2, Error.T, Parsed<List<&2, Universe.T>>>:
        +u : Parsed<Universe.T> <- read_level(parse(PLevel{}, scope, tokens))
        universe_arguments(False{}, rest(Universe.T, u), scope, value(Universe.T, u) <> acc)

@unsafe
def mapping_list(entry: Bool, tokens: List<&2, Token.T>, acc: List<&2, Common.Pair<String, String>>) -> Result<&2, &2, Error.T, Parsed<List<&2, Common.Pair<String, String>>>>:
  match entry tokens:
    case True{} Con{Token.Tok{Token.Ident{local}, l1}, Con{Token.Tok{Token.ColonEq{}, l2}, Con{Token.Tok{Token.Ident{target}, l3}, tail}}}:
      mapping_list(False{}, tail, Common.Pair{local, target} <> acc)
    case False{} Con{Token.Tok{Token.Comma{}, loc}, tail}:
      mapping_list(True{}, tail, acc)
    case False{} Con{Token.Tok{Token.RParen{}, loc}, tail}:
      Done{Parsed{List.reverse(&2, Common.Pair<String, String>, acc), tail}}
    case True{} other:
      expected(Parsed<List<&2, Common.Pair<String, String>>>, "a family binding 'LOCAL := EXISTING'", other)
    case False{} other:
      expected(Parsed<List<&2, Common.Pair<String, String>>>, "',' or ')' after a family binding", other)

@unsafe
def export_mapping_list(entry: Bool, tokens: List<&2, Token.T>, acc: List<&2, Common.Pair<String, String>>) -> Result<&2, &2, Error.T, Parsed<List<&2, Common.Pair<String, String>>>>:
  match entry tokens:
    case True{} Con{Token.Tok{Token.Ident{local}, l1}, Con{Token.Tok{Token.ColonEq{}, l2}, Con{Token.Tok{Token.Ident{target}, l3}, tail}}}:
      export_mapping_list(False{}, tail, Common.Pair{local, target} <> acc)
    case True{} Con{Token.Tok{Token.Ident{local}, l1}, Con{Token.Tok{Token.ColonEq{}, l2}, tail}}:
      expected(Parsed<List<&2, Common.Pair<String, String>>>, "a global name after ':='", tail)
    case False{} Con{Token.Tok{Token.Comma{}, loc}, tail}:
      export_mapping_list(True{}, tail, acc)
    case False{} Con{Token.Tok{Token.RParen{}, loc}, tail}:
      Done{Parsed{List.reverse(&2, Common.Pair<String, String>, acc), tail}}
    case True{} other:
      expected(Parsed<List<&2, Common.Pair<String, String>>>, "a member export 'MEMBER := NAME'", other)
    case False{} other:
      expected(Parsed<List<&2, Common.Pair<String, String>>>, "',' or ')' after a member export", other)

def reuse_bindings(tokens: List<&2, Token.T>) -> Result<&2, &2, Error.T, Parsed<List<&2, Common.Pair<String, String>>>>:
  match tokens:
    case Con{Token.Tok{Token.KWith{}, l1}, Con{Token.Tok{Token.LParen{}, l2}, tail}}:
      mapping_list(True{}, tail, [])
    case Con{Token.Tok{Token.KWith{}, loc}, tail}:
      expected(Parsed<List<&2, Common.Pair<String, String>>>, "'(' and a nonempty list of family bindings", tail)
    case other:
      Done{Parsed{[], other}}

def export_bindings(tokens: List<&2, Token.T>) -> Result<&2, &2, Error.T, Parsed<Maybe<&2, List<&2, Common.Pair<String, String>>>>>:
  match tokens:
    case Con{Token.Tok{Token.Ident{SCon{'e', SCon{'x', SCon{'p', SCon{'o', SCon{'r', SCon{'t', SNil{}}}}}}}}, l1}, Con{Token.Tok{Token.UnitTok{}, l2}, tail}}:
      Done{Parsed{Some{[]}, tail}}
    case Con{Token.Tok{Token.Ident{SCon{'e', SCon{'x', SCon{'p', SCon{'o', SCon{'r', SCon{'t', SNil{}}}}}}}}, l1}, Con{Token.Tok{Token.LParen{}, l2}, Con{Token.Tok{Token.RParen{}, l3}, tail}}}:
      Done{Parsed{Some{[]}, tail}}
    case Con{Token.Tok{Token.Ident{SCon{'e', SCon{'x', SCon{'p', SCon{'o', SCon{'r', SCon{'t', SNil{}}}}}}}}, l1}, Con{Token.Tok{Token.LParen{}, l2}, tail}}:
      do Result<&2, &2, Error.T, Parsed<Maybe<&2, List<&2, Common.Pair<String, String>>>>>:
        +mapping : Parsed<List<&2, Common.Pair<String, String>>> <- export_mapping_list(True{}, tail, [])
        return Parsed{Some{value(List<&2, Common.Pair<String, String>>, mapping)}, rest(List<&2, Common.Pair<String, String>>, mapping)}
    case Con{Token.Tok{Token.Ident{SCon{'e', SCon{'x', SCon{'p', SCon{'o', SCon{'r', SCon{'t', SNil{}}}}}}}}, loc}, tail}:
      expected(Parsed<Maybe<&2, List<&2, Common.Pair<String, String>>>>, "'(' after 'export'", tail)
    case other:
      Done{Parsed{None{}, other}}

def specialization_tail(exports: Maybe<&2, List<&2, Common.Pair<String, String>>>, tokens: List<&2, Token.T>) -> Result<&2, &2, Error.T, Unit>:
  match exports tokens:
    case None{} Con{Token.Tok{Token.KWith{}, loc}, tail}:
      Fail{error(loc, "one 'with' clause per specialization")}
    case Some{mapping} Con{Token.Tok{Token.KWith{}, loc}, tail}:
      Fail{error(loc, "'with' must precede 'export' in a specialization")}
    case exports Con{Token.Tok{Token.Ident{SCon{'e', SCon{'x', SCon{'p', SCon{'o', SCon{'r', SCon{'t', SNil{}}}}}}}}, loc}, tail}:
      Fail{error(loc, "one export clause per specialization")}
    case exports other:
      Done{Unit{}}

def specialization_as(tokens: List<&2, Token.T>) -> Result<&2, &2, Error.T, Parsed<String>>:
  match tokens:
    case Con{Token.Tok{Token.KAs{}, l1}, Con{Token.Tok{Token.Ident{name}, l2}, tail}}:
      Done{Parsed{name, tail}}
    case other:
      expected(Parsed<String>, "'as' and a fresh name", other)

def specialization(tokens: List<&2, Token.T>, scope: List<&2, String>) -> Result<&2, &2, Error.T, Parsed<Syntax.Specialization>>:
  match tokens:
    case Con{Token.Tok{Token.KSpecialize{}, l1}, Con{Token.Tok{Token.Ident{name}, l2}, Con{Token.Tok{Token.LParen{}, l3}, tail}}}:
      do Result<&2, &2, Error.T, Parsed<Syntax.Specialization>>:
        +levels : Parsed<List<&2, Universe.T>> <- universe_arguments(True{}, tail, scope, [])
        +alias : Parsed<String> <- specialization_as(rest(List<&2, Universe.T>, levels))
        +reuse : Parsed<List<&2, Common.Pair<String, String>>> <- reuse_bindings(rest(String, alias))
        +exports : Parsed<Maybe<&2, List<&2, Common.Pair<String, String>>>> <- export_bindings(rest(List<&2, Common.Pair<String, String>>, reuse))
        checked : Unit <- specialization_tail(value(Maybe<&2, List<&2, Common.Pair<String, String>>>, exports), rest(Maybe<&2, List<&2, Common.Pair<String, String>>>, exports))
        return Parsed{Syntax.Specialization{name, value(List<&2, Universe.T>, levels), value(String, alias), value(List<&2, Common.Pair<String, String>>, reuse), value(Maybe<&2, List<&2, Common.Pair<String, String>>>, exports)}, rest(Maybe<&2, List<&2, Common.Pair<String, String>>>, exports)}
    case other:
      expected(Parsed<Syntax.Specialization>, "'specialize NAME (LEVELS) as NAME'", other)

def member(decl: Syntax.Decl, tokens: List<&2, Token.T>) -> Result<&2, &2, Error.T, Syntax.RecDef<Syntax.T>>:
  match decl:
    case Syntax.DDef{name, ty, body}:
      Done{Syntax.RecDef{name, ty, body}}
    case other:
      expected(Syntax.RecDef<Syntax.T>, "a nonrecursive member definition", tokens)

def finish_members(acc: List<&2, Syntax.RecDef<Syntax.T>>, tail: List<&2, Token.T>, loc: Token.Loc) -> Result<&2, &2, Error.T, Parsed<List<&2, Syntax.RecDef<Syntax.T>>>>:
  match acc:
    case Nil{}:
      expected(Parsed<List<&2, Syntax.RecDef<Syntax.T>>>, "a nonempty list of member definitions", [Token.Tok{Token.KEnd{}, loc}])
    case Con{head, rest}:
      Done{Parsed{List.reverse(&2, Syntax.RecDef<Syntax.T>, head <> rest), tail}}

def member_start(tokens: List<&2, Token.T>) -> Bool:
  match tokens:
    case Con{Token.Tok{Token.Ident{name}, loc}, tail}: True{}
    case Con{Token.Tok{Token.KRec{}, loc}, tail}: True{}
    case other: False{}

def member_input(valid: Bool, tokens: List<&2, Token.T>) -> Result<&2, &2, Error.T, List<&2, Token.T>>:
  match valid:
    case True{}: Done{tokens}
    case False{}: expected(List<&2, Token.T>, "'def' or 'end' in family members", tokens)

@unsafe
def members(tokens: List<&2, Token.T>, +scope: List<&2, String>, acc: List<&2, Syntax.RecDef<Syntax.T>>) -> Result<&2, &2, Error.T, Parsed<List<&2, Syntax.RecDef<Syntax.T>>>>:
  match tokens:
    case Con{Token.Tok{Token.KEnd{}, loc}, tail}:
      finish_members(acc, tail, loc)
    case Con{Token.Tok{Token.KDef{}, +loc}, +tail}:
      do Result<&2, &2, Error.T, Parsed<List<&2, Syntax.RecDef<Syntax.T>>>>:
        +input : List<&2, Token.T> <- member_input(member_start(tail), Token.Tok{Token.KDef{}, loc} <> tail)
        +decl : Parsed<Syntax.Decl> <- read_decl(parse(PDecl{}, scope, input))
        entry : Syntax.RecDef<Syntax.T> <- member(value(Syntax.Decl, decl), input)
        members(rest(Syntax.Decl, decl), scope, entry <> acc)
    case other:
      expected(Parsed<List<&2, Syntax.RecDef<Syntax.T>>>, "'def' or 'end' in family members", other)

def optional_members(tokens: List<&2, Token.T>, scope: List<&2, String>) -> Result<&2, &2, Error.T, Parsed<List<&2, Syntax.RecDef<Syntax.T>>>>:
  match tokens:
    case Con{Token.Tok{Token.KWhere{}, loc}, tail}:
      members(tail, scope, [])
    case other:
      Done{Parsed{[], other}}

def finish_dependencies(acc: List<&2, Syntax.Specialization>, tail: List<&2, Token.T>) -> Result<&2, &2, Error.T, Parsed<List<&2, Syntax.Specialization>>>:
  match acc:
    case Nil{}:
      expected(Parsed<List<&2, Syntax.Specialization>>, "at least one template specialization in a group", tail)
    case Con{head, rest}:
      Done{Parsed{List.reverse(&2, Syntax.Specialization, head <> rest), tail}}

@unsafe
def dependencies(tokens: List<&2, Token.T>, +scope: List<&2, String>, acc: List<&2, Syntax.Specialization>) -> Result<&2, &2, Error.T, Parsed<List<&2, Syntax.Specialization>>>:
  match tokens:
    case Con{Token.Tok{Token.KSpecialize{}, loc}, tail}:
      do Result<&2, &2, Error.T, Parsed<List<&2, Syntax.Specialization>>>:
        +entry : Parsed<Syntax.Specialization> <- specialization(Token.Tok{Token.KSpecialize{}, loc} <> tail, scope)
        dependencies(rest(Syntax.Specialization, entry), scope, value(Syntax.Specialization, entry) <> acc)
    case other:
      finish_dependencies(acc, other)

def compose_members(tokens: List<&2, Token.T>, scope: List<&2, String>) -> Result<&2, &2, Error.T, Parsed<List<&2, Syntax.RecDef<Syntax.T>>>>:
  match tokens:
    case Con{Token.Tok{Token.KEnd{}, loc}, tail}:
      Done{Parsed{[], tail}}
    case other:
      members(other, scope, [])

def poly_family(arity: Nat, families: List<&2, Syntax.Fam<Syntax.T>>, definitions: List<&2, Syntax.RecDef<Syntax.T>>) -> Result<&2, &2, Error.T, Syntax.Decl>:
  match families definitions:
    case Con{family, Nil{}} Nil{}:
      Done{Syntax.DPolyMu{arity, family}}
    case Con{family, tail} definitions:
      Done{Syntax.DPolyGroup{arity, family, tail, definitions}}
    case Nil{} definitions:
      Fail{Error.Parse{"expected a nonempty family group", 1n, 1n}}

def poly_def(arity: Nat, decl: Syntax.Decl, loc: Token.Loc) -> Result<&2, &2, Error.T, Syntax.Decl>:
  match decl:
    case Syntax.DDef{name, ty, body}:
      Done{Syntax.DPoly{arity, name, ty, body}}
    case other:
      expected(Syntax.Decl, "a nonrecursive definition or single family after universe binders", [Token.Tok{Token.KPoly{}, loc}])

def scoped_poly(tokens: List<&2, Token.T>, +scope: List<&2, String>, original_loc: Token.Loc) -> Result<&2, &2, Error.T, Value>:
  match tokens:
    case Con{Token.Tok{Token.KGroup{}, l1}, Con{Token.Tok{Token.Ident{name}, l2}, Con{Token.Tok{Token.KWhere{}, l3}, tail}}}:
      do Result<&2, &2, Error.T, Value>:
        +deps : Parsed<List<&2, Syntax.Specialization>> <- dependencies(tail, scope, [])
        +defs : Parsed<List<&2, Syntax.RecDef<Syntax.T>>> <- compose_members(rest(List<&2, Syntax.Specialization>, deps), scope)
        return DeclValue{Syntax.DPolyCompose{List.length(&2, String, scope), name, value(List<&2, Syntax.Specialization>, deps), value(List<&2, Syntax.RecDef<Syntax.T>>, defs)}, rest(List<&2, Syntax.RecDef<Syntax.T>>, defs)}
    case Con{Token.Tok{Token.KGroup{}, loc}, tail}:
      expected(Value, "'group NAME where' after universe binders", Token.Tok{Token.KGroup{}, loc} <> tail)
    case Con{Token.Tok{Token.KMu{}, loc}, tail}:
      do Result<&2, &2, Error.T, Value>:
        +families : Parsed<List<&2, Syntax.Fam<Syntax.T>>> <- read_families(parse(PFamilyGroup{[]}, scope, tail))
        +definitions : Parsed<List<&2, Syntax.RecDef<Syntax.T>>> <- optional_members(rest(List<&2, Syntax.Fam<Syntax.T>>, families), scope)
        decl : Syntax.Decl <- poly_family(List.length(&2, String, scope), value(List<&2, Syntax.Fam<Syntax.T>>, families), value(List<&2, Syntax.RecDef<Syntax.T>>, definitions))
        return DeclValue{decl, rest(List<&2, Syntax.RecDef<Syntax.T>>, definitions)}
    case other:
      do Result<&2, &2, Error.T, Value>:
        +parsed : Parsed<Syntax.Decl> <- read_decl(parse(PDecl{}, scope, other))
        decl : Syntax.Decl <- poly_def(List.length(&2, String, scope), value(Syntax.Decl, parsed), original_loc)
        return DeclValue{decl, rest(Syntax.Decl, parsed)}

@unsafe
def extended_decl(tokens, scope):
  match tokens:
    case Con{Token.Tok{Token.KPoly{}, l1}, Con{Token.Tok{Token.LParen{}, l2}, tail}}:
      do Result<&2, &2, Error.T, Value>:
        +universes : Parsed<List<&2, String>> <- universe_names(tail, [], True{})
        scoped_poly(rest(List<&2, String>, universes), value(List<&2, String>, universes), l1)
    case Con{Token.Tok{Token.KPoly{}, loc}, tail}:
      expected(Value, "'(' and a nonempty list of universe binders after 'poly'", Token.Tok{Token.KPoly{}, loc} <> tail)
    case Con{Token.Tok{Token.KSpecialize{}, l1}, Con{Token.Tok{Token.Ident{name}, l2}, Con{Token.Tok{Token.LParen{}, l3}, tail}}}:
      do Result<&2, &2, Error.T, Value>:
        +entry : Parsed<Syntax.Specialization> <- specialization(Token.Tok{Token.KSpecialize{}, l1} <> Token.Tok{Token.Ident{name}, l2} <> Token.Tok{Token.LParen{}, l3} <> tail, scope)
        return DeclValue{Syntax.DSpecialize{value(Syntax.Specialization, entry)}, rest(Syntax.Specialization, entry)}
    case other:
      expected(Value, "NAME, '(' with a nonempty list of universe levels, ')', 'as' and a name after 'specialize'", other)
