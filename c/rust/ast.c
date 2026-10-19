/* C port of rust/ast.bend: the Rust AST of the M0 frontend. Bend 2 has no mutually recursive
 * types, so expressions and items share the one recursive type Node, and the helper types take it
 * (or Ty, Pat) as a parameter, as surface/syntax.bend does. A failed allocation sets heap->oom
 * and returns the placeholder of the type (a node without children). */
#include "rust.h"

static const AstAttr ast_attr_placeholder = {{0}};
static const AstSeg ast_seg_placeholder = {{0}};
static const AstTy ast_ty_placeholder = {AST_TINFER};
static const AstGParam ast_gparam_placeholder = {{0}};
static const AstPat ast_pat_placeholder = {AST_PWILD};
static const AstFieldPat ast_field_pat_placeholder = {{0}};
static const AstCParam ast_cparam_placeholder = {&ast_pat_placeholder};
static const AstParam ast_param_placeholder = {AST_PSELF};
static const AstField ast_field_placeholder = {AST_TFIELD, {0}, AST_PRIV, {0}, &ast_ty_placeholder};
static const AstVariant ast_variant_placeholder = {{0}};
static const AstUseTree ast_use_tree_placeholder = {AST_UGLOB};
static const AstNode ast_node_placeholder = {AST_EPATH};
static const AstStmt ast_stmt_placeholder = {AST_SITEM, {0}, NULL, {0}, {0}, NULL, false, &ast_node_placeholder};
static const AstArm ast_arm_placeholder = {{0}, &ast_pat_placeholder, {0}, &ast_node_placeholder};
static const AstFieldInit ast_field_init_placeholder = {{0}};

/* ---- Attr, Pos, Meta, Seg ---- */

const AstAttr *ast_attr(Heap *heap, Str name, MaybeVec args) {
  AstAttr *attr = heap_alloc(heap, sizeof(AstAttr));
  if (!attr) return &ast_attr_placeholder;
  attr->name = name;
  attr->args = args;
  return attr;
}

AstPos ast_pos(Nat line, Nat col) {
  AstPos pos;
  pos.line = line;
  pos.col = col;
  return pos;
}

/* gap: a blank line precedes the construct. docs: outer `///` lines. pos: the first token of the
 * construct after its outer docs and attributes. */
AstMeta ast_meta(bool gap, Vec docs, Vec attrs, AstPos pos) {
  AstMeta meta;
  meta.gap = gap;
  meta.docs = docs;
  meta.attrs = attrs;
  meta.pos = pos;
  return meta;
}

/* One path segment; args are its generic arguments (`::<..>` in expressions). */
const AstSeg *ast_seg(Heap *heap, Str name, Vec args) {
  AstSeg *seg = heap_alloc(heap, sizeof(AstSeg));
  if (!seg) return &ast_seg_placeholder;
  seg->name = name;
  seg->args = args;
  return seg;
}

/* ---- Ty ---- */

AstMaybeTy ast_no_ty(void) {
  AstMaybeTy none;
  none.some = false;
  none.value = &ast_ty_placeholder;
  return none;
}

AstMaybeTy ast_some_ty(const AstTy *ty) {
  AstMaybeTy some;
  some.some = true;
  some.value = ty;
  return some;
}

static AstTy *ast_ty_alloc(Heap *heap, AstTyKind kind) {
  AstTy *ty = heap_alloc(heap, sizeof(AstTy));
  if (ty) ty->kind = kind;
  return ty;
}

const AstTy *ast_tpath(Heap *heap, Vec segs) {
  AstTy *ty = ast_ty_alloc(heap, AST_TPATH);
  if (!ty) return &ast_ty_placeholder;
  ty->segs = segs;
  return ty;
}

const AstTy *ast_tref(Heap *heap, const AstTy *inner) {
  AstTy *ty = ast_ty_alloc(heap, AST_TREF);
  if (!ty) return &ast_ty_placeholder;
  ty->inner = inner;
  return ty;
}

