import Base
import ../wasm/gc_encode.bend as G

def bytes_text(xs: List<&2, U32>) -> String:
  match xs:
    case Nil{}: ""
    case Con{x, Nil{}}: U32.show(x)
    case Con{x, rest}: U32.show(x) ++ "," ++ bytes_text(rest)

def fixture() -> G.T:
  G.Module{
    [[G.CFunc{[], [G.I32{}]}],
     [G.CStruct{[G.I32{}]}],
     [G.CArray{G.I32{}}],
     [G.CFunc{[], [G.Ref{G.HI31{}}]}],
     [G.CStruct{[G.Ref{G.HType{5}}]}, G.CFunc{[G.Ref{G.HType{4}}], [G.I32{}]}]],
    [],
    [G.Func{0, [], [G.I32_const{42}, G.Ref_i31{}, G.I31_get_u{}]},
     G.Func{0, [], [G.I32_const{42}, G.Struct_new{1}, G.Struct_get{1, 0}]},
     G.Func{0, [], [G.I32_const{42}, G.I32_const{3}, G.Array_new{2}, G.I32_const{1}, G.Array_get{2}]},
     G.Func{0, [], [G.I32_const{1}, G.If{Some{G.I32{}}, [G.I32_const{42}], [G.I32_const{0}]}]},
     G.Func{0, [], [G.Return_call{0}]},
     G.Func{0, [G.Ref{G.HType{4}}], [G.Ref_func{6}, G.Struct_new{4}, G.Local_set{0}, G.Local_get{0}, G.Local_get{0}, G.Struct_get{4, 0}, G.Call_ref{5}]},
     G.Func{5, [], [G.I32_const{42}]}],
    [G.Export{"i31", 0}, G.Export{"struct", 1}, G.Export{"array", 2}, G.Export{"if", 3}, G.Export{"tail", 4}, G.Export{"closure", 5}],
    [6]}

def effects() -> G.T:
  G.Module{
    [[G.CFunc{[G.I32{}], [G.I32{}]}], [G.CFunc{[], [G.I32{}]}]],
    [G.Import{"host", "effect", 0}],
    [G.Func{1, [], [G.I32_const{40}, G.Call{0}]}],
    [G.Export{"run", 1}],
    []}

def main() -> IO(Unit):
  do IO<Unit>:
    IO.print("[" ++ bytes_text(G.encode(fixture())) ++ "]")
    IO.print("[" ++ bytes_text(G.encode(effects())) ++ "]")
    IO.print("[" ++ bytes_text(G.uleb(4294967295)) ++ "]")
    IO.print("[" ++ bytes_text(G.sleb(2147483648)) ++ "]")
    IO.print("[" ++ bytes_text(G.sleb(4294967295)) ++ "]")

