/* A <dir>/<file>.bend path in a comment names bend2/<dir>/<file>.bend in commit 8c90b64,
 * the last tree with the Bend sources. */
/* Mechanism wasm component: C port of the wasm .bend files, one .c per .bend file and one
 * header for the component. A Bend byte list (List<&2, U32> whose items are bytes) is a Str: an
 * immutable byte slice built on the heap arena. Every function that allocates takes the Heap it
 * allocates from and never aborts: a failed allocation sets heap->oom and returns a well formed
 * placeholder (an empty slice or NULL); the program boundary checks heap->oom before it exits. */
#ifndef MECH_WASM_H
#define MECH_WASM_H

#include "kernel.h"

/* ---- gc_encode (wasm/gc_encode.bend): the WasmGC module model and its binary encoder ---- */

/* Heaptype: HEq, HI31, HFunc, HType{index}. */
typedef enum { GC_HEAP_EQ, GC_HEAP_I31, GC_HEAP_FUNC, GC_HEAP_TYPE } GcHeaptypeKind;
typedef struct {
  GcHeaptypeKind kind;
  uint32_t index; /* GC_HEAP_TYPE */
} GcHeaptype;

/* Valtype: I32, Ref{heap}. */
typedef enum { GC_VAL_I32, GC_VAL_REF } GcValtypeKind;
typedef struct {
  GcValtypeKind kind;
  GcHeaptype heap_type; /* GC_VAL_REF */
} GcValtype;

/* Comptype: CStruct{fields}, CArray{field}, CFunc{params, results}. */
typedef enum { GC_COMP_STRUCT, GC_COMP_ARRAY, GC_COMP_FUNC } GcComptypeKind;
typedef struct {
  GcComptypeKind kind;
  Vec fields;      /* GC_COMP_STRUCT: GcValtype*, in order */
  GcValtype field; /* GC_COMP_ARRAY */
  Vec params;      /* GC_COMP_FUNC: GcValtype*, in order */
  Vec results;     /* GC_COMP_FUNC: GcValtype*, in order */
} GcComptype;

/* Instr, one constructor per WasmGC instruction the emitter uses, in the Bend order. */
typedef enum {
  GC_UNREACHABLE,
  GC_BLOCK,
  GC_IF,
  GC_BR,
  GC_BR_IF,
  GC_BR_ON_CAST,
  GC_RETURN,
  GC_CALL,
  GC_RETURN_CALL,
  GC_CALL_REF,
  GC_RETURN_CALL_REF,
  GC_LOCAL_GET,
  GC_LOCAL_SET,
  GC_LOCAL_TEE,
  GC_I32_CONST,
  GC_I32_ADD,
  GC_I32_SUB,
  GC_I32_MUL,
  GC_I32_DIV_U,
  GC_I32_EQ,
  GC_I32_NE,
  GC_I32_LT_U,
  GC_I32_GT_U,
  GC_REF_I31,
  GC_I31_GET_S,
  GC_I31_GET_U,
  GC_REF_CAST,
  GC_REF_FUNC,
  GC_STRUCT_NEW,
  GC_STRUCT_GET,
  GC_ARRAY_NEW,
  GC_ARRAY_GET,
  GC_ARRAY_SET,
  GC_ARRAY_LEN,
  GC_END,
  GC_ELSE
} GcInstrKind;

/* Maybe<&2, Valtype> */
typedef struct {
  bool some;
  GcValtype value;
} GcMaybeValtype;

typedef struct GcInstr GcInstr;
struct GcInstr {
  GcInstrKind kind;
  GcMaybeValtype result; /* GC_BLOCK, GC_IF: the block type */
  Vec body;              /* GC_BLOCK: GcInstr* */
  Vec yes;               /* GC_IF: GcInstr* */
  Vec no;                /* GC_IF: GcInstr* */
  uint32_t index;        /* the label of GC_BR, GC_BR_IF and GC_BR_ON_CAST; the index of the call, local, ref,
                            struct and array instructions; the value of GC_I32_CONST */
  uint32_t field;        /* GC_STRUCT_GET */
  GcHeaptype source;     /* GC_BR_ON_CAST */
  GcHeaptype target;     /* GC_BR_ON_CAST */
  GcHeaptype heap_type;  /* GC_REF_CAST */
};

/* Func{ftype, locals, body} */
typedef struct {
  uint32_t ftype;
  Vec locals; /* GcValtype* */
  Vec body;   /* GcInstr* */
} GcFunc;

/* Export{name, index} */
typedef struct {
  Str name;
  uint32_t index;
} GcExport;

/* Import{module, name, ftype} */
typedef struct {
  Str module;
  Str name;
  uint32_t ftype;
} GcImport;

/* List<&2, U32> */
typedef struct {
  const uint32_t *items;
  size_t len;
} U32List;

/* T = Module{types, imports, funcs, exports, declared} */
typedef struct {
  Vec types;        /* Vec*: one rec group per item, each a Vec of GcComptype* */
  Vec imports;      /* GcImport* */
  Vec funcs;        /* GcFunc* */
  Vec exports;      /* GcExport* */
  U32List declared; /* the declared function indices (element section) */
} GcModule;