const AstTy *ast_ttuple(Heap *heap, Vec items) {
  AstTy *ty = ast_ty_alloc(heap, AST_TTUPLE);
  if (!ty) return &ast_ty_placeholder;
  ty->items = items;
  return ty;
}

const AstTy *ast_tslice(Heap *heap, const AstTy *inner) {
  AstTy *ty = ast_ty_alloc(heap, AST_TSLICE);
  if (!ty) return &ast_ty_placeholder;
  ty->inner = inner;
  return ty;
}

const AstTy *ast_tinfer(Heap *heap) {
  AstTy *ty = ast_ty_alloc(heap, AST_TINFER);
  return ty ? ty : &ast_ty_placeholder;
}

const AstTy *ast_timpl(Heap *heap, Vec bounds) {
  AstTy *ty = ast_ty_alloc(heap, AST_TIMPL);
  if (!ty) return &ast_ty_placeholder;
  ty->bounds = bounds;
  return ty;
}

const AstTy *ast_tfn_sugar(Heap *heap, Str name, Vec args, AstMaybeTy ret) {
  AstTy *ty = ast_ty_alloc(heap, AST_TFN_SUGAR);
  if (!ty) return &ast_ty_placeholder;
  ty->name = name;
  ty->args = args;
  ty->ret = ret;
  return ty;
}

/* ---- GParam, Lit, FieldPat ---- */

const AstGParam *ast_gparam(Heap *heap, Str name, Vec bounds) {
  AstGParam *param = heap_alloc(heap, sizeof(AstGParam));
  if (!param) return &ast_gparam_placeholder;
  param->name = name;
  param->bounds = bounds;
  return param;
}

static AstLit ast_lit_text(AstLitKind kind, Str text) {
  AstLit lit;
  lit.kind = kind;
  lit.text = text;
  lit.boolean = false;
  return lit;
}

AstLit ast_lint(Str text) { return ast_lit_text(AST_LINT, text); }

AstLit ast_lstr(Str text) { return ast_lit_text(AST_LSTR, text); }

AstLit ast_lchar(Str text) { return ast_lit_text(AST_LCHAR, text); }

AstLit ast_lbool(bool value) {
  AstLit lit = ast_lit_text(AST_LBOOL, str_empty());
  lit.boolean = value;
  return lit;
}

AstMaybePat ast_no_pat(void) {
  AstMaybePat none;
  none.some = false;
  none.value = &ast_pat_placeholder;
  return none;
}

AstMaybePat ast_some_pat(const AstPat *pat) {
  AstMaybePat some;
  some.some = true;
  some.value = pat;
  return some;
}

const AstFieldPat *ast_field_pat(Heap *heap, Str name, AstMaybePat pat) {
  AstFieldPat *field = heap_alloc(heap, sizeof(AstFieldPat));
  if (!field) return &ast_field_pat_placeholder;
  field->name = name;
  field->pat = pat;
  return field;
}

/* ---- Pat ---- */

static AstPat *ast_pat_alloc(Heap *heap, AstPatKind kind) {
  AstPat *pat = heap_alloc(heap, sizeof(AstPat));
  if (pat) pat->kind = kind;
  return pat;
}

const AstPat *ast_pwild(Heap *heap) {
  AstPat *pat = ast_pat_alloc(heap, AST_PWILD);
  return pat ? pat : &ast_pat_placeholder;
}

const AstPat *ast_ppath(Heap *heap, Vec segs) {
  AstPat *pat = ast_pat_alloc(heap, AST_PPATH);
  if (!pat) return &ast_pat_placeholder;
  pat->segs = segs;
  return pat;
}

const AstPat *ast_ptuple_struct(Heap *heap, Vec segs, Vec items) {
  AstPat *pat = ast_pat_alloc(heap, AST_PTUPLE_STRUCT);
  if (!pat) return &ast_pat_placeholder;
  pat->segs = segs;
  pat->items = items;
  return pat;
}

