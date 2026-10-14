/* Mechanism Rust frontend: C port of the bend2/rust .bend files, one .c per .bend file and one header
 * for the component. Every function that allocates takes the Heap it allocates from and never aborts:
 * a failed allocation sets heap->oom and returns a well formed placeholder; the program boundary
 * checks heap->oom before it prints any result. Lists are Vecs of pointers to the element type. */
#ifndef MECH_RUST_H
#define MECH_RUST_H

#include "kernel.h"

/* ---- rir (bend2/rust/rir.bend): the erased program IR for `mech rust-out` ---- */

/* Ty: TBool, TCon, TBox, TRef, TFn. */
typedef enum { RIR_TBOOL, RIR_TCON, RIR_TBOX, RIR_TREF, RIR_TFN } RirTyKind;
typedef struct RirTy RirTy;
struct RirTy {
  RirTyKind kind;
  Str name;           /* TCON */
  Vec args;           /* TCON, TFN (RirTy pointers) */
  const RirTy *inner; /* TBOX, TREF */
  const RirTy *ret;   /* TFN */
};
/* Bind: BVar{name} or BSkip{} (`_`). */
typedef enum { RIR_BVAR, RIR_BSKIP } RirBindKind;
typedef struct {
  RirBindKind kind;
  Str name; /* BVAR */
} RirBind;
/* Pat.PCtor */
typedef struct {
  Str family;
  Str ctor;
  Vec binds; /* RirBind pointers */
} RirPat;
/* Param.Param */
typedef struct {
  Str name;
  const RirTy *ty;
} RirParam;
typedef struct RirEx RirEx;
/* Arm<Ex>.Arm */
typedef struct {
  RirPat pat;
  const RirEx *body;
} RirArm;
/* Maybe<Ty> */
typedef struct {
  bool some;
  const RirTy *value;
} RirMaybeTy;
/* Ex: XVar, XInt, XBool, XCall, XCtor, XMatch, XIf, XClosure, XBorrow, XClone, XBox, XLet. */
typedef enum {
  RIR_XVAR,
  RIR_XINT,
  RIR_XBOOL,
  RIR_XCALL,
  RIR_XCTOR,
  RIR_XMATCH,
  RIR_XIF,
  RIR_XCLOSURE,
  RIR_XBORROW,
  RIR_XCLONE,
  RIR_XBOX,
  RIR_XLET
} RirExKind;
struct RirEx {
  RirExKind kind;
  Str name;           /* XVAR, XLET */
  Str text;           /* XINT */
  bool boolean;       /* XBOOL value */
  Str func;           /* XCALL */
  Vec targs;          /* XCALL (RirTy pointers) */
  Vec args;           /* XCALL, XCTOR (RirEx pointers) */
  Str family;         /* XCTOR */
  Str ctor;           /* XCTOR */
  const RirEx *scrut; /* XMATCH */
  Vec arms;           /* XMATCH (RirArm pointers) */
  const RirEx *cond;  /* XIF */
  const RirEx *yes;   /* XIF */
  const RirEx *no;    /* XIF */
  Vec params;         /* XCLOSURE (RirParam pointers) */
  const RirEx *body;  /* XCLOSURE, XLET */
  const RirEx *inner; /* XBORROW, XCLONE, XBOX */
  RirMaybeTy ty;      /* XLET */
  const RirEx *value; /* XLET */
};
/* Variant.Variant */
typedef struct {
  Str name;
  Vec fields; /* RirTy pointers */
} RirVariant;
/* Item: IUse, IEnum, IUnit, IFn. */
typedef enum { RIR_IUSE, RIR_IENUM, RIR_IUNIT, RIR_IFN } RirItemKind;
typedef struct {
  RirItemKind kind;
  Str module;        /* IUSE */
  Str name;          /* IENUM, IUNIT, IFN */
  Vec generics;      /* IENUM, IFN (Str pointers) */
  bool copy;         /* IENUM */
  Vec variants;      /* IENUM (RirVariant pointers) */
  Str ctor;          /* IUNIT */
  Vec params;        /* IFN (RirParam pointers) */
  const RirTy *ret;  /* IFN */
  const RirEx *body; /* IFN */
} RirItem;
/* Module.Module */
typedef struct {
  Str source;
  Vec items; /* RirItem pointers */
} RirModule;

/* Constructors, in the rir.bend type order. */
const RirTy *rir_tbool(Heap *heap);
const RirTy *rir_tcon(Heap *heap, Str name, Vec args);
const RirTy *rir_tbox(Heap *heap, const RirTy *inner);
const RirTy *rir_tref(Heap *heap, const RirTy *inner);
const RirTy *rir_tfn(Heap *heap, Vec args, const RirTy *ret);
const RirBind *rir_bvar(Heap *heap, Str name);
const RirBind *rir_bskip(Heap *heap);
RirPat rir_pctor(Str family, Str ctor, Vec binds);
const RirParam *rir_param(Heap *heap, Str name, const RirTy *ty);
const RirArm *rir_arm(Heap *heap, RirPat pat, const RirEx *body);
RirMaybeTy rir_no_ty(void);
RirMaybeTy rir_some_ty(const RirTy *ty);
const RirEx *rir_xvar(Heap *heap, Str name);
const RirEx *rir_xint(Heap *heap, Str text);
const RirEx *rir_xbool(Heap *heap, bool value);
const RirEx *rir_xcall(Heap *heap, Str func, Vec targs, Vec args);
const RirEx *rir_xctor(Heap *heap, Str family, Str ctor, Vec args);
const RirEx *rir_xmatch(Heap *heap, const RirEx *scrut, Vec arms);
const RirEx *rir_xif(Heap *heap, const RirEx *cond, const RirEx *yes, const RirEx *no);
const RirEx *rir_xclosure(Heap *heap, Vec params, const RirEx *body);
const RirEx *rir_xborrow(Heap *heap, const RirEx *inner);
const RirEx *rir_xclone(Heap *heap, const RirEx *inner);
const RirEx *rir_xbox(Heap *heap, const RirEx *inner);
const RirEx *rir_xlet(Heap *heap, Str name, RirMaybeTy ty, const RirEx *value, const RirEx *body);
const RirVariant *rir_variant(Heap *heap, Str name, Vec fields);
const RirItem *rir_iuse(Heap *heap, Str module);
const RirItem *rir_ienum(Heap *heap, Str name, Vec generics, bool copy, Vec variants);
const RirItem *rir_iunit(Heap *heap, Str name, Str ctor);
const RirItem *rir_ifn(Heap *heap, Str name, Vec generics, Vec params, const RirTy *ret, const RirEx *body);
RirModule rir_module(Str source, Vec items);

