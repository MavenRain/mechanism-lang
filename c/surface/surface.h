/* Mechanism surface: C port of the bend2/surface .bend files, one .c per .bend file and one header
 * for the component. Same allocation discipline as kernel.h: every function that allocates takes
 * the Heap, a failed allocation sets heap->oom and returns a well formed placeholder, library code
 * never aborts. Lists are Vec values of pointers to the element type named at each field. */
#ifndef MECH_SURFACE_H
#define MECH_SURFACE_H

#include "kernel.h"

/* String concatenation of COUNT parts in order (Bend `++` chains). */
Str surface_cat(Heap *heap, const Str *parts, size_t count);
#define SURFACE_NARGS_(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, N, ...) N
#define SURFACE_NARGS(...) SURFACE_NARGS_(__VA_ARGS__, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)
#define SURFACE_CAT(heap, ...) surface_cat(heap, (const Str[]){__VA_ARGS__}, SURFACE_NARGS(__VA_ARGS__))

/* ---- universe (bend2/surface/universe.bend) ---- */
typedef enum {
  UNIVERSE_NAT_LEVEL,
  UNIVERSE_VAR_LEVEL,
  UNIVERSE_SUCC_LEVEL,
  UNIVERSE_MAX_LEVEL,
  UNIVERSE_IMAX_LEVEL
} UniverseKind;
typedef struct Universe Universe;
struct Universe {
  UniverseKind kind;
  Big value;             /* NAT_LEVEL */
  Nat index;             /* VAR_LEVEL */
  const Universe *left;  /* SUCC_LEVEL (value), MAX_LEVEL, IMAX_LEVEL */
  const Universe *right; /* MAX_LEVEL, IMAX_LEVEL */
};
const Universe *universe_nat_level(Heap *heap, Big value);
const Universe *universe_var_level(Heap *heap, Nat index);
const Universe *universe_succ_level(Heap *heap, const Universe *value);
const Universe *universe_max_level(Heap *heap, const Universe *left, const Universe *right);
const Universe *universe_imax_level(Heap *heap, const Universe *left, const Universe *right);
Str universe_text(Heap *heap, const Universe *level);
/* Result<Error.T, Level.T>: RESULT_DONE carries a const LevelVar *. */
Result universe_lower_nat(Heap *heap, MaybeBig value);
Result universe_lower(Heap *heap, const Universe *level);

/* ---- syntax (bend2/surface/syntax.bend) ---- */
/* The generic parameter A of Binder, Motive, Field, Branch, FamCtor, Fam and RecDef is STerm. */
typedef enum { SPRIM_ADD, SPRIM_SUB, SPRIM_MUL, SPRIM_EQ, SPRIM_LT } SPrim;
typedef struct STerm STerm;
typedef struct {
  Quantity quantity;
  Str name;
  const STerm *ty;
} SBinder;
/* indices: List<String> as a Vec of Str pointers. */
typedef struct {
  Str self;
  MaybeStr ind;
  Vec indices;
  const STerm *body;
} SMotive;
typedef struct {
  bool some;
  SMotive value;
} MaybeSMotive;
typedef struct {
  bool some;
  const STerm *value;
} MaybeSTerm;
typedef struct {
  Quantity quantity;
  Str name;
  MaybeSTerm ty;
} SField;
typedef enum { SBRANCH_LEG, SBRANCH_CTOR } SBranchKind;
typedef struct {
  SBranchKind kind;
  Big index;         /* LEG */
  Vec binders;       /* LEG: SBinder pointers */
  Str name;          /* CTOR */
  Vec fields;        /* CTOR: SField pointers */
  const STerm *body; /* both */
} SBranch;
typedef struct {
  Str name;
  const STerm *ty;
} SFamCtor;
typedef struct {
  Str name;
  Vec params; /* SBinder pointers */
  const STerm *ty;
  Vec ctors; /* SFamCtor pointers */
} SFam;
typedef struct {
  Str name;
  const STerm *ty;
  const STerm *body;
} SRecDef;
typedef struct {
  bool some;
  Quantity value;
} MaybeQuantity;
typedef enum {
  STERM_VAR,
  STERM_NAT,
  STERM_PROP,
  STERM_TYPE,
  STERM_SORT,
  STERM_PRIM,
  STERM_UNIT,
  STERM_AUTO,
  STERM_PAIR,
  STERM_TUPLE,
  STERM_SUM,
  STERM_PROD,
  STERM_PROJ,
  STERM_INJ,
  STERM_ABSURD,
  STERM_APP,
  STERM_FUN,
  STERM_ARROW,
  STERM_STAR,
  STERM_ZK_TY,
  STERM_PROVE,
  STERM_VERIFY,
  STERM_FHC_TY,
  STERM_ENC,
  STERM_EVAL,
  STERM_DEC,
  STERM_MPC_TY,
  STERM_SHARE,
  STERM_INPUT,
  STERM_JOIN,
  STERM_OPEN,
  STERM_LET,
  STERM_ANN,
  STERM_CASE,
  STERM_MATCH
} STermKind;
struct STerm {
  STermKind kind;
  Str name;               /* VAR, LET */
  Big nat;                /* NAT value, TYPE level */
  Big index;              /* PROJ, INJ */
  Big count;              /* INJ */
  const Universe *sort;   /* SORT level */
  SPrim prim;             /* PRIM */
  const STerm *left;      /* PAIR */
  const STerm *right;     /* PAIR */
  Vec items;              /* TUPLE, SUM, PROD; JOIN values (STerm pointers) */
  const STerm *head;      /* PROJ, APP */
  const STerm *arg;       /* APP */
  const STerm *value;     /* INJ, ABSURD, ENC, EVAL, DEC, SHARE, INPUT, OPEN, LET, ANN */
  Vec binders;            /* FUN (SBinder pointers) */
  SBinder binder;         /* ARROW, STAR, ZK_TY */
  const STerm *body;      /* FUN, ARROW, STAR, ZK_TY, LET */
  const STerm *key;       /* PROVE, VERIFY, ENC, DEC */
  const STerm *witness;   /* PROVE */
  const STerm *relation;  /* PROVE */
  const STerm *proof;     /* VERIFY, OPEN */
  const STerm *level;     /* FHC_TY */
  const STerm *ty;        /* FHC_TY, MPC_TY, LET, ANN */
  const STerm *function;  /* EVAL, JOIN */
  const STerm *parties;   /* MPC_TY, JOIN, OPEN */
  const STerm *access;    /* MPC_TY */
  const STerm *party;     /* INPUT */
  MaybeQuantity quantity; /* OPEN */
  const STerm *scrut;     /* CASE, MATCH */
  MaybeSMotive motive;    /* CASE, MATCH */
  Vec branches;           /* CASE, MATCH (SBranch pointers) */
};
/* levels: Universe pointers; names and helpers: PolyRename pointers (source, target). */
typedef struct {
  Str template;
  Vec levels;
  Str name;
  Vec names;
  MaybeVec helpers;
} SSpecialization;
typedef enum {
  SDECL_POLY,
  SDECL_POLY_MU,
  SDECL_POLY_GROUP,
  SDECL_POLY_COMPOSE,
  SDECL_SPECIALIZE,
  SDECL_DEF,
  SDECL_AXIOM,
  SDECL_MU,
  SDECL_REC
} SDeclKind;
/* The tag SDecl lets rust.h name the type without this header (lower_fn_decl). */
typedef struct SDecl {
  SDeclKind kind;
  Nat universes;                  /* POLY, POLY_MU, POLY_GROUP, POLY_COMPOSE */
  Str name;                       /* POLY, POLY_COMPOSE, DEF, AXIOM */
  const STerm *ty;                /* POLY, DEF, AXIOM */
  const STerm *body;              /* POLY, DEF */
  SFam family;                    /* POLY_MU, POLY_GROUP */
  Vec families;                   /* POLY_GROUP, MU (SFam pointers) */
  Vec helpers;                    /* POLY_GROUP, POLY_COMPOSE (SRecDef pointers) */
  Vec specializations;            /* POLY_COMPOSE (SSpecialization pointers) */
  SSpecialization specialization; /* SPECIALIZE */
  Vec definitions;                /* REC (SRecDef pointers) */
} SDecl;