const AstPat *ast_pstruct(Heap *heap, Vec segs, Vec fields, bool rest) {
  AstPat *pat = ast_pat_alloc(heap, AST_PSTRUCT);
  if (!pat) return &ast_pat_placeholder;
  pat->segs = segs;
  pat->fields = fields;
  pat->rest = rest;
  return pat;
}

const AstPat *ast_ptuple(Heap *heap, Vec items) {
  AstPat *pat = ast_pat_alloc(heap, AST_PTUPLE);
  if (!pat) return &ast_pat_placeholder;
  pat->items = items;
  return pat;
}

const AstPat *ast_plit(Heap *heap, AstLit lit) {
  AstPat *pat = ast_pat_alloc(heap, AST_PLIT);
  if (!pat) return &ast_pat_placeholder;
  pat->lit = lit;
  return pat;
}

const AstPat *ast_pref(Heap *heap, const AstPat *inner) {
  AstPat *pat = ast_pat_alloc(heap, AST_PREF);
  if (!pat) return &ast_pat_placeholder;
  pat->inner = inner;
  return pat;
}

const AstPat *ast_por(Heap *heap, Vec alts) {
  AstPat *pat = ast_pat_alloc(heap, AST_POR);
  if (!pat) return &ast_pat_placeholder;
  pat->alts = alts;
  return pat;
}

/* ---- CParam, Param, Sig ---- */

const AstCParam *ast_cparam(Heap *heap, const AstPat *pat, AstMaybeTy ty) {
  AstCParam *param = heap_alloc(heap, sizeof(AstCParam));
  if (!param) return &ast_cparam_placeholder;
  param->pat = pat;
  param->ty = ty;
  return param;
}

const AstParam *ast_pself(Heap *heap, bool by_ref) {
  AstParam *param = heap_alloc(heap, sizeof(AstParam));
  if (!param) return &ast_param_placeholder;
  param->kind = AST_PSELF;
  param->by_ref = by_ref;
  return param;
}

const AstParam *ast_ptyped(Heap *heap, const AstPat *pat, const AstTy *ty) {
  AstParam *param = heap_alloc(heap, sizeof(AstParam));
  if (!param) return &ast_param_placeholder;
  param->kind = AST_PTYPED;
  param->pat = pat;
  param->ty = ty;
  return param;
}

AstSig ast_sig(Str name, Vec generics, Vec params, AstMaybeTy ret) {
  AstSig sig;
  sig.name = name;
  sig.generics = generics;
  sig.params = params;
  sig.ret = ret;
  return sig;
}

/* ---- Field, Body (a struct or variant body), Variant ---- */

static const AstField *ast_field_new(Heap *heap, AstFieldKind kind, AstMeta meta, AstVis vis, Str name,
                                     const AstTy *ty) {
  AstField *field = heap_alloc(heap, sizeof(AstField));
  if (!field) return &ast_field_placeholder;
  field->kind = kind;
  field->meta = meta;
  field->vis = vis;
  field->name = name;
  field->ty = ty;
  return field;
}

const AstField *ast_nfield(Heap *heap, AstMeta meta, AstVis vis, Str name, const AstTy *ty) {
  return ast_field_new(heap, AST_NFIELD, meta, vis, name, ty);
}

const AstField *ast_tfield(Heap *heap, AstMeta meta, AstVis vis, const AstTy *ty) {
  return ast_field_new(heap, AST_TFIELD, meta, vis, str_empty(), ty);
}

static AstBody ast_body(AstBodyKind kind, Vec fields) {
  AstBody body;
  body.kind = kind;
  body.fields = fields;
  return body;
}

AstBody ast_bunit(void) {
  Vec none = {0};
  return ast_body(AST_BUNIT, none);
}

AstBody ast_btuple(Vec fields) { return ast_body(AST_BTUPLE, fields); }

AstBody ast_bnamed(Vec fields) { return ast_body(AST_BNAMED, fields); }

const AstVariant *ast_variant(Heap *heap, AstMeta meta, Str name, AstBody body) {
  AstVariant *variant = heap_alloc(heap, sizeof(AstVariant));
  if (!variant) return &ast_variant_placeholder;
  variant->meta = meta;
  variant->name = name;
  variant->body = body;
  return variant;
}

