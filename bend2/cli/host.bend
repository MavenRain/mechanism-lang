import Base
import ../io.bend as F
import ../kernel/common.bend as C
import ../kernel/bignum.bend as Big
import ../kernel/error.bend as Error
import ../kernel/eval.bend as Eval
import ../kernel/global.bend as Global
import ../kernel/term.bend as Term
import ../kernel/value.bend as V
import ../kernel/literal.bend as Literal

type T is Data:
  Node{}
  Wasmtime{}
  Kernel{}

type Outcome is Data:
  Value{number:Big.T}
  Trap{message:String}
  Invalid{message:String}

type Runner is Data:
  Runner{launcher:String,relative:String}

def host_executable() -> IO(String):
  import "./effects/executable.c"
  import "./effects/executable.js"

def host_process(launcher:String,runner:String,wasm:String,export:String,out_path:String,err_path:String) -> IO(U32):
  import "./effects/process.c"
  import "./effects/process.js"

def name(h:T) -> String:
  match h:
    case Node{}: "node"
    case Wasmtime{}: "wasmtime"
    case Kernel{}: "kernel"

def runner(h:T) -> Maybe<&2,Runner>:
  match h:
    case Node{}: Some{Runner{"node","dev/run-node.mjs"}}
    case Wasmtime{}: Some{Runner{"zsh","dev/run-wasmtime.sh"}}
    case Kernel{}: None{}

def trim_slashes(cs:List<&2,Char>) -> List<&2,Char>:
  match cs:
    case Con{'/',rest}: trim_slashes(rest)
    case _: cs

def parent_chars(cs:List<&2,Char>) -> String:
  match cs:
    case Nil{}: "/"
    case Con{c,rest}: String.from_list(List.reverse(&2,Char,c <> rest))

def dirname_rev(cs:List<&2,Char>) -> String:
  match cs:
    case Nil{}: "."
    case Con{'/',rest}: parent_chars(trim_slashes(rest))
    case Con{c,rest}: dirname_rev(rest)

def dirname_trimmed(cs:List<&2,Char>,original:String) -> String:
  match cs:
    case Nil{}: Bool.pick(String,String.eq(original,""),".","/")
    case Con{c,rest}: dirname_rev(c <> rest)

def dirname(+path:String) -> String:
  dirname_trimmed(trim_slashes(List.reverse(&2,Char,String.to_list(path))),path)

def root() -> IO(String):
  do IO<String>:
    path : String <- host_executable()
    return dirname(dirname(dirname(path)))

def join(+root:String,+relative:String) -> String:
  Bool.pick(String,String.eq(root,""),relative,root ++ "/" ++ relative)

def missing_at(r:Maybe<&2,Runner>,root:String) -> IO(Maybe<&2,String>):
  match r:
    case None{}: IO.pure(Maybe<&2,String>,None{})
    case Some{Runner{launcher,+rel}}:
      do IO<Maybe<&2,String>>:
        found : Bool <- F.exists(join(root,rel))
        return Bool.pick(Maybe<&2,String>,found,None{},Some{"kanon: run: cannot find " ++ rel ++ " beside the executable"})

def missing_runner(root:String,h:T) -> IO(Maybe<&2,String>):
  missing_at(runner(h),root)

def out_path(wasm:String) -> String:
  wasm ++ ".out"

def err_path(wasm:String) -> String:
  wasm ++ ".err"

def captures(+wasm:String) -> List<&2,String>:
  [out_path(wasm),err_path(wasm)]

def text_result(r:Result<&2,&2,String,String>) -> String:
  match r:
    case Done{s}: s
    case Fail{e}: ""

def read_text(path:String) -> IO(String):
  do IO<String>:
    r : Result<&2,&2,String,String> <- F.read_file(path)
    return text_result(r)

def first_nonblank(xs:List<&2,String>) -> String:
  match xs:
    case Nil{}: "no message"
    case Con{x,rest}:
      +line = String.trim(x)
      Bool.pick(String,String.eq(line,""),first_nonblank(rest),line)

def first_line(path:String) -> IO(String):
  do IO<String>:
    text : String <- read_text(path)
    return first_nonblank(String.split(text,'\n'))

def strip(+prefix:String,+line:String) -> String:
  Bool.pick(String,String.starts_with(line,prefix),String.drop(line,String.length(prefix)),line)

def digit(+c:Char) -> U32:
  +u = Char.to_u32(c)
  Bool.pick(U32,U32.is_ge(u,48) && U32.is_le(u,57),U32.sub(u,48),
    Bool.pick(U32,U32.is_ge(u,97) && U32.is_le(u,102),U32.add(U32.sub(u,97),10),
      Bool.pick(U32,U32.is_ge(u,65) && U32.is_le(u,70),U32.add(U32.sub(u,65),10),255)))

def number_digit(valid:Bool,d:U32,base:U32,n:Big.T) -> Maybe<&2,Big.T>:
  match valid:
    case False{}: None{}
    case True{}: Some{Big.add(Big.mul(n,Big.of_u32(base)),Big.of_u32(d))}

def number_rest(s:String,+base:U32,n:Maybe<&2,Big.T>) -> Maybe<&2,Big.T>:
  match s n:
    case _ None{}: None{}
    case SNil{} Some{x}: Some{x}
    case SCon{'_',rest} Some{x}: number_rest(rest,base,Some{x})
    case SCon{c,rest} Some{x}:
      +d = digit(c)
      number_rest(rest,base,number_digit(U32.is_lt(d,base),d,base,x))

