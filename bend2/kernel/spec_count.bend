import Base
import ./rules.bend as Rules
import ./term.bend as Term
import ./shape.bend as Shape
import ./common.bend as P

def join(items: List<&2,String>) -> String:
  match items:
    case Nil{}: ""
    case Con{x,Nil{}}: x
    case Con{x,xs}: x ++ " " ++ join(xs)

def pick(index: Nat, items: List<&2,String>) -> String:
  match index items:
    case 0n Con{x,xs}: x
    case 1n+n Con{x,xs}: pick(n,xs)
    case _ Nil{}: "?"

def eta_names(rows: List<&2,P.Pair<String,Rules.EtaRow>>, +keep: Bool) -> List<&2,String>:
  match rows:
    case Nil{}: []
    case Con{P.Pair{+name,Rules.EtaRow{ran,lan}},rest}:
      +formers = Term.formers()
      List.append(&2,String,Bool.pick(List<&2,String>,Bool.not(Bool.xor(ran,keep)),[pick(1n,formers) ++ "-" ++ name],[]),List.append(&2,String,Bool.pick(List<&2,String>,Bool.not(Bool.xor(lan,keep)),[pick(0n,formers) ++ "-" ++ name],[]),eta_names(rest,keep)))

def row(label: String, +items: List<&2,String>) -> String:
  label ++ " " ++ Nat.show(List.length(&2,String,items)) ++ ": " ++ join(items) ++ "\n"

def print() -> String:
  row("formers",Term.formers()) ++ row("schema constructors",Term.schema()) ++ row("shapes declared",Shape.declared()) ++ row("shapes admitted",Rules.admitted()) ++ row("named rules declared",Rules.named_declared()) ++ row("named rules present",Rules.named_present()) ++ row("eta rows",eta_names(Rules.eta_table(),True{})) ++ row("no eta",eta_names(Rules.eta_table(),False{}))