/* ---- UseTree ---- */

static AstUseTree *ast_use_tree_alloc(Heap *heap, AstUseTreeKind kind) {
  AstUseTree *tree = heap_alloc(heap, sizeof(AstUseTree));
  if (tree) tree->kind = kind;
  return tree;
}

const AstUseTree *ast_upath(Heap *heap, Str seg, const AstUseTree *sub) {
  AstUseTree *tree = ast_use_tree_alloc(heap, AST_UPATH);
  if (!tree) return &ast_use_tree_placeholder;
  tree->seg = seg;
  tree->sub = sub;
  return tree;
}

const AstUseTree *ast_uname(Heap *heap, Str name, MaybeStr alias) {
  AstUseTree *tree = ast_use_tree_alloc(heap, AST_UNAME);
  if (!tree) return &ast_use_tree_placeholder;
  tree->name = name;
  tree->alias = alias;
  return tree;
}

const AstUseTree *ast_uglob(Heap *heap) {
  AstUseTree *tree = ast_use_tree_alloc(heap, AST_UGLOB);
  return tree ? tree : &ast_use_tree_placeholder;
}

const AstUseTree *ast_ugroup(Heap *heap, Vec items) {
  AstUseTree *tree = ast_use_tree_alloc(heap, AST_UGROUP);
  if (!tree) return &ast_use_tree_placeholder;
  tree->items = items;
  return tree;
}

/* ---- Stmt (a block's tail is its last SExpr with semi false), Arm, FieldInit ---- */

AstMaybeNode ast_no_node(void) {
  AstMaybeNode none;
  none.some = false;
  none.value = &ast_node_placeholder;
  return none;
}

AstMaybeNode ast_some_node(const AstNode *node) {
  AstMaybeNode some;
  some.some = true;
  some.value = node;
  return some;
}

static AstStmt *ast_stmt_alloc(Heap *heap, AstStmtKind kind) {
  AstStmt *stmt = heap_alloc(heap, sizeof(AstStmt));
  if (stmt) stmt->kind = kind;
  return stmt;
}

const AstStmt *ast_slet(Heap *heap, AstMeta meta, const AstPat *pat, AstMaybeTy ty, AstMaybeNode init) {
  AstStmt *stmt = ast_stmt_alloc(heap, AST_SLET);
  if (!stmt) return &ast_stmt_placeholder;
  stmt->meta = meta;
  stmt->pat = pat;
  stmt->ty = ty;
  stmt->init = init;
  return stmt;
}

const AstStmt *ast_sexpr(Heap *heap, AstMeta meta, const AstNode *expr, bool semi) {
  AstStmt *stmt = ast_stmt_alloc(heap, AST_SEXPR);
  if (!stmt) return &ast_stmt_placeholder;
  stmt->meta = meta;
  stmt->expr = expr;
  stmt->semi = semi;
  return stmt;
}

const AstStmt *ast_sitem(Heap *heap, const AstNode *item) {
  AstStmt *stmt = ast_stmt_alloc(heap, AST_SITEM);
  if (!stmt) return &ast_stmt_placeholder;
  stmt->item = item;
  return stmt;
}

const AstArm *ast_arm(Heap *heap, AstMeta meta, const AstPat *pat, AstMaybeNode guard, const AstNode *body) {
  AstArm *arm = heap_alloc(heap, sizeof(AstArm));
  if (!arm) return &ast_arm_placeholder;
  arm->meta = meta;
  arm->pat = pat;
  arm->guard = guard;
  arm->body = body;
  return arm;
}

const AstFieldInit *ast_field_init(Heap *heap, Str name, AstMaybeNode value) {
  AstFieldInit *field = heap_alloc(heap, sizeof(AstFieldInit));
  if (!field) return &ast_field_init_placeholder;
  field->name = name;
  field->value = value;
  return field;
}

/* ---- Node: expressions ---- */

