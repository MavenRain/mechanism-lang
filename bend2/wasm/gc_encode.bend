import Base

type Heaptype is Data:
  HEq{}
  HI31{}
  HFunc{}
  HType{index: U32}

type Valtype is Data:
  I32{}
  Ref{heap: Heaptype}

type Comptype is Data:
  CStruct{fields: List<&2, Valtype>}
  CArray{field: Valtype}
  CFunc{params: List<&2, Valtype>, results: List<&2, Valtype>}

type Instr is Data:
  Unreachable{}
  Block{result: Maybe<&2, Valtype>, body: List<&2, Instr>}
  If{result: Maybe<&2, Valtype>, yes: List<&2, Instr>, no: List<&2, Instr>}
  Br{label: U32}
  Br_if{label: U32}
  Br_on_cast{label: U32, source: Heaptype, target: Heaptype}
  Return{}
  Call{index: U32}
  Return_call{index: U32}
  Call_ref{index: U32}
  Return_call_ref{index: U32}
  Local_get{index: U32}
  Local_set{index: U32}
  Local_tee{index: U32}
  I32_const{value: U32}
  I32_add{}
  I32_sub{}
  I32_mul{}
  I32_div_u{}
  I32_eq{}
  I32_ne{}
  I32_lt_u{}
  I32_gt_u{}
  Ref_i31{}
  I31_get_s{}
  I31_get_u{}
  Ref_cast{heap: Heaptype}
  Ref_func{index: U32}
  Struct_new{index: U32}
  Struct_get{index: U32, field: U32}
  Array_new{index: U32}
  Array_get{index: U32}
  Array_set{index: U32}
  Array_len{}
  End{}
  Else{}

type Func is Data:
  Func{ftype: U32, locals: List<&2, Valtype>, body: List<&2, Instr>}

type Export is Data:
  Export{name: String, index: U32}

type Import is Data:
  Import{module: String, name: String, ftype: U32}

type T is Data:
  Module{types: List<&2, List<&2, Comptype>>, imports: List<&2, Import>, funcs: List<&2, Func>, exports: List<&2, Export>, declared: List<&2, U32>}

def cat(xs: List<&2, U32>, ys: List<&2, U32>) -> List<&2, U32>:
  List.append(&2, U32, xs, ys)

def byte(n: U32) -> List<&2, U32>:
  [U32.and(n, 255)]

def leb_step(done: Bool, low: U32, rest: List<&2, U32>) -> List<&2, U32>:
  match done:
    case True{}: [low]
    case False{}: U32.or(128, low) <> rest

def uleb_go(fuel: Nat, +n: U32, +limit: U32) -> List<&2, U32>:
  match fuel:
    case 0n: []
    case 1n+p:
      leb_step(U32.is_lt(n, limit), U32.and(n, 127), uleb_go(p, U32.shrn(n, 7n), limit))

def uleb(n: U32) -> List<&2, U32>:
  uleb_go(5n, n, 128)

def signed_rest(sign: Bool, rest: U32) -> U32:
  match sign:
    case True{}: U32.or(rest, 4261412864)
    case False{}: rest

def sleb_go(fuel: Nat, +n: U32) -> List<&2, U32>:
  match fuel:
    case 0n: []
    case 1n+p:
      +low = U32.and(n, 127)
      +rest = signed_rest(U32.is_ge(n, 2147483648), U32.shrn(n, 7n))
      done = (U32.is_eq(rest, 0) && U32.is_eq(U32.and(low, 64), 0)) || (U32.is_eq(rest, 4294967295) && U32.is_ne(U32.and(low, 64), 0))
      leb_step(done, low, sleb_go(p, rest))

def sleb(n: U32) -> List<&2, U32>:
  sleb_go(5n, n)

def positive_sleb(n: U32) -> List<&2, U32>:
  uleb_go(5n, n, 64)

def heaptype(h: Heaptype) -> List<&2, U32>:
  match h:
    case HEq{}: [109]
    case HI31{}: [108]
    case HFunc{}: [112]
    case HType{index}: positive_sleb(index)

def valtype(v: Valtype) -> List<&2, U32>:
  match v:
    case I32{}: [127]
    case Ref{heap}: 100 <> heaptype(heap)

def valtypes(vs: List<&2, Valtype>) -> List<&2, U32>:
  match vs:
    case Nil{}: []
    case Con{v, rest}: cat(valtype(v), valtypes(rest))