/* ---- ast (bend2/rust/ast.bend): the Rust AST of the M0 frontend ---- */

/* Vis: Priv, Pub, PubCrate. */
typedef enum { AST_PRIV, AST_PUB, AST_PUB_CRATE } AstVis;
/* Attr.Attr: `#[name]` or `#[name(arg, ...)]`; each arg is a `::`-joined path. */
typedef struct {
  Str name;
  MaybeVec args; /* Str pointers */
} AstAttr;
/* Pos.Pos: one-based `line:col`; `0:0` = no source text. */
typedef struct {
  Nat line;
  Nat col;
} AstPos;
/* Meta.Meta */
typedef struct {
  bool gap;
  Vec docs;  /* Str pointers */
  Vec attrs; /* AstAttr pointers */
  AstPos pos;
} AstMeta;
/* Seg<Ty>.Seg */
typedef struct {
  Str name;
  Vec args; /* AstTy pointers */
} AstSeg;
typedef struct AstTy AstTy;
/* Maybe<Ty> */
typedef struct {
  bool some;
  const AstTy *value;
} AstMaybeTy;
/* Ty: TPath, TRef, TTuple, TSlice, TInfer, TImpl, TFnSugar. */
typedef enum { AST_TPATH, AST_TREF, AST_TTUPLE, AST_TSLICE, AST_TINFER, AST_TIMPL, AST_TFN_SUGAR } AstTyKind;
struct AstTy {
  AstTyKind kind;
  Vec segs;           /* TPATH (AstSeg pointers) */
  const AstTy *inner; /* TREF, TSLICE */
  Vec items;          /* TTUPLE (AstTy pointers) */
  Vec bounds;         /* TIMPL (AstTy pointers) */
  Str name;           /* TFN_SUGAR */
  Vec args;           /* TFN_SUGAR (AstTy pointers) */
  AstMaybeTy ret;     /* TFN_SUGAR */
};
/* GParam.GParam */
typedef struct {
  Str name;
  Vec bounds; /* AstTy pointers */
} AstGParam;
/* Lit: LInt, LStr, LChar, LBool. */
typedef enum { AST_LINT, AST_LSTR, AST_LCHAR, AST_LBOOL } AstLitKind;
typedef struct {
  AstLitKind kind;
  Str text;     /* LINT, LSTR, LCHAR */
  bool boolean; /* LBOOL value */
} AstLit;
typedef struct AstPat AstPat;
/* Maybe<Pat> */
typedef struct {
  bool some;
  const AstPat *value;
} AstMaybePat;
/* FieldPat<Pat>.FieldPat */
typedef struct {
  Str name;
  AstMaybePat pat;
} AstFieldPat;
/* Pat: PWild, PPath, PTupleStruct, PStruct, PTuple, PLit, PRef, POr. */
typedef enum {
  AST_PWILD,
  AST_PPATH,
  AST_PTUPLE_STRUCT,
  AST_PSTRUCT,
  AST_PTUPLE,
  AST_PLIT,
  AST_PREF,
  AST_POR
} AstPatKind;
struct AstPat {
  AstPatKind kind;
  Vec segs;            /* PPATH, PTUPLE_STRUCT, PSTRUCT (AstSeg pointers) */
  Vec items;           /* PTUPLE_STRUCT, PTUPLE (AstPat pointers) */
  Vec fields;          /* PSTRUCT (AstFieldPat pointers) */
  bool rest;           /* PSTRUCT */
  AstLit lit;          /* PLIT */
  const AstPat *inner; /* PREF */
  Vec alts;            /* POR (AstPat pointers) */
};
/* CParam.CParam */
typedef struct {
  const AstPat *pat;
  AstMaybeTy ty;
} AstCParam;
/* Param: PSelf, PTyped. */
typedef enum { AST_PSELF, AST_PTYPED } AstParamKind;
typedef struct {
  AstParamKind kind;
  bool by_ref;       /* PSELF */
  const AstPat *pat; /* PTYPED */
  const AstTy *ty;   /* PTYPED */
} AstParam;
/* Sig.Sig */
typedef struct {
  Str name;
  Vec generics; /* AstGParam pointers */
  Vec params;   /* AstParam pointers */
  AstMaybeTy ret;
} AstSig;
/* Field: NField, TField. */
typedef enum { AST_NFIELD, AST_TFIELD } AstFieldKind;
typedef struct {
  AstFieldKind kind;
  AstMeta meta;
  AstVis vis;
  Str name; /* NFIELD */
  const AstTy *ty;
} AstField;
/* Body: BUnit, BTuple, BNamed (a struct or variant body). */
typedef enum { AST_BUNIT, AST_BTUPLE, AST_BNAMED } AstBodyKind;
typedef struct {
  AstBodyKind kind;
  Vec fields; /* BTUPLE, BNAMED (AstField pointers) */
} AstBody;
/* Variant.Variant */
typedef struct {
  AstMeta meta;
  Str name;
  AstBody body;
} AstVariant;
/* UseTree: UPath, UName, UGlob, UGroup. */
typedef enum { AST_UPATH, AST_UNAME, AST_UGLOB, AST_UGROUP } AstUseTreeKind;
typedef struct AstUseTree AstUseTree;
struct AstUseTree {
  AstUseTreeKind kind;
  Str seg;                /* UPATH */
  const AstUseTree *sub;  /* UPATH */
  Str name;               /* UNAME */
  MaybeStr alias;         /* UNAME */
  Vec items;              /* UGROUP (AstUseTree pointers) */
};
typedef struct AstNode AstNode;
/* Maybe<Node> */
typedef struct {
  bool some;
  const AstNode *value;
} AstMaybeNode;
/* Stmt<Node>: SLet, SExpr, SItem. A block's tail is its last SEXPR with semi false. */
typedef enum { AST_SLET, AST_SEXPR, AST_SITEM } AstStmtKind;
typedef struct {
  AstStmtKind kind;
  AstMeta meta;         /* SLET, SEXPR */
  const AstPat *pat;    /* SLET */
  AstMaybeTy ty;        /* SLET */
  AstMaybeNode init;    /* SLET */
  const AstNode *expr;  /* SEXPR */
  bool semi;            /* SEXPR */
  const AstNode *item;  /* SITEM */
} AstStmt;
/* Arm<Node>.Arm */
typedef struct {
  AstMeta meta;
  const AstPat *pat;
  AstMaybeNode guard;
  const AstNode *body;
} AstArm;
/* FieldInit<Node>.FieldInit */
typedef struct {
  Str name;
  AstMaybeNode value;
} AstFieldInit;
/* Node: the expressions EPath .. EMacro, then the items IFn .. IType. */
typedef enum {
  AST_EPATH,
  AST_ELIT,
  AST_ECALL,
  AST_EMETHOD,
  AST_EFIELD,
  AST_ETRY,
  AST_EUNARY,
  AST_EBINARY,
  AST_ECLOSURE,
  AST_EBLOCK,
  AST_EIF,
  AST_ELET,
  AST_EMATCH,
  AST_ESTRUCT,
  AST_ETUPLE,
  AST_EPAREN,
  AST_EMACRO,
  AST_IFN,
  AST_ISTRUCT,
  AST_IENUM,
  AST_IUSE,
  AST_IMOD,
  AST_IIMPL,
  AST_ITRAIT,
  AST_ICONST,
  AST_ITYPE
} AstNodeKind;
/* Bend field names that differ from the C names: EIf.body = stmts, IFn.body = fn_body,
 * IStruct.body = struct_body, IMod.inline = is_inline, IMod.inner = inner_docs, IType.ty = maybe_ty. */
