import Base
import ./gc_encode.bend as G
import ./link.bend as L
import ../kernel/error.bend as Err

type Runtime is Data:
  Runtime{array:U32, big:U32, length:U32, digit:U32, copy:U32, normal:U32, cmp_digits:U32, cmp:U32, add_loop:U32, sub_loop:U32, mul_loop:U32, slow_add:U32, slow_sub:U32, slow_mul:U32}

type Body is Data:
  Body{locals:List<&2,G.Valtype>,code:List<&2,G.Instr>}

def cat(a:List<&2,G.Instr>,b:List<&2,G.Instr>) -> List<&2,G.Instr>:
  List.append(&2,G.Instr,a,b)

def low_limb(+i:U32) -> List<&2,G.Instr>:
  [G.Local_get{i},G.Local_get{i},G.I32_const{32768},G.I32_div_u{},G.I32_const{32768},G.I32_mul{},G.I32_sub{}]

def nat_length(r:Runtime) -> Body:
  match r:
    case Runtime{+r_array, +r_big, +r_length, +r_digit, +r_copy, +r_normal, +r_cmp_digits, +r_cmp, +r_add_loop, +r_sub_loop, +r_mul_loop, +r_slow_add, +r_slow_sub, +r_slow_mul}:
      Body{[G.I32{}], [G.Block{Some{G.Ref{G.HI31{}}}, [G.Local_get{0}, G.Br_on_cast{0, G.HEq{}, G.HI31{}}, G.Ref_cast{G.HType{r_big}}, G.Struct_get{r_big, 1}, G.Array_len{}, G.Return{}]}, G.I31_get_u{}, G.Local_set{1}, G.Local_get{1}, G.I32_const{0}, G.I32_eq{}, G.If{Some{G.I32{}}, [G.I32_const{0}], [G.Local_get{1}, G.I32_const{32768}, G.I32_lt_u{}, G.If{Some{G.I32{}}, [G.I32_const{1}], [G.I32_const{2}]}]}]}

def nat_digit(r:Runtime) -> Body:
  match r:
    case Runtime{+r_array, +r_big, +r_length, +r_digit, +r_copy, +r_normal, +r_cmp_digits, +r_cmp, +r_add_loop, +r_sub_loop, +r_mul_loop, +r_slow_add, +r_slow_sub, +r_slow_mul}:
      Body{[G.Ref{G.HType{r_array}}, G.I32{}], [G.Block{Some{G.Ref{G.HI31{}}}, [G.Local_get{0}, G.Br_on_cast{0, G.HEq{}, G.HI31{}}, G.Ref_cast{G.HType{r_big}}, G.Struct_get{r_big, 1}, G.Local_set{2}, G.Local_get{1}, G.Local_get{2}, G.Array_len{}, G.I32_lt_u{}, G.If{Some{G.I32{}}, [G.Local_get{2}, G.Local_get{1}, G.Array_get{r_array}], [G.I32_const{0}]}, G.Return{}]}, G.I31_get_u{}, G.Local_set{3}, G.Local_get{1}, G.I32_const{0}, G.I32_eq{}, G.If{Some{G.I32{}}, low_limb(3), [G.Local_get{1}, G.I32_const{1}, G.I32_eq{}, G.If{Some{G.I32{}}, [G.Local_get{3}, G.I32_const{32768}, G.I32_div_u{}], [G.I32_const{0}]}]}]}

def nat_copy(r:Runtime) -> Body:
  match r:
    case Runtime{+r_array, +r_big, +r_length, +r_digit, +r_copy, +r_normal, +r_cmp_digits, +r_cmp, +r_add_loop, +r_sub_loop, +r_mul_loop, +r_slow_add, +r_slow_sub, +r_slow_mul}:
      Body{[], [G.Local_get{2}, G.Local_get{3}, G.I32_lt_u{}, G.If{Some{G.Ref{G.HType{r_array}}}, [G.Local_get{1}, G.Local_get{2}, G.Local_get{0}, G.Local_get{2}, G.Array_get{r_array}, G.Array_set{r_array}, G.Local_get{0}, G.Local_get{1}, G.Local_get{2}, G.I32_const{1}, G.I32_add{}, G.Local_get{3}, G.Return_call{r_copy}], [G.Local_get{1}]}]}

