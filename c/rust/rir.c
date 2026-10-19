/* C port of rust/rir.bend: the erased program IR for `mech rust-out`. Each name is a final Rust
 * name. Ownership is in the types: a binder of type TRef is a borrow (quantity many, not Copy), and a
 * binder of any other type owns its value. Each `.clone()`, `&` and `Box::new` is an explicit node,
 * so the emitter adds nothing. A failed allocation sets heap->oom and returns the placeholder of the
 * type (a node without children). */
#include "rust.h"

static const RirTy rir_ty_placeholder = {RIR_TBOOL};
static const RirBind rir_bind_placeholder = {RIR_BSKIP};
static const RirEx rir_ex_placeholder = {RIR_XVAR};
static const RirParam rir_param_placeholder = {{0}, &rir_ty_placeholder};
static const RirArm rir_arm_placeholder = {{{0}}, &rir_ex_placeholder};
static const RirVariant rir_variant_placeholder = {{0}};
static const RirItem rir_item_placeholder = {RIR_IUSE};

/* ---- Ty ---- */

static RirTy *rir_ty_alloc(Heap *heap, RirTyKind kind) {
  RirTy *ty = heap_alloc(heap, sizeof(RirTy));
  if (ty) ty->kind = kind;
  return ty;
}

const RirTy *rir_tbool(Heap *heap) {
  RirTy *ty = rir_ty_alloc(heap, RIR_TBOOL);
  return ty ? ty : &rir_ty_placeholder;
}

const RirTy *rir_tcon(Heap *heap, Str name, Vec args) {
  RirTy *ty = rir_ty_alloc(heap, RIR_TCON);
  if (!ty) return &rir_ty_placeholder;
  ty->name = name;
  ty->args = args;
  return ty;
}

const RirTy *rir_tbox(Heap *heap, const RirTy *inner) {
  RirTy *ty = rir_ty_alloc(heap, RIR_TBOX);
  if (!ty) return &rir_ty_placeholder;
  ty->inner = inner;
  return ty;
}

const RirTy *rir_tref(Heap *heap, const RirTy *inner) {
  RirTy *ty = rir_ty_alloc(heap, RIR_TREF);
  if (!ty) return &rir_ty_placeholder;
  ty->inner = inner;
  return ty;
}

const RirTy *rir_tfn(Heap *heap, Vec args, const RirTy *ret) {
  RirTy *ty = rir_ty_alloc(heap, RIR_TFN);
  if (!ty) return &rir_ty_placeholder;
  ty->args = args;
  ty->ret = ret;
  return ty;
}

/* ---- Bind: a constructor field in a pattern, a name or `_` for a field without a use ---- */

const RirBind *rir_bvar(Heap *heap, Str name) {
  RirBind *bind = heap_alloc(heap, sizeof(RirBind));
  if (!bind) return &rir_bind_placeholder;
  bind->kind = RIR_BVAR;
  bind->name = name;
  return bind;
}

const RirBind *rir_bskip(Heap *heap) {
  RirBind *bind = heap_alloc(heap, sizeof(RirBind));
  if (!bind) return &rir_bind_placeholder;
  bind->kind = RIR_BSKIP;
  return bind;
}

/* ---- Pat, Param, Arm ---- */

RirPat rir_pctor(Str family, Str ctor, Vec binds) {
  RirPat pat;
  pat.family = family;
  pat.ctor = ctor;
  pat.binds = binds;
  return pat;
}

const RirParam *rir_param(Heap *heap, Str name, const RirTy *ty) {
  RirParam *param = heap_alloc(heap, sizeof(RirParam));
  if (!param) return &rir_param_placeholder;
  param->name = name;
  param->ty = ty;
  return param;
}

const RirArm *rir_arm(Heap *heap, RirPat pat, const RirEx *body) {
  RirArm *arm = heap_alloc(heap, sizeof(RirArm));
  if (!arm) return &rir_arm_placeholder;
  arm->pat = pat;
  arm->body = body;
  return arm;
}

RirMaybeTy rir_no_ty(void) {
  RirMaybeTy none;
  none.some = false;
  none.value = &rir_ty_placeholder;
  return none;
}

RirMaybeTy rir_some_ty(const RirTy *ty) {
  RirMaybeTy some;
  some.some = true;
  some.value = ty;
  return some;
}

/* ---- Ex: XCall calls a function or a local closure; targs go in a turbofish. XLet is
 * `{ let name: ty = value; body }`; the emitter always gives the type, the importer keeps None for
 * `let y = x;` so that the lift prints back byte for byte (E1). ---- */

static RirEx *rir_ex_alloc(Heap *heap, RirExKind kind) {
  RirEx *ex = heap_alloc(heap, sizeof(RirEx));
  if (ex) ex->kind = kind;
  return ex;
}

const RirEx *rir_xvar(Heap *heap, Str name) {
  RirEx *ex = rir_ex_alloc(heap, RIR_XVAR);
  if (!ex) return &rir_ex_placeholder;
  ex->name = name;
  return ex;
}

const RirEx *rir_xint(Heap *heap, Str text) {
  RirEx *ex = rir_ex_alloc(heap, RIR_XINT);
  if (!ex) return &rir_ex_placeholder;
  ex->text = text;
  return ex;
}