struct AstNode {
  AstNodeKind kind;
  Vec segs;                /* EPATH, ESTRUCT (AstSeg pointers) */
  AstLit lit;              /* ELIT */
  const AstNode *func;     /* ECALL */
  Vec args;                /* ECALL, EMETHOD, EMACRO (AstNode pointers) */
  const AstNode *recv;     /* EMETHOD, EFIELD */
  Str name;                /* EMETHOD, EFIELD, EMACRO, ISTRUCT, IENUM, IMOD, ITRAIT, ICONST, ITYPE */
  Vec generics;            /* EMETHOD (AstTy pointers); ISTRUCT, IENUM, IIMPL, ITRAIT, ITYPE (AstGParam pointers) */
  const AstNode *inner;    /* ETRY, EUNARY, EPAREN */
  Str op;                  /* EUNARY, EBINARY */
  const AstNode *lhs;      /* EBINARY */
  const AstNode *rhs;      /* EBINARY */
  bool mv;                 /* ECLOSURE */
  Vec params;              /* ECLOSURE (AstCParam pointers) */
  AstMaybeTy ret;          /* ECLOSURE */
  const AstNode *body;     /* ECLOSURE */
  Vec stmts;               /* EBLOCK stmts, EIF body (AstStmt pointers) */
  const AstNode *cond;     /* EIF */
  AstMaybeNode els;        /* EIF */
  const AstPat *pat;       /* ELET */
  const AstNode *init;     /* ELET */
  const AstNode *scrut;    /* EMATCH */
  Vec arms;                /* EMATCH (AstArm pointers) */
  Vec fields;              /* ESTRUCT (AstFieldInit pointers) */
  AstMaybeNode base;       /* ESTRUCT */
  Vec items;               /* ETUPLE, IMOD, IIMPL, ITRAIT (AstNode pointers) */
  bool bracket;            /* EMACRO */
  AstMeta meta;            /* IFN .. ITYPE */
  AstVis vis;              /* IFN, ISTRUCT, IENUM, IUSE, IMOD, ITRAIT, ICONST, ITYPE */
  AstSig sig;              /* IFN */
  MaybeVec fn_body;        /* IFN body (AstStmt pointers) */
  AstBody struct_body;     /* ISTRUCT body */
  Vec variants;            /* IENUM (AstVariant pointers) */
  const AstUseTree *tree;  /* IUSE */
  bool is_inline;          /* IMOD inline */
  Vec inner_docs;          /* IMOD inner (Str pointers) */
  AstMaybeTy tr;           /* IIMPL */
  const AstTy *self_ty;    /* IIMPL */
  Vec supers;              /* ITRAIT (AstTy pointers) */
  const AstTy *ty;         /* ICONST */
  AstMaybeNode value;      /* ICONST */
  AstMaybeTy maybe_ty;     /* ITYPE ty */
};
/* SrcFile.SrcFile: `inner` = the file's leading `//!` lines. */
typedef struct {
  Vec inner; /* Str pointers */
  Vec items; /* AstNode pointers */
} AstSrcFile;