/* Constructors: the Bend data constructors, boxed on the heap; NULL after an allocation failure. */
GcHeaptype gc_heaptype(GcHeaptypeKind kind, uint32_t index);
GcValtype gc_valtype_i32(void);
GcValtype gc_valtype_ref(GcHeaptype heap_type);
GcValtype *gc_valtype_box(Heap *heap, GcValtype value);
GcMaybeValtype gc_valtype_some(GcValtype value);
GcMaybeValtype gc_valtype_none(void);
GcComptype *gc_comptype_struct(Heap *heap, Vec fields);
GcComptype *gc_comptype_array(Heap *heap, GcValtype field);
GcComptype *gc_comptype_func(Heap *heap, Vec params, Vec results);
/* An instruction without an immediate (Unreachable, Return, the arithmetic, End, Else, ...). */
GcInstr *gc_instr(Heap *heap, GcInstrKind kind);
/* An instruction with one U32 immediate: the label, index or value. */
GcInstr *gc_instr_index(Heap *heap, GcInstrKind kind, uint32_t index);
GcInstr *gc_instr_block(Heap *heap, GcMaybeValtype result, Vec body);
GcInstr *gc_instr_if(Heap *heap, GcMaybeValtype result, Vec yes, Vec no);
GcInstr *gc_instr_br_on_cast(Heap *heap, uint32_t label, GcHeaptype source, GcHeaptype target);
GcInstr *gc_instr_ref_cast(Heap *heap, GcHeaptype heap_type);
GcInstr *gc_instr_struct_get(Heap *heap, uint32_t index, uint32_t field);
GcFunc *gc_func(Heap *heap, uint32_t ftype, Vec locals, Vec body);
GcExport *gc_export(Heap *heap, Str name, uint32_t index);
GcImport *gc_import(Heap *heap, Str module, Str name, uint32_t ftype);
GcModule *gc_module(Heap *heap, Vec types, Vec imports, Vec funcs, Vec exports, U32List declared);
/* A heap copy of a rec group (Vec of GcComptype*), boxed for GcModule.types. */
Vec *gc_group_box(Heap *heap, Vec group);

/* byte(n) = [n & 255] */
Str gc_byte(Heap *heap, uint32_t n);
/* uleb(n): unsigned LEB128, at most 5 bytes */
Str gc_uleb(Heap *heap, uint32_t n);
/* sleb(n): signed LEB128 of the 32-bit two's complement value, at most 5 bytes */
Str gc_sleb(Heap *heap, uint32_t n);
/* encode(m): the binary module (magic, type, import, function, export, element and code sections) */
Str gc_encode(Heap *heap, const GcModule *m);

/* ---- link (wasm/link.bend): the program table and the layout of the runtime type ids ---- */

/* LinkLeg: LUnit, LRepr{repr}, LMu{tid, fields}. */
typedef enum { LINK_LEG_UNIT, LINK_LEG_REPR, LINK_LEG_MU } LinkLegKind;
typedef struct {
  LinkLegKind kind;
  Repr repr;  /* LINK_LEG_REPR */
  Str tid;    /* LINK_LEG_MU */
  Vec fields; /* LINK_LEG_MU: Repr pointers */
} LinkLeg;

/* Form: FI31, FUnit, FAny, FFields{fields}, FSum{legs}, FFn{arity}. */
typedef enum { FORM_I31, FORM_UNIT, FORM_ANY, FORM_FIELDS, FORM_SUM, FORM_FN } FormKind;
typedef struct {
  FormKind kind;
  Vec fields; /* FORM_FIELDS: Repr pointers */
  Vec legs;   /* FORM_SUM: LinkLeg pointers */
  Nat arity;  /* FORM_FN */
} Form;

/* Fn{params, result, body}: LinkFn, since the WasmGC function above is GcFunc. */
typedef struct {
  Vec params; /* Repr pointers */
  Repr result;
  const Ktm *body;
} LinkFn;

/* MuLeg{tid, family, tag, fields} */
typedef struct {
  Str tid;
  Str family;
  Nat tag;
  Vec fields; /* Repr pointers */
} MuLeg;

/* Prog{funs, posts, mus}: funs and posts are C.Pair<String, A> rows, Pair pointers with first =
 * Str * (the name) and second = LinkFn * or Repr *; mus holds MuLeg pointers. */
typedef struct {
  Vec funs;
  Vec posts;
  Vec mus;
} Prog;

/* Maybe<&2, String> */
typedef struct {
  bool some;
  Str value;
} LinkMaybeStr;

/* Constructors; the boxed ones are NULL after a failed allocation (heap->oom set). Maybe<MuLeg> is
 * a MuLeg pointer, NULL = None. */
LinkLeg link_lunit(void);
LinkLeg link_lrepr(Repr repr);
LinkLeg link_lmu(Str tid, Vec fields);
LinkLeg *link_leg_new(Heap *heap, LinkLeg leg);
Form link_fi31(void);
Form link_funit(void);
Form link_fany(void);
Form link_ffields(Vec fields);
Form link_fsum(Vec legs);
Form link_ffn(Nat arity);
Form *link_form_new(Heap *heap, Form form);
LinkFn *link_fn_new(Heap *heap, Vec params, Repr result, const Ktm *body);
MuLeg *link_mu_leg_new(Heap *heap, Str tid, Str family, Nat tag, Vec fields);
/* C.Pair{name, value}: first = a boxed Str, second = value. */
Pair *link_row_new(Heap *heap, Str name, void *value);

/* Part 1 (Bend lines 30-298). Result values: Repr *, Vec * (Repr, Str or LinkLeg pointers), Form *,
 * Nat * (erase_util_nat_new). */
LinkMaybeStr link_inside(Str prefix, Str s);
/* The pieces of s split at sep outside <...>, Str pointers; "" splits to one empty piece. */
Vec link_split_chars(Heap *heap, Str s, char sep, size_t depth);
Vec link_split_top(Heap *heap, char sep, Str s);
Result link_repr_of_text(Heap *heap, Str s);
/* xs: Str pointers. */
Result link_parse_reprs(Heap *heap, const Vec *xs);
Result link_parse_fields_case(Heap *heap, bool empty, Str body);
Result link_parse_fields(Heap *heap, Str body);
Result link_parse_leg(Heap *heap, bool unit, Str s);
Result link_parse_legs(Heap *heap, const Vec *xs);
Result link_parse_sum(Heap *heap, bool empty, Str s);
Result link_parse_arity(Heap *heap, MaybeNat n, Str t);
Result link_parse_wrapped(Heap *heap, LinkMaybeStr fn, LinkMaybeStr pair, LinkMaybeStr tuple,
                          LinkMaybeStr sum, Str t);
Result link_parse_tid(Heap *heap, Str t);
MuLeg *link_mu_fields(Heap *heap, Result parsed, Str t, Str fam, Nat k);
MuLeg *link_mu_index(Heap *heap, LinkMaybeStr family, MaybeNat index, Str t, Str fam,
                     const Vec *fields);
