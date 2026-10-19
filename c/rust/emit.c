/* C port of rust/emit.bend: lowers the erased program IR (rir.c) to the Rust AST of the
 * frontend (ast.c) and prints it with print.c, which has the rustfmt layout rules. Bend 2 has no
 * mutual recursion, so each Bend walker takes a list and a single value goes through it as a list of
 * one element (`one_ty(tys([t]))`, `one(nodes([e]))`). C has mutual recursion: emit_ty and emit_ex
 * are the single-value walkers, emit_tys and emit_nodes map them over a list, and the Bend helpers
 * one_ty and one (the head of a one-element list) have no C counterpart. A failed allocation sets
 * heap->oom; the ast constructors then return placeholders and the caller checks heap->oom. */
#include "rust.h"

static const Str emit_str_placeholder = {0};

/* Appends `item` to `acc`; a failed push sets heap->oom. */
static void emit_push(Heap *heap, Vec *acc, const void *item) {
  if (!vec_push(acc, (void *)item)) heap->oom = true;
}

static Vec emit_list1(Heap *heap, const void *a) {
  Vec out = vec_new(heap->arena);
  emit_push(heap, &out, a);
  return out;
}

static Vec emit_list2(Heap *heap, const void *a, const void *b) {
  Vec out = emit_list1(heap, a);
  emit_push(heap, &out, b);
  return out;
}

/* A heap copy of `s`, for the lists of Str pointers. */
static const Str *emit_str_box(Heap *heap, Str s) {
  Str *box = heap_alloc(heap, sizeof(Str));
  if (!box) return &emit_str_placeholder;
  *box = s;
  return box;
}

/* An emitted construct has no source text, so each `Meta` has the position 0:0. */
static AstMeta emit_m0(Heap *heap) {
  return ast_meta(false, vec_new(heap->arena), vec_new(heap->arena), ast_no_pos(heap));
}

static AstMeta emit_gap(Heap *heap, Vec ats) { return ast_meta(true, vec_new(heap->arena), ats, ast_no_pos(heap)); }

static Vec emit_path1(Heap *heap, Str a) { return emit_list1(heap, ast_seg(heap, a, vec_new(heap->arena))); }

static Vec emit_path2(Heap *heap, Str a, Str b) {
  return emit_list2(heap, ast_seg(heap, a, vec_new(heap->arena)), ast_seg(heap, b, vec_new(heap->arena)));
}

static const AstTy *emit_ty(Heap *heap, const RirTy *t);

static Vec emit_tys(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    emit_push(heap, &out, emit_ty(heap, xs->items[i]));
  }
  return out;
}

static const AstTy *emit_ty(Heap *heap, const RirTy *t) {
  switch (t->kind) {
    case RIR_TBOOL:
      return ast_tpath(heap, emit_path1(heap, str_lit("bool")));
    case RIR_TCON:
      return ast_tpath(heap, emit_list1(heap, ast_seg(heap, t->name, emit_tys(heap, &t->args))));
    case RIR_TBOX:
      return ast_tpath(heap, emit_list1(heap, ast_seg(heap, str_lit("Box"), emit_list1(heap, emit_ty(heap, t->inner)))));
    case RIR_TREF:
      return ast_tref(heap, emit_ty(heap, t->inner));
    case RIR_TFN:
      return ast_timpl(
          heap, emit_list1(heap, ast_tfn_sugar(heap, str_lit("Fn"), emit_tys(heap, &t->args), ast_some_ty(emit_ty(heap, t->ret)))));
  }
  return ast_tinfer(heap);
}

static const AstPat *emit_name_pat(Heap *heap, Str name) { return ast_ppath(heap, emit_path1(heap, name)); }

static Vec emit_params(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    const RirParam *p = xs->items[i];
    emit_push(heap, &out, ast_ptyped(heap, emit_name_pat(heap, p->name), emit_ty(heap, p->ty)));
  }
  return out;
}

static Vec emit_cparams(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    const RirParam *p = xs->items[i];
    emit_push(heap, &out, ast_cparam(heap, emit_name_pat(heap, p->name), ast_some_ty(emit_ty(heap, p->ty))));
  }
  return out;
}

static const AstPat *emit_bind(Heap *heap, const RirBind *b) {
  switch (b->kind) {
    case RIR_BVAR:
      return emit_name_pat(heap, b->name);
    case RIR_BSKIP:
      return ast_pwild(heap);
  }
  return ast_pwild(heap);
}

static Vec emit_binds(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    emit_push(heap, &out, emit_bind(heap, xs->items[i]));
  }
  return out;
}