static AstNode *ast_node_alloc(Heap *heap, AstNodeKind kind) {
  AstNode *node = heap_alloc(heap, sizeof(AstNode));
  if (node) node->kind = kind;
  return node;
}

const AstNode *ast_epath(Heap *heap, Vec segs) {
  AstNode *node = ast_node_alloc(heap, AST_EPATH);
  if (!node) return &ast_node_placeholder;
  node->segs = segs;
  return node;
}

const AstNode *ast_elit(Heap *heap, AstLit lit) {
  AstNode *node = ast_node_alloc(heap, AST_ELIT);
  if (!node) return &ast_node_placeholder;
  node->lit = lit;
  return node;
}

const AstNode *ast_ecall(Heap *heap, const AstNode *func, Vec args) {
  AstNode *node = ast_node_alloc(heap, AST_ECALL);
  if (!node) return &ast_node_placeholder;
  node->func = func;
  node->args = args;
  return node;
}

const AstNode *ast_emethod(Heap *heap, const AstNode *recv, Str name, Vec generics, Vec args) {
  AstNode *node = ast_node_alloc(heap, AST_EMETHOD);
  if (!node) return &ast_node_placeholder;
  node->recv = recv;
  node->name = name;
  node->generics = generics;
  node->args = args;
  return node;
}

const AstNode *ast_efield(Heap *heap, const AstNode *recv, Str name) {
  AstNode *node = ast_node_alloc(heap, AST_EFIELD);
  if (!node) return &ast_node_placeholder;
  node->recv = recv;
  node->name = name;
  return node;
}

const AstNode *ast_etry(Heap *heap, const AstNode *inner) {
  AstNode *node = ast_node_alloc(heap, AST_ETRY);
  if (!node) return &ast_node_placeholder;
  node->inner = inner;
  return node;
}

const AstNode *ast_eunary(Heap *heap, Str op, const AstNode *inner) {
  AstNode *node = ast_node_alloc(heap, AST_EUNARY);
  if (!node) return &ast_node_placeholder;
  node->op = op;
  node->inner = inner;
  return node;
}