MuLeg *link_mu_parts(Heap *heap, const Vec *parts, Str t);
MuLeg *link_mu_inside(Heap *heap, LinkMaybeStr body, Str t);
MuLeg *link_parse_mu_leg(Heap *heap, Str t);
Result link_fields_form(Heap *heap, const Form *f, Str t);
Result link_fields_of(Heap *heap, Str t);
Result link_arity_form(Heap *heap, const Form *f, Str t);
Result link_arity_of_fn(Heap *heap, Str t);
Result link_sum_form(Heap *heap, const Form *f, Str t);
Result link_sum_legs(Heap *heap, Str t);
Str link_fn_key(Heap *heap, Nat n);
Repr link_nat_repr(void);
Repr link_any_repr(void);
LinkMaybeStr link_tid_text(Repr r);
Vec link_decls_of_entry(Heap *heap, EraseEntry e);
Vec link_funs_of_decls(Heap *heap, const Vec *ds);
Vec link_funs_of_rows(Heap *heap, const Vec *rows);
Vec link_posts_of_rows(Heap *heap, const Vec *rows);
void link_mu_cons(Heap *heap, MuLeg *m, Vec *rest);
Vec link_mus_of_tids(Heap *heap, const Vec *ts);
Vec link_mus_of_decls(Heap *heap, const Vec *ds);
Vec link_mus_of_rows(Heap *heap, const Vec *rows);
Prog link_program_table(Heap *heap, const Vec *rows);
void *link_lookup(const Vec *rows, Str name);
Vec link_mu_family(Heap *heap, const Vec *ms, Str family);
Nat link_mu_top(Heap *heap, const Vec *ms);
LinkLeg link_mu_at(const Vec *ms, Nat index);
Vec link_mu_order(Heap *heap, Nat n, Nat index, const Vec *ms);
Result link_sum_mu(Heap *heap, const Vec *ms, Str t);
Result link_sum_legs_p(Heap *heap, const Prog *p, Str t);
Vec link_leg_fields(Heap *heap, LinkLeg l);

/* Part 2 (Bend lines 300-763): the representation inference and the key collector. */

/* HeadKind: HFun{name, fn}, HPrim{name}, HValue{}; LinkHead since kernel.h owns HeadKind. fn is a
 * Prog.funs row value. */
typedef enum { LINK_HEAD_FUN, LINK_HEAD_PRIM, LINK_HEAD_VALUE } LinkHeadKind;
typedef struct {
  LinkHeadKind kind;
  Str name;          /* LINK_HEAD_FUN, LINK_HEAD_PRIM */
  const LinkFn *fn;  /* LINK_HEAD_FUN */
} LinkHead;

/* InferQuery: Infer{term, env}, InferApp{kind, head, args, env}. env: List<E.Repr> in list order
 * (index 0 = the head), Repr pointers; args: Ktm pointers. */
typedef enum { INFER_QUERY_INFER, INFER_QUERY_APP } InferQueryKind;
typedef struct {
  InferQueryKind kind;
  const Ktm *term;    /* INFER_QUERY_INFER */
  Vec env;
  LinkHead head_kind; /* INFER_QUERY_APP */
  const Ktm *head;    /* INFER_QUERY_APP */
  Vec args;           /* INFER_QUERY_APP */
} InferQuery;

/* Tspec: TSLimbs, TSBigNat, TSRuntime{params, result}, TSClos, TSFn{arity}, TSStruct{fields},
 * TSLeg{fields}, TSPap{count}, TSSig{params, result}, TSApply{arity}, TSEntry. */
typedef enum {
  TSPEC_LIMBS,
  TSPEC_BIG_NAT,
  TSPEC_RUNTIME,
  TSPEC_CLOS,
  TSPEC_FN,
  TSPEC_STRUCT,
  TSPEC_LEG,
  TSPEC_PAP,
  TSPEC_SIG,
  TSPEC_APPLY,
  TSPEC_ENTRY
} TspecKind;
typedef struct {
  TspecKind kind;
  Vec names;   /* TSPEC_RUNTIME: params, Str pointers */
  Str text;    /* TSPEC_RUNTIME: result */
  Nat arity;   /* TSPEC_FN, TSPEC_APPLY */
  Vec fields;  /* TSPEC_STRUCT, TSPEC_LEG: Repr pointers */
  Nat count;   /* TSPEC_PAP */
  Vec params;  /* TSPEC_SIG: Repr pointers */
  Repr result; /* TSPEC_SIG */
} Tspec;

/* Fspec: FSRuntime{name}, FSProg{name}, FSWrap{name, captures}, FSApply{arity}, FSPapw{arity, applied},
 * FSEntry. */
typedef enum { FSPEC_RUNTIME, FSPEC_PROG, FSPEC_WRAP, FSPEC_APPLY, FSPEC_PAPW, FSPEC_ENTRY } FspecKind;
typedef struct {
  FspecKind kind;
  Str name;     /* FSPEC_RUNTIME, FSPEC_PROG, FSPEC_WRAP */
  Nat captures; /* FSPEC_WRAP */
  Nat arity;    /* FSPEC_APPLY, FSPEC_PAPW */
  Nat applied;  /* FSPEC_PAPW */
} Fspec;

/* Keys{types, funcs}: C.Pair rows, Pair pointers with first = Str * (the key) and second = Tspec *
 * (types) or Fspec * (funcs). */
typedef struct {
  Vec types;
  Vec funcs;
} Keys;

/* Need: the collect query. Lists are Vecs of pointers: reprs and env Repr, legs LinkLeg, terms and args
 * Ktm, branches KtmBranch, funs Pair (first = Str *, second = LinkFn *). */