/* Constructors, in the ast.bend type order. */
const AstAttr *ast_attr(Heap *heap, Str name, MaybeVec args);
AstPos ast_pos(Nat line, Nat col);
AstMeta ast_meta(bool gap, Vec docs, Vec attrs, AstPos pos);
const AstSeg *ast_seg(Heap *heap, Str name, Vec args);
AstMaybeTy ast_no_ty(void);
AstMaybeTy ast_some_ty(const AstTy *ty);
const AstTy *ast_tpath(Heap *heap, Vec segs);
const AstTy *ast_tref(Heap *heap, const AstTy *inner);
const AstTy *ast_ttuple(Heap *heap, Vec items);
const AstTy *ast_tslice(Heap *heap, const AstTy *inner);
const AstTy *ast_tinfer(Heap *heap);
const AstTy *ast_timpl(Heap *heap, Vec bounds);
const AstTy *ast_tfn_sugar(Heap *heap, Str name, Vec args, AstMaybeTy ret);
const AstGParam *ast_gparam(Heap *heap, Str name, Vec bounds);
AstLit ast_lint(Str text);
AstLit ast_lstr(Str text);
AstLit ast_lchar(Str text);
AstLit ast_lbool(bool value);
AstMaybePat ast_no_pat(void);
AstMaybePat ast_some_pat(const AstPat *pat);
const AstFieldPat *ast_field_pat(Heap *heap, Str name, AstMaybePat pat);
const AstPat *ast_pwild(Heap *heap);
const AstPat *ast_ppath(Heap *heap, Vec segs);
const AstPat *ast_ptuple_struct(Heap *heap, Vec segs, Vec items);
const AstPat *ast_pstruct(Heap *heap, Vec segs, Vec fields, bool rest);
const AstPat *ast_ptuple(Heap *heap, Vec items);
const AstPat *ast_plit(Heap *heap, AstLit lit);
const AstPat *ast_pref(Heap *heap, const AstPat *inner);
const AstPat *ast_por(Heap *heap, Vec alts);
const AstCParam *ast_cparam(Heap *heap, const AstPat *pat, AstMaybeTy ty);
const AstParam *ast_pself(Heap *heap, bool by_ref);
const AstParam *ast_ptyped(Heap *heap, const AstPat *pat, const AstTy *ty);
AstSig ast_sig(Str name, Vec generics, Vec params, AstMaybeTy ret);
const AstField *ast_nfield(Heap *heap, AstMeta meta, AstVis vis, Str name, const AstTy *ty);
const AstField *ast_tfield(Heap *heap, AstMeta meta, AstVis vis, const AstTy *ty);
AstBody ast_bunit(void);
AstBody ast_btuple(Vec fields);
AstBody ast_bnamed(Vec fields);
const AstVariant *ast_variant(Heap *heap, AstMeta meta, Str name, AstBody body);
const AstUseTree *ast_upath(Heap *heap, Str seg, const AstUseTree *sub);
const AstUseTree *ast_uname(Heap *heap, Str name, MaybeStr alias);
const AstUseTree *ast_uglob(Heap *heap);
const AstUseTree *ast_ugroup(Heap *heap, Vec items);
AstMaybeNode ast_no_node(void);
AstMaybeNode ast_some_node(const AstNode *node);
const AstStmt *ast_slet(Heap *heap, AstMeta meta, const AstPat *pat, AstMaybeTy ty, AstMaybeNode init);
const AstStmt *ast_sexpr(Heap *heap, AstMeta meta, const AstNode *expr, bool semi);
const AstStmt *ast_sitem(Heap *heap, const AstNode *item);
const AstArm *ast_arm(Heap *heap, AstMeta meta, const AstPat *pat, AstMaybeNode guard, const AstNode *body);
const AstFieldInit *ast_field_init(Heap *heap, Str name, AstMaybeNode value);
const AstNode *ast_epath(Heap *heap, Vec segs);
const AstNode *ast_elit(Heap *heap, AstLit lit);
const AstNode *ast_ecall(Heap *heap, const AstNode *func, Vec args);
const AstNode *ast_emethod(Heap *heap, const AstNode *recv, Str name, Vec generics, Vec args);
const AstNode *ast_efield(Heap *heap, const AstNode *recv, Str name);
const AstNode *ast_etry(Heap *heap, const AstNode *inner);
const AstNode *ast_eunary(Heap *heap, Str op, const AstNode *inner);
const AstNode *ast_ebinary(Heap *heap, Str op, const AstNode *lhs, const AstNode *rhs);
const AstNode *ast_eclosure(Heap *heap, bool mv, Vec params, AstMaybeTy ret, const AstNode *body);
const AstNode *ast_eblock(Heap *heap, Vec stmts);
const AstNode *ast_eif(Heap *heap, const AstNode *cond, Vec body, AstMaybeNode els);
const AstNode *ast_elet(Heap *heap, const AstPat *pat, const AstNode *init);
const AstNode *ast_ematch(Heap *heap, const AstNode *scrut, Vec arms);
const AstNode *ast_estruct(Heap *heap, Vec segs, Vec fields, AstMaybeNode base);
const AstNode *ast_etuple(Heap *heap, Vec items);
const AstNode *ast_eparen(Heap *heap, const AstNode *inner);
const AstNode *ast_emacro(Heap *heap, Str name, bool bracket, Vec args);
const AstNode *ast_ifn(Heap *heap, AstMeta meta, AstVis vis, AstSig sig, MaybeVec body);
const AstNode *ast_istruct(Heap *heap, AstMeta meta, AstVis vis, Str name, Vec generics, AstBody body);
const AstNode *ast_ienum(Heap *heap, AstMeta meta, AstVis vis, Str name, Vec generics, Vec variants);
const AstNode *ast_iuse(Heap *heap, AstMeta meta, AstVis vis, const AstUseTree *tree);
const AstNode *ast_imod(Heap *heap, AstMeta meta, AstVis vis, Str name, bool is_inline, Vec inner, Vec items);
const AstNode *ast_iimpl(Heap *heap, AstMeta meta, Vec generics, AstMaybeTy tr, const AstTy *self_ty, Vec items);
const AstNode *ast_itrait(Heap *heap, AstMeta meta, AstVis vis, Str name, Vec generics, Vec supers, Vec items);
const AstNode *ast_iconst(Heap *heap, AstMeta meta, AstVis vis, Str name, const AstTy *ty, AstMaybeNode value);
const AstNode *ast_itype(Heap *heap, AstMeta meta, AstVis vis, Str name, Vec generics, AstMaybeTy ty);
AstSrcFile ast_src_file(Vec inner, Vec items);