/* Constructors. The boxing constructors return a static placeholder after setting heap->oom. */
const STerm *sterm_var(Heap *heap, Str name);
const STerm *sterm_nat(Heap *heap, Big value);
const STerm *sterm_prop(void);
const STerm *sterm_type(Heap *heap, Big level);
const STerm *sterm_sort(Heap *heap, const Universe *level);
const STerm *sterm_prim(Heap *heap, SPrim prim);
const STerm *sterm_unit(void);
const STerm *sterm_auto(void);
const STerm *sterm_pair(Heap *heap, const STerm *left, const STerm *right);
const STerm *sterm_tuple(Heap *heap, Vec items);
const STerm *sterm_sum(Heap *heap, Vec items);
const STerm *sterm_prod(Heap *heap, Vec items);
const STerm *sterm_proj(Heap *heap, const STerm *head, Big index);
const STerm *sterm_inj(Heap *heap, Big index, Big count, const STerm *value);
const STerm *sterm_absurd(Heap *heap, const STerm *value);
const STerm *sterm_app(Heap *heap, const STerm *head, const STerm *arg);
const STerm *sterm_fun(Heap *heap, Vec binders, const STerm *body);
const STerm *sterm_arrow(Heap *heap, SBinder binder, const STerm *body);
const STerm *sterm_star(Heap *heap, SBinder binder, const STerm *body);
const STerm *sterm_zk_ty(Heap *heap, SBinder binder, const STerm *body);
const STerm *sterm_prove(Heap *heap, const STerm *key, const STerm *witness, const STerm *relation);
const STerm *sterm_verify(Heap *heap, const STerm *key, const STerm *proof);
const STerm *sterm_fhc_ty(Heap *heap, const STerm *level, const STerm *ty);
const STerm *sterm_enc(Heap *heap, const STerm *key, const STerm *value);
const STerm *sterm_eval(Heap *heap, const STerm *function, const STerm *value);
const STerm *sterm_dec(Heap *heap, const STerm *key, const STerm *value);
const STerm *sterm_mpc_ty(Heap *heap, const STerm *parties, const STerm *access, const STerm *ty);
const STerm *sterm_share(Heap *heap, const STerm *value);
const STerm *sterm_input(Heap *heap, const STerm *party, const STerm *value);
const STerm *sterm_join(Heap *heap, const STerm *parties, const STerm *function, Vec values);
const STerm *sterm_open(Heap *heap, MaybeQuantity quantity, const STerm *parties, const STerm *proof,
                        const STerm *value);
const STerm *sterm_let(Heap *heap, Str name, const STerm *ty, const STerm *value, const STerm *body);
const STerm *sterm_ann(Heap *heap, const STerm *value, const STerm *ty);
const STerm *sterm_case(Heap *heap, const STerm *scrut, MaybeSMotive motive, Vec branches);
const STerm *sterm_match(Heap *heap, const STerm *scrut, MaybeSMotive motive, Vec branches);
SBinder sbinder(Quantity quantity, Str name, const STerm *ty);
const SBinder *sbinder_new(Heap *heap, SBinder binder);
SMotive smotive(Str self, MaybeStr ind, Vec indices, const STerm *body);
SField sfield(Quantity quantity, Str name, MaybeSTerm ty);
const SField *sfield_new(Heap *heap, SField field);
const SBranch *sbranch_leg_new(Heap *heap, Big index, Vec binders, const STerm *body);
const SBranch *sbranch_ctor_new(Heap *heap, Str name, Vec fields, const STerm *body);
const SFamCtor *sfam_ctor_new(Heap *heap, Str name, const STerm *ty);
SFam sfam(Str name, Vec params, const STerm *ty, Vec ctors);
const SFam *sfam_new(Heap *heap, SFam family);
const SRecDef *srec_def_new(Heap *heap, Str name, const STerm *ty, const STerm *body);
SSpecialization sspecialization(Str template, Vec levels, Str name, Vec names, MaybeVec helpers);
const SSpecialization *sspecialization_new(Heap *heap, SSpecialization specialization);
const SDecl *sdecl_poly_new(Heap *heap, Nat universes, Str name, const STerm *ty, const STerm *body);
const SDecl *sdecl_poly_mu_new(Heap *heap, Nat universes, SFam family);
const SDecl *sdecl_poly_group_new(Heap *heap, Nat universes, SFam family, Vec families, Vec helpers);
const SDecl *sdecl_poly_compose_new(Heap *heap, Nat universes, Str name, Vec specializations, Vec helpers);
const SDecl *sdecl_specialize_new(Heap *heap, SSpecialization specialization);
const SDecl *sdecl_def_new(Heap *heap, Str name, const STerm *ty, const STerm *body);
const SDecl *sdecl_axiom_new(Heap *heap, Str name, const STerm *ty);
const SDecl *sdecl_mu_new(Heap *heap, Vec families);
const SDecl *sdecl_rec_new(Heap *heap, Vec definitions);

/* Printers, in the order of syntax.bend. */
Str syntax_prim_name(SPrim prim);
Str syntax_mark(Quantity quantity);
Str syntax_binder_text(Heap *heap, SBinder binder);
Str syntax_binders_text(Heap *heap, const Vec *binders);
Str syntax_fields_text(Heap *heap, const Vec *fields);
Str syntax_names_text(Heap *heap, const Vec *names);
Str syntax_index_text(Heap *heap, MaybeStr ind, const Vec *indices);
Str syntax_motive_text(Heap *heap, MaybeSMotive motive);
Str syntax_branches_text(Heap *heap, const Vec *branches);
Str syntax_terms_text(Heap *heap, const Vec *terms, Str separator);
Str syntax_optional_mark(MaybeQuantity quantity);
Str syntax_raw(Heap *heap, const STerm *term);
Str syntax_show(Heap *heap, const STerm *term);
Str syntax_fam_ctor_text(Heap *heap, SFamCtor ctor);
Str syntax_fam_ctors_text(Heap *heap, const Vec *ctors);
Str syntax_fam_text(Heap *heap, Str word, SFam family);
Str syntax_families_text(Heap *heap, const Vec *families, Str word);
Str syntax_bindings_text(Heap *heap, const Vec *bindings);
Str syntax_reuse_text(Heap *heap, const Vec *bindings);
Str syntax_exports_text(Heap *heap, MaybeVec exports);
Str syntax_universe_names(Heap *heap, Nat count, Nat index);
Str syntax_universe_arguments(Heap *heap, const Vec *levels);
Str syntax_specialization_text(Heap *heap, SSpecialization specialization);
Str syntax_specializations_text(Heap *heap, const Vec *specializations);
Str syntax_rec_defs_text(Heap *heap, const Vec *definitions, Str word, Str next);
Str syntax_members_text(Heap *heap, const Vec *members);
Str syntax_decl_text(Heap *heap, const SDecl *decl);
Str syntax_print(Heap *heap, const Vec *decls);
/* level_of: the grammar level 0, 1 or 2 of a term (a Nat in Bend, always one of three constants). */
uint32_t syntax_level_of(const STerm *term);
Str syntax_precedence_at_level(Heap *heap, uint32_t level, const STerm *term);
Str syntax_precedence_binder_text(Heap *heap, SBinder binder);
Str syntax_precedence_binders_text(Heap *heap, const Vec *binders);
Str syntax_precedence_branch_binders(Heap *heap, const Vec *binders);
Str syntax_precedence_field_text(Heap *heap, SField field);
Str syntax_precedence_fields_text(Heap *heap, const Vec *fields);
Str syntax_precedence_ind_text(Heap *heap, MaybeStr ind, const Vec *indices);
Str syntax_precedence_motive_text(Heap *heap, SMotive motive);
Str syntax_precedence_optional_motive(Heap *heap, MaybeSMotive motive);
Str syntax_precedence_branch_text(Heap *heap, const SBranch *branch);
Str syntax_precedence_branches_text(Heap *heap, const Vec *branches);
Str syntax_precedence_terms_text(Heap *heap, const Vec *terms);
Str syntax_precedence_join_values(Heap *heap, const Vec *terms);
Str syntax_precedence_raw(Heap *heap, const STerm *term);
Str syntax_precedence_wrap(Heap *heap, bool enough, Str text);
/* at: the signed Bignum level keeps the OCaml int domain of the original Syntax.at. */
Str syntax_at(Heap *heap, Big level, const STerm *term);