typedef enum {
  NEED_REPR,
  NEED_REPRS,
  NEED_STRUCT,
  NEED_SUM,
  NEED_LEGS,
  NEED_WRAP,
  NEED_WALK,
  NEED_TERMS,
  NEED_BRANCHES,
  NEED_APP,
  NEED_APPLY,
  NEED_SATURATED,
  NEED_GLOBAL,
  NEED_FUN,
  NEED_FUNS
} NeedKind;
typedef struct {
  NeedKind kind;
  Repr repr;          /* NEED_REPR */
  Vec reprs;          /* NEED_REPRS */
  Str tid;            /* NEED_STRUCT, NEED_SUM */
  Vec legs;           /* NEED_LEGS */
  Str name;           /* NEED_WRAP, NEED_SATURATED, NEED_GLOBAL, NEED_FUN */
  LinkHead head_kind; /* NEED_WRAP, NEED_APPLY, NEED_GLOBAL */
  const Ktm *term;    /* NEED_WALK */
  Vec env;            /* NEED_WALK, NEED_TERMS, NEED_BRANCHES, NEED_APP, NEED_APPLY */
  Vec terms;          /* NEED_TERMS */
  Vec branches;       /* NEED_BRANCHES */
  Repr scrut;         /* NEED_BRANCHES */
  const Ktm *head;    /* NEED_APP, NEED_APPLY */
  Vec args;           /* NEED_APP */
  Nat count;          /* NEED_APPLY, NEED_SATURATED */
  Nat arity;          /* NEED_SATURATED */
  Repr result;        /* NEED_SATURATED */
  bool saturated;     /* NEED_SATURATED */
  const LinkFn *fn;   /* NEED_FUN */
  Vec funs;           /* NEED_FUNS */
} Need;

/* Constructors; the boxed ones are NULL after a failed allocation (heap->oom set). */
LinkHead link_hfun(Str name, const LinkFn *fn);
LinkHead link_hprim(Str name);
LinkHead link_hvalue(void);
InferQuery link_infer_of(const Ktm *term, Vec env);
InferQuery link_infer_app(LinkHead kind, const Ktm *head, Vec args, Vec env);
Tspec link_ts_limbs(void);
Tspec link_ts_big_nat(void);
Tspec link_ts_runtime(Vec params, Str result);
Tspec link_ts_clos(void);
Tspec link_ts_fn(Nat arity);
Tspec link_ts_struct(Vec fields);
Tspec link_ts_leg(Vec fields);
Tspec link_ts_pap(Nat count);
Tspec link_ts_sig(Vec params, Repr result);
Tspec link_ts_apply(Nat arity);
Tspec link_ts_entry(void);
Tspec *link_tspec_new(Heap *heap, Tspec s);
Fspec link_fs_runtime(Str name);
Fspec link_fs_prog(Str name);
Fspec link_fs_wrap(Str name, Nat captures);
Fspec link_fs_apply(Nat arity);
Fspec link_fs_papw(Nat arity, Nat applied);
Fspec link_fs_entry(void);
Fspec *link_fspec_new(Heap *heap, Fspec s);
Keys *link_keys_new(Heap *heap, Keys k);
Need link_nrepr(Repr repr);
Need link_nreprs(Vec reprs);
Need link_nstruct(Str tid);
Need link_nsum(Str tid);
Need link_nlegs(Vec legs);
Need link_nwrap(Str name, LinkHead kind);
Need link_nwalk(const Ktm *term, Vec env);
Need link_nterms(Vec terms, Vec env);
Need link_nbranches(Vec branches, Repr scrut, Vec env);
Need link_napp(const Ktm *head, Vec args, Vec env);
Need link_napply(LinkHead kind, const Ktm *head, Nat count, Vec env);
Need link_nsaturated(Str name, Nat arity, Repr result, Nat count, bool saturated);
Need link_nglobal(Str name, LinkHead kind);
Need link_nfun(Str name, const LinkFn *fn);
Need link_nfuns(Vec funs);

/* Result values: Repr *, LinkLeg *, Vec * (Repr pointers), Keys *. Maybe arguments are pointers,
 * NULL = None. */
Result link_repr_at(Repr *r, Error error);
Result link_branch_at(Heap *heap, LinkLeg *l, Str t, Nat k);
Result link_branch_tid(Heap *heap, LinkMaybeStr t, const Prog *p, Nat k);
Result link_branch_leg(Heap *heap, const Prog *p, Repr scrut, KtmBranch b);
Result link_branch_fields(Heap *heap, bool ok, const Vec *fs, Nat k, Nat n, Nat count);
Result link_branch_binders(Heap *heap, const Prog *p, Repr scrut, KtmBranch b);
bool link_is_prim(Str n);
Repr link_prim_result(Str n);
LinkHead link_head_lookup(const LinkFn *f, Str name);
LinkHead link_head_kind(const Prog *p, const Ktm *h);
bool link_nullary_closure(Heap *heap, Repr r);
Repr link_steps_arity(Heap *heap, Nat m, Nat k);
Result link_steps_result(Heap *heap, Repr r, Nat k);
Result link_after_compare(Heap *heap, Cmp c, Nat m, Repr res, Nat k);
Result link_after_head(Heap *heap, Nat m, Repr res, Nat k);
Result link_global_value(Heap *heap, const Repr *post, Str name);
Result link_global_head(Heap *heap, LinkHead h, const Prog *p, Str name);
Result link_global_repr(Heap *heap, const Prog *p, Str name);
Result link_infer_query(Heap *heap, InferQuery q, const Prog *p);
Result link_infer(Heap *heap, const Prog *p, const Vec *env, const Ktm *tm);
Result link_capture_found(Heap *heap, const LinkFn *f, Str name, Nat count);
Result link_capture_reprs(Heap *heap, const Prog *p, Str name, Nat count);
Keys link_no_keys(Heap *heap);
Keys link_merge(Heap *heap, Keys a, Keys b);
/* ks: Keys pointers. */
Keys link_merge_all(Heap *heap, const Vec *ks);
Keys link_ty_key(Heap *heap, Str k, Tspec s);
Keys link_fn_key_of(Heap *heap, Str k, Fspec s);
Keys link_fnty_keys(Heap *heap, Nat n);
Keys link_nat_data_keys(Heap *heap);
/* ps: Str pointers. */
Keys link_runtime_key(Heap *heap, Str n, Vec ps, Str r);
Keys link_nat_runtime_keys(Heap *heap);
/* rs and ps: Repr pointers. */
Str link_repr_texts(Heap *heap, const Vec *rs);
Str link_leg_key(Heap *heap, const Vec *rs);
Str link_sig_key(Heap *heap, const Vec *ps, Repr r);
Str link_env_tid(Heap *heap, const Vec *rs);
Str link_wrap_fkey(Heap *heap, Str n, Nat c);
Str link_apply_fkey(Heap *heap, Nat n);
Str link_apply_ty_key(Heap *heap, Nat n);
Str link_pap_key(Heap *heap, Nat m, Nat k);
Str link_papw_fkey(Heap *heap, Nat m, Nat k);
/* Maybe<C.Pair<String, List<E.Repr>>>: first = Str *, second = Vec * (Repr pointers); NULL = None. */
Pair *link_leg_type(Heap *heap, LinkLeg l);
Keys link_apply_keys(Heap *heap, Nat k);
Result link_need_apply_positive(Heap *heap, Repr r, Nat k);
Result link_need_apply(Heap *heap, Repr r, Nat k);
Result link_collect(Heap *heap, Need q, const Prog *p);