/* no_pos: Pos{0n, 0n}. */
AstPos ast_no_pos(Heap *heap);
/* show_pos: `line:col`, as Tk.show_loc prints a token position. */
Str ast_show_pos(Heap *heap, AstPos p);
/* meta_pos */
AstPos ast_meta_pos(const AstMeta *m);
/* item_pos: the position of an item; `0:0` for an expression (it has no Meta). */
AstPos ast_item_pos(Heap *heap, const AstNode *n);
/* item_line: `fn name`, `struct name`, ..., `use`, `impl`; `expr` for an expression. */
Str ast_item_line(Heap *heap, const AstNode *n);

/* ---- print (bend2/rust/print.bend) ---- */
Str print_file(Heap *heap, const AstSrcFile *f);

/* ---- emit (bend2/rust/emit.bend): rir -> Rust AST -> text ---- */
/* arm_bodies: the bodies of the arms (RirArm pointers), as RirEx pointers. */
Vec emit_arm_bodies(Heap *heap, const Vec *arms);
AstSrcFile emit_lower(Heap *heap, const RirModule *m);
/* text: the text of one emitted module. */
Str emit_text(Heap *heap, const RirModule *m);
/* derive(copy): the one derive attribute of an emitted enum or unit struct (AstAttr pointers). */
Vec emit_derive(Heap *heap, bool copy);

/* ---- erase_typed (bend2/rust/erase_typed.bend): typed erasure for `mech rust-out` ---- */
/* Bd: the erasure of one binder (defined in erase_typed.c). */
typedef struct EtBd EtBd;
/* Knd: the kind of a bound variable (a generic, a proposition or predicate, or a value). */
typedef enum { ET_KGEN, ET_KPROP, ET_KVAL, ET_KLAM } EtKndKind;
typedef struct {
  EtKndKind kind;
  Str name; /* ET_KGEN */
  Vec bds;  /* ET_KLAM: EtBd pointers */
} EtKnd;
/* List<Knd> as a cons list: NULL = Nil, a prepend is one node, index 0 = the head. */
typedef struct EtCtx EtCtx;
struct EtCtx {
  EtKnd head;
  const EtCtx *rest;
};
/* End: the end of a telescope (`Prop`, `Type n`, or no universe). */
typedef enum { ET_END_PROP, ET_END_TYPE, ET_END_NONE } EtEnd;
/* Er: the erasure of a type. */
typedef enum { ET_EDATA, ET_EPROP, ET_ENO } EtErKind;
typedef struct {
  EtErKind kind;
  const RirTy *ty; /* ET_EDATA */
  Str why;         /* ET_ENO */
} EtEr;
/* Env{globals, copies}: copies = the runtime families that derive `Copy`, in declaration order
 * (Str pointers). */
typedef struct {
  Globals globals;
  Vec copies;
} EtEnv;
bool erase_typed_is_lower(unsigned char c);
Str erase_typed_upper_first(Heap *heap, Str s);
/* has: whether `name` is in `xs` (Str pointers). */
bool erase_typed_has(const Vec *xs, Str name);
Str erase_typed_rust_name(Heap *heap, Str s);
Str erase_typed_fn_name(Heap *heap, Str s);
Str erase_typed_lower_first(Heap *heap, Str s);
Str erase_typed_fn_back(Heap *heap, Str rust);
/* first: the head of `xs` (Str pointers), or "" for an empty list. */
Str erase_typed_first(const Vec *xs);
EtEnv erase_typed_env(Globals globals, Vec copies);
Globals erase_typed_globals_of(const EtEnv *env);

/* Class: the classify answer for one mech name. */
typedef enum { ET_CFN, ET_CDATA, ET_CBOOL, ET_CPROP, ET_CTYPE, ET_CABSURD, ET_CNO, ET_CSKIP } EtClassKind;
typedef struct {
  EtClassKind kind;
  Str name;            /* ET_CFN */
  Vec generics;        /* ET_CFN: Str pointers */
  Vec params;          /* ET_CFN: RirParam pointers */
  const RirTy *ret;    /* ET_CFN */
  const RirItem *item; /* ET_CDATA */
  Str why;             /* ET_CNO */
} EtClass;
Vec erase_typed_param_tys(Heap *heap, const Vec *ps);
EtClass erase_typed_family_or_skip(Heap *heap, const EtEnv *env, MaybeFamily fam);
EtClass erase_typed_name_class(Heap *heap, const EtEnv *env, Str name);
Str erase_typed_join(Heap *heap, Str sep, const Vec *xs);
Vec erase_typed_decl_names(Heap *heap, const Vec *decls); /* SDecl pointers -> Str pointers */
Str erase_typed_first_dup(const Vec *names);
Str erase_typed_classify1(Heap *heap, Str source);
Str erase_typed_classify2(Heap *heap, Str first_source, Str second_source);

/* ctor_tys: one entry for each argument of constructor `c` of `fam`: a RirTy pointer = a kept field
 * with its variant type, NULL = an erased field. */
Vec erase_typed_ctor_tys(Heap *heap, const EtEnv *env, MaybeFamily fam, Str c);
Vec erase_typed_let_tys(Heap *heap, const RirTy *t);

