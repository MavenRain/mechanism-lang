import Base
import ../import/export.bend as E
import ../import/mapping.bend as M
import ../kernel/common.bend as C

def source() -> String:
  "{\"meta\":{\"exporter\":{\"name\":\"lean4export\",\"version\":\"3.1.0\"},\"format\":{\"version\":\"3.1.0\"},\"lean\":{\"githash\":\"test\",\"version\":\"4.31.0\"}}}\n{\"in\":1,\"str\":{\"pre\":0,\"str\":\"Zeta\"}}\n{\"in\":2,\"str\":{\"pre\":0,\"str\":\"Alpha\"}}\n{\"in\":3,\"str\":{\"pre\":0,\"str\":\"Alpha\"}}\n{\"in\":4,\"str\":{\"pre\":0,\"str\":\"Unused\"}}\n{\"in\":5,\"str\":{\"pre\":0,\"str\":\"UnifiedAggregation\"}}\n{\"in\":6,\"str\":{\"pre\":5,\"str\":\"inside\"}}\n{\"in\":7,\"str\":{\"pre\":0,\"str\":\"UnifiedAggregationExtra\"}}\n{\"in\":8,\"str\":{\"pre\":7,\"str\":\"outside\"}}\n{\"in\":9,\"str\":{\"pre\":0,\"str\":\"Lean\"}}\n{\"in\":10,\"str\":{\"pre\":9,\"str\":\"Omega\"}}\n{\"in\":11,\"str\":{\"pre\":10,\"str\":\"reflect\"}}\n{\"in\":12,\"str\":{\"pre\":9,\"str\":\"OmegaExtra\"}}\n{\"in\":13,\"str\":{\"pre\":12,\"str\":\"ordinary\"}}\n{\"in\":14,\"str\":{\"pre\":0,\"str\":\"_private\"}}\n{\"in\":15,\"str\":{\"pre\":14,\"str\":\"Init\"}}\n{\"in\":16,\"str\":{\"pre\":15,\"str\":\"Omega\"}}\n{\"in\":17,\"str\":{\"pre\":16,\"str\":\"helper\"}}\n{\"in\":18,\"str\":{\"pre\":9,\"str\":\"Syntax\"}}\n{\"in\":19,\"str\":{\"pre\":0,\"str\":\"Array\"}}\n{\"in\":20,\"str\":{\"pre\":19,\"str\":\"push\"}}\n{\"in\":21,\"str\":{\"pre\":0,\"str\":\"ArrowCat\"}}\n{\"in\":22,\"str\":{\"pre\":21,\"str\":\"inside\"}}\n{\"in\":23,\"str\":{\"pre\":0,\"str\":\"CompCatTheory\"}}\n{\"in\":24,\"str\":{\"pre\":23,\"str\":\"inside\"}}\n{\"ie\":0,\"sort\":0}\n{\"ie\":1,\"const\":{\"name\":1,\"us\":[]}}\n{\"ie\":2,\"const\":{\"name\":2,\"us\":[]}}\n{\"ie\":3,\"const\":{\"name\":3,\"us\":[]}}\n{\"ie\":4,\"const\":{\"name\":6,\"us\":[]}}\n{\"ie\":5,\"const\":{\"name\":8,\"us\":[]}}\n{\"ie\":6,\"const\":{\"name\":11,\"us\":[]}}\n{\"ie\":7,\"const\":{\"name\":13,\"us\":[]}}\n{\"ie\":8,\"const\":{\"name\":17,\"us\":[]}}\n{\"ie\":9,\"const\":{\"name\":18,\"us\":[]}}\n{\"ie\":10,\"const\":{\"name\":20,\"us\":[]}}\n{\"ie\":11,\"const\":{\"name\":22,\"us\":[]}}\n{\"ie\":12,\"const\":{\"name\":24,\"us\":[]}}\n{\"axiom\":{\"name\":1,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":2,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":6,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":8,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":11,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":13,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":17,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":18,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":20,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":22,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":24,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n"

def export_result(r:Result<&2,&2,E.Error,E.T>) -> Result<&2,&2,String,E.T>:
  match r:
    case Done{x}: Done{x}
    case Fail{E.ImportError{line,message}}: Fail{message}

def refused(r:Result<&2,&2,String,M.T>) -> Bool:
  match r:
    case Fail{message}: True{}
    case Done{x}: False{}

def row_names(rows:List<&2,M.Row>) -> String:
  match rows:
    case Nil{}: ""
    case Con{M.Row{name,target,verdict},rest}: name ++ "\n" ++ row_names(rest)

def names(inventory:M.T) -> String:
  match inventory:
    case M.Inventory{rows,never,declared,constants}: row_names(rows)

def counts(inventory:M.T) -> Bool:
  match inventory:
    case M.Inventory{rows,never,declared,constants}:
      Bool.and(Nat.is_eq(List.length(&2,M.Row,rows),8n),Bool.and(Nat.is_eq(declared,9n),Nat.is_eq(constants,11n)))

def target_eq(a:Maybe<&2,String>,b:Maybe<&2,String>) -> Bool:
  match a b:
    case None{} None{}: True{}
    case Some{x} Some{y}: String.eq(x,y)
    case _ _: False{}