/* ---- elab_term (bend2/surface/elab_term.bend) ---- */
typedef enum { ELAB_RFAM, ELAB_RCTOR } ElabMuRefKind;
/* MuRef: RFam{family} | RCtor{family, ctor} */
typedef struct {
  ElabMuRefKind kind;
  Family family; /* RFAM, RCTOR */
  Ctor ctor;     /* RCTOR */
} ElabMuRef;
typedef struct {
  bool some;
  ElabMuRef value;
} MaybeElabMuRef;
typedef struct {
  Quantity quantity;
  Str name;
  const Value *domain;
} ElabPoint;
typedef struct {
  bool some;
  ElabPoint value;
} MaybeElabPoint;
/* args: List<Syntax.T> as STerm pointers. */
typedef struct {
  const STerm *head;
  Vec args;
} ElabSpine;
typedef struct {
  Shape shape;
  Closure diagram;
  MaybeLevelVar level;
} ElabFormer;
typedef struct {
  Quantity quantity;
  Str name;
  const Term *ty;
  Ctx context;
} ElabBound;
/* Common.Pair<Positivity.Family, Positivity.Ctor> */
typedef struct {
  Family family;
  Ctor ctor;
} ElabFamilyCtor;
typedef struct {
  bool some;
  ElabFamilyCtor value;
} MaybeElabFamilyCtor;
/* The law elab: the Comp value is a const Term *; expected is Maybe<Value.T>; lists of Maybe<Value.T>
 * are Vecs of MaybeValue pointers. */
Comp elab_term_elab(Heap *heap, Ctx ctx, MaybeValue expected, const STerm *syntax);
/* injection_expectations: the Comp value is a Vec * of the requested leg types (MaybeValue pointers),
 * empty when the expected type is absent or not a collection of the same width. */
Comp elab_term_injection_expectations(Heap *heap, Ctx ctx, MaybeValue expected, Big count);
/* elab_fam_decl: the Comp value is a FamilyDecl *. elab_ctor_decls: a Vec * of CtorDecl pointers
 * (ctors holds SFamCtor pointers). elab_decl: a Decl *. */
Comp elab_term_elab_fam_decl(Heap *heap, SFam family, Ctx ctx);
Comp elab_term_elab_ctor_decls(Heap *heap, const Vec *ctors, Ctx ctx, Str name);
Comp elab_term_elab_decl(Heap *heap, const SDecl *decl, Ctx ctx);
/* Exports for elab.c: eval_in (the Comp value is a const Value *), elab_telescope (the Comp value is
 * an opaque TelescopeState *, read through telescope_context) and elab_ctor_decl (a CtorDecl *). */
Comp elab_term_eval_in(Heap *heap, Ctx ctx, const Term *term);
Comp elab_term_elab_telescope(Heap *heap, const Vec *binders, Ctx ctx, Vec telescope);
Ctx elab_term_telescope_context(const void *state);
Comp elab_term_elab_ctor_decl(Heap *heap, SFamCtor ctor, Ctx ctx, Str name);


/* ---- token (bend2/surface/token.bend) ---- */
typedef struct {
  Nat line;
  Nat col;
} TokenLoc;
/* KindT in declaration order; tag(k) is the position. */
typedef enum {
  TOKEN_LPAREN,
  TOKEN_RPAREN,
  TOKEN_COLON,
  TOKEN_COLON_EQ,
  TOKEN_ARROW,
  TOKEN_DARROW,
  TOKEN_STAR,
  TOKEN_COMMA,
  TOKEN_DOT,
  TOKEN_DOT1,
  TOKEN_DOT2,
  TOKEN_PIPE,
  TOKEN_UNIT_TOK,
  TOKEN_KDEF,
  TOKEN_KAXIOM,
  TOKEN_KFUN,
  TOKEN_KINJ,
  TOKEN_KOF,
  TOKEN_KCASE,
  TOKEN_KMATCH,
  TOKEN_KAS,
  TOKEN_KRETURN,
  TOKEN_KWITH,
  TOKEN_KTUPLE,
  TOKEN_KSUM,
  TOKEN_KPROD,
  TOKEN_KABSURD,
  TOKEN_KPROP,
  TOKEN_KTYPE,
  TOKEN_KSORT,
  TOKEN_KPOLY,
  TOKEN_KGROUP,
  TOKEN_KSPECIALIZE,
  TOKEN_KWHERE,
  TOKEN_KLET,
  TOKEN_KIN,
  TOKEN_KAUTO,
  TOKEN_KMU,
  TOKEN_KMUTUAL,
  TOKEN_KEND,
  TOKEN_KNU,
  TOKEN_KZK,
  TOKEN_KPROVE,
  TOKEN_KVERIFY,
  TOKEN_KFHC,
  TOKEN_KENC,
  TOKEN_KEVAL,
  TOKEN_KDEC,
  TOKEN_KMPC,
  TOKEN_KINPUT,
  TOKEN_KSHARE,
  TOKEN_KOPEN,
  TOKEN_LBRACKET,
  TOKEN_RBRACKET,
  TOKEN_KAND,
  TOKEN_KREC,
  TOKEN_KNAT_ADD,
  TOKEN_KNAT_SUB,
  TOKEN_KNAT_MUL,
  TOKEN_KNAT_EQ,
  TOKEN_KNAT_LT,
  TOKEN_IDENT,
  TOKEN_NAT_TOK,
  TOKEN_BYTES,
  TOKEN_EOF
} TokenKindTag;
typedef struct {
  TokenKindTag tag;
  Str name;   /* IDENT */
  Str digits; /* NAT_TOK */
  Vec bytes;  /* BYTES: uint32_t pointers, the literal bytes in source order */
} TokenKind;
/* T.Tok{kind, loc} */
typedef struct {
  TokenKind kind;
  TokenLoc loc;
} Token;
TokenKind token_kind(TokenKindTag tag);
TokenKind token_ident(Str name);
TokenKind token_nat_tok(Str digits);
TokenKind token_bytes(Vec bytes);
TokenLoc token_start(Heap *heap);
TokenLoc token_advance(Heap *heap, TokenLoc l, uint64_t n);
TokenLoc token_next_line(Heap *heap, TokenLoc l);
Str token_describe(Heap *heap, TokenKind k);
uint32_t token_tag(TokenKind k);

/* ---- lexer (bend2/surface/lexer.bend) ---- */
/* lex(source): the Done value is a Vec * of Token pointers (the Eof token last); Fail carries
 * Error.Parse with the message, line and column of the offending byte. */
Result lexer_lex(Heap *heap, Str source);
/* ident_kind(name): a keyword kind when name is a keyword, else Ident{name}. */
TokenKind lexer_ident_kind(Str name);