def nat_normal(r:Runtime) -> Body:
  match r:
    case Runtime{+r_array, +r_big, +r_length, +r_digit, +r_copy, +r_normal, +r_cmp_digits, +r_cmp, +r_add_loop, +r_sub_loop, +r_mul_loop, +r_slow_add, +r_slow_sub, +r_slow_mul}:
      Body{[], [G.Local_get{1}, G.I32_const{0}, G.I32_eq{}, G.If{Some{G.Ref{G.HEq{}}}, [G.I32_const{0}, G.Ref_i31{}], [G.Local_get{0}, G.Local_get{1}, G.I32_const{1}, G.I32_sub{}, G.Array_get{r_array}, G.I32_const{0}, G.I32_eq{}, G.If{Some{G.Ref{G.HEq{}}}, [G.Local_get{0}, G.Local_get{1}, G.I32_const{1}, G.I32_sub{}, G.Return_call{r_normal}], [G.Local_get{1}, G.I32_const{3}, G.I32_lt_u{}, G.If{Some{G.Ref{G.HEq{}}}, [G.Local_get{0}, G.I32_const{0}, G.Array_get{r_array}, G.Local_get{1}, G.I32_const{2}, G.I32_eq{}, G.If{Some{G.I32{}}, [G.Local_get{0}, G.I32_const{1}, G.Array_get{r_array}, G.I32_const{32768}, G.I32_mul{}], [G.I32_const{0}]}, G.I32_add{}, G.Ref_i31{}], [G.I32_const{1}, G.Local_get{0}, G.I32_const{0}, G.Local_get{1}, G.Array_new{r_array}, G.I32_const{0}, G.Local_get{1}, G.Call{r_copy}, G.Struct_new{r_big}]}]}]}]}

def nat_compare_digits(r:Runtime) -> Body:
  match r:
    case Runtime{+r_array, +r_big, +r_length, +r_digit, +r_copy, +r_normal, +r_cmp_digits, +r_cmp, +r_add_loop, +r_sub_loop, +r_mul_loop, +r_slow_add, +r_slow_sub, +r_slow_mul}:
      Body{[G.I32{}, G.I32{}], [G.Local_get{2}, G.I32_const{0}, G.I32_eq{}, G.If{Some{G.I32{}}, [G.I32_const{0}], [G.Local_get{2}, G.I32_const{1}, G.I32_sub{}, G.Local_set{2}, G.Local_get{0}, G.Local_get{2}, G.Call{r_digit}, G.Local_set{3}, G.Local_get{1}, G.Local_get{2}, G.Call{r_digit}, G.Local_set{4}, G.Local_get{3}, G.Local_get{4}, G.I32_eq{}, G.If{Some{G.I32{}}, [G.Local_get{0}, G.Local_get{1}, G.Local_get{2}, G.Return_call{r_cmp_digits}], [G.Local_get{3}, G.Local_get{4}, G.I32_lt_u{}, G.If{Some{G.I32{}}, [G.I32_const{1}], [G.I32_const{2}]}]}]}]}

def nat_compare(r:Runtime) -> Body:
  match r:
    case Runtime{+r_array, +r_big, +r_length, +r_digit, +r_copy, +r_normal, +r_cmp_digits, +r_cmp, +r_add_loop, +r_sub_loop, +r_mul_loop, +r_slow_add, +r_slow_sub, +r_slow_mul}:
      Body{[G.I32{}, G.I32{}], [G.Local_get{0}, G.Call{r_length}, G.Local_set{2}, G.Local_get{1}, G.Call{r_length}, G.Local_set{3}, G.Local_get{2}, G.Local_get{3}, G.I32_eq{}, G.If{Some{G.I32{}}, [G.Local_get{0}, G.Local_get{1}, G.Local_get{2}, G.Return_call{r_cmp_digits}], [G.Local_get{2}, G.Local_get{3}, G.I32_lt_u{}, G.If{Some{G.I32{}}, [G.I32_const{1}], [G.I32_const{2}]}]}]}