static const AstPat *emit_pat(Heap *heap, const RirPat *p) {
  if (p->binds.len == 0) return ast_ppath(heap, emit_path2(heap, p->family, p->ctor));
  return ast_ptuple_struct(heap, emit_path2(heap, p->family, p->ctor), emit_binds(heap, &p->binds));
}

static Vec emit_arm_pats(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    const RirArm *a = xs->items[i];
    emit_push(heap, &out, emit_pat(heap, &a->pat));
  }
  return out;
}

Vec emit_arm_bodies(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    const RirArm *a = xs->items[i];
    emit_push(heap, &out, a->body);
  }
  return out;
}

/* A block with `e` as its tail expression. A let in tail position gives its statements, so
 * `fn f() -> T { let x: T = v; x }` has no inner block (E1). */
static Vec emit_tail(Heap *heap, const AstNode *e) {
  if (e->kind == AST_EBLOCK) return e->stmts;
  return emit_list1(heap, ast_sexpr(heap, emit_m0(heap), e, false));
}

static AstMaybeTy emit_let_ty(Heap *heap, RirMaybeTy t) {
  if (!t.some) return ast_no_ty();
  return ast_some_ty(emit_ty(heap, t.value));
}

/* rustfmt puts an `if` that is an arm body in a block. */
static const AstNode *emit_arm_body(Heap *heap, const AstNode *e) {
  if (e->kind == AST_EIF) return ast_eblock(heap, emit_tail(heap, e));
  return e;
}

/* zip_arms(Two{pats, bodies}): the arms of one match, up to the shorter of the two lists. */
static Vec emit_zip_arms(Heap *heap, const Vec *pats, const Vec *bodies) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < pats->len && i < bodies->len; i++) {
    emit_push(heap, &out, ast_arm(heap, emit_m0(heap), pats->items[i], ast_no_node(), emit_arm_body(heap, bodies->items[i])));
  }
  return out;
}

static const AstNode *emit_ex(Heap *heap, const RirEx *e);

static Vec emit_nodes(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    emit_push(heap, &out, emit_ex(heap, xs->items[i]));
  }
  return out;
}

static const AstNode *emit_ex(Heap *heap, const RirEx *e) {
  switch (e->kind) {
    case RIR_XVAR:
      return ast_epath(heap, emit_path1(heap, e->name));
    case RIR_XINT:
      return ast_elit(heap, ast_lint(e->text));
    case RIR_XBOOL:
      return ast_elit(heap, ast_lbool(e->boolean));
    case RIR_XCALL:
      return ast_ecall(heap, ast_epath(heap, emit_list1(heap, ast_seg(heap, e->func, emit_tys(heap, &e->targs)))),
                       emit_nodes(heap, &e->args));
    case RIR_XCTOR:
      if (e->args.len == 0) return ast_epath(heap, emit_path2(heap, e->family, e->ctor));
      return ast_ecall(heap, ast_epath(heap, emit_path2(heap, e->family, e->ctor)), emit_nodes(heap, &e->args));
    case RIR_XMATCH: {
      Vec pats = emit_arm_pats(heap, &e->arms);
      Vec raw = emit_arm_bodies(heap, &e->arms);
      Vec bodies = emit_nodes(heap, &raw);
      return ast_ematch(heap, emit_ex(heap, e->scrut), emit_zip_arms(heap, &pats, &bodies));
    }
    case RIR_XIF:
      return ast_eif(heap, emit_ex(heap, e->cond), emit_tail(heap, emit_ex(heap, e->yes)),
                     ast_some_node(ast_eblock(heap, emit_tail(heap, emit_ex(heap, e->no)))));
    case RIR_XCLOSURE:
      return ast_eclosure(heap, false, emit_cparams(heap, &e->params), ast_no_ty(), emit_ex(heap, e->body));
    case RIR_XBORROW:
      return ast_eunary(heap, str_lit("&"), emit_ex(heap, e->inner));
    case RIR_XCLONE:
      return ast_emethod(heap, emit_ex(heap, e->inner), str_lit("clone"), vec_new(heap->arena), vec_new(heap->arena));
    case RIR_XBOX:
      return ast_ecall(heap, ast_epath(heap, emit_path2(heap, str_lit("Box"), str_lit("new"))),
                       emit_list1(heap, emit_ex(heap, e->inner)));
    case RIR_XLET: {
      const AstStmt *let = ast_slet(heap, emit_m0(heap), emit_name_pat(heap, e->name), emit_let_ty(heap, e->ty),
                                    ast_some_node(emit_ex(heap, e->value)));
      Vec head = emit_list1(heap, let);
      Vec rest = emit_tail(heap, emit_ex(heap, e->body));
      return ast_eblock(heap, heap_vec_concat(heap, &head, &rest));
    }
  }
  return ast_etuple(heap, vec_new(heap->arena));
}