/* ---- parser (bend2/surface/parser.bend) ---- */
/* term(source): the Done value is a const STerm *. program(source): the Done value is a Vec * of SDecl
 * pointers. Fail carries the lexer or parser Error.Parse; a host allocation failure during the parse
 * surfaces as Error.HostOom. */
Result parser_term(Heap *heap, Str source);
Result parser_program(Heap *heap, Str source);

/* ---- poly (bend2/surface/poly.bend) ---- */
/* LevelAction: PreserveLevels | ScopeLevels{arity} | SubstituteLevels{levels}; levels is a Vec of
 * const LevelVar pointers. */
typedef enum { POLY_PRESERVE_LEVELS, POLY_SCOPE_LEVELS, POLY_SUBSTITUTE_LEVELS } PolyLevelActionKind;
typedef struct {
  PolyLevelActionKind kind;
  Nat arity;  /* POLY_SCOPE_LEVELS */
  Vec levels; /* POLY_SUBSTITUTE_LEVELS */
} PolyLevelAction;
/* Common.Pair<String, String>: one source -> target renaming. */
typedef struct {
  Str source;
  Str target;
} PolyRename;
/* Action{levels, names, forbidden}: names a Vec of PolyRename pointers, forbidden a Vec of Str pointers. */
typedef struct {
  PolyLevelAction levels;
  Vec names;
  Vec forbidden;
} PolyAction;
PolyLevelAction poly_preserve_levels(void);
PolyLevelAction poly_scope_levels(Nat arity);
PolyLevelAction poly_substitute_levels(Vec levels);
PolyAction poly_action(PolyLevelAction levels, Vec names, Vec forbidden);
/* map_term(action, term): a Comp whose Done value is the mapped const Term *. */
Comp poly_map_term(Heap *heap, PolyAction action, const Term *term);
/* map_levels(action, term): Budget.unlimited run of map_term with no renames; Done value const Term *. */
Result poly_map_levels(Heap *heap, PolyLevelAction action, const Term *term);
/* Scheme{arity, decl} and T = List<Common.Pair<String, Scheme>> as a persistent cons list (NULL = empty). */
typedef struct {
  Nat arity;
  Decl decl;
} PolyScheme;
typedef struct PolyRow PolyRow;
struct PolyRow {
  Str name;
  PolyScheme scheme;
  const PolyRow *rest;
};
typedef const PolyRow *PolyCatalog;
typedef struct {
  bool some;
  PolyScheme value;
} MaybePolyScheme;
PolyCatalog poly_empty(void);
MaybePolyScheme poly_lookup(PolyCatalog catalog, Str name);
bool poly_occupied(Globals globals, PolyCatalog catalog, Str name);
Error poly_collision(Heap *heap, Str name);
/* map_decl_action(decl, action) and map_decl(levels, decl): the Done value is a boxed Decl *. */
Comp poly_map_decl_action(Heap *heap, Decl decl, PolyAction action);
Result poly_map_decl(Heap *heap, PolyLevelAction levels, Decl decl);
MaybeNat poly_arity(PolyCatalog catalog, Str name);
/* declare_comp and declare: the Done value is the extended catalog (a PolyRow *, cast to PolyCatalog). */
Comp poly_declare_comp(Heap *heap, Globals globals, PolyCatalog catalog, Nat arity, Decl decl);
Result poly_declare(Heap *heap, Globals globals, PolyCatalog catalog, Budget budget, Nat arity, Decl decl);
/* Common.Pair<Global.T, Term.T> of instantiate: the Done value is a PolyInstance *. */
typedef struct {
  Globals globals;
  const Term *term;
} PolyInstance;
Comp poly_instantiate_comp(Heap *heap, Globals globals, PolyCatalog catalog, Str name, Vec levels, Str as_name);
Result poly_instantiate(Heap *heap, Globals globals, PolyCatalog catalog, Budget budget, Str name, Vec levels, Str as_name);
/* Exported for family_poly: member(names, name) over Str pointers, map_name (Done a boxed Str *),
 * map_level (Done the const LevelVar *), map_terms (Done a boxed Vec * of Term pointers), decl_name. */
bool poly_member(const Vec *names, Str name);
Result poly_map_name(Heap *heap, PolyAction action, Str name);
Result poly_map_level(Heap *heap, PolyAction action, const LevelVar *level);
Comp poly_map_terms(Heap *heap, const Vec *terms, PolyAction action);
Str poly_decl_name(Decl decl);

/* ---- family_poly (bend2/surface/family_poly.bend) ---- */
/* Lists: Telescope and ctors as in kernel.h (TeleBinder / CtorDecl pointers), List<Decl> as Decl
 * pointers, List<String> as Str pointers, Bindings = List<Common.Pair<String, String>> as PolyRename
 * pointers. */
/* RawFamily = Common.Pair<Check.FamilyDecl, List<Check.CtorDecl>> */
typedef struct {
  FamilyDecl family;
  Vec ctors;
} FamilyRaw;
/* Scheme{arity, family, ctors, companions, members, origin}: companions a Vec of FamilyRaw pointers. */
typedef struct {
  Nat arity;
  FamilyDecl family;
  Vec ctors;
  Vec companions;
  Vec members;
  Globals origin;
} FamilyScheme;
/* T = List<Common.Pair<String, Scheme>> as a persistent cons list (NULL = empty). */
typedef struct FamilyRow FamilyRow;
struct FamilyRow {
  Str name;
  FamilyScheme scheme;
  const FamilyRow *rest;
};
typedef const FamilyRow *FamilyCatalog;
typedef struct {
  bool some;
  FamilyScheme value;
} MaybeFamilyScheme;
/* Maybe<List<...>>: names as Str pointers, or bindings as PolyRename pointers. */
typedef struct {
  bool some;
  Vec value;
} FamilyMaybeVec;
/* Common.Pair<List<String>, List<String>> of instance_names: fresh family names, exported member names. */
typedef struct {
  Vec families;
  Vec members;
} FamilyInstanceNames;
typedef struct {
  bool some;
  FamilyInstanceNames value;
} MaybeFamilyInstanceNames;
/* Raw traversals: Done values are a boxed Vec * (telescope, ctors), CtorDecl *, FamilyRaw *, Decl *. */
Comp family_map_telescope(Heap *heap, const Vec *telescope, PolyAction action);
Comp family_map_ctor(Heap *heap, CtorDecl ctor, PolyAction action);
Comp family_map_ctors(Heap *heap, const Vec *ctors, PolyAction action);
Comp family_map_family(Heap *heap, FamilyDecl family, const Vec *ctors, PolyAction action);
Result family_member_kind(DeclKind kind);
Result family_member_body(Heap *heap, MaybeTermPtr body, Str name);
Comp family_map_member(Heap *heap, Decl decl, PolyAction action);
Comp family_unique_ctors(Heap *heap, const Vec *ctors, Str family, const Vec *seen);
/* The catalog. */
FamilyCatalog family_empty(void);
MaybeFamilyScheme family_lookup(FamilyCatalog catalog, Str name);
MaybeNat family_scheme_arity(MaybeFamilyScheme scheme);
MaybeNat family_arity(FamilyCatalog catalog, Str name);
Vec family_member_names(Heap *heap, const Vec *members);
Str family_family_name(FamilyDecl family);
Vec family_companion_names(Heap *heap, const Vec *companions);
Vec family_ctor_names(Heap *heap, const Vec *ctors);
Vec family_companion_ctors(Heap *heap, const Vec *companions);
FamilyMaybeVec family_scheme_members(Heap *heap, MaybeFamilyScheme scheme);
FamilyMaybeVec family_members(Heap *heap, FamilyCatalog catalog, Str name);
FamilyMaybeVec family_scheme_companions(Heap *heap, MaybeFamilyScheme scheme);
FamilyMaybeVec family_companions(Heap *heap, FamilyCatalog catalog, Str name);
FamilyMaybeVec family_scheme_constructors(Heap *heap, MaybeFamilyScheme scheme);
FamilyMaybeVec family_constructors(Heap *heap, FamilyCatalog catalog, Str name);
/* Instance renaming and export validation. */
Str family_rename_instance(Heap *heap, FamilyScheme scheme, Str name, Str as_name, Str value);
MaybeStr family_mapping_find(const Vec *mapping, Str name);
Str family_mapped_name(MaybeStr value, Str fallback);
Str family_rename_export(Heap *heap, FamilyScheme scheme, Str name, Str as_name, const Vec *exports, Str value);
Vec family_planned_families(Heap *heap, const Vec *families, FamilyScheme scheme, Str name, Str as_name, const Vec *reuse);
Error family_collision(Heap *heap, Str name);
Result family_validate_export_binding(Heap *heap, Str source, Str target, const Vec *names, const Vec *sources,
                                      const Vec *reserved, const Vec *targets);