def nat_add_loop(r:Runtime) -> Body:
  match r:
    case Runtime{+r_array, +r_big, +r_length, +r_digit, +r_copy, +r_normal, +r_cmp_digits, +r_cmp, +r_add_loop, +r_sub_loop, +r_mul_loop, +r_slow_add, +r_slow_sub, +r_slow_mul}:
      Body{[G.I32{}], [G.Local_get{3}, G.Local_get{4}, G.I32_eq{}, G.If{Some{G.Ref{G.HEq{}}}, [G.Local_get{2}, G.Local_get{3}, G.Local_get{5}, G.Array_set{r_array}, G.Local_get{2}, G.Local_get{4}, G.I32_const{1}, G.I32_add{}, G.Return_call{r_normal}], cat([G.Local_get{0}, G.Local_get{3}, G.Call{r_digit}, G.Local_get{1}, G.Local_get{3}, G.Call{r_digit}, G.I32_add{}, G.Local_get{5}, G.I32_add{}, G.Local_set{6}, G.Local_get{2}, G.Local_get{3}], cat(low_limb(6), [G.Array_set{r_array}, G.Local_get{0}, G.Local_get{1}, G.Local_get{2}, G.Local_get{3}, G.I32_const{1}, G.I32_add{}, G.Local_get{4}, G.Local_get{6}, G.I32_const{32768}, G.I32_div_u{}, G.Return_call{r_add_loop}]))}]}

def nat_sub_loop(r:Runtime) -> Body:
  match r:
    case Runtime{+r_array, +r_big, +r_length, +r_digit, +r_copy, +r_normal, +r_cmp_digits, +r_cmp, +r_add_loop, +r_sub_loop, +r_mul_loop, +r_slow_add, +r_slow_sub, +r_slow_mul}:
      Body{[G.I32{}, G.I32{}, G.I32{}], [G.Local_get{3}, G.Local_get{4}, G.I32_eq{}, G.If{Some{G.Ref{G.HEq{}}}, [G.Local_get{2}, G.Local_get{4}, G.Return_call{r_normal}], [G.Local_get{0}, G.Local_get{3}, G.Call{r_digit}, G.Local_set{6}, G.Local_get{1}, G.Local_get{3}, G.Call{r_digit}, G.Local_get{5}, G.I32_add{}, G.Local_set{7}, G.Local_get{6}, G.Local_get{7}, G.I32_lt_u{}, G.Local_set{5}, G.Local_get{5}, G.If{Some{G.I32{}}, [G.Local_get{6}, G.I32_const{32768}, G.I32_add{}, G.Local_get{7}, G.I32_sub{}], [G.Local_get{6}, G.Local_get{7}, G.I32_sub{}]}, G.Local_set{8}, G.Local_get{2}, G.Local_get{3}, G.Local_get{8}, G.Array_set{r_array}, G.Local_get{0}, G.Local_get{1}, G.Local_get{2}, G.Local_get{3}, G.I32_const{1}, G.I32_add{}, G.Local_get{4}, G.Local_get{5}, G.Return_call{r_sub_loop}]}]}