def number_first(s:String,+base:U32) -> Maybe<&2,Big.T>:
  match s:
    case SNil{}: None{}
    case SCon{c,rest}:
      +d = digit(c)
      number_rest(rest,base,number_digit(U32.is_lt(d,base),d,base,Big.zero()))

def int_limit() -> Big.T:
  Big.Big{False{},[0,0,0,0,4]}

def int_modulus() -> Big.T:
  Big.Big{False{},[0,0,0,0,8]}

def int_max() -> Big.T:
  Big.Big{False{},[32767,32767,32767,32767,3]}

def unsigned_max() -> Big.T:
  Big.Big{False{},[32767,32767,32767,32767,7]}

def signed_wrap(+negative:Bool,+n:Big.T) -> Big.T:
  +value = Bool.pick(Big.T,negative,Big.neg(n),n)
  Bool.pick(Big.T,negative,
    Bool.pick(Big.T,Big.le(n,int_limit()),value,Big.add(value,int_modulus())),
    Bool.pick(Big.T,Big.le(n,int_max()),value,Big.add(value,Big.neg(int_modulus()))))

def number_bound(n:Maybe<&2,Big.T>,+negative:Bool,unsigned:Bool) -> Maybe<&2,Big.T>:
  match n:
    case None{}: None{}
    case Some{+n}:
      limit = Bool.pick(Big.T,unsigned,unsigned_max(),Bool.pick(Big.T,negative,int_limit(),int_max()))
      Bool.pick(Maybe<&2,Big.T>,Big.le(n,limit),Some{signed_wrap(negative,n)},None{})

def number_base(s:String,negative:Bool) -> Maybe<&2,Big.T>:
  match s:
    case SCon{'0',SCon{'x',rest}}: number_bound(number_first(rest,16),negative,True{})
    case SCon{'0',SCon{'X',rest}}: number_bound(number_first(rest,16),negative,True{})
    case SCon{'0',SCon{'o',rest}}: number_bound(number_first(rest,8),negative,True{})
    case SCon{'0',SCon{'O',rest}}: number_bound(number_first(rest,8),negative,True{})
    case SCon{'0',SCon{'b',rest}}: number_bound(number_first(rest,2),negative,True{})
    case SCon{'0',SCon{'B',rest}}: number_bound(number_first(rest,2),negative,True{})
    case SCon{'0',SCon{'u',rest}}: number_bound(number_first(rest,10),negative,True{})
    case SCon{'0',SCon{'U',rest}}: number_bound(number_first(rest,10),negative,True{})
    case _: number_bound(number_first(s,10),negative,False{})

def parse_number(s:String) -> Maybe<&2,Big.T>:
  match s:
    case SCon{'-',rest}: number_base(rest,True{})
    case SCon{'+',rest}: number_base(rest,False{})
    case _: number_base(s,False{})

def printed(n:Maybe<&2,Big.T>,text:String) -> Outcome:
  match n:
    case None{}: Invalid{"runner printed " ++ text}
    case Some{x}: Value{x}

def of_exit(code:U32,wasm:String) -> IO(Outcome):
  match code:
    case 0:
      do IO<Outcome>:
        raw : String <- read_text(out_path(wasm))
        +text : String = String.trim(raw)
        return printed(parse_number(text),text)
    case 1:
      do IO<Outcome>:
        line : String <- first_line(err_path(wasm))
        return Trap{strip("trap: ",line)}
    case _:
      do IO<Outcome>:
        line : String <- first_line(err_path(wasm))
        return Invalid{strip("invalid: ",line)}

def spawn(r:Runner,root:String,+wasm:String,export:String) -> IO(Outcome):
  match r:
    case Runner{launcher,relative}:
      do IO<Outcome>:
        code : U32 <- host_process(launcher,join(root,relative),wasm,export,out_path(wasm),err_path(wasm))
        of_exit(code,wasm)

def kernel_small(n:Big.T,small:Maybe<&2,U32>) -> Outcome:
  match small:
    case None{}: Trap{Big.to_string(n) ++ " is outside the i31 range"}
    case Some{x}: Value{Big.of_u32(x)}

def kernel_value(result:Result<&2,&2,Error.T,V.T>) -> Outcome:
  match result:
    case Fail{e}: Invalid{Error.to_string(e)}
    case Done{V.VLit{Literal.LInt{+n}}}: kernel_small(n,Big.to_i31(n))
    case Done{v}: Invalid{"the kernel value is not a literal"}

def kernel_outcome(+globals:Global.T,export:String) -> Outcome:
  kernel_value(do Result<&2,&2,Error.T,V.T>:
    value : V.T <- Eval.eval(globals,[],Term.Global{export})
    Eval.whnf(globals,value))

def run_module(root:String,globals:Global.T,h:T,wasm:String,export:String) -> IO(Outcome):
  match h:
    case Node{}: spawn(Runner{"node","dev/run-node.mjs"},root,wasm,export)
    case Wasmtime{}: spawn(Runner{"zsh","dev/run-wasmtime.sh"},root,wasm,export)
    case Kernel{}: IO.pure(Outcome,kernel_outcome(globals,export))