def has_row(rows:List<&2,M.Row>,+name:String,+target:Maybe<&2,String>,+verdict:String) -> Bool:
  match rows:
    case Nil{}: False{}
    case Con{M.Row{n,t,v},rest}:
      Bool.or(Bool.and(String.eq(n,name),Bool.and(target_eq(t,target),String.eq(M.verdict_name(v),verdict))),has_row(rest,name,target,verdict))

def mapped_rows(inventory:M.T) -> Bool:
  match inventory:
    case M.Inventory{+rows,never,declared,constants}:
      Bool.and(has_row(rows,"Alpha",Some{"MechAlpha"},"NAME_ONLY"),has_row(rows,"Zeta",None{},"UNMAPPED"))

def cases() -> Result<&2,&2,String,List<&2,C.Pair<String,Bool>>>:
  do Result<&2,&2,String,List<&2,C.Pair<String,Bool>>>:
    +source : E.T <- export_result(E.read_string(source()))
    +base : M.T <- M.inventory(source,[])
    mapped : M.T <- M.inventory(source,[M.Target{"Alpha","MechAlpha"}])
    +first : M.T <- M.inventory(source,[M.Target{"Zeta","MechZeta"},M.Target{"Alpha","MechAlpha"}])
    reordered : M.T <- M.inventory(source,[M.Target{"Alpha","MechAlpha"},M.Target{"Zeta","MechZeta"}])
    return [
      C.Pair{"canonical-identities-and-stable-order",String.eq(names(base),"Alpha\nArray.push\nLean.Omega.reflect\nLean.OmegaExtra.ordinary\nLean.Syntax\nUnifiedAggregationExtra.outside\nZeta\n_private.Init.Omega.helper\n")},
      C.Pair{"declared-and-referenced-counts-stay-distinct",counts(base)},
      C.Pair{"only-exact-public-omega-namespace-is-never",String.eq(M.never_tsv(base),"lean_name\tratification\tmilestone\nLean.Omega.reflect\tR-V4:omega-reflection\tNEVER\n")},
      C.Pair{"candidate-never-claims-type-parity",mapped_rows(mapped)},
      C.Pair{"duplicate-source-target-refused",refused(M.inventory(source,[M.Target{"Alpha","One"},M.Target{"Alpha","Two"}]))},
      C.Pair{"unknown-source-target-refused",refused(M.inventory(source,[M.Target{"Missing","One"}]))},
      C.Pair{"unreferenced-source-target-refused",refused(M.inventory(source,[M.Target{"Unused","One"}]))},
      C.Pair{"in-house-source-target-refused",refused(M.inventory(source,[M.Target{"UnifiedAggregation.inside","One"}]))},
      C.Pair{"never-target-conflict-refused",refused(M.inventory(source,[M.Target{"Lean.Omega.reflect","One"}]))},
      C.Pair{"tab-target-refused",refused(M.inventory(source,[M.Target{"Alpha","bad\ttarget"}]))},
      C.Pair{"newline-target-refused",refused(M.inventory(source,[M.Target{"Alpha","bad\ntarget"}]))},
      C.Pair{"empty-target-refused",refused(M.inventory(source,[M.Target{"Alpha",""}]))},
      C.Pair{"sentinel-target-refused",refused(M.inventory(source,[M.Target{"Alpha","-"}]))},
      C.Pair{"tsv-schema-is-stable",Bool.and(String.eq(M.map_tsv(first),M.map_tsv(reordered)),Bool.and(String.starts_with(M.map_tsv(first),"lean_name\ttarget\tverdict\nAlpha\tMechAlpha\tNAME_ONLY\n"),String.eq(M.never_tsv(first),"lean_name\tratification\tmilestone\nLean.Omega.reflect\tR-V4:omega-reflection\tNEVER\n")))}]

def report(ok:Bool,name:String) -> IO(Nat):
  match ok:
    case True{}: do IO<Nat>:
      IO.print("PASS mapping " ++ name)
      return 0n
    case False{}: do IO<Nat>:
      IO.print("FAIL mapping " ++ name)
      return 1n

def verdict(failed:Nat) -> IO(Unit):
  match failed:
    case Zero{}: IO.print("MAPPING-OK")
    case Succ{n}: IO.die(Unit,1,"MAPPING-FAIL " ++ Nat.show(Succ{n}) ++ " cases")

def reports(rows:List<&2,C.Pair<String,Bool>>,failed:Nat) -> IO(Unit):
  match rows:
    case Nil{}: verdict(failed)
    case Con{C.Pair{name,ok},rest}: do IO<Unit>:
      bad : Nat <- report(ok,name)
      reports(rest,Nat.add(failed,bad))

def result(r:Result<&2,&2,String,List<&2,C.Pair<String,Bool>>>) -> IO(Unit):
  match r:
    case Fail{message}: IO.die(Unit,1,"MAPPING-FAIL " ++ message)
    case Done{rows}: reports(rows,0n)

def arguments(args:List<String>) -> IO(Unit):
  result(cases())

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