def nat_mul_loop(r:Runtime) -> Body:
  match r:
    case Runtime{+r_array, +r_big, +r_length, +r_digit, +r_copy, +r_normal, +r_cmp_digits, +r_cmp, +r_add_loop, +r_sub_loop, +r_mul_loop, +r_slow_add, +r_slow_sub, +r_slow_mul}:
      Body{[G.I32{}, G.I32{}, G.I32{}], [G.Local_get{0}, G.Call{r_length}, G.Local_set{7}, G.Local_get{1}, G.Call{r_length}, G.Local_set{8}, G.Local_get{3}, G.Local_get{7}, G.I32_eq{}, G.If{Some{G.Ref{G.HEq{}}}, [G.Local_get{2}, G.Local_get{2}, G.Array_len{}, G.Return_call{r_normal}], [G.Local_get{4}, G.Local_get{8}, G.I32_eq{}, G.If{Some{G.Ref{G.HEq{}}}, [G.Local_get{2}, G.Local_get{3}, G.Local_get{4}, G.I32_add{}, G.Local_get{5}, G.Array_set{r_array}, G.Local_get{0}, G.Local_get{1}, G.Local_get{2}, G.Local_get{3}, G.I32_const{1}, G.I32_add{}, G.I32_const{0}, G.I32_const{0}, G.Return_call{r_mul_loop}], cat([G.Local_get{0}, G.Local_get{3}, G.Call{r_digit}, G.Local_get{1}, G.Local_get{4}, G.Call{r_digit}, G.I32_mul{}, G.Local_get{2}, G.Local_get{3}, G.Local_get{4}, G.I32_add{}, G.Array_get{r_array}, G.I32_add{}, G.Local_get{5}, G.I32_add{}, G.Local_set{6}, G.Local_get{2}, G.Local_get{3}, G.Local_get{4}, G.I32_add{}], cat(low_limb(6), [G.Array_set{r_array}, G.Local_get{0}, G.Local_get{1}, G.Local_get{2}, G.Local_get{3}, G.Local_get{4}, G.I32_const{1}, G.I32_add{}, G.Local_get{6}, G.I32_const{32768}, G.I32_div_u{}, G.Return_call{r_mul_loop}]))}]}]}

def nat_slow_add(r:Runtime) -> Body:
  match r:
    case Runtime{+r_array, +r_big, +r_length, +r_digit, +r_copy, +r_normal, +r_cmp_digits, +r_cmp, +r_add_loop, +r_sub_loop, +r_mul_loop, +r_slow_add, +r_slow_sub, +r_slow_mul}:
      Body{[G.I32{}, G.I32{}], [G.Local_get{0}, G.Call{r_length}, G.Local_set{2}, G.Local_get{1}, G.Call{r_length}, G.Local_set{3}, G.Local_get{2}, G.Local_get{3}, G.I32_lt_u{}, G.If{None{}, [G.Local_get{3}, G.Local_set{2}], []}, G.Local_get{0}, G.Local_get{1}, G.I32_const{0}, G.Local_get{2}, G.I32_const{1}, G.I32_add{}, G.Array_new{r_array}, G.I32_const{0}, G.Local_get{2}, G.I32_const{0}, G.Return_call{r_add_loop}]}

def nat_slow_sub(r:Runtime) -> Body:
  match r:
    case Runtime{+r_array, +r_big, +r_length, +r_digit, +r_copy, +r_normal, +r_cmp_digits, +r_cmp, +r_add_loop, +r_sub_loop, +r_mul_loop, +r_slow_add, +r_slow_sub, +r_slow_mul}:
      Body{[], [G.Local_get{0}, G.Local_get{1}, G.Call{r_cmp}, G.I32_const{1}, G.I32_eq{}, G.If{Some{G.Ref{G.HEq{}}}, [G.I32_const{0}, G.Ref_i31{}], [G.Local_get{0}, G.Local_get{1}, G.I32_const{0}, G.Local_get{0}, G.Call{r_length}, G.Array_new{r_array}, G.I32_const{0}, G.Local_get{0}, G.Call{r_length}, G.I32_const{0}, G.Return_call{r_sub_loop}]}]}

def nat_slow_mul(r:Runtime) -> Body:
  match r:
    case Runtime{+r_array, +r_big, +r_length, +r_digit, +r_copy, +r_normal, +r_cmp_digits, +r_cmp, +r_add_loop, +r_sub_loop, +r_mul_loop, +r_slow_add, +r_slow_sub, +r_slow_mul}:
      Body{[], [G.Local_get{0}, G.Local_get{1}, G.I32_const{0}, G.Local_get{0}, G.Call{r_length}, G.Local_get{1}, G.Call{r_length}, G.I32_add{}, G.Array_new{r_array}, G.I32_const{0}, G.I32_const{0}, G.I32_const{0}, G.Return_call{r_mul_loop}]}


