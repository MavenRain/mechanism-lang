import Base

type Loc is Data:
  Loc{line: Nat, col: Nat}

type KindT is Data:
  LParen{}
  RParen{}
  Colon{}
  ColonEq{}
  Arrow{}
  DArrow{}
  Star{}
  Comma{}
  Dot{}
  Dot1{}
  Dot2{}
  Pipe{}
  UnitTok{}
  KDef{}
  KAxiom{}
  KFun{}
  KInj{}
  KOf{}
  KCase{}
  KMatch{}
  KAs{}
  KReturn{}
  KWith{}
  KTuple{}
  KSum{}
  KProd{}
  KAbsurd{}
  KProp{}
  KType{}
  KSort{}
  KPoly{}
  KGroup{}
  KSpecialize{}
  KWhere{}
  KLet{}
  KIn{}
  KAuto{}
  KMu{}
  KMutual{}
  KEnd{}
  KNu{}
  KZk{}
  KProve{}
  KVerify{}
  KFhc{}
  KEnc{}
  KEval{}
  KDec{}
  KMpc{}
  KInput{}
  KShare{}
  KOpen{}
  LBracket{}
  RBracket{}
  KAnd{}
  KRec{}
  KNatAdd{}
  KNatSub{}
  KNatMul{}
  KNatEq{}
  KNatLt{}
  Ident{name: String}
  NatTok{digits: String}
  Bytes{bytes: List<&2, U32>}
  Eof{}

type T is Data:
  Tok{kind: KindT, loc: Loc}

def start() -> Loc:
  Loc{1n, 1n}

def advance(l: Loc, n: Nat) -> Loc:
  match l:
    case Loc{line, col}:
      Loc{line, Nat.add(col, n)}

def next_line(l: Loc) -> Loc:
  match l:
    case Loc{line, col}:
      Loc{1n+line, 1n}

def describe(k: KindT) -> String:
  match k:
    case LParen{}:
      "'('"
    case RParen{}:
      "')'"
    case Colon{}:
      "':'"
    case ColonEq{}:
      "':='"
    case Arrow{}:
      "'->'"
    case DArrow{}:
      "'=>'"
    case Star{}:
      "'*'"
    case Comma{}:
      "','"
    case Dot{}:
      "'.'"
    case Dot1{}:
      "'.1'"
    case Dot2{}:
      "'.2'"
    case Pipe{}:
      "'|'"
    case UnitTok{}:
      "'()'"
    case KDef{}:
      "'def'"
    case KAxiom{}:
      "'axiom'"
    case KFun{}:
      "'fun'"
    case KInj{}:
      "'inj'"
    case KOf{}:
      "'of'"
    case KCase{}:
      "'case'"
    case KMatch{}:
      "'match'"
    case KAs{}:
      "'as'"
    case KReturn{}:
      "'return'"
    case KWith{}:
      "'with'"
    case KTuple{}:
      "'tuple'"
    case KSum{}:
      "'sum'"
    case KProd{}:
      "'prod'"
    case KAbsurd{}:
      "'absurd'"
    case KProp{}:
      "'Prop'"
    case KType{}:
      "'Type'"
    case KSort{}:
      "'Sort'"
    case KPoly{}:
      "'poly'"
    case KGroup{}:
      "'group'"
    case KSpecialize{}:
      "'specialize'"
    case KWhere{}:
      "'where'"
    case KLet{}:
      "'let'"
    case KIn{}:
      "'in'"
    case KAuto{}:
      "'auto'"
    case KMu{}:
      "'mu'"
    case KMutual{}:
      "'mutual'"
    case KEnd{}:
      "'end'"
    case KNu{}:
      "'nu'"
    case KZk{}:
      "'zk'"
    case KProve{}:
      "'prove'"
    case KVerify{}:
      "'verify'"
    case KFhc{}:
      "'fhc'"
    case KEnc{}:
      "'enc'"
    case KEval{}:
      "'eval'"
    case KDec{}:
      "'dec'"
    case KMpc{}:
      "'mpc'"
    case KInput{}:
      "'input'"
    case KShare{}:
      "'share'"
    case KOpen{}:
      "'open'"
    case LBracket{}:
      "'['"
    case RBracket{}:
      "']'"
    case KAnd{}:
      "'and'"
    case KRec{}:
      "'rec'"
    case KNatAdd{}:
      "'natAdd'"
    case KNatSub{}:
      "'natSub'"
    case KNatMul{}:
      "'natMul'"
    case KNatEq{}:
      "'natEq'"
    case KNatLt{}:
      "'natLt'"
    case Ident{name}:
      "identifier " ++ name
    case NatTok{digits}:
      "number " ++ digits
    case Bytes{bytes}:
      "byte literal"
    case Eof{}:
      "end of input"

def tag(k: KindT) -> U32:
  match k:
    case LParen{}:
      0
    case RParen{}:
      1
    case Colon{}:
      2
    case ColonEq{}:
      3
    case Arrow{}:
      4
    case DArrow{}:
      5
    case Star{}:
      6
    case Comma{}:
      7
    case Dot{}:
      8
    case Dot1{}:
      9
    case Dot2{}:
      10
    case Pipe{}:
      11
    case UnitTok{}:
      12
    case KDef{}:
      13
    case KAxiom{}:
      14
    case KFun{}:
      15
    case KInj{}:
      16
    case KOf{}:
      17
    case KCase{}:
      18
    case KMatch{}:
      19
    case KAs{}:
      20
    case KReturn{}:
      21
    case KWith{}:
      22
    case KTuple{}:
      23
    case KSum{}:
      24
    case KProd{}:
      25
    case KAbsurd{}:
      26
    case KProp{}:
      27
    case KType{}:
      28
    case KSort{}:
      29
    case KPoly{}:
      30
    case KGroup{}:
      31
    case KSpecialize{}:
      32
    case KWhere{}:
      33
    case KLet{}:
      34
    case KIn{}:
      35
    case KAuto{}:
      36
    case KMu{}:
      37
    case KMutual{}:
      38
    case KEnd{}:
      39
    case KNu{}:
      40
    case KZk{}:
      41
    case KProve{}:
      42
    case KVerify{}:
      43
    case KFhc{}:
      44
    case KEnc{}:
      45
    case KEval{}:
      46
    case KDec{}:
      47
    case KMpc{}:
      48
    case KInput{}:
      49
    case KShare{}:
      50
    case KOpen{}:
      51
    case LBracket{}:
      52
    case RBracket{}:
      53
    case KAnd{}:
      54
    case KRec{}:
      55
    case KNatAdd{}:
      56
    case KNatSub{}:
      57
    case KNatMul{}:
      58
    case KNatEq{}:
      59
    case KNatLt{}:
      60
    case Ident{name}:
      61
    case NatTok{digits}:
      62
    case Bytes{bytes}:
      63
    case Eof{}:
      64