def fields(vs: List<&2, Valtype>) -> List<&2, U32>:
  match vs:
    case Nil{}: []
    case Con{v, rest}: cat(valtype(v), 0 <> fields(rest))

def length(-A: Data, xs: List<&2, A>) -> List<&2, U32>:
  uleb(U32.from_nat(List.length(&2, A, xs)))

def comptype(c: Comptype) -> List<&2, U32>:
  match c:
    case CStruct{+vs}: 95 <> cat(length(Valtype, vs), fields(vs))
    case CArray{v}: 94 <> cat(valtype(v), [1])
    case CFunc{+params, +results}:
      96 <> cat(length(Valtype, params), cat(valtypes(params), cat(length(Valtype, results), valtypes(results))))

def sub_finals(cs: List<&2, Comptype>) -> List<&2, U32>:
  match cs:
    case Nil{}: []
    case Con{c, rest}: 79 <> 0 <> cat(comptype(c), sub_finals(rest))

def rectype(cs: List<&2, Comptype>) -> List<&2, U32>:
  match cs:
    case Nil{}: [78, 0]
    case Con{c, Nil{}}: comptype(c)
    case Con{+a, Con{+b, +rest}}:
      78 <> cat(length(Comptype, a <> b <> rest), sub_finals(a <> b <> rest))

def rectypes(gs: List<&2, List<&2, Comptype>>) -> List<&2, U32>:
  match gs:
    case Nil{}: []
    case Con{g, rest}: cat(rectype(g), rectypes(rest))

def blocktype(b: Maybe<&2, Valtype>) -> List<&2, U32>:
  match b:
    case None{}: [64]
    case Some{v}: valtype(v)

@unsafe def instrs(xs: List<&2, Instr>) -> List<&2, U32>:
  match xs:
    case Nil{}: []
    case Con{i, rest}:
      match i:
        case Unreachable{}: 0 <> instrs(rest)
        case Block{bt, body}: 2 <> cat(blocktype(bt), instrs(List.append(&2, Instr, body, End{} <> rest)))
        case If{bt, yes, no}: 4 <> cat(blocktype(bt), instrs(List.append(&2, Instr, yes, Else{} <> List.append(&2, Instr, no, End{} <> rest))))
        case Br{n}: 12 <> cat(uleb(n), instrs(rest))
        case Br_if{n}: 13 <> cat(uleb(n), instrs(rest))
        case Br_on_cast{n, src, dst}: 251 <> 24 <> 0 <> cat(uleb(n), cat(heaptype(src), cat(heaptype(dst), instrs(rest))))
        case Return{}: 15 <> instrs(rest)
        case Call{n}: 16 <> cat(uleb(n), instrs(rest))
        case Return_call{n}: 18 <> cat(uleb(n), instrs(rest))
        case Call_ref{n}: 20 <> cat(uleb(n), instrs(rest))
        case Return_call_ref{n}: 21 <> cat(uleb(n), instrs(rest))
        case Local_get{n}: 32 <> cat(uleb(n), instrs(rest))
        case Local_set{n}: 33 <> cat(uleb(n), instrs(rest))
        case Local_tee{n}: 34 <> cat(uleb(n), instrs(rest))
        case I32_const{n}: 65 <> cat(sleb(n), instrs(rest))
        case I32_add{}: 106 <> instrs(rest)
        case I32_sub{}: 107 <> instrs(rest)
        case I32_mul{}: 108 <> instrs(rest)
        case I32_div_u{}: 110 <> instrs(rest)
        case I32_eq{}: 70 <> instrs(rest)
        case I32_ne{}: 71 <> instrs(rest)
        case I32_lt_u{}: 73 <> instrs(rest)
        case I32_gt_u{}: 75 <> instrs(rest)
        case Ref_i31{}: 251 <> 28 <> instrs(rest)
        case I31_get_s{}: 251 <> 29 <> instrs(rest)
        case I31_get_u{}: 251 <> 30 <> instrs(rest)
        case Ref_cast{h}: 251 <> 22 <> cat(heaptype(h), instrs(rest))
        case Ref_func{n}: 210 <> cat(uleb(n), instrs(rest))
        case Struct_new{n}: 251 <> 0 <> cat(uleb(n), instrs(rest))
        case Struct_get{n, f}: 251 <> 2 <> cat(uleb(n), cat(uleb(f), instrs(rest)))
        case Array_new{n}: 251 <> 6 <> cat(uleb(n), instrs(rest))
        case Array_get{n}: 251 <> 11 <> cat(uleb(n), instrs(rest))
        case Array_set{n}: 251 <> 14 <> cat(uleb(n), instrs(rest))
        case Array_len{}: 251 <> 15 <> instrs(rest)
        case End{}: 11 <> instrs(rest)
        case Else{}: 5 <> instrs(rest)