Comp family_export_loop(Heap *heap, const Vec *bindings, const Vec *names, const Vec *reserved, const Vec *sources,
                        const Vec *targets);
/* check_exports: the Done value is a boxed Vec * of PolyRename pointers. */
Comp family_check_exports(Heap *heap, FamilyScheme scheme, Str name, Str as_name, const Vec *reuse, FamilyMaybeVec exports);
Vec family_fresh_family_names(Heap *heap, const Vec *families, FamilyScheme scheme, Str name, Str as_name, const Vec *reuse,
                              const Vec *exports);
Vec family_exported_member_names(Heap *heap, const Vec *names, FamilyScheme scheme, Str name, Str as_name, const Vec *exports);
MaybeFamilyInstanceNames family_instance_names_result(Heap *heap, Result result, FamilyScheme scheme, Str name, Str as_name,
                                                      const Vec *reuse);
MaybeFamilyInstanceNames family_instance_names_scheme(Heap *heap, MaybeFamilyScheme scheme, Str name, Str as_name,
                                                      const Vec *reuse, FamilyMaybeVec exports);
MaybeFamilyInstanceNames family_instance_names(Heap *heap, FamilyCatalog catalog, Str name, Str as_name, const Vec *reuse,
                                               FamilyMaybeVec exports);
/* ---- chunk 2 (Bend 289-600): symbolic declaration of schema groups ---- */
/* Imported{reused, family, ctors}; lists of Imported as FamilyImported pointers. */
typedef struct {
  bool reused;
  FamilyDecl family;
  Vec ctors;
} FamilyImported;
/* Dependency{source, levels, alias, reuse, exports}: levels LevelVar pointers, reuse and exports PolyRename pointers. */
typedef struct {
  Str source;
  Vec levels;
  Str alias;
  Vec reuse;
  FamilyMaybeVec exports;
} FamilyDependency;
/* MemberElaborator = Global.T -> Budget.Comp(Check.Decl): the Done value is a Decl *. Lists as pointers. */
typedef struct {
  Comp (*fn)(Heap *heap, void *ctx, Globals globals);
  void *ctx;
} FamilyMemberElaborator;
/* GroupState (host only, no Bend counterpart): the state of declare_nonempty between two members of a
 * `poly ... group ... end` entry, so the compact elaboration path can reset its arena per member (USER
 * ruling 2026-10-12 (b)). globals = the scheme origin, symbolic = the table the members elaborate in,
 * members = the checked Decl pointers so far in declaration order (trusted first). */
typedef struct {
  Globals globals;
  Globals symbolic;
  FamilyCatalog catalog;
  Nat arity;
  FamilyRaw raw;
  Vec companions;
  Vec members;
} FamilyGroupState;
/* ComposeState (host only, no Bend counterpart): the state of a `poly ... group X where specialize ...`
 * entry (SDECL_POLY_COMPOSE) between two members, so the compact elaboration path can reset its arena
 * per member (USER ruling 2026-10-12 (b)). group = the declare_nonempty state of finish_composition after
 * the trusted members (raw = the head of the fresh imports), name = the composition root, pending = the
 * Decl pointers the specializations contributed (Composed.members), elaborated in order before the
 * supplied definitions; every member callback is guarded against the root name. */
typedef struct {
  FamilyGroupState group;
  Str name;
  Vec pending;
} FamilyComposeState;
/* Comp Done values: Globals * (check_members, add_members, check_reuse, install_symbolic*), Vec * of Decl *
 * (map_members, scope_members, elaborate_members), Entry * (trusted_entry), Family * (family_option),
 * FamilyScheme * (require_schema), kernel_unit() (require_fresh, reserve_members, family_members,
 * validate_exports_comp) and the new FamilyRow * for every catalog-valued declaration. */
Vec family_catalog_names(Heap *heap, FamilyCatalog catalog);
bool family_has_ctor(const FamilyRows *families, Str name);
bool family_ctor_declared(Globals globals, Str name);
bool family_occupied(Globals globals, FamilyCatalog catalog, Str name);
Comp family_require_fresh(Heap *heap, bool used, Str name);
Comp family_map_members(Heap *heap, const Vec *members, PolyAction action);
Comp family_check_members(Heap *heap, const Vec *members, FamilyCatalog catalog, Nat arity, Globals globals);
Comp family_trusted_entry(Heap *heap, Decl decl);
Comp family_add_members(Heap *heap, const Vec *members, FamilyCatalog catalog, Globals globals);
Comp family_scope_members(Heap *heap, const Vec *members, FamilyCatalog catalog, Nat arity);
Comp family_reserve_members(Heap *heap, const Vec *members, FamilyCatalog catalog);
Comp family_family_members(Heap *heap, const Vec *members, Globals globals, FamilyCatalog catalog, Nat arity,
                           FamilyDecl family, Vec ctors);
Comp family_declared_family(Heap *heap, FamilyRaw raw, Globals globals, FamilyCatalog catalog, Nat arity,
                            const Vec *members, PolyAction action);
Comp family_declare_comp(Heap *heap, Globals globals, FamilyCatalog catalog, Nat arity, FamilyDecl family, Vec ctors,
                         const Vec *members);
Result family_declare(Heap *heap, Globals globals, FamilyCatalog catalog, Budget budget, Nat arity, FamilyDecl family,
                      Vec ctors, const Vec *members);
Comp family_family_option(Heap *heap, MaybeFamily value, Error error);
Globals family_drop_family(Heap *heap, Globals globals, Str name);
Family family_replace_level(Family family, const LevelVar *level);
const LevelVar *family_kernel_level(Family family);
Vec family_erase_rec(Heap *heap, const Vec *ctors);
Family family_erase_self_rec(Heap *heap, Family family);
Comp family_reuse_agrees(Heap *heap, bool level_ok, Family actual, Family expected, Str name, Globals globals);
Comp family_check_reuse(Heap *heap, Globals globals, Nat arity, FamilyDecl family, Vec ctors);
Result family_check_binding(Heap *heap, Str local, Str existing, const Vec *bound, const Vec *families,
                            const Vec *available, Globals globals);
Comp family_check_bindings(Heap *heap, const Vec *reuse, const Vec *families, const Vec *available, Globals globals,
                           const Vec *bound);
Vec family_raw_families(Heap *heap, FamilyScheme scheme);
Vec family_schema_members(FamilyScheme scheme);
Nat family_schema_arity(FamilyScheme scheme);
Globals family_schema_table(FamilyScheme scheme);
Comp family_require_schema(Heap *heap, MaybeFamilyScheme scheme, Str name);
Comp family_validate_exports_comp(Heap *heap, Globals globals, const Vec *available, FamilyCatalog catalog, Str name,
                                  Str as_name, const Vec *reuse, const Vec *exports);
Result family_validate_exports(Heap *heap, Globals globals, const Vec *available, FamilyCatalog catalog, Budget budget,
                               Str name, Str as_name, const Vec *reuse, const Vec *exports);