const AstNode *ast_ebinary(Heap *heap, Str op, const AstNode *lhs, const AstNode *rhs) {
  AstNode *node = ast_node_alloc(heap, AST_EBINARY);
  if (!node) return &ast_node_placeholder;
  node->op = op;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

const AstNode *ast_eclosure(Heap *heap, bool mv, Vec params, AstMaybeTy ret, const AstNode *body) {
  AstNode *node = ast_node_alloc(heap, AST_ECLOSURE);
  if (!node) return &ast_node_placeholder;
  node->mv = mv;
  node->params = params;
  node->ret = ret;
  node->body = body;
  return node;
}

const AstNode *ast_eblock(Heap *heap, Vec stmts) {
  AstNode *node = ast_node_alloc(heap, AST_EBLOCK);
  if (!node) return &ast_node_placeholder;
  node->stmts = stmts;
  return node;
}

const AstNode *ast_eif(Heap *heap, const AstNode *cond, Vec body, AstMaybeNode els) {
  AstNode *node = ast_node_alloc(heap, AST_EIF);
  if (!node) return &ast_node_placeholder;
  node->cond = cond;
  node->stmts = body;
  node->els = els;
  return node;
}

const AstNode *ast_elet(Heap *heap, const AstPat *pat, const AstNode *init) {
  AstNode *node = ast_node_alloc(heap, AST_ELET);
  if (!node) return &ast_node_placeholder;
  node->pat = pat;
  node->init = init;
  return node;
}

const AstNode *ast_ematch(Heap *heap, const AstNode *scrut, Vec arms) {
  AstNode *node = ast_node_alloc(heap, AST_EMATCH);
  if (!node) return &ast_node_placeholder;
  node->scrut = scrut;
  node->arms = arms;
  return node;
}

const AstNode *ast_estruct(Heap *heap, Vec segs, Vec fields, AstMaybeNode base) {
  AstNode *node = ast_node_alloc(heap, AST_ESTRUCT);
  if (!node) return &ast_node_placeholder;
  node->segs = segs;
  node->fields = fields;
  node->base = base;
  return node;
}

const AstNode *ast_etuple(Heap *heap, Vec items) {
  AstNode *node = ast_node_alloc(heap, AST_ETUPLE);
  if (!node) return &ast_node_placeholder;
  node->items = items;
  return node;
}

const AstNode *ast_eparen(Heap *heap, const AstNode *inner) {
  AstNode *node = ast_node_alloc(heap, AST_EPAREN);
  if (!node) return &ast_node_placeholder;
  node->inner = inner;
  return node;
}

const AstNode *ast_emacro(Heap *heap, Str name, bool bracket, Vec args) {
  AstNode *node = ast_node_alloc(heap, AST_EMACRO);
  if (!node) return &ast_node_placeholder;
  node->name = name;
  node->bracket = bracket;
  node->args = args;
  return node;
}

/* ---- Node: items (each carries Meta) ---- */

static AstNode *ast_item_alloc(Heap *heap, AstNodeKind kind, AstMeta meta, AstVis vis) {
  AstNode *node = ast_node_alloc(heap, kind);
  if (!node) return node;
  node->meta = meta;
  node->vis = vis;
  return node;
}

const AstNode *ast_ifn(Heap *heap, AstMeta meta, AstVis vis, AstSig sig, MaybeVec body) {
  AstNode *node = ast_item_alloc(heap, AST_IFN, meta, vis);
  if (!node) return &ast_node_placeholder;
  node->sig = sig;
  node->fn_body = body;
  return node;
}

const AstNode *ast_istruct(Heap *heap, AstMeta meta, AstVis vis, Str name, Vec generics, AstBody body) {
  AstNode *node = ast_item_alloc(heap, AST_ISTRUCT, meta, vis);
  if (!node) return &ast_node_placeholder;
  node->name = name;
  node->generics = generics;
  node->struct_body = body;
  return node;
}

const AstNode *ast_ienum(Heap *heap, AstMeta meta, AstVis vis, Str name, Vec generics, Vec variants) {
  AstNode *node = ast_item_alloc(heap, AST_IENUM, meta, vis);
  if (!node) return &ast_node_placeholder;
  node->name = name;
  node->generics = generics;
  node->variants = variants;
  return node;
}

const AstNode *ast_iuse(Heap *heap, AstMeta meta, AstVis vis, const AstUseTree *tree) {
  AstNode *node = ast_item_alloc(heap, AST_IUSE, meta, vis);
  if (!node) return &ast_node_placeholder;
  node->tree = tree;
  return node;
}

const AstNode *ast_imod(Heap *heap, AstMeta meta, AstVis vis, Str name, bool is_inline, Vec inner, Vec items) {
  AstNode *node = ast_item_alloc(heap, AST_IMOD, meta, vis);
  if (!node) return &ast_node_placeholder;
  node->name = name;
  node->is_inline = is_inline;
  node->inner_docs = inner;
  node->items = items;
  return node;
}

/* IImpl has no Vis in ast.bend: vis stays AST_PRIV. */
const AstNode *ast_iimpl(Heap *heap, AstMeta meta, Vec generics, AstMaybeTy tr, const AstTy *self_ty, Vec items) {
  AstNode *node = ast_item_alloc(heap, AST_IIMPL, meta, AST_PRIV);
  if (!node) return &ast_node_placeholder;
  node->generics = generics;
  node->tr = tr;
  node->self_ty = self_ty;
  node->items = items;
  return node;
}

const AstNode *ast_itrait(Heap *heap, AstMeta meta, AstVis vis, Str name, Vec generics, Vec supers, Vec items) {
  AstNode *node = ast_item_alloc(heap, AST_ITRAIT, meta, vis);
  if (!node) return &ast_node_placeholder;
  node->name = name;
  node->generics = generics;
  node->supers = supers;
  node->items = items;
  return node;
}

const AstNode *ast_iconst(Heap *heap, AstMeta meta, AstVis vis, Str name, const AstTy *ty, AstMaybeNode value) {
  AstNode *node = ast_item_alloc(heap, AST_ICONST, meta, vis);
  if (!node) return &ast_node_placeholder;
  node->name = name;
  node->ty = ty;
  node->value = value;
  return node;
}

const AstNode *ast_itype(Heap *heap, AstMeta meta, AstVis vis, Str name, Vec generics, AstMaybeTy ty) {
  AstNode *node = ast_item_alloc(heap, AST_ITYPE, meta, vis);
  if (!node) return &ast_node_placeholder;
  node->name = name;
  node->generics = generics;
  node->maybe_ty = ty;
  return node;
}

/* ---- SrcFile: inner = the file's leading `//!` lines ---- */

AstSrcFile ast_src_file(Vec inner, Vec items) {
  AstSrcFile file;
  file.inner = inner;
  file.items = items;
  return file;
}

/* ---- Definitions ---- */

AstPos ast_no_pos(Heap *heap) { return ast_pos(heap_nat_of_u64(heap, 0), heap_nat_of_u64(heap, 0)); }

/* The text of a position, as Tk.show_loc prints a token position. */
Str ast_show_pos(Heap *heap, AstPos p) {
  Str line = heap_nat_show(heap, p.line);
  Str col = heap_nat_show(heap, p.col);
  return heap_str_cat(heap, heap_str_cat(heap, line, str_lit(":")), col);
}

AstPos ast_meta_pos(const AstMeta *m) { return m->pos; }

/* The position of an item; `0:0` for an expression (it has no Meta). */
AstPos ast_item_pos(Heap *heap, const AstNode *n) {
  switch (n->kind) {
    case AST_IFN:
    case AST_ISTRUCT:
    case AST_IENUM:
    case AST_IUSE:
    case AST_IMOD:
    case AST_IIMPL:
    case AST_ITRAIT:
    case AST_ICONST:
    case AST_ITYPE:
      return ast_meta_pos(&n->meta);
    case AST_EPATH:
    case AST_ELIT:
    case AST_ECALL:
    case AST_EMETHOD:
    case AST_EFIELD:
    case AST_ETRY:
    case AST_EUNARY:
    case AST_EBINARY:
    case AST_ECLOSURE:
    case AST_EBLOCK:
    case AST_EIF:
    case AST_ELET:
    case AST_EMATCH:
    case AST_ESTRUCT:
    case AST_ETUPLE:
    case AST_EPAREN:
    case AST_EMACRO:
      break;
  }
  return ast_no_pos(heap);
}

Str ast_item_line(Heap *heap, const AstNode *n) {
  switch (n->kind) {
    case AST_IFN:
      return heap_str_cat(heap, str_lit("fn "), n->sig.name);
    case AST_ISTRUCT:
      return heap_str_cat(heap, str_lit("struct "), n->name);
    case AST_IENUM:
      return heap_str_cat(heap, str_lit("enum "), n->name);
    case AST_IUSE:
      return str_lit("use");
    case AST_IMOD:
      return heap_str_cat(heap, str_lit("mod "), n->name);
    case AST_IIMPL:
      return str_lit("impl");
    case AST_ITRAIT:
      return heap_str_cat(heap, str_lit("trait "), n->name);
    case AST_ICONST:
      return heap_str_cat(heap, str_lit("const "), n->name);
    case AST_ITYPE:
      return heap_str_cat(heap, str_lit("type "), n->name);
    case AST_EPATH:
    case AST_ELIT:
    case AST_ECALL:
    case AST_EMETHOD:
    case AST_EFIELD:
    case AST_ETRY:
    case AST_EUNARY:
    case AST_EBINARY:
    case AST_ECLOSURE:
    case AST_EBLOCK:
    case AST_EIF:
    case AST_ELET:
    case AST_EMATCH:
    case AST_ESTRUCT:
    case AST_ETUPLE:
    case AST_EPAREN:
    case AST_EMACRO:
      break;
  }
  return str_lit("expr");
}
