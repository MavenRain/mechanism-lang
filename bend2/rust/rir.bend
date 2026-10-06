import Base

# Erased program IR for `mech rust-out`. Each name is a final Rust name.
# Ownership is in the types: a binder of type `TRef` is a borrow (quantity
# many, not Copy), and a binder of any other type owns its value. Each
# `.clone()`, `&` and `Box::new` is an explicit node, so the emitter adds
# nothing.

type Ty is Data:
  TBool{}
  TCon{name: String, args: List<&2, Ty>}
  TBox{inner: Ty}
  TRef{inner: Ty}
  TFn{args: List<&2, Ty>, ret: Ty}

# A constructor field in a pattern: a name, or `_` for a field without a use.
type Bind is Data:
  BVar{name: String}
  BSkip{}

# `PStruct`: `S { a: x, b: _ }` when `named`, else `S(x, _)`. `fields` are
# the field names of a named struct (empty for a tuple struct) (E2).
type Pat is Data:
  PCtor{family: String, ctor: String, binds: List<&2, Bind>}
  PStruct{family: String, named: Bool, fields: List<&2, String>, binds: List<&2, Bind>}

type Param is Data:
  Param{name: String, ty: Ty}

type Arm<-X: Data> is Data:
  Arm{pat: Pat, body: X}

# `XCall` calls a function or a local closure; `targs` go in a turbofish.
# `XLet` is `{ let name: ty = value; body }`; the emitter always gives the
# type, the importer keeps `None` for `let y = x;` so that the lift prints
# back byte for byte (E1).
type Ex is Data:
  XVar{name: String}
  XInt{text: String}
  XBool{value: Bool}
  XCall{func: String, targs: List<&2, Ty>, args: List<&2, Ex>}
  XCtor{family: String, ctor: String, args: List<&2, Ex>}
  XMatch{scrut: Ex, arms: List<&2, Arm<Ex>>}
  XIf{cond: Ex, yes: Ex, no: Ex}
  XClosure{params: List<&2, Param>, body: Ex}
  XBorrow{inner: Ex}
  XClone{inner: Ex}
  XBox{inner: Ex}
  XLet{name: String, ty: Maybe<&2, Ty>, value: Ex, body: Ex}

type Variant is Data:
  Variant{name: String, fields: List<&2, Ty>}

# `IUse`: `use crate::<module>::*;`. `IEnum.copy`: the enum derives `Copy`.
# `IUnit`: a unit struct and its constructor function. Each generic of an
# `IFn` gets a `Clone` bound.
# `IStruct`: a struct with private fields, its constructor function and one
# accessor per getter (E2). `named` false is a tuple struct: the field names
# are then the constructor parameter names and `getters` is empty. A getter
# type `TRef` means the accessor borrows (`&self.a`), any other type means it
# copies (`self.a`).
type Item is Data:
  IUse{module: String}
  IEnum{name: String, generics: List<&2, String>, copy: Bool, variants: List<&2, Variant>}
  IUnit{name: String, ctor: String}
  IStruct{name: String, ctor: String, named: Bool, fields: List<&2, Param>, getters: List<&2, Param>}
  IFn{name: String, generics: List<&2, String>, params: List<&2, Param>, ret: Ty, body: Ex}

# `source`: the name of the input file, for the `//!` line.
type Module is Data:
  Module{source: String, items: List<&2, Item>}