Vec family_fresh_imports(Heap *heap, const Vec *imports);
Comp family_install_symbolic_import(Heap *heap, FamilyImported imported, Globals globals, FamilyCatalog catalog, Nat arity);
Comp family_install_symbolic(Heap *heap, const Vec *imports, Globals globals, FamilyCatalog catalog, Nat arity);
Comp family_elaborate_members(Heap *heap, const Vec *members, Globals globals, FamilyCatalog catalog, Nat arity,
                              const Vec *reversed);
Comp family_declare_nonempty(Heap *heap, const Vec *families, const Vec *imports, const Vec *trusted, const Vec *members,
                             Globals globals, FamilyCatalog catalog, Nat arity);
Comp family_declare_group_with_reuse(Heap *heap, Globals globals, FamilyCatalog catalog, Nat arity, const Vec *imports,
                                     const Vec *trusted, const Vec *members);
Vec family_fresh_raw(Heap *heap, const Vec *families);
Vec family_raw_callbacks(Heap *heap, const Vec *members);
Comp family_declare_group_elaborated_comp(Heap *heap, Globals globals, FamilyCatalog catalog, Nat arity,
                                          const Vec *families, const Vec *members);
Result family_declare_group_elaborated(Heap *heap, Globals globals, FamilyCatalog catalog, Budget budget, Nat arity,
                                       const Vec *families, const Vec *members);
Result family_declare_group(Heap *heap, Globals globals, FamilyCatalog catalog, Budget budget, Nat arity,
                            const Vec *families, const Vec *members);
/* declare_group_elaborated_comp split at member granularity (host only): open runs install_symbolic,
 * scope_members and add_members (Done value a FamilyGroupState *), member runs one elaborate_members step
 * (Done value the next FamilyGroupState *), close builds the row (Done value the new FamilyRow *). */
Comp family_group_open(Heap *heap, Globals globals, FamilyCatalog catalog, Nat arity, const Vec *families);
Comp family_group_member(Heap *heap, const FamilyGroupState *group, FamilyMemberElaborator elaborator);
Comp family_group_close(Heap *heap, const FamilyGroupState *group);
/* compose_comp split at specialization and member granularity (host only): begin runs tick and require_fresh
 * (Done value a FamilyComposeStart *, declared with FamilyComposed below), each specialize line then runs
 * family_compose_dependency from the Composed value so far, open runs the composed_group guards and the
 * declare_nonempty prefix over the trusted members (Done value a FamilyComposeState *), member runs one
 * guarded elaborate_members step (Done value the next FamilyComposeState *), close builds the group row and
 * the composition schema row (Done value the new FamilyRow *). family_raw_elaborator is the raw_callbacks
 * closure over one Decl *. family_compose_begin and family_compose_open are declared after FamilyComposed. */
Comp family_compose_member(Heap *heap, const FamilyComposeState *state, FamilyMemberElaborator elaborator);
Comp family_compose_close(Heap *heap, const FamilyComposeState *state);
FamilyMemberElaborator family_raw_elaborator(void *member);

/* ---- chunk 3 (Bend 603-819): instance renaming, composition and closed instantiation ---- */
/* Composed{aliases, available, imports, members, trusted}: Str, Str, FamilyImported, Decl and Decl pointers. */
typedef struct {
  Vec aliases;
  Vec available;
  Vec imports;
  Vec members;
  Vec trusted;
} FamilyComposed;
/* ComposeStart (host only, decision 173): the state of compose_comp between two specialize lines of one
 * entry: the lowered dependencies (FamilyDependency pointers) and the Composed fold value so far. */
typedef struct {
  Vec dependencies;
  FamilyComposed composed;
} FamilyComposeStart;
Comp family_compose_begin(Heap *heap, Globals globals, FamilyCatalog catalog, Nat arity, Str name,
                          const Vec *dependencies);
Comp family_compose_open(Heap *heap, Globals globals, FamilyCatalog catalog, Nat arity, Str name,
                         const FamilyComposed *composed);
/* Comp Done values: Vec * of FamilyImported * (map_imports), FamilyComposed * (compose_dependency,
 * compose_dependencies), Decl * (check_root_member), Globals * (install_import, install_imports,
 * instantiate_comp), the new FamilyRow * (composition_schema, finish_composition, composed_group, compose_comp)
 * and kernel_unit() (check_schema, check_levels, collision_list, guard_trusted). Lists of Dependency are
 * FamilyDependency pointers. */
Vec family_rename_bindings(Heap *heap, const Vec *names, FamilyScheme scheme, Str source, Str alias, const Vec *reuse,
                           const Vec *exports);
PolyAction family_instance_action(Heap *heap, FamilyScheme scheme, Str source, Str alias, Vec levels, const Vec *reuse,
                                  const Vec *exports);
FamilyImported family_mapped_import(FamilyRaw raw, bool reused);
Comp family_map_imports(Heap *heap, const Vec *families, PolyAction action, const Vec *reuse);
Comp family_check_schema(Heap *heap, FamilyScheme scheme);
bool family_scopes(Nat arity, const Vec *levels);
Error family_level_count_error(Heap *heap, Str source, Nat want, Nat got);
Comp family_check_levels(Heap *heap, const Vec *levels, Nat want, Str source, Nat arity, bool closed);
Comp family_collision_list(Heap *heap, const Vec *names, const Vec *occupied);
Comp family_compose_dependency(Heap *heap, FamilyDependency dependency, FamilyComposed state, Globals globals,
                               FamilyCatalog catalog, Nat arity, Str root);
Comp family_compose_dependencies(Heap *heap, const Vec *dependencies, FamilyComposed state, Globals globals,
                                 FamilyCatalog catalog, Nat arity, Str root);
Comp family_check_root_member(Heap *heap, Decl decl, Str root);
FamilyMemberElaborator family_guarded_callback(Heap *heap, FamilyMemberElaborator elaborate, Str root);
Comp family_guard_trusted(Heap *heap, const Vec *members, Str root);
Vec family_guard_callbacks(Heap *heap, const Vec *members, Str root);
Comp family_composition_schema(Heap *heap, MaybeFamilyScheme scheme, Str name, FamilyCatalog catalog);
Comp family_finish_composition(Heap *heap, const Vec *fresh, const Vec *imports, const Vec *trusted, const Vec *members,
                               Globals globals, FamilyCatalog catalog, Nat arity, Str name);
Vec family_import_families(Heap *heap, const Vec *imports);
Comp family_composed_group(Heap *heap, FamilyComposed state, const Vec *supplied, Globals globals, FamilyCatalog catalog,
                           Nat arity, Str root);
Comp family_compose_comp(Heap *heap, Globals globals, FamilyCatalog catalog, Nat arity, Str name, const Vec *dependencies,
                         const Vec *members);
Result family_compose(Heap *heap, Globals globals, FamilyCatalog catalog, Budget budget, Nat arity, Str name,
                      const Vec *dependencies, const Vec *members);
const LevelVar *family_normalize_from(Heap *heap, size_t remaining, const LevelVar *candidate, const LevelVar *level);
const LevelVar *family_normal_level(Heap *heap, const LevelVar *level);
Vec family_normal_levels(Heap *heap, const Vec *levels);
Comp family_install_import(Heap *heap, FamilyImported imported, Globals globals, FamilyCatalog catalog);
Comp family_install_imports(Heap *heap, const Vec *imports, Globals globals, FamilyCatalog catalog);
Comp family_instantiate_comp(Heap *heap, Globals globals, FamilyCatalog catalog, Str name, const Vec *levels, Str as_name,
                             const Vec *reuse, FamilyMaybeVec exports);
Result family_instantiate(Heap *heap, Globals globals, FamilyCatalog catalog, Budget budget, Str name, const Vec *levels,
                          Str as_name, const Vec *reuse, FamilyMaybeVec exports);
