import Base

type T is Data:
  Parse{message: String, line: Nat, column: Nat}
  Not_yet{message: String}
  Carry{message: String}
  Unbound{message: String}
  Mismatch{message: String}
  Universe{message: String}
  Quantity{message: String}
  Wrong_leg{message: String}
  Missing_branch{message: String}
  Overflow{message: String}
  Cannot_infer{message: String}
  Budget_exhausted{message: String}
  Index_not_zero{message: String}
  Index_above_universe{message: String}
  Termination{message: String}

def termination_msg(name: String) -> String:
  "recursive definition " ++ name ++ " failed the structural termination guard"

def message(e: T) -> String:
  match e:
    case Parse{m, line, column}:
      m
    case Not_yet{m}:
      m
    case Carry{m}:
      m
    case Unbound{m}:
      m
    case Mismatch{m}:
      m
    case Universe{m}:
      m
    case Quantity{m}:
      m
    case Wrong_leg{m}:
      m
    case Missing_branch{m}:
      m
    case Overflow{m}:
      m
    case Cannot_infer{m}:
      m
    case Budget_exhausted{m}:
      m
    case Index_not_zero{m}:
      m
    case Index_above_universe{m}:
      m
    case Termination{m}:
      termination_msg(m)

def to_string(e: T) -> String:
  match e:
    case Parse{m, line, column}:
      "line " ++ Nat.show(line) ++ ", column " ++ Nat.show(column) ++ ": " ++ m
    case Not_yet{m}:
      "not yet: " ++ m
    case Carry{m}:
      "carry: " ++ m
    case Unbound{m}:
      "unbound: " ++ m
    case Mismatch{m}:
      "mismatch: " ++ m
    case Universe{m}:
      "universe: " ++ m
    case Quantity{m}:
      "quantity: " ++ m
    case Wrong_leg{m}:
      "wrong leg: " ++ m
    case Missing_branch{m}:
      "missing branch: " ++ m
    case Overflow{m}:
      "overflow: " ++ m
    case Cannot_infer{m}:
      "cannot infer: " ++ m
    case Budget_exhausted{m}:
      "budget: " ++ m
    case Index_not_zero{m}:
      "index not zero: " ++ m
    case Index_above_universe{m}:
      "index above universe: " ++ m
    case Termination{m}:
      "termination: " ++ termination_msg(m)