Vec emit_derive(Heap *heap, bool copy) {
  static const char *const with_copy[] = {"Clone", "Copy", "Debug", "PartialEq", "Eq"};
  static const char *const without_copy[] = {"Clone", "Debug", "PartialEq", "Eq"};
  const char *const *names = copy ? with_copy : without_copy;
  size_t count = copy ? 5 : 4;
  Vec args = vec_new(heap->arena);
  for (size_t i = 0; i < count; i++) {
    emit_push(heap, &args, emit_str_box(heap, str_lit(names[i])));
  }
  MaybeVec some = {true, args};
  return emit_list1(heap, ast_attr(heap, str_lit("derive"), some));
}

/* One generic parameter for each name, each with the bounds `bs`. */
static Vec emit_bounds(Heap *heap, const Vec *xs, Vec bs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    const Str *x = xs->items[i];
    emit_push(heap, &out, ast_gparam(heap, *x, bs));
  }
  return out;
}

static Vec emit_fields(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    emit_push(heap, &out, ast_tfield(heap, emit_m0(heap), AST_PRIV, xs->items[i]));
  }
  return out;
}

static Vec emit_variants(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    const RirVariant *v = xs->items[i];
    if (v->fields.len == 0) {
      emit_push(heap, &out, ast_variant(heap, emit_m0(heap), v->name, ast_bunit()));
    } else {
      Vec tys = emit_tys(heap, &v->fields);
      emit_push(heap, &out, ast_variant(heap, emit_m0(heap), v->name, ast_btuple(emit_fields(heap, &tys))));
    }
  }
  return out;
}

static const AstNode *emit_fn_item(Heap *heap, Str name, const Vec *gs, const Vec *ps, const AstTy *ret, const AstNode *body) {
  Vec clone = emit_list1(heap, ast_tpath(heap, emit_path1(heap, str_lit("Clone"))));
  AstSig sig = ast_sig(name, emit_bounds(heap, gs, clone), emit_params(heap, ps), ast_some_ty(ret));
  MaybeVec fn_body = {true, emit_tail(heap, body)};
  return ast_ifn(heap, emit_gap(heap, vec_new(heap->arena)), AST_PUB, sig, fn_body);
}

/* `use_gap`: a blank line goes before the next `use` (not between two of them). */
static Vec emit_items(Heap *heap, const Vec *xs, bool use_gap) {
  Vec out = vec_new(heap->arena);
  bool gap = use_gap;
  for (size_t i = 0; i < xs->len; i++) {
    const RirItem *it = xs->items[i];
    switch (it->kind) {
      case RIR_IUSE: {
        const AstUseTree *tree = ast_upath(heap, str_lit("crate"), ast_upath(heap, it->module, ast_uglob(heap)));
        emit_push(heap, &out,
                  ast_iuse(heap, ast_meta(gap, vec_new(heap->arena), vec_new(heap->arena), ast_no_pos(heap)), AST_PRIV, tree));
        gap = false;
        break;
      }
      case RIR_IENUM: {
        Vec none = vec_new(heap->arena);
        emit_push(heap, &out,
                  ast_ienum(heap, emit_gap(heap, emit_derive(heap, it->copy)), AST_PUB, it->name,
                            emit_bounds(heap, &it->generics, none), emit_variants(heap, &it->variants)));
        gap = true;
        break;
      }
      case RIR_IUNIT: {
        Vec none = vec_new(heap->arena);
        emit_push(heap, &out,
                  ast_istruct(heap, emit_gap(heap, emit_derive(heap, true)), AST_PUB, it->name, vec_new(heap->arena), ast_bunit()));
        emit_push(heap, &out,
                  emit_fn_item(heap, it->ctor, &none, &none, ast_tpath(heap, emit_path1(heap, it->name)),
                               ast_epath(heap, emit_path1(heap, it->name))));
        gap = true;
        break;
      }
      case RIR_IFN:
        emit_push(heap, &out, emit_fn_item(heap, it->name, &it->generics, &it->params, emit_ty(heap, it->ret), emit_ex(heap, it->body)));
        gap = true;
        break;
    }
  }
  return out;
}

AstSrcFile emit_lower(Heap *heap, const RirModule *m) {
  Str doc = heap_str_cat(heap, heap_str_cat(heap, str_lit(" Emitted by `mech rust-out` from `"), m->source), str_lit("`."));
  return ast_src_file(emit_list1(heap, emit_str_box(heap, doc)), emit_items(heap, &m->items, true));
}

/* The text of one emitted module. */
Str emit_text(Heap *heap, const RirModule *m) {
  AstSrcFile f = emit_lower(heap, m);
  return print_file(heap, &f);
}