/* Es{env, items, bad}: the emit state of one file. D-196 (a): `items` (RirItem pointers) and
 * `bad` (Str pointers) are in source order; the Bend lists are in reverse order, so a C
 * consumer drops each Bend List.reverse of them. */
typedef struct {
  EtEnv env;
  Vec items;
  Vec bad;
} EtEs;
/* Result<String, R.Module>: ok = Done{module}, else Fail{why}. */
typedef struct {
  bool ok;
  RirModule module; /* ok */
  Str why;          /* not ok */
} EtModuleResult;
EtEs erase_typed_emit_source(Heap *heap, const EtEnv *env, Str source);
Str erase_typed_stem(Str s);
EtModuleResult erase_typed_emit1(Heap *heap, Str name, Str source);
EtModuleResult erase_typed_emit2(Heap *heap, Str first_name, Str first_source, Str name, Str source);

/* ---- token (bend2/rust/token.bend): Rust tokens and their one-based positions ---- */
/* surface.h owns token_ and lexer_ and rust_out.c includes both headers, so the Rust frontend
 * definitions take the prefixes rust_token_ and rust_lexer_. */

/* Loc.Loc: one-based line and column; a column counts code points (Bend Chars). */
typedef struct {
  Nat line;
  Nat col;
} RustTokLoc;
/* KindT in declaration order. Keywords stay RUST_TOK_IDENT; the parser compares names.
 * RUST_TOK_COMMENT is a non-doc comment, kept so the fragment pass can refuse it at its position. */
typedef enum {
  RUST_TOK_IDENT,
  RUST_TOK_RAW_IDENT,
  RUST_TOK_LIFETIME,
  RUST_TOK_INT,
  RUST_TOK_STR,
  RUST_TOK_CHR_LIT,
  RUST_TOK_DOC_OUTER,
  RUST_TOK_DOC_INNER,
  RUST_TOK_COMMENT,
  RUST_TOK_PUNCT,
  RUST_TOK_EOF
} RustTokKindTag;
/* KindT: `text` is the name (Ident, RawIdent, Lifetime) or the text (the others); "" for Eof. */
typedef struct {
  RustTokKindTag tag;
  Str text;
} RustTokKind;
/* T.Tok: `gap` is true when one or more blank lines precede the token. */
typedef struct {
  RustTokKind kind;
  RustTokLoc loc;
  bool gap;
} RustTok;
RustTokKind rust_token_kind(RustTokKindTag tag, Str text);
RustTokLoc rust_token_start(Heap *heap);
RustTokLoc rust_token_advance(Heap *heap, RustTokLoc l, uint64_t n);
RustTokLoc rust_token_next_line(Heap *heap, RustTokLoc l);
Str rust_token_show_loc(Heap *heap, RustTokLoc l);
Str rust_token_show_kind(Heap *heap, RustTokKind k);
Str rust_token_show_gap(bool g);
Str rust_token_show(Heap *heap, const RustTok *t);

/* ---- lexer (bend2/rust/lexer.bend): Rust source text to tokens ---- */
/* Result<String, List<Tk.T>>: ok = Done{tokens} (RustTok pointers in source order, Eof last),
 * else Fail{message} (`line:col message`). */
typedef struct {
  bool ok;
  Vec tokens;  /* ok */
  Str message; /* not ok */
} RustLexResult;
RustLexResult rust_lexer_lex(Heap *heap, Str source);

/* ---- parser (bend2/rust/parser.bend): Rust tokens to the AST ---- */
/* Result<String, A.SrcFile>: ok = Done{file}, else Fail{message} (`line:col expected ...`). */
typedef struct {
  bool ok;
  AstSrcFile file; /* ok */
  Str message;     /* not ok */
} RustSrcFileResult;
RustSrcFileResult rust_parser_from_tokens(Heap *heap, const Vec *raw);
RustSrcFileResult rust_parser_file(Heap *heap, Str source);

/* ---- fragment (bend2/rust/fragment.bend): the refusals of the house-convention Rust subset ---- */
/* Refusal.Refusal: the position and the construct name. */
typedef struct {
  RustTokLoc loc;
  Str what;
} FragmentRefusal;
Str fragment_show(Heap *heap, const FragmentRefusal *r);
/* `ts` holds the lexer's RustTok pointers; the result holds FragmentRefusal pointers in source
 * order (empty = the source is in the fragment). */
Vec fragment_refusals(Heap *heap, const Vec *ts);

/* ---- lift (bend2/rust/lift.bend): the Rust AST of the frontend back to the erased program IR ---- */
/* Refusal.Refusal: the position of the nearest construct with a Meta, and the construct. */
typedef struct {
  AstPos pos;
  Str what;
} LiftRefusal;
/* show(r): `<line>:<col>: <construct>`. */
Str lift_show(Heap *heap, const LiftRefusal *r);
/* C: Result<Refusal, R.Module>, the result of lift. */
typedef struct {
  bool ok;
  LiftRefusal refusal; /* !ok */
  RirModule module;    /* ok */
} LiftModuleResult;
/* lift(f): the erased program IR of the file f (items first, then the source name of its `//!`
 * line), or the first refusal. */
LiftModuleResult lift_lift(Heap *heap, const AstSrcFile *f);
/* C: Result<Refusal, Bool> of plain, for resolve.bend (Li.plain). */
typedef struct {
  bool ok;
  LiftRefusal refusal; /* !ok */
  bool flag;           /* ok */
} LiftFlagResult;
/* plain(m): Done{True} for a construct with no docs and no attributes, else the refusal. */
LiftFlagResult lift_plain_result(const AstMeta *m);