/* InstantiateStart (host only, no Bend counterpart): the state of instantiate after install_imports, so the
 * compact elaboration path can reset its arena per check_members item (USER ruling 2026-10-12 (e)).
 * installed = the table after the closed imports, members = the schema members still to check, in order. */
typedef struct {
  Globals installed;
  Vec members;
} FamilyInstantiateStart;
/* instantiate open split (host only, D-264a): head = instantiate up to map_members (k0..k5 and map_imports;
 * Done = FamilyInstantiateHead *: the table, the catalog, the instance action, the mapped imports and the
 * schema members still to map), one family_map_member per schema member with head.action, in order, tail =
 * install_imports over head.imports (Done = FamilyInstantiateStart * with the mapped members). */
typedef struct {
  Globals globals;
  FamilyCatalog catalog;
  PolyAction action;
  Vec imports;
  Vec members;
} FamilyInstantiateHead;
Comp family_instantiate_open_head(Heap *heap, Globals globals, FamilyCatalog catalog, Str name, const Vec *levels,
                                  Str as_name, const Vec *reuse, FamilyMaybeVec exports);
Comp family_instantiate_open_tail(Heap *heap, const FamilyInstantiateHead *head, const Vec *mapped);
/* One check_members item (tick, require_fresh, kernel check, global_add); Done = Globals *. */
Comp family_check_member(Heap *heap, Decl decl, FamilyCatalog catalog, Nat arity, Globals globals);
/* compose_dependency split (host only, no Bend counterpart; D-256a, D-267a): head = k0..k6 up to map_members,
 * Done = FamilyDependencyHead * (frame = the opaque compose_dependency frame, in the head's arena; members and
 * action = the map_members input); one family_map_member per member with head.action, in order; mapped =
 * k7 over the mapped Decl * list and the renamed test, Done = FamilyDependencyHead * with renamed set;
 * tail = the k9 body, Done = FamilyComposed *. check_schema_open = check_schema up to scope_members,
 * Done = FamilySchemaOpen *; the check_members fold is then one family_check_member per member. */
typedef struct {
  void *frame;
  bool renamed;
  FamilyScheme scheme;
  Vec members;
  PolyAction action;
} FamilyDependencyHead;
typedef struct {
  Globals symbolic;
  Vec members;
  Nat arity;
} FamilySchemaOpen;
Comp family_compose_dependency_head(Heap *heap, FamilyDependency dependency, FamilyComposed state, Globals globals,
                                    FamilyCatalog catalog, Nat arity, Str root);
Comp family_compose_dependency_mapped(Heap *heap, const FamilyDependencyHead *head, const Vec *mapped);
Comp family_compose_dependency_tail(Heap *heap, const FamilyDependencyHead *head);
Comp family_check_schema_open(Heap *heap, FamilyScheme scheme);
/* composition open split (host only, D-259a): head = family_compose_open up to install_symbolic (Done =
 * FamilyComposeState * with the trusted members not yet scoped), one family_map_member per trusted member
 * with one family_scope_action, tail = add_members over the scoped members (Done = FamilyComposeState *). */
Comp family_compose_open_head(Heap *heap, Globals globals, FamilyCatalog catalog, Nat arity, Str name,
                              const FamilyComposed *composed);
PolyAction family_scope_action(Heap *heap, FamilyCatalog catalog, Nat arity);
Comp family_compose_open_tail(Heap *heap, const FamilyComposeState *head, const Vec *scoped);

/* ---- elab (bend2/surface/elab.bend): the program pass ---- */
/* Rows = List<Common.Pair<String, Global.Entry>> is a Vec of CheckRow pointers (check.c). */
/* Checked = Common.Pair<Global.T, Rows> */
typedef struct {
  Globals globals;
  Vec rows;
} ElabChecked;
/* RecTyped{name, ty, syntax} */
typedef struct {
  Str name;
  const Term *ty;
  const STerm *syntax;
} ElabRecTyped;
/* RecBody{name, ty, expected, body} */
typedef struct {
  Str name;
  const Term *ty;
  const Value *expected;
  const Term *body;
} ElabRecBody;
/* Result values: const Term * (elab), Decl * (elab_decl), FamilyDecl * (elab_fam_decl), CtorDecl *
 * (elab_ctor_decl), Globals * (elab_mu_group), ElabChecked * (elab_rec_group). */
Result elab_elab(Heap *heap, Ctx ctx, MaybeValue expected, const STerm *syntax);
Result elab_elab_decl(Heap *heap, const SDecl *decl, Ctx ctx);
Result elab_elab_fam_decl(Heap *heap, SFam family, Ctx ctx);
Result elab_elab_ctor_decl(Heap *heap, SFamCtor ctor, Ctx ctx, Str name);
Ctx elab_context(Heap *heap, Globals globals, Nat arity);
/* Name lists: Vecs of Str pointers into the syntax records. families: SFam pointers; definitions:
 * SRecDef pointers; ctors: SFamCtor pointers. */
Vec elab_family_names(Heap *heap, const Vec *families);
Vec elab_rec_names(Heap *heap, const Vec *definitions);
Vec elab_syntax_ctor_names(Heap *heap, const Vec *ctors);
Vec elab_all_ctor_names(Heap *heap, const Vec *families);
/* Comp values: Globals * */
Comp elab_declare_headers(Heap *heap, const Vec *families, Globals globals);
Comp elab_define_families(Heap *heap, const Vec *families, Globals globals, Vec group);
Comp elab_elab_mu_group_comp(Heap *heap, Globals globals, const Vec *families);
Result elab_elab_mu_group(Heap *heap, Globals globals, Budget budget, const Vec *families);
Globals elab_add_rows(Heap *heap, const Vec *rows, Globals globals);
/* Comp values: Vec * of ElabRecTyped pointers (rec_types), of ElabRecBody pointers (rec_bodies), of
 * CheckRow pointers (guarded_bodies, check_rec_bodies); ElabChecked * (elab_rec_group_comp). Pure
 * lists: Decl pointers (rec_postulates, rec_declarations), Pair pointers {Str *, const Term *}
 * (rec_pairs). */
Comp elab_rec_types(Heap *heap, const Vec *definitions, Globals globals);
Vec elab_rec_postulates(Heap *heap, const Vec *definitions);
Comp elab_rec_bodies(Heap *heap, const Vec *definitions, Globals globals);
Vec elab_rec_pairs(Heap *heap, const Vec *bodies);
Vec elab_rec_declarations(Heap *heap, const Vec *bodies);
MaybeNat elab_rec_arg(Heap *heap, OrderCert cert, const Term *body);
Comp elab_guarded_bodies(Heap *heap, const Vec *bodies, Globals globals, OrderCert cert);
Comp elab_check_rec_bodies(Heap *heap, MaybeOrderCert cert, const Vec *bodies, Globals globals, Globals provisional);
Comp elab_elab_rec_group_comp(Heap *heap, Globals globals, const Vec *definitions);
Result elab_elab_rec_group(Heap *heap, Globals globals, Budget budget, const Vec *definitions);


/* ---- elab chunk 2 (Bend 195-379): family templates, schema groups and compositions ---- */
typedef struct {
  FamilyDecl family;
  Vec ctors; /* CtorDecl pointers */
  Globals provisional;
} ElabFamilyParts;
Comp elab_ctor_distinct(Heap *heap, const Vec *ctors, Str family);
Comp elab_elab_family_parts(Heap *heap, SFam syntax, Globals globals, Nat arity);
Comp elab_declare_template(Heap *heap, const ElabFamilyParts *parts, Globals globals, FamilyCatalog catalog, Nat arity);
Comp elab_elab_family_template(Heap *heap, Globals globals, FamilyCatalog catalog, Nat arity, SFam family);
Comp elab_template_reservations(Heap *heap, const Vec *names, Globals globals, PolyCatalog definitions,
                                FamilyCatalog catalog);