/* Part 3 (Bend lines 763-1180): the layout. T = Linked{prog, types, groups, tmap, funcs, fmap,
 * arities, calls, declared}. Lists are Vecs of pointers: rows Pair (first = Str *, second = Tspec *,
 * Fspec *, Nat * or Str *), nats Nat *, names Str *, groups Vec *. */
typedef struct {
  Prog prog;
  Vec types;        /* C.Pair<String, Tspec> rows */
  Vec groups;       /* Vec * of type rows */
  Vec tmap;         /* C.Pair<String, Nat> rows */
  Vec funcs;        /* C.Pair<String, Fspec> rows */
  Vec fmap;         /* C.Pair<String, Nat> rows */
  Vec arities;      /* Nat * */
  Vec calls;        /* Nat * */
  U32List declared; /* the FSWrap and FSPapw function indices */
} Linked;
void *link_first(const Pair *p);
void *link_second(const Pair *p);
Vec link_keys_types(Keys k);
Vec link_keys_funcs(Keys k);
Vec link_prog_funcs(const Prog *p);
Vec link_prog_mus(const Prog *p);
bool link_contains_name(const Vec *xs, Str n);
bool link_contains_nat(const Vec *xs, Nat n);
Vec link_dedup_rows(Heap *heap, const Vec *xs, const Vec *seen);
Vec link_dedup_nat(Heap *heap, const Vec *xs, const Vec *seen);
Vec link_arity_keys(Heap *heap, const Vec *ts);
Vec link_call_keys(Heap *heap, const Vec *ts);
Vec link_arity_diffs(Heap *heap, const Vec *ks, Nat m);
Vec link_call_diffs(Heap *heap, const Vec *ms, Nat k);
Vec link_all_arity_diffs(Heap *heap, const Vec *ms, const Vec *ks);
Vec link_all_call_diffs(Heap *heap, const Vec *ks, const Vec *ms);
Nat link_max_nat(const Vec *xs);
Pair *link_close_arities(Heap *heap, Nat fuel, Vec a, Vec k);
Keys link_helper_pap(Heap *heap, const Vec *ks, Nat m);
Keys link_helper_arities(Heap *heap, const Vec *ms, const Vec *ks);
Keys link_helper_calls(Heap *heap, const Vec *ks);
Keys link_helper_keys(Heap *heap, const Vec *a, const Vec *k);
unsigned link_trank(const Tspec *t);
unsigned link_frank(const Fspec *f);
size_t link_text_depth(Str s);
bool link_type_before(const Pair *a, const Pair *b);
Vec link_insert_type(Heap *heap, const Vec *xs, Pair *x);
Vec link_order_types(Heap *heap, const Vec *xs);
Vec link_insert_func(Heap *heap, const Vec *xs, Pair *x);
Vec link_order_funcs(Heap *heap, const Vec *xs);
Vec link_family_names(Heap *heap, const Vec *ms);
Vec link_edges_fields(Heap *heap, const Vec *rs, Str fam, const Vec *families);
Vec link_mu_edges(Heap *heap, const Vec *ms, const Vec *families);
Vec link_edge_targets(Heap *heap, const Vec *es, Str name);
Vec link_reach_go(Heap *heap, Vec todo, Vec seen, const Vec *es);
Vec link_reach(Heap *heap, const Vec *es, Str f);
Str link_min_compare(Cmp c, Str a, Str b);
Str link_string_min(Str a, Str b);
Str link_component_min(Heap *heap, const Vec *fams, const Vec *es, Str f, Str best);
Str link_group_mu(Heap *heap, const MuLeg *m, Str key, const Vec *fams, const Vec *es);
Str link_group_key(Heap *heap, Str key, const Vec *fams, const Vec *es);
Pair *link_take_group(Heap *heap, const Vec *xs, Str g, const Vec *fams, const Vec *es);
Vec link_group_types(Heap *heap, const Vec *xs, const Vec *fams, const Vec *es);
Vec link_flatten_types(Heap *heap, const Vec *gs);
Vec link_indices(Heap *heap, const Vec *xs, Nat index);
U32List link_declared(Heap *heap, const Vec *fs, Nat index);
Linked *link_linked_new(Heap *heap, Linked l);
Linked link_finish_layout(Heap *heap, const Prog *p, Keys base);
/* Result values: Linked * (build), uint32_t * (the index results), GcValtype *, Vec * (valtypes,
 * comptypes, groups), GcComptype *. */
Result link_build(Heap *heap, const Vec *rows);
Result link_index_result(Heap *heap, const Nat *found, Str msg);
Result link_type_index(Heap *heap, const Linked *l, Str key);
Result link_func_index(Heap *heap, const Linked *l, Str key);
Result link_valtype_of(Heap *heap, const Linked *l, Repr r);
Result link_valtypes_of(Heap *heap, const Vec *rs, const Linked *l);
Vec link_eqs(Heap *heap, Nat n);
Result link_runtime_vtype(Heap *heap, bool other, const Linked *l, Str s);
Result link_runtime_valtype(Heap *heap, const Linked *l, Str s);
Result link_runtime_vtypes(Heap *heap, const Vec *ss, const Linked *l);
Result link_comptype_of(Heap *heap, const Tspec *s, const Linked *l);
Result link_comptypes(Heap *heap, const Vec *xs, const Linked *l);
Result link_comptypes_groups(Heap *heap, const Vec *gs, const Linked *l);
Result link_comptype_groups(Heap *heap, const Linked *l);