const RirEx *rir_xbool(Heap *heap, bool value) {
  RirEx *ex = rir_ex_alloc(heap, RIR_XBOOL);
  if (!ex) return &rir_ex_placeholder;
  ex->boolean = value;
  return ex;
}

const RirEx *rir_xcall(Heap *heap, Str func, Vec targs, Vec args) {
  RirEx *ex = rir_ex_alloc(heap, RIR_XCALL);
  if (!ex) return &rir_ex_placeholder;
  ex->func = func;
  ex->targs = targs;
  ex->args = args;
  return ex;
}

const RirEx *rir_xctor(Heap *heap, Str family, Str ctor, Vec args) {
  RirEx *ex = rir_ex_alloc(heap, RIR_XCTOR);
  if (!ex) return &rir_ex_placeholder;
  ex->family = family;
  ex->ctor = ctor;
  ex->args = args;
  return ex;
}

const RirEx *rir_xmatch(Heap *heap, const RirEx *scrut, Vec arms) {
  RirEx *ex = rir_ex_alloc(heap, RIR_XMATCH);
  if (!ex) return &rir_ex_placeholder;
  ex->scrut = scrut;
  ex->arms = arms;
  return ex;
}

const RirEx *rir_xif(Heap *heap, const RirEx *cond, const RirEx *yes, const RirEx *no) {
  RirEx *ex = rir_ex_alloc(heap, RIR_XIF);
  if (!ex) return &rir_ex_placeholder;
  ex->cond = cond;
  ex->yes = yes;
  ex->no = no;
  return ex;
}

const RirEx *rir_xclosure(Heap *heap, Vec params, const RirEx *body) {
  RirEx *ex = rir_ex_alloc(heap, RIR_XCLOSURE);
  if (!ex) return &rir_ex_placeholder;
  ex->params = params;
  ex->body = body;
  return ex;
}

static const RirEx *rir_wrap(Heap *heap, RirExKind kind, const RirEx *inner) {
  RirEx *ex = rir_ex_alloc(heap, kind);
  if (!ex) return &rir_ex_placeholder;
  ex->inner = inner;
  return ex;
}

const RirEx *rir_xborrow(Heap *heap, const RirEx *inner) { return rir_wrap(heap, RIR_XBORROW, inner); }

const RirEx *rir_xclone(Heap *heap, const RirEx *inner) { return rir_wrap(heap, RIR_XCLONE, inner); }

const RirEx *rir_xbox(Heap *heap, const RirEx *inner) { return rir_wrap(heap, RIR_XBOX, inner); }

const RirEx *rir_xlet(Heap *heap, Str name, RirMaybeTy ty, const RirEx *value, const RirEx *body) {
  RirEx *ex = rir_ex_alloc(heap, RIR_XLET);
  if (!ex) return &rir_ex_placeholder;
  ex->name = name;
  ex->ty = ty;
  ex->value = value;
  ex->body = body;
  return ex;
}

/* ---- Variant ---- */

const RirVariant *rir_variant(Heap *heap, Str name, Vec fields) {
  RirVariant *variant = heap_alloc(heap, sizeof(RirVariant));
  if (!variant) return &rir_variant_placeholder;
  variant->name = name;
  variant->fields = fields;
  return variant;
}

/* ---- Item: IUse = `use crate::<module>::*;`. IEnum.copy: the enum derives Copy. IUnit: a unit struct
 * and its constructor function. Each generic of an IFn gets a Clone bound. ---- */

static RirItem *rir_item_alloc(Heap *heap, RirItemKind kind) {
  RirItem *item = heap_alloc(heap, sizeof(RirItem));
  if (item) item->kind = kind;
  return item;
}

const RirItem *rir_iuse(Heap *heap, Str module) {
  RirItem *item = rir_item_alloc(heap, RIR_IUSE);
  if (!item) return &rir_item_placeholder;
  item->module = module;
  return item;
}

const RirItem *rir_ienum(Heap *heap, Str name, Vec generics, bool copy, Vec variants) {
  RirItem *item = rir_item_alloc(heap, RIR_IENUM);
  if (!item) return &rir_item_placeholder;
  item->name = name;
  item->generics = generics;
  item->copy = copy;
  item->variants = variants;
  return item;
}

const RirItem *rir_iunit(Heap *heap, Str name, Str ctor) {
  RirItem *item = rir_item_alloc(heap, RIR_IUNIT);
  if (!item) return &rir_item_placeholder;
  item->name = name;
  item->ctor = ctor;
  return item;
}

const RirItem *rir_ifn(Heap *heap, Str name, Vec generics, Vec params, const RirTy *ret, const RirEx *body) {
  RirItem *item = rir_item_alloc(heap, RIR_IFN);
  if (!item) return &rir_item_placeholder;
  item->name = name;
  item->generics = generics;
  item->params = params;
  item->ret = ret;
  item->body = body;
  return item;
}

/* ---- Module: source = the name of the input file, for the `//!` line ---- */

RirModule rir_module(Str source, Vec items) {
  RirModule module;
  module.source = source;
  module.items = items;
  return module;
}
