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

type Pat is Data:
  PCtor{family: String, ctor: String, binds: List<&2, Bind>}

type Param is Data:
  Param{name: String, ty: Ty}

type Arm<-X: Data> is Data:
  Arm{pat: Pat, body: X}

# `XCall` calls a function or a local closure; `targs` go in a turbofish.
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

type Variant is Data:
  Variant{name: String, fields: List<&2, Ty>}

# `IUse`: `use crate::<module>::*;`. `IEnum.copy`: the enum derives `Copy`.
# `IUnit`: a unit struct and its constructor function. Each generic of an
# `IFn` gets a `Clone` bound.
type Item is Data:
  IUse{module: String}
  IEnum{name: String, generics: List<&2, String>, copy: Bool, variants: List<&2, Variant>}
  IUnit{name: String, ctor: String}
  IFn{name: String, generics: List<&2, String>, params: List<&2, Param>, ret: Ty, body: Ex}

# `source`: the name of the input file, for the `//!` line.
type Module is Data:
  Module{source: String, items: List<&2, Item>}