type Primitive is Data:
  NatAdd{}
  NatSub{}
  NatMul{}
  NatEq{}
  NatLt{}

type HelperKind is Data:
  Length{}
  Digit{}
  Copy{}
  Normal{}
  CompareDigits{}
  Compare{}
  AddLoop{}
  SubLoop{}
  MulLoop{}
  SlowAdd{}
  SlowSub{}
  SlowMul{}
  Prim{primitive:Primitive}

type Named is Data:
  Named{name:String,kind:HelperKind}

def find_kind(ns:List<&2,Named>,+name:String) -> Maybe<&2,HelperKind>:
  match ns:
    case Nil{}: None{}
    case Con{Named{n,k},rest}: L.choose(Maybe<&2,HelperKind>,String.eq(n,name),Some{k},find_kind(rest,name))

def kind(name:String) -> Maybe<&2,HelperKind>:
  find_kind([Named{"length",Length{}},Named{"digit",Digit{}},Named{"copy",Copy{}},Named{"normal",Normal{}},Named{"compareDigits",CompareDigits{}},Named{"compare",Compare{}},Named{"addLoop",AddLoop{}},Named{"subLoop",SubLoop{}},Named{"mulLoop",MulLoop{}},Named{"slowAdd",SlowAdd{}},Named{"slowSub",SlowSub{}},Named{"slowMul",SlowMul{}},Named{"natAdd",Prim{NatAdd{}}},Named{"natSub",Prim{NatSub{}}},Named{"natMul",Prim{NatMul{}}},Named{"natEq",Prim{NatEq{}}},Named{"natLt",Prim{NatLt{}}}],name)

def nat_indices(+l:L.T) -> Result<&2,&2,Err.T,Runtime>:
  do Result<&2,&2,Err.T,Runtime>:
    array : U32 <- L.type_index(l,"nat-limbs")
    big : U32 <- L.type_index(l,"nat-big")
    length : U32 <- L.func_index(l,"nat-runtime:length")
    digit : U32 <- L.func_index(l,"nat-runtime:digit")
    copy : U32 <- L.func_index(l,"nat-runtime:copy")
    normal : U32 <- L.func_index(l,"nat-runtime:normal")
    cmp_digits : U32 <- L.func_index(l,"nat-runtime:compareDigits")
    cmp : U32 <- L.func_index(l,"nat-runtime:compare")
    add_loop : U32 <- L.func_index(l,"nat-runtime:addLoop")
    sub_loop : U32 <- L.func_index(l,"nat-runtime:subLoop")
    mul_loop : U32 <- L.func_index(l,"nat-runtime:mulLoop")
    slow_add : U32 <- L.func_index(l,"nat-runtime:slowAdd")
    slow_sub : U32 <- L.func_index(l,"nat-runtime:slowSub")
    slow_mul : U32 <- L.func_index(l,"nat-runtime:slowMul")
    return Runtime{array,big,length,digit,copy,normal,cmp_digits,cmp,add_loop,sub_loop,mul_loop,slow_add,slow_sub,slow_mul}

def slow(p:Primitive,r:Runtime) -> List<&2,G.Instr>:
  match p r:
    case NatAdd{} Runtime{a,b,l,d,c,n,cd,cmp,al,sl,ml,sa,ss,sm}: [G.Local_get{0},G.Local_get{1},G.Return_call{sa}]
    case NatSub{} Runtime{a,b,l,d,c,n,cd,cmp,al,sl,ml,sa,ss,sm}: [G.Local_get{0},G.Local_get{1},G.Return_call{ss}]
    case NatMul{} Runtime{a,b,l,d,c,n,cd,cmp,al,sl,ml,sa,ss,sm}: [G.Local_get{0},G.Local_get{1},G.Return_call{sm}]
    case NatEq{} Runtime{a,b,l,d,c,n,cd,cmp,al,sl,ml,sa,ss,sm}: [G.Local_get{0},G.Local_get{1},G.Call{cmp},G.I32_const{0},G.I32_eq{},G.Ref_i31{},G.Return{}]
    case NatLt{} Runtime{a,b,l,d,c,n,cd,cmp,al,sl,ml,sa,ss,sm}: [G.Local_get{0},G.Local_get{1},G.Call{cmp},G.I32_const{1},G.I32_eq{},G.Ref_i31{},G.Return{}]