def section(id: U32, +payload: List<&2, U32>) -> List<&2, U32>:
  id <> cat(length(U32, payload), payload)

def locals_body(ls: List<&2, Valtype>) -> List<&2, U32>:
  match ls:
    case Nil{}: []
    case Con{v, rest}: 1 <> cat(valtype(v), locals_body(rest))

def code_entry(f: Func) -> List<&2, U32>:
  match f:
    case Func{ftype, +ls, body}:
      +payload = cat(length(Valtype, ls), cat(locals_body(ls), cat(instrs(body), [11])))
      cat(length(U32, payload), payload)

def utf8_4(four: Bool, +n: U32) -> List<&2, U32>:
  match four:
    case False{}: [U32.or(224, U32.shrn(n, 12n)), U32.or(128, U32.and(U32.shrn(n, 6n), 63)), U32.or(128, U32.and(n, 63))]
    case True{}: [U32.or(240, U32.shrn(n, 18n)), U32.or(128, U32.and(U32.shrn(n, 12n), 63)), U32.or(128, U32.and(U32.shrn(n, 6n), 63)), U32.or(128, U32.and(n, 63))]

def utf8_2(two: Bool, +n: U32) -> List<&2, U32>:
  match two:
    case True{}: [U32.or(192, U32.shrn(n, 6n)), U32.or(128, U32.and(n, 63))]
    case False{}: utf8_4(U32.is_ge(n, 65536), n)

def utf8_1(one: Bool, +n: U32) -> List<&2, U32>:
  match one:
    case True{}: [n]
    case False{}: utf8_2(U32.is_lt(n, 2048), n)

def utf8(cs: List<&2, Char>) -> List<&2, U32>:
  match cs:
    case Nil{}: []
    case Con{c, rest}:
      +n = Char.to_u32(c)
      cat(utf8_1(U32.is_lt(n, 128), n), utf8(rest))

def name(s: String) -> List<&2, U32>:
  +bs = utf8(String.to_list(s))
  cat(length(U32, bs), bs)

def exports(es: List<&2, Export>) -> List<&2, U32>:
  match es:
    case Nil{}: []
    case Con{Export{n, idx}, rest}: cat(name(n), 0 <> cat(uleb(idx), exports(rest)))

def imports(xs: List<&2, Import>) -> List<&2, U32>:
  match xs:
    case Nil{}: []
    case Con{Import{m, n, t}, rest}: cat(name(m), cat(name(n), 0 <> cat(uleb(t), imports(rest))))

def func_types(fs: List<&2, Func>) -> List<&2, U32>:
  match fs:
    case Nil{}: []
    case Con{Func{t, ls, body}, rest}: cat(uleb(t), func_types(rest))

def codes(fs: List<&2, Func>) -> List<&2, U32>:
  match fs:
    case Nil{}: []
    case Con{f, rest}: cat(code_entry(f), codes(rest))

def indices(ns: List<&2, U32>) -> List<&2, U32>:
  match ns:
    case Nil{}: []
    case Con{n, rest}: cat(uleb(n), indices(rest))

def element_section(ds: List<&2, U32>) -> List<&2, U32>:
  match ds:
    case Nil{}: []
    case Con{+n, +rest}:
      section(9, 1 <> 3 <> 0 <> cat(length(U32, n <> rest), indices(n <> rest)))

def import_section(xs: List<&2, Import>) -> List<&2, U32>:
  match xs:
    case Nil{}: []
    case Con{+x, +rest}: section(2, cat(length(Import, x <> rest), imports(x <> rest)))

def encode(m: T) -> List<&2, U32>:
  match m:
    case Module{+ts, ims, +fs, +es, ds}:
      cat([0, 97, 115, 109, 1, 0, 0, 0],
        cat(section(1, cat(length(List<&2, Comptype>, ts), rectypes(ts))),
        cat(import_section(ims),
        cat(section(3, cat(length(Func, fs), func_types(fs))),
        cat(section(7, cat(length(Export, es), exports(es))),
        cat(element_section(ds), section(10, cat(length(Func, fs), codes(fs)))))))))