/* ---- state (wasm/state.bend): the local allocation state of one emitted function ---- */

/* T = State{base, extra}: base is the first free local index, extra the allocated locals
 * (GcValtype *) in order. Wasm prefix because kernel.h already defines Comp. */
typedef struct {
  Nat base;
  Vec extra;
} WasmState;
/* C.Pair<T, Result<Err.T, A>> with A erased to a pointer */
typedef struct {
  WasmState state;
  Result result;
} WasmStep;
/* Comp(A) = T -> C.Pair<T, Result<Err.T, A>> as a closure */
typedef struct {
  WasmStep (*fn)(Heap *heap, void *ctx, WasmState state);
  void *ctx;
} WasmComp;
/* A -> Comp(B) as a closure */
typedef struct {
  WasmComp (*fn)(Heap *heap, void *ctx, void *value);
  void *ctx;
} WasmNext;

WasmComp state_comp_pure(Heap *heap, void *value);
WasmStep state_bind_result(Heap *heap, WasmStep step, WasmNext next);
WasmComp state_comp_bind(Heap *heap, WasmComp action, WasmNext next);
WasmComp state_lift(Heap *heap, Result result);
WasmComp state_fail(Heap *heap, Error error);
/* allocate(state, v): the state with v appended to extra, and Done{uint32_t *} = base + |extra| */
WasmStep state_allocate(Heap *heap, WasmState state, GcValtype v);
WasmComp state_alloc(Heap *heap, GcValtype v);
WasmComp state_pure(Heap *heap, void *value);
/* finish(ft, step): Done{GcFunc *} of the code (Vec * of GcInstr *) and the extra locals */
Result state_finish(Heap *heap, uint32_t ft, WasmStep step);

/* ---- nat_runtime (wasm/nat_runtime.bend): the WasmGC bodies of the Nat runtime helpers ---- */

/* Runtime{array, big, length, digit, copy, normal, cmp_digits, cmp, add_loop, sub_loop, mul_loop,
 * slow_add, slow_sub, slow_mul}: the type and function indices the helper bodies refer to. */
typedef struct {
  uint32_t array;
  uint32_t big;
  uint32_t length;
  uint32_t digit;
  uint32_t copy;
  uint32_t normal;
  uint32_t cmp_digits;
  uint32_t cmp;
  uint32_t add_loop;
  uint32_t sub_loop;
  uint32_t mul_loop;
  uint32_t slow_add;
  uint32_t slow_sub;
  uint32_t slow_mul;
} NatRuntime;
/* Body{locals, code}: locals are GcValtype *, code is GcInstr * */
typedef struct {
  Vec locals;
  Vec code;
} NatBody;
/* Primitive: NatAdd, NatSub, NatMul, NatEq, NatLt */
typedef enum { NAT_PRIM_ADD, NAT_PRIM_SUB, NAT_PRIM_MUL, NAT_PRIM_EQ, NAT_PRIM_LT } NatPrimitive;
/* HelperKind: Length .. SlowMul, Prim{primitive} */
typedef enum {
  NAT_HELPER_LENGTH,
  NAT_HELPER_DIGIT,
  NAT_HELPER_COPY,
  NAT_HELPER_NORMAL,
  NAT_HELPER_COMPARE_DIGITS,
  NAT_HELPER_COMPARE,
  NAT_HELPER_ADD_LOOP,
  NAT_HELPER_SUB_LOOP,
  NAT_HELPER_MUL_LOOP,
  NAT_HELPER_SLOW_ADD,
  NAT_HELPER_SLOW_SUB,
  NAT_HELPER_SLOW_MUL,
  NAT_HELPER_PRIM
} NatHelperTag;
typedef struct {
  NatHelperTag kind;
  NatPrimitive primitive; /* NAT_HELPER_PRIM */
} NatHelperKind;
/* Maybe<&2, HelperKind> */
typedef struct {
  bool some;
  NatHelperKind value;
} MaybeNatHelperKind;
/* Named{name, kind} */
typedef struct {
  const char *name;
  NatHelperKind kind;
} NatNamed;

Vec nat_runtime_cat(Heap *heap, const Vec *a, const Vec *b);
Vec nat_runtime_low_limb(Heap *heap, uint32_t i);
NatBody nat_runtime_nat_length(Heap *heap, const NatRuntime *r);
NatBody nat_runtime_nat_digit(Heap *heap, const NatRuntime *r);
NatBody nat_runtime_nat_copy(Heap *heap, const NatRuntime *r);
NatBody nat_runtime_nat_normal(Heap *heap, const NatRuntime *r);
NatBody nat_runtime_nat_compare_digits(Heap *heap, const NatRuntime *r);
NatBody nat_runtime_nat_compare(Heap *heap, const NatRuntime *r);
NatBody nat_runtime_nat_add_loop(Heap *heap, const NatRuntime *r);
NatBody nat_runtime_nat_sub_loop(Heap *heap, const NatRuntime *r);
NatBody nat_runtime_nat_mul_loop(Heap *heap, const NatRuntime *r);
NatBody nat_runtime_nat_slow_add(Heap *heap, const NatRuntime *r);
NatBody nat_runtime_nat_slow_sub(Heap *heap, const NatRuntime *r);
NatBody nat_runtime_nat_slow_mul(Heap *heap, const NatRuntime *r);
MaybeNatHelperKind nat_runtime_find_kind(const NatNamed *ns, size_t count, Str name);
MaybeNatHelperKind nat_runtime_kind(Str name);
/* nat_indices(l): Done{NatRuntime *} or the first failed index lookup */
Result nat_runtime_nat_indices(Heap *heap, const Linked *l);
Vec nat_runtime_slow(Heap *heap, NatPrimitive p, const NatRuntime *r);
Vec nat_runtime_cast(Heap *heap, uint32_t param, uint32_t dst, const Vec *slow);
Vec nat_runtime_small(Heap *heap, NatPrimitive p, const Vec *slow);
NatBody nat_runtime_nat_primitive(Heap *heap, const NatRuntime *r, NatPrimitive p);
NatBody nat_runtime_make_body(Heap *heap, NatHelperKind k, const NatRuntime *r);
GcFunc *nat_runtime_finish(Heap *heap, uint32_t ft, NatBody b);
/* emit_kind(k, name, l) and runtime_func(l, name): Done{GcFunc *} */
Result nat_runtime_emit_kind(Heap *heap, MaybeNatHelperKind k, Str name, const Linked *l);
Result nat_runtime_runtime_func(Heap *heap, const Linked *l, Str name);