/* ---- resolve (bend2/rust/resolve.bend): module list, item table and name checks of the importer ---- */
/* Bad.Bad: the module of the refusal (`lib`, `nat` or a module name), its position, the construct. */
typedef struct {
  Str module;
  AstPos pos;
  Str what;
} ResolveBad;
/* show(b): `src/<module>.rs:<line>:<col>: <construct>`. */
Str resolve_show(Heap *heap, const ResolveBad *b);
/* Src.Src: a module name and the text of its file. */
typedef struct {
  Str name;
  Str text;
} ResolveSrc;
/* Placed.Placed: an item of the erased IR with the position of its first Rust item. */
typedef struct {
  AstPos pos;
  const RirItem *item;
} ResolvePlaced;
/* Mod.Mod: `file` = the `.mech` file of the module (CD4). */
typedef struct {
  Str name;
  Str file;
  Vec items; /* ResolvePlaced pointers */
} ResolveMod;
/* Rk */
typedef enum { RESOLVE_RK_ENUM, RESOLVE_RK_VARIANT, RESOLVE_RK_STRUCT, RESOLVE_RK_CTOR, RESOLVE_RK_FN } ResolveRk;
/* Row.Row: one row of the item table. `rust` = the Rust name (a variant as `<enum>::<variant>`),
 * `mech` = the mech name (CD5). */
typedef struct {
  Str module;
  ResolveRk kind;
  Str rust;
  Str mech;
  AstPos pos;
} ResolveRow;
/* Out.Out: the modules in manifest order and the item table in the same order. */
typedef struct {
  Vec mods; /* ResolveMod pointers */
  Vec rows; /* ResolveRow pointers */
} ResolveOut;
/* C: Result<Bad, List<String>> of lib_mods. */
typedef struct {
  bool ok;
  ResolveBad bad; /* !ok */
  Vec names;      /* ok: Str pointers */
} ResolveNamesResult;
/* lib_mods(r): the module names of the parsed lib.rs, in manifest order. */
ResolveNamesResult resolve_lib_mods(Heap *heap, const RustSrcFileResult *r);
/* C: Result<Bad, Out> of resolve. */
typedef struct {
  bool ok;
  ResolveBad bad;  /* !ok */
  ResolveOut out;  /* ok */
} ResolveOutResult;
/* resolve(lib, srcs): `lib` = the text of src/lib.rs; `srcs` = the text of each module file
 * (ResolveSrc pointers). */
ResolveOutResult resolve_resolve(Heap *heap, Str lib, const Vec *srcs);
/* table_text(o): one `<module> <kind> <rust name> -> <mech name>` line for each row, then one
 * `manifest <module> <file>` line for each module, in manifest order. */
Str resolve_table_text(Heap *heap, const ResolveOut *o);

/* ---- infer (bend2/rust/infer.bend): type inference of the Rust importer (C3, CD8 and CD9) ---- */
/* STerm is the surface term of surface.h (Syntax.T). */
struct STerm;
/* Note: NCtor{targs} gives the type arguments of an XCtor, NMatch{ret} the result type of an XMatch. */
typedef enum { INFER_NCTOR, INFER_NMATCH } InferNoteKind;
typedef struct {
  InferNoteKind kind;
  Vec targs;        /* NCTOR: RirTy pointers */
  const RirTy *ret; /* NMATCH */
} InferNote;
/* Typed.Typed: the mech telescope of one fn and the notes of its body in pre-order. */
typedef struct {
  Str module;
  Str rust;
  Str mech;
  AstPos pos;
  const struct STerm *tele;
  Vec notes; /* InferNote pointers */
} InferTyped;
/* Sig.Sig */
typedef struct {
  Str name;
  Vec gens;   /* Str pointers */
  Vec params; /* RirTy pointers */
  const RirTy *ret;
} InferSig;
/* Fam.Fam */
typedef struct {
  Str name;
  Vec gens;     /* Str pointers */
  Vec variants; /* RirVariant pointers */
} InferFam;
/* Loc.Loc */
typedef struct {
  Str name;
  const RirTy *ty;
} InferLoc;
/* Env.Env: each list in the Bend list order (head first). */
typedef struct {
  Vec sigs; /* InferSig pointers */
  Vec fams; /* InferFam pointers */
  Vec gens; /* Str pointers */
} InferEnv;
/* strip(t): `t` with TRef and TBox removed. */
const RirTy *infer_strip(Heap *heap, const RirTy *t);
/* qty(copies, t): the quantity of an argument of type `t`. */
Quantity infer_qty(const Vec *copies, const RirTy *t);
/* apps(xs, h): `h` applied to each of `xs` (STerm pointers), left to right. */
const struct STerm *infer_apps(Heap *heap, const Vec *xs, const struct STerm *h);
/* mtys(copies, ts): the mech type of each type (STerm pointers). */
Vec infer_mtys(Heap *heap, const Vec *copies, const Vec *ts);
/* mty(copies, t): the mech type of `t`. */
const struct STerm *infer_mty(Heap *heap, const Vec *copies, const RirTy *t);
/* binder_name(name) = Et.fn_back(name) */
Str infer_binder_name(Heap *heap, Str name);
/* nat_sigs(): the Sigs of nat_small and nat_add (InferSig pointers). */
Vec infer_nat_sigs(Heap *heap);
/* mods_env(ms, e): `e` with the Sigs and Fams of the items of each module (ResolveMod pointers). */
InferEnv infer_mods_env(Heap *heap, const Vec *ms, InferEnv e);
/* mods_copies(ms, cs): `cs` with the Copy enums and unit structs of each module (Str pointers). */
Vec infer_mods_copies(Heap *heap, const Vec *ms, const Vec *cs);
/* find_loc(ls, name): the type of the first local (InferLoc pointers) named `name`, NULL = None. */
const RirTy *infer_find_loc(const Vec *ls, Str name);
/* fn_env(e, gs): `e` with the generics of one fn. */
InferEnv infer_fn_env(InferEnv e, Vec gs);
/* Job: JChk{locals, e, want} checks `e` against `want` with the locals in scope; JArm{locals, fam,
 * args, arm, want} checks one arm of a match on a value of family `fam` with type arguments `args`. */