FamilyMemberElaborator elab_member_callback(Heap *heap, const SRecDef *member, Nat arity);
Vec elab_member_callbacks(Heap *heap, const Vec *members, Nat arity);
Comp elab_symbolic_families(Heap *heap, const Vec *families, Globals globals, Nat arity);
Comp elab_elab_family_group(Heap *heap, Globals globals, FamilyCatalog catalog, PolyCatalog definitions, Nat arity,
                            const Vec *group, const Vec *members);
Comp elab_lower_levels(Heap *heap, const Vec *levels);
Comp elab_lower_dependencies(Heap *heap, const Vec *dependencies);
Vec elab_planned_names(Heap *heap, MaybeFamilyInstanceNames value);
Vec elab_planned_families(Heap *heap, MaybeFamilyInstanceNames value);
Comp elab_validate_dependency_exports(Heap *heap, FamilyMaybeVec exports, Globals globals, const Vec *available,
                                      FamilyCatalog catalog, Str source, Str alias, const Vec *reuse);
Comp elab_validate_dependencies(Heap *heap, const Vec *dependencies, Globals globals, FamilyCatalog catalog,
                                Vec available);
Vec elab_imported_names(Heap *heap, const Vec *dependencies, FamilyCatalog catalog);
Vec elab_option_names(Heap *heap, FamilyMaybeVec value);
Vec elab_imported_labels(Heap *heap, const Vec *dependencies, FamilyCatalog catalog);
Comp elab_label_reservations(Heap *heap, const Vec *labels, const Vec *names, Str root, Globals globals,
                             PolyCatalog definitions, FamilyCatalog catalog);
Comp elab_elab_composition(Heap *heap, Globals globals, FamilyCatalog catalog, PolyCatalog definitions, Nat arity,
                           Str name, const Vec *dependencies, const Vec *members);


/* ---- elab chunk 3 (Bend 380-608): programs, declaration steps and the text entry points ---- */
typedef struct {
  Globals globals;
  PolyCatalog definitions;
  FamilyCatalog families;
  Vec rows; /* CheckRow pointers in declaration order (Bend keeps them reversed) */
} ElabProgram;
Vec elab_declared_names(Heap *heap, const SDecl *decl);
Vec elab_ctor_reserved(Heap *heap, const SDecl *decl);
Comp elab_reserve_catalogs(Heap *heap, const Vec *names, PolyCatalog definitions, FamilyCatalog families);
Comp elab_reserve_ctors(Heap *heap, const Vec *names, Globals globals);
Comp elab_global_entry(Heap *heap, MaybeEntry value, Str message);
Comp elab_instance_members(Heap *heap, const Vec *names, Globals globals);
Vec elab_kernel_ctor_names(Heap *heap, Family family);
Comp elab_instance_labels(Heap *heap, const Vec *names, Globals globals);
Comp elab_specialize_family_names(Heap *heap, MaybeFamilyInstanceNames names, Globals installed, Globals globals,
                                  PolyCatalog definitions, FamilyCatalog families, Vec rows, Str source,
                                  Str alias);
Comp elab_definition_clauses(Heap *heap, const Vec *reuse, FamilyMaybeVec exports, bool known);
Comp elab_specialize_definition(Heap *heap, PolyInstance result, PolyCatalog definitions, FamilyCatalog families,
                                Vec rows, Str alias);
Comp elab_specialize_kind(Heap *heap, bool is_family, Globals globals, PolyCatalog definitions,
                          FamilyCatalog families, Vec rows, Str source, const Vec *levels, Str alias,
                          const Vec *reuse, FamilyMaybeVec exports);
ElabProgram elab_append_checked(Heap *heap, ElabChecked result, PolyCatalog definitions, FamilyCatalog families,
                               Vec rows);
Comp elab_append_one(Heap *heap, const Vec *checked, Globals globals, PolyCatalog definitions,
                     FamilyCatalog families, Vec rows);
Comp elab_elab_declaration(Heap *heap, const SDecl *decl, Globals globals, PolyCatalog definitions,
                           FamilyCatalog families, Vec rows);
Comp elab_program_step(Heap *heap, ElabProgram state, const SDecl *decl);
Comp elab_program_loop(Heap *heap, const Vec *decls, ElabProgram state);
ElabChecked elab_finish_program(ElabProgram state);
Comp elab_elab_program_comp(Heap *heap, Globals globals, const Vec *decls);
Result elab_elab_program_in(Heap *heap, Globals globals, Budget budget, const Vec *decls);
Result elab_result_rows(Result result);
Result elab_elab_program(Heap *heap, Globals globals, Budget budget, const Vec *decls);
Result elab_check_in(Heap *heap, Globals globals, Budget budget, Str source);
Result elab_check_in_compact(Heap *heap, Globals globals, Str source);
Result elab_elab_program_in_compact(Heap *heap, Globals globals, const Vec *decls);
Result elab_check_text(Heap *heap, Globals globals, Budget budget, Str source);
Result elab_check_text_compact(Heap *heap, Globals globals, Str source);


/* ---- elab chunk 4 (Bend 609-779): checked form, axiom names and disclosures ---- */
Str elab_entry_text(Heap *heap, const CheckRow *row);
Str elab_checked_form(Heap *heap, const Vec *rows);
Vec elab_axiom_names(Heap *heap, const Vec *rows);
MaybeStr elab_disclosure_of_shape(Shape shape);
Vec elab_prepend_disclosure(Heap *heap, MaybeStr value, const Vec *tail);
Vec elab_term_list_disclosures(Heap *heap, const Vec *terms);
Vec elab_shape_disclosures(Heap *heap, Shape shape);
Vec elab_addr_disclosures(Heap *heap, Addr addr);
Vec elab_leg_disclosures(Heap *heap, const Leg *leg);
Vec elab_legs_disclosures(Heap *heap, const Vec *legs);
Vec elab_motive_disclosures(Heap *heap, MaybeMotive motive);
Vec elab_branches_disclosures(Heap *heap, const Vec *branches);
Vec elab_elim_disclosures(Heap *heap, const ElimData *elim);
Vec elab_term_disclosures(Heap *heap, const Term *term);
Vec elab_entry_disclosures(Heap *heap, Entry entry);
Vec elab_row_disclosures(Heap *heap, const Vec *rows);
Vec elab_unique_disclosures(Heap *heap, const Vec *names, const Vec *seen);
Vec elab_disclosure_names(Heap *heap, const Vec *rows);


/* ---- compact (host only, no Bend counterpart: c/surface/compact.c) ----
 * Deep copies into heap->arena of the surface catalogs that survive a declaration (decision 156). */
Vec compact_ctor_decls(Heap *heap, const Vec *ctors);
FamilyDecl compact_family_decl(Heap *heap, FamilyDecl family);
FamilyScheme compact_family_scheme(Heap *heap, FamilyScheme scheme);
PolyScheme compact_poly_scheme(Heap *heap, PolyScheme scheme);
PolyCatalog compact_poly_catalog(Heap *heap, PolyCatalog catalog);
FamilyCatalog compact_family_catalog(Heap *heap, FamilyCatalog catalog);
ElabProgram compact_program(Heap *heap, const ElabProgram *program);
FamilyGroupState compact_family_group_state(Heap *heap, const FamilyGroupState *group);
FamilyComposeState compact_family_compose_state(Heap *heap, const FamilyComposeState *state);
FamilyComposeStart compact_family_compose_start(Heap *heap, const FamilyComposeStart *start);
/* The instantiate survivor: the installed table and the members from index `from` on (the checked ones drop). */
FamilyInstantiateStart compact_family_instantiate_start(Heap *heap, const FamilyInstantiateStart *start, size_t from);

#endif