/* ---- emit (wasm/emit.bend): kernel terms to WasmGC functions ----
 * An S.Comp(A) def is ported in direct state-passing form: it takes WasmState *st, updates it in
 * place and returns the Result (Done value boxed). Instruction lists are Vec * of GcInstr *. */
typedef struct {
  const Linked *layout;
  Repr result;
} EmitCtx;
typedef struct {
  uint32_t index;
  Repr repr;
} EmitBinding;

const Linked *emit_layout(const EmitCtx *c);
const Prog *emit_prog(const Linked *l);
Vec emit_cat(Heap *heap, const Vec *a, const Vec *b);
Vec emit_cats(Heap *heap, const Vec *xs);
bool emit_same(Heap *heap, Repr a, Repr b);
Result emit_coerce_diff(Heap *heap, const Linked *l, Repr src, Repr dst);
Result emit_coerce(Heap *heap, const Linked *l, Repr src, Repr dst);
bool emit_terminal(const Vec *body);
Vec emit_with_coercion(Heap *heap, const Vec *body, const Vec *cast);
bool emit_empty_cast(Result r);
bool emit_tail_ok(Heap *heap, const EmitCtx *c, bool tail, Repr r);
Vec emit_reprs(Heap *heap, const Vec *env);
Result emit_rep(Heap *heap, const EmitCtx *c, const Vec *env, const Ktm *tm);
Vec emit_stores(Heap *heap, Limbs xs, uint32_t arr, uint32_t local, uint32_t i);
Result emit_literal_big(Heap *heap, WasmState *st, const Linked *l, MaybeLimbs digits);
Result emit_literal_small(Heap *heap, WasmState *st, const Linked *l, Big n, MaybeU32 small);
Result emit_literal(Heap *heap, WasmState *st, const Linked *l, const Literal *lit);
Result emit_closure_value(Heap *heap, WasmState *st, const EmitCtx *c, Str n, Nat m);
Result emit_optional(const void *m, Error e);
VecGet emit_nth(const Vec *xs, Nat i);
/* b: Maybe<Binding> as an EmitBinding pointer, NULL = None. */
Result emit_var(Heap *heap, WasmState *st, const EmitBinding *b, Nat i);
Result emit_global_fun(Heap *heap, WasmState *st, Nat m, const EmitCtx *c, Str name, bool tail, Repr r);
Result emit_global_kind(Heap *heap, WasmState *st, LinkHead k, const EmitCtx *c, Str name, bool tail);
Result emit_global(Heap *heap, WasmState *st, const EmitCtx *c, Str name, bool tail);
/* The law block of emit.bend (go, arg_at, apply, steps, case_term): mutually recursive. Lists are
 * Vecs of pointers: env EmitBinding, args/xs Ktm, rs/params Repr, head/code GcInstr. */
Result emit_go(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, const Ktm *tm);
Result emit_arg_at(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, const Ktm *tm, Repr r);
Result emit_apply(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, const Ktm *h,
                  const Vec *args);
Result emit_steps(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, const Vec *head, Repr r,
                  const Vec *args);
Result emit_each(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, const Vec *xs);
Result emit_each2(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, const Vec *xs, const Vec *rs);
Result emit_direct_last(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, Repr r,
                        const Vec *ia, uint32_t fi, const Vec *rest);
Result emit_direct_short(Heap *heap, WasmState *st, bool is_short, const EmitCtx *c, const Vec *env, bool tail,
                         Str name, const Vec *params, Repr result, const Vec *args);
Result emit_direct(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, Str name,
                   const Vec *params, Repr result, const Vec *args);
Result emit_primitive_short(Heap *heap, WasmState *st, bool is_short, const EmitCtx *c, const Vec *env, bool tail,
                            Str name, const Vec *args);
Result emit_apply_kind(Heap *heap, WasmState *st, LinkHead k, const EmitCtx *c, const Vec *env, bool tail,
                       const Ktm *head, const Vec *args);
Result emit_steps_apply(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, const Vec *head,
                        Repr r, const Vec *args);
Result emit_steps_type(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, const Vec *head,
                       Repr r, const Vec *args);
/* Reads{code, env}: the result of leg_reads; code GcInstr pointers, env EmitBinding pointers. */
typedef struct {
  Vec code;
  Vec env;
} EmitReads;
/* Maybe<C.Pair<String, List<Repr>>> is a const Pair * (first Str *, second Vec * of Repr *), NULL = None.
 * Shapes are Vecs of uint32_t pointers; keys and seen are Vecs of Str pointers; bs holds KtmBranch pointers. */
Result emit_capture_env(Heap *heap, const Linked *l, Nat n, const Vec *rs, const Vec *code);
Result emit_tag_payload(Heap *heap, WasmState *st, const Pair *p, const EmitCtx *c, const Vec *env, const Vec *head,
                        const Vec *args);
Result emit_case_nonempty(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, Tid tid,
                          const Ktm *sc, const Vec *code, const Vec *bs);