def cast(+param:U32,dst:U32,slow:List<&2,G.Instr>) -> List<&2,G.Instr>:
  [G.Block{Some{G.Ref{G.HI31{}}},cat([G.Local_get{param},G.Br_on_cast{0,G.HEq{},G.HI31{}},G.Local_set{param}],slow)},G.I31_get_u{},G.Local_set{dst}]

def small(p:Primitive,slow:List<&2,G.Instr>) -> List<&2,G.Instr>:
  match p:
    case NatAdd{}:
      [G.Local_get{2},G.Local_get{3},G.I32_add{},G.Local_set{4},G.Local_get{4},G.I32_const{1073741823},G.I32_gt_u{},G.If{Some{G.Ref{G.HEq{}}},slow,[G.Local_get{4},G.Ref_i31{}]}]
    case NatSub{}:
      [G.Local_get{2},G.Local_get{3},G.I32_lt_u{},G.If{Some{G.I32{}},[G.I32_const{0}],[G.Local_get{2},G.Local_get{3},G.I32_sub{}]},G.Ref_i31{}]
    case NatMul{}:
      [G.Local_get{2},G.I32_const{0},G.I32_eq{},G.If{Some{G.Ref{G.HEq{}}},[G.I32_const{0},G.Ref_i31{}],[G.Local_get{3},G.I32_const{1073741823},G.Local_get{2},G.I32_div_u{},G.I32_gt_u{},G.If{Some{G.Ref{G.HEq{}}},slow,[G.Local_get{2},G.Local_get{3},G.I32_mul{},G.Ref_i31{}]}]}]
    case NatEq{}: [G.Local_get{2},G.Local_get{3},G.I32_eq{},G.Ref_i31{}]
    case NatLt{}: [G.Local_get{2},G.Local_get{3},G.I32_lt_u{},G.Ref_i31{}]

def nat_primitive(r:Runtime,+p:Primitive) -> Body:
  +ss = slow(p,r)
  Body{[G.I32{},G.I32{},G.I32{}],cat(cast(0,2,ss),cat(cast(1,3,ss),small(p,ss)))}

def make_body(k:HelperKind,r:Runtime) -> Body:
  match k:
    case Length{}: nat_length(r)
    case Digit{}: nat_digit(r)
    case Copy{}: nat_copy(r)
    case Normal{}: nat_normal(r)
    case CompareDigits{}: nat_compare_digits(r)
    case Compare{}: nat_compare(r)
    case AddLoop{}: nat_add_loop(r)
    case SubLoop{}: nat_sub_loop(r)
    case MulLoop{}: nat_mul_loop(r)
    case SlowAdd{}: nat_slow_add(r)
    case SlowSub{}: nat_slow_sub(r)
    case SlowMul{}: nat_slow_mul(r)
    case Prim{p}: nat_primitive(r,p)

def finish(ft:U32,b:Body) -> G.Func:
  match b:
    case Body{ls,code}: G.Func{ft,ls,code}

def emit_kind(k:Maybe<&2,HelperKind>,name:String,+l:L.T) -> Result<&2,&2,Err.T,G.Func>:
  match k:
    case None{}: Fail{Err.Unbound{"unknown Nat runtime helper: " ++ name}}
    case Some{k}:
      do Result<&2,&2,Err.T,G.Func>:
        ft : U32 <- L.type_index(l,"nat-sig:" ++ name)
        r : Runtime <- nat_indices(l)
        return finish(ft,make_body(k,r))

def runtime_func(+l:L.T,+name:String) -> Result<&2,&2,Err.T,G.Func>:
  emit_kind(kind(name),name,l)