typedef enum { INFER_JCHK, INFER_JARM } InferJobKind;
typedef struct {
  InferJobKind kind;
  Vec locals;          /* InferLoc pointers, head first */
  const RirEx *e;      /* JCHK */
  const InferFam *fam; /* JARM */
  Vec args;            /* JARM (RirTy pointers) */
  const RirArm *arm;   /* JARM */
  const RirTy *want;
} InferJob;
/* St.St: the jobs left (head first) and the notes so far (the last note first). */
typedef struct {
  Vec jobs;  /* InferJob pointers */
  Vec notes; /* InferNote pointers */
} InferSt;
/* Syn.Syn: the type of a scrutinee and the jobs of its parts. */
typedef struct {
  const RirTy *ty;
  Vec jobs; /* InferJob pointers */
} InferSyn;
/* C: Result<String, Syn>. */
typedef struct {
  bool ok;
  Str msg;      /* !ok */
  InferSyn syn; /* ok */
} InferSynResult;
/* C: Result<String, St>. */
typedef struct {
  bool ok;
  Str msg;    /* !ok */
  InferSt st; /* ok */
} InferStResult;
/* JChk{ls, x, want} */
const InferJob *infer_jchk(Heap *heap, Vec ls, const RirEx *x, const RirTy *want);
/* synth(e, ls, x): the type of `x` in a synthesis position (a scrutinee). */
InferSynResult infer_synth(Heap *heap, const InferEnv *e, const Vec *ls, const RirEx *x);
/* arm_list(ls, fam, args, arms, want): one JArm for each arm (RirArm pointers). */
Vec infer_arm_list(Heap *heap, const Vec *ls, const InferFam *fam, const Vec *args, const Vec *arms, const RirTy *want);
/* scrut_fam(e, t): the Fam of a TCon scrutinee type, NULL = None. */
const InferFam *infer_scrut_fam(const InferEnv *e, const RirTy *t);
/* scrut_args(t): the type arguments of a TCon, else []. */
Vec infer_scrut_args(Heap *heap, const RirTy *t);
/* closure_locs(ps, ls): a Loc for each param (RirParam pointers), the last param first, before `ls`. */
Vec infer_closure_locs(Heap *heap, const Vec *ps, const Vec *ls);
/* C: Result<String, List<Note>>. */
typedef struct {
  bool ok;
  Str msg;   /* !ok */
  Vec notes; /* ok: InferNote pointers, in pre-order */
} InferNotesResult;
/* C: Result<Rs.Bad, List<Typed>>. */
typedef struct {
  bool ok;
  ResolveBad bad; /* !ok */
  Vec typed;      /* ok: InferTyped pointers */
} InferTypedResult;
/* step(e, j, st): the result of one job on the state `st`. */
InferStResult infer_step(Heap *heap, const InferEnv *e, const InferJob *j, const InferSt *st);
/* walk(e, r): every job of `r` in order; the notes in pre-order. */
InferNotesResult infer_walk(Heap *heap, const InferEnv *e, InferStResult r);
/* infer(o): the Typed of each fn of the crate; the first refusal wins. */
InferTypedResult infer_infer(Heap *heap, const ResolveOut *o);
/* types_text(ts): `<module> <mech> : <tele> annotated <note count>` and a newline for each Typed. */
Str infer_types_text(Heap *heap, const Vec *ts);

/* ---- lower (bend2/rust/lower.bend) ---- */
/* Low.Low: the `.mech` text of one module and its file name. */
typedef struct {
  Str file;
  Str text;
} LowerLow;
struct SDecl;
/* fn_decl(e, globals, copies, name, gs, ps, ret, body): one fn item as a `def` or a `def rec`
 * (CD10). globals, copies and gs hold Str pointers; ps holds RirParam pointers. */
const struct SDecl *lower_fn_decl(Heap *heap, const InferEnv *e, const Vec *globals, const Vec *copies,
                                  Str name, const Vec *gs, const Vec *ps, const RirTy *ret,
                                  const RirEx *body);
/* lower(o): one Low for each code module in manifest order (LowerLow pointers); the `nat` module
 * has no file (CD7). */
Vec lower_lower(Heap *heap, const ResolveOut *o);
/* files_text(ls): mode `lower`, a `-- file <name>` line and the text of each file. */
Str lower_files_text(Heap *heap, const Vec *ls);
/* source(ls): mode `check`, the files joined in manifest order (CF11). */
Str lower_source(Heap *heap, const Vec *ls);

/* ---- roundtrip (bend2/rust/roundtrip.bend): RT-MECH, two checked programs equal up to bound names ---- */
/* alpha(a, b): term_equal with the bound names ignored (the SPi and SZk names, the names of a leg,
 * the self and index names of a motive, the Let name). */
bool roundtrip_alpha(const Term *a, const Term *b);
/* first_diff(a, b): the name of the first row that differs (Elab.Rows: Vec of CheckRow pointers),
 * in program order; none (some = false) if all rows agree. */
MaybeStr roundtrip_first_diff(const Vec *a, const Vec *b);

/* ---- cli/rust_in (bend2/cli/rust_in.bend; S6.13a): the exports of c/cli/rust_in.c that need the
 * resolve types (cli.h holds rust_in_run; D-226a) ---- */
/* C: Result<Rs.Bad, List<Lo.Low>> of lower_if and lows_of. */
typedef struct {
  bool ok;
  ResolveBad bad; /* !ok */
  Vec lows;       /* ok: LowerLow pointers */
} RustInLows;
/* lows_of(r): the lowered files of a resolved crate, or the refusal of resolve or infer. */
RustInLows rust_in_lows_of(Heap *heap, const ResolveOutResult *r);

#endif