Vec emit_add_leg_key(Heap *heap, const Pair *p, const Vec *keys);
Vec emit_leg_keys(Heap *heap, const Vec *ls);
bool emit_has_string(const Vec *xs, Str x);
Vec emit_unique(Heap *heap, const Vec *xs, const Vec *seen);
Result emit_indices(Heap *heap, const Linked *l, const Vec *keys);
Vec emit_cast_tags(Heap *heap, const Vec *shapes, uint32_t depth);
Vec emit_tag_layers(Heap *heap, const Vec *shapes, const Vec *code);
GcInstr *emit_tag_block(Heap *heap, uint32_t sv, const Vec *shapes);
Vec emit_reads_code(EmitReads r);
Vec emit_reads_env(EmitReads r);
Result emit_leg_reads(Heap *heap, WasmState *st, const EmitCtx *c, uint32_t sv, uint32_t li, uint32_t i,
                      const Vec *rs);
Result emit_branch_code(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, Repr cr,
                        const Ktm *body);
Result emit_branch_payload(Heap *heap, WasmState *st, const Pair *p, const EmitCtx *c, const Vec *env, bool tail,
                           uint32_t sv, Repr cr, const Ktm *body);
Result emit_branch_bindings(Heap *heap, WasmState *st, const Vec *rs, const EmitCtx *c, const Vec *env, bool tail,
                            Repr sr, uint32_t sv, Repr cr, const KtmBranch *b);
Result emit_branches(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, Repr sr, uint32_t sv,
                     GcValtype cvt, Repr cr, uint32_t tg, const Vec *bs);
Result emit_case_term(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, Tid tid, const Ktm *sc,
                      const Vec *code, const Vec *bs);
Vec emit_param_bindings(Heap *heap, const Vec *ps, uint32_t i);
Vec emit_param_env(Heap *heap, const Vec *ps);
Result emit_prog_func(Heap *heap, const Linked *l, const LinkFn *f);
Result emit_wrapper_args(Heap *heap, const Vec *ps, const Linked *l, uint32_t i);
Result emit_wrapper_captures(Heap *heap, const Vec *rs, const Linked *l, uint32_t ei, uint32_t i);
Result emit_wrapper_env(Heap *heap, Nat n, const Linked *l, const Vec *rs);
Result emit_wrapper_kind(Heap *heap, LinkHead k, const Linked *l, Str name, Nat caps);
Result emit_wrapper(Heap *heap, const Linked *l, Str name, Nat caps);
Vec emit_gets(Heap *heap, Nat n, uint32_t start);
Result emit_apply_arm_kind(Heap *heap, Cmp order, const Linked *l, Nat k, Nat m, uint32_t cl, uint32_t ci,
                           uint32_t fm);
Result emit_apply_arm(Heap *heap, const Linked *l, Nat k, Nat m, uint32_t cl, uint32_t ci);
Result emit_apply_chain(Heap *heap, const Vec *ms, const Linked *l, Nat k, uint32_t cl, uint32_t ci, uint32_t ar);
const Vec *emit_arities(const Linked *l);
Result emit_apply_func(Heap *heap, const Linked *l, Nat k);
Vec emit_saved_args(Heap *heap, Nat n, uint32_t pv, uint32_t pi, uint32_t i);
Result emit_papw_func(Heap *heap, const Linked *l, Nat m, Nat k);
Result emit_entry_func(Heap *heap, const Linked *l, Str export);
Result emit_one_func(Heap *heap, const Linked *l, Str export, const Fspec *spec);
Str emit_export_name(const Str *e);
Result emit_funcs(Heap *heap, const Vec *fs, const Linked *l, const Str *export);
Result emit_check_export(Heap *heap, const LinkFn *f, Str export);
Result emit_program_linked(Heap *heap, const Linked *l, Str export);
Result emit_program(Heap *heap, const Vec *rows, Str export);
/* HostHandle{repr, index, padding, payload} */
typedef struct {
  Repr repr;
  uint32_t index;
  Nat padding;
  GcValtype payload;
} EmitHostHandle;
Result emit_host_find(Heap *heap, const Vec *hs, Repr r);
GcValtype emit_handle_type(const EmitHostHandle *h);
Result emit_host_type(Heap *heap, const Vec *hs, Repr r);
Result emit_host_types(Heap *heap, const Vec *rs, const Vec *hs);
Vec emit_host_read(Heap *heap, const EmitHostHandle *h, uint32_t i);
Result emit_host_argument(Heap *heap, const Vec *hs, uint32_t i, Repr r);
Vec emit_zeros(Heap *heap, Nat n);
NatBody emit_host_wrap(Heap *heap, const EmitHostHandle *h);
Result emit_host_result(Heap *heap, const Vec *hs, uint32_t local, Repr r);
Result emit_host_arguments(Heap *heap, const Vec *ps, const Vec *hs, uint32_t i);
GcFunc *emit_host_func(Heap *heap, uint32_t ft, uint32_t fi, const Vec *args, const NatBody *body);
Result emit_host_wrapper(Heap *heap, const Linked *l, const Vec *hs, uint32_t ft, Str name, const LinkFn *f);
Result emit_host_definitions(Heap *heap, const Vec *ns, const Prog *p);
Vec emit_host_reprs(Heap *heap, const Vec *ds);
Cmp emit_text_order(int order);
Vec emit_repr_insert_order(Heap *heap, Cmp order, const Repr *x, const Repr *y, const Vec *rest);
Vec emit_repr_insert(Heap *heap, const Vec *xs, const Repr *x);
Vec emit_repr_unique(Heap *heap, const Vec *xs);
Result emit_supported(Heap *heap, const Vec *rs);
Result emit_handles(Heap *heap, const Vec *rs, const Linked *l, Nat base, Nat padding);
Vec emit_i32s(Heap *heap, Nat n);
Vec emit_handle_types(Heap *heap, const Vec *hs);
Result emit_host_signatures(Heap *heap, const Vec *ds, const Vec *hs);
Result emit_host_wrappers(Heap *heap, const Vec *ds, const Linked *l, const Vec *hs, uint32_t ft);
Vec emit_host_exports(Heap *heap, const Vec *ds, uint32_t base);
Result emit_reactor_linked(Heap *heap, const Linked *l, const Vec *ns);
Result emit_valid_exports(Heap *heap, const Vec *ns);
Result emit_reactor(Heap *heap, const Vec *rows, const Vec *exports);

#endif
