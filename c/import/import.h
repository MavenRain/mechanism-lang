/* Mechanism import component: C port of the bend2/import .bend files, one .c per .bend file and one
 * header for the component. Every function that allocates takes the Heap it allocates from and never
 * aborts: a failed allocation sets heap->oom and returns a well formed placeholder; the program
 * boundary checks heap->oom before it prints any result. */
#ifndef MECH_IMPORT_H
#define MECH_IMPORT_H

#include "kernel.h"

/* ---- ndjson (bend2/import/ndjson.bend): JSON values, the parser and the renderer ---- */

/* T: Null, Boolean, Number (its decimal text), Text, Items, Object. */
typedef enum { JSON_NULL, JSON_BOOLEAN, JSON_NUMBER, JSON_TEXT, JSON_ITEMS, JSON_OBJECT } JsonKind;
typedef struct Json Json;
/* Field<T> */
typedef struct {
  Str key;
  Json *value;
} JsonField;
struct Json {
  JsonKind kind;
  bool boolean; /* JSON_BOOLEAN */
  Str text;     /* JSON_NUMBER: the number text as written; JSON_TEXT: the decoded UTF-8 text */
  Vec values;   /* JSON_ITEMS: Json*, in source order */
  Vec fields;   /* JSON_OBJECT: JsonField*, in source order */
};

/* Result<String, A>: JSON_FAIL carries the message. */
typedef enum { JSON_DONE, JSON_FAIL } JsonResultKind;
/* Result<String, T> */
typedef struct {
  JsonResultKind kind;
  Json *value;
  Str message;
} JsonResult;
/* Result<String, String> */
typedef struct {
  JsonResultKind kind;
  Str value;
  Str message;
} JsonTextResult;
/* Result<String, Bool> */
typedef struct {
  JsonResultKind kind;
  bool value;
  Str message;
} JsonBoolResult;
/* Result<String, List<T>> (Json*), Result<String, List<Field<T>>> (JsonField*) or
 * Result<String, List<String>> (Str*), as the function comment says. */
typedef struct {
  JsonResultKind kind;
  Vec value;
  Str message;
} JsonListResult;

/* Constructors (Null{}, Boolean{}, Number{}, Text{}, Items{}, Object{}, Field{}). */
Json *json_null(Heap *heap);
Json *json_boolean(Heap *heap, bool value);
Json *json_number(Heap *heap, Str value);
Json *json_text(Heap *heap, Str value);
Json *json_items(Heap *heap, Vec values);
Json *json_object(Heap *heap, Vec fields);
JsonField *json_field_new(Heap *heap, Str key, Json *value);

/* byte_width: the UTF-8 width of the codepoint. */
uint32_t json_byte_width(uint32_t code);
/* key_exists */
bool json_key_exists(const Vec *fields, Str key);
/* parse: the whole source is one JSON value (512 nesting levels, no trailing content). */
JsonResult json_parse(Heap *heap, Str source);
/* quoted: the JSON string literal of the text. */
Str json_quoted(Heap *heap, Str text);
/* to_string: the compact rendering. */
Str json_to_string(Heap *heap, const Json *value);
/* object_fields: Result<String, List<Field<T>>> */
JsonListResult json_object_fields(const Json *value);
/* string */
JsonTextResult json_string(const Json *value);
/* bool */
JsonBoolResult json_bool(const Json *value);
/* array: Result<String, List<T>> */
JsonListResult json_array(const Json *value);
/* field: the first field named key. */
JsonResult json_field(Heap *heap, const Vec *fields, Str key);
/* required: every expected key (Str*) is a field. */
bool json_required(const Vec *expected, const Vec *fields);
/* join_strings: separator before the first item, between before every other one. */
Str json_join_strings(Heap *heap, const Vec *xs, Str separator, Str between);
/* exact_object: the object has exactly the expected keys (Str*); Result<String, List<Field<T>>>. */
JsonListResult json_exact_object(Heap *heap, const Json *value, const Vec *expected);
/* lexical: codepoint order of two strings. */
Cmp json_lexical(Str a, Str b);
/* nat: a canonical decimal natural within the host integer range, as text. */
JsonTextResult json_nat(const Json *value);
/* string_field */
JsonTextResult json_string_field(Heap *heap, const Vec *fields, Str key);
/* nat_field */
JsonTextResult json_nat_field(Heap *heap, const Vec *fields, Str key);
/* bool_field */
JsonBoolResult json_bool_field(Heap *heap, const Vec *fields, Str key);
/* nats: every item (Json*) a natural; Result<String, List<String>> of Str*. */
JsonListResult json_nats(Heap *heap, const Vec *values);
/* nat_list */
JsonListResult json_nat_list(Heap *heap, const Json *value);
/* nats_field */
JsonListResult json_nats_field(Heap *heap, const Vec *fields, Str key);

/* Shared list helpers of the import modules: a heap-boxed Str (NULL on allocation failure, heap->oom set)
 * and a Vec with one more item (vec_push; a failed push sets heap->oom). */
Str *json_boxed_str(Heap *heap, Str value);
Vec json_vec_with(Heap *heap, Vec vec, void *item);
/* decimal_digits(text): every character a decimal digit (true of the empty text). */
bool json_decimal_digits(Str text);
/* The expected key list of exact_object from C string literals, as heap-boxed Str items. */
Vec json_expected_keys(Heap *heap, const char *const *names, size_t count);

/* ===== names.bend: Name.T (Anonymous{}, Str{prefix, text}, Num{prefix, index}) ===== */
typedef enum { NAME_ANONYMOUS, NAME_STR, NAME_NUM } NameKind;
typedef struct {
  NameKind kind;
  Str prefix; /* NAME_STR, NAME_NUM: the prefix name index as decimal text */
  Str text;   /* NAME_STR */
  Str index;  /* NAME_NUM: the numeral as decimal text */
} Name;
/* Result<String, Name.T> */
typedef struct {
  JsonResultKind kind;
  Name value;
  Str message;
} NameResult;
/* parse(tag, value) */
NameResult name_parse(Heap *heap, Str tag, const Json *value);
/* references(value): List<String> of Str*. */
Vec name_references(Heap *heap, Name value);

/* ===== levels.bend: Level.T (LZero{}, LSucc{index}, LMax{left, right}, LImax{left, right}, LParam{index}) ===== */
typedef enum { LEVEL_ZERO, LEVEL_SUCC, LEVEL_MAX, LEVEL_IMAX, LEVEL_PARAM } LevelKind;
typedef struct {
  LevelKind kind;
  Str index; /* LEVEL_SUCC: the level index; LEVEL_PARAM: the name index */
  Str left;  /* LEVEL_MAX, LEVEL_IMAX */
  Str right; /* LEVEL_MAX, LEVEL_IMAX */
} Level;
/* Result<String, Level.T> */
typedef struct {
  JsonResultKind kind;
  Level value;
  Str message;
} LevelResult;
/* parse(tag, value) */
LevelResult level_parse(Heap *heap, Str tag, const Json *value);
/* name_references(value): List<String> of Str*. */
Vec level_name_references(Heap *heap, Level value);
/* level_references(value): List<String> of Str*. */
Vec level_level_references(Heap *heap, Level value);

/* ===== exprs.bend: Expr.T (Bvar, Sort, Const, Apply, Lam, Forall, Let, Proj, NatLiteral, StringLiteral,
 * Mdata); every index is the decimal text of a stream index ===== */
typedef enum {
  EXPR_BINDER_DEFAULT,
  EXPR_BINDER_IMPLICIT,
  EXPR_BINDER_STRICT_IMPLICIT,
  EXPR_BINDER_INST_IMPLICIT
} ExprBinderInfo;
/* Binder{name, typ, body, info} */
typedef struct {
  Str name;
  Str typ;
  Str body;
  ExprBinderInfo info;
} ExprBinder;
typedef enum {
  EXPR_BVAR,
  EXPR_SORT,
  EXPR_CONST,
  EXPR_APPLY,
  EXPR_LAM,
  EXPR_FORALL,
  EXPR_LET,
  EXPR_PROJ,
  EXPR_NAT_LITERAL,
  EXPR_STRING_LITERAL,
  EXPR_MDATA
} ExprKind;
typedef struct {
  ExprKind kind;
  Str index;         /* EXPR_BVAR: the de Bruijn index; EXPR_SORT: the level index; EXPR_PROJ: the field index */
  Str name;          /* EXPR_CONST, EXPR_LET: the name index */
  Vec universes;     /* EXPR_CONST: level indices, Str* */
  Str fn;            /* EXPR_APPLY */
  Str arg;           /* EXPR_APPLY */
  ExprBinder binder; /* EXPR_LAM, EXPR_FORALL */
  Str typ;           /* EXPR_LET */
  Str value;         /* EXPR_LET */
  Str body;          /* EXPR_LET */
  bool nondep;       /* EXPR_LET */
  Str type_name;     /* EXPR_PROJ */
  Str structure;     /* EXPR_PROJ */
  Str text;          /* EXPR_NAT_LITERAL: the decimal digits; EXPR_STRING_LITERAL: the decoded text */
  Str expr;          /* EXPR_MDATA */
  Vec data;          /* EXPR_MDATA: JsonField*, in source order */
} Expr;
/* Result<String, Expr.T> */
typedef struct {
  JsonResultKind kind;
  Expr value;
  Str message;
} ExprResult;
/* parse(tag, value) */
ExprResult expr_parse(Heap *heap, Str tag, const Json *value);
/* name_references(value): List<String> of Str*. */
Vec expr_name_references(Heap *heap, const Expr *value);
/* level_references(value): List<String> of Str*. */
Vec expr_level_references(Heap *heap, const Expr *value);
/* expr_references(value): List<String> of Str*. */
Vec expr_expr_references(Heap *heap, const Expr *value);

/* ===== decls.bend: Decls.T and the inductive groups ===== */
/* Hints: OpaqueHint{}, Abbrev{}, Regular{height} */
typedef enum { DECL_HINT_OPAQUE, DECL_HINT_ABBREV, DECL_HINT_REGULAR } DeclHintsKind;
typedef struct {
  DeclHintsKind kind;
  Str height; /* DECL_HINT_REGULAR */
} DeclHints;
/* Safety: Unsafe{}, Safe{}, Partial{} */
typedef enum { DECL_UNSAFE, DECL_SAFE, DECL_PARTIAL } DeclSafety;
/* QuotKind: QuotType{}, QuotCtor{}, QuotLift{}, QuotInd{} */
typedef enum { DECL_QUOT_TYPE, DECL_QUOT_CTOR, DECL_QUOT_LIFT, DECL_QUOT_IND } DeclQuotKind;
/* Rule{ctor, nfields, rhs} */
typedef struct {
  Str ctor;
  Str nfields;
  Str rhs;
} DeclRule;
/* DeclKind: Axiom, Definition, Theorem, Opaque, Quotient, Inductive, Constructor, Recursor */
typedef enum {
  DECL_KIND_AXIOM,
  DECL_KIND_DEFINITION,
  DECL_KIND_THEOREM,
  DECL_KIND_OPAQUE,
  DECL_KIND_QUOTIENT,
  DECL_KIND_INDUCTIVE,
  DECL_KIND_CONSTRUCTOR,
  DECL_KIND_RECURSOR
} DeclKindTag;
typedef struct {
  DeclKindTag tag;
  bool unsafe;       /* AXIOM, OPAQUE, INDUCTIVE, CONSTRUCTOR, RECURSOR */
  Str value;         /* DEFINITION, THEOREM, OPAQUE: the value expression index */
  DeclHints hint;    /* DEFINITION */
  DeclSafety safe;   /* DEFINITION */
  Vec all;           /* DEFINITION, THEOREM, OPAQUE, INDUCTIVE, RECURSOR: name indices, Str* */
  DeclQuotKind quot; /* QUOTIENT */
  Str num_params;    /* INDUCTIVE, CONSTRUCTOR, RECURSOR */
  Str num_indices;   /* INDUCTIVE, RECURSOR */
  Vec ctors;         /* INDUCTIVE: constructor name indices, Str* */
  Str num_nested;    /* INDUCTIVE */
  bool recursive;    /* INDUCTIVE */
  bool reflexive;    /* INDUCTIVE */
  Str induct;        /* CONSTRUCTOR: the inductive type name index */
  Str cidx;          /* CONSTRUCTOR */
  Str num_fields;    /* CONSTRUCTOR */
  Str num_motives;   /* RECURSOR */
  Str num_minors;    /* RECURSOR */
  Vec rules;         /* RECURSOR: DeclRule* */
  bool k;            /* RECURSOR */
} DeclKindData;
/* T: Decl{name, level_params, typ, kind, line} */
typedef struct {
  Str name;
  Vec level_params; /* name indices, Str* */
  Str typ;
  DeclKindData kind;
  uint32_t line;
} DeclEntry;
/* Group{types, ctors, recs, line}: DeclEntry* items */
typedef struct {
  Vec types;
  Vec ctors;
  Vec recs;
  uint32_t line;
} DeclGroup;
/* Parsed{declarations, group}: DeclEntry* items; has_group false = None{} */
typedef struct {
  Vec declarations;
  bool has_group;
  DeclGroup group;
} DeclParsed;
/* Result<String, Parsed> */
typedef struct {
  JsonResultKind kind;
  DeclParsed value;
  Str message;
} DeclParsedResult;
/* parse(tag, value, line) */
DeclParsedResult decl_parse(Heap *heap, Str tag, const Json *value, uint32_t line);
/* name_references(declaration): List<String> of Str*. */
Vec decl_name_references(Heap *heap, const DeclEntry *declaration);
/* expr_references(declaration): List<String> of Str*. */
Vec decl_expr_references(Heap *heap, const DeclEntry *declaration);
/* kind_name(kind) */
Str decl_kind_name(const DeclKindData *kind);

/* ===== table.bend: Table.T<A> = Map<String, Maybe<A>> keyed by index text, and Indexed<A>. The rows
 * are heap-boxed values held as void*; the Bend table is persistent and threaded linearly, so the C
 * table is updated in place by table_put ===== */
typedef struct {
  Map tree;
} Table;
/* Indexed{index, value} */
typedef struct {
  Str index;
  void *value;
} TableIndexed;
/* Maybe<A>: present false = None{} (value NULL) */
typedef struct {
  bool present;
  void *value;
} TableGet;
/* Result<String, A> */
typedef struct {
  JsonResultKind kind;
  void *value;
  Str message;
} TableLookup;
/* Result<String, Unit> */
typedef struct {
  JsonResultKind kind;
  Str message;
} TableCheck;
/* empty() */
Table table_empty(Heap *heap);
/* put(table, key, value): in place; a NULL value (a failed boxing allocation) is not stored. */
void table_put(Heap *heap, Table *table, Str key, void *value);
/* get(table, key) */
TableGet table_get(const Table *table, Str key);
/* present(value) */
bool table_present(TableGet value);
/* has(table, key) */
bool table_has(const Table *table, Str key);
/* keys(table): Str* items in insertion order */
Vec table_keys(Heap *heap, const Table *table);
/* size(table) */
size_t table_size(const Table *table);
/* default(value, fallback) */
void *table_default(TableGet value, void *fallback);
/* require(value, message) */
TableLookup table_require(TableGet value, Str message);
/* lookup(table, kind, index) */
TableLookup table_lookup(Heap *heap, const Table *table, Str kind, Str index);
/* reference_present(yes, kind, index) */
TableCheck table_reference_present(Heap *heap, bool yes, Str kind, Str index);
/* references(indices, table, kind): indices are Str* items */
TableCheck table_references(Heap *heap, const Vec *indices, const Table *table, Str kind);
/* a heap-boxed Indexed{index, value} (NULL on allocation failure, heap->oom set) */
TableIndexed *table_indexed(Heap *heap, Str index, void *value);

/* ===== export.bend: the reader of a Lean 4 NDJSON export (format 3.1.0) ===== */
/* Error: ImportError{line, message} */
typedef struct {
  uint32_t line;
  Str message;
} ExportError;
/* Metadata{exporter_name, exporter_version, lean_githash, lean_version, format_version} */
typedef struct {
  Str exporter_name;
  Str exporter_version;
  Str lean_githash;
  Str lean_version;
  Str format_version;
} ExportMetadata;
/* Counts{lines, names, levels, exprs, declarations} */
typedef struct {
  uint32_t lines;
  size_t names;
  size_t levels;
  size_t exprs;
  size_t declarations;
} ExportCounts;
/* NameRow{node, key, text, canonical} */
typedef struct {
  Name node;
  Str key;
  Str text;
  Str canonical;
} ExportNameRow;
/* ExprRow{node, line} */
typedef struct {
  Expr node;
  uint32_t line;
} ExportExprRow;
/* State{names, interned, levels, exprs, decls, expr_order, decl_order, groups}: the tables hold
 * ExportNameRow*, Str*, Level*, ExportExprRow* and DeclEntry* rows; the orders are Vecs in source
 * order (the Bend lists are prepended and reversed on reading) of TableIndexed* (value Expr*),
 * DeclEntry* and DeclGroup* items. */
typedef struct {
  Table names;
  Table interned;
  Table levels;
  Table exprs;
  Table decls;
  Vec expr_order;
  Vec decl_order;
  Vec groups;
} ExportState;
/* T: Export{meta, totals, state} */
typedef struct {
  ExportMetadata meta;
  ExportCounts totals;
  ExportState state;
} Export;
/* Result<Error, T> */
typedef struct {
  JsonResultKind kind;
  Export value;
  ExportError error;
} ExportResult;
/* Maybe<Name.T>, Maybe<String>, Maybe<Level.T>, Maybe<Expr.T>, Maybe<U32>, Maybe<Decls.T> */
typedef struct {
  bool present;
  Name value;
} ExportName;
typedef struct {
  bool present;
  Str value;
} ExportText;
typedef struct {
  bool present;
  Level value;
} ExportLevel;
typedef struct {
  bool present;
  Expr value;
} ExportExpr;
typedef struct {
  bool present;
  uint32_t value;
} ExportLine;
typedef struct {
  bool present;
  DeclEntry value;
} ExportDeclaration;
/* read_string(source) */
ExportResult export_read_string(Heap *heap, Str source);
/* metadata(source) */
ExportMetadata export_metadata(const Export *source);
/* counts(source) */
ExportCounts export_counts(const Export *source);
/* name(source, index) */
ExportName export_name(const Export *source, Str index);
/* name_string(source, index) */
ExportText export_name_string(const Export *source, Str index);
/* canonical_name(source, index) */
ExportText export_canonical_name(const Export *source, Str index);
/* level(source, index) */
ExportLevel export_level(const Export *source, Str index);
/* expr(source, index) */
ExportExpr export_expr(const Export *source, Str index);
/* expr_line(source, index) */
ExportLine export_expr_line(const Export *source, Str index);
/* expressions(source): TableIndexed* items (value Expr*) in source order */
Vec export_expressions(const Export *source);
/* declaration(source, index) */
ExportDeclaration export_declaration(const Export *source, Str index);
/* declarations(source): DeclEntry* items in source order */
Vec export_declarations(const Export *source);
/* groups(source): DeclGroup* items in source order */
Vec export_groups(const Export *source);
/* names(source): TableIndexed* items (value Name*) sorted by index length, then codepoint order */
Vec export_names(Heap *heap, const Export *source);

/* sort_keys(keys): Str* keys by index length, then codepoint order (shared with translate.bend) */
Vec export_sort_keys(Heap *heap, const Vec *keys);

/* ===== translate.bend: the typed view of an export (identities stay decimal strings). The one
 * Translated is heap-boxed and threaded linearly, so its tables are updated in place; the id sets are
 * persistent values shared by cached summaries, so set_add and union return fresh sets ===== */
/* IdSet{values: Table.T<Unit>}: every row is the one unit marker */
typedef struct {
  Table values;
} TranslateIdSet;
/* Level: LZero, LSucc{child}, LMax{left, right}, LImax{left, right}, LParam{source_name, name} */
typedef enum { TRANSLATE_LZERO, TRANSLATE_LSUCC, TRANSLATE_LMAX, TRANSLATE_LIMAX, TRANSLATE_LPARAM } TranslateLevelKind;
typedef struct {
  TranslateLevelKind kind;
  Str child;       /* LSucc: the level index */
  Str left;        /* LMax, LImax */
  Str right;       /* LMax, LImax */
  Str source_name; /* LParam: the canonical identity */
  Str name;        /* LParam: the display name */
} TranslateLevel;
/* Binder{name, typ, body, info}: name is the display name */
typedef struct {
  Str name;
  Str typ;
  Str body;
  ExprBinderInfo info;
} TranslateBinder;
/* Node: Bound, Sort, Constant, Apply, Lambda, Pi, Let, Projection, NatLiteral, StringLiteral, Metadata */
typedef enum {
  TRANSLATE_BOUND,
  TRANSLATE_SORT,
  TRANSLATE_CONSTANT,
  TRANSLATE_APPLY,
  TRANSLATE_LAMBDA,
  TRANSLATE_PI,
  TRANSLATE_LET,
  TRANSLATE_PROJECTION,
  TRANSLATE_NAT_LITERAL,
  TRANSLATE_STRING_LITERAL,
  TRANSLATE_METADATA
} TranslateNodeKind;
typedef struct {
  TranslateNodeKind kind;
  Str index;              /* Bound: the de Bruijn index; Projection: the field index */
  Str level;              /* Sort: the level index */
  Str source_name;        /* Constant: the name index */
  Str name;               /* Constant, Let: the display name */
  Vec universes;          /* Constant: level indices, Str* */
  Str fn;                 /* Apply */
  Str arg;                /* Apply */
  TranslateBinder binder; /* Lambda, Pi */
  Str typ;                /* Let */
  Str value;              /* Let: the value index; NatLiteral: the digits; StringLiteral: the text */
  Str body;               /* Let */
  bool nondep;            /* Let */
  Str source_type_name;   /* Projection: the type name index */
  Str type_name;          /* Projection: the display type name */
  Str structure;          /* Projection */
  Str expr;               /* Metadata */
  Vec data;               /* Metadata: JsonField*, in source order */
} TranslateNode;
/* Parameter{source_name, name}: the canonical identity and the display name */
typedef struct {
  Str source_name;
  Str name;
} TranslateParameter;
/* Row{declaration, name, parameters, root, dependencies}: parameters TranslateParameter*, dependencies Str* */
typedef struct {
  DeclEntry declaration;
  Str name;
  Vec parameters;
  Str root;
  Vec dependencies;
} TranslateRow;
/* Summary{binders, params, constants} */
typedef struct {
  Big binders;
  TranslateIdSet params;
  TranslateIdSet constants;
} TranslateSummary;
/* T: Translated{type_nodes, level_nodes, summaries, level_params, decl_rows}: the tables hold
 * TranslateNode*, TranslateLevel*, TranslateSummary* and TranslateIdSet* rows; decl_rows holds
 * TranslateRow* in source order */
typedef struct {
  Table type_nodes;
  Table level_nodes;
  Table summaries;
  Table level_params;
  Vec decl_rows;
} Translated;
/* Result<Export.Error, T> */
typedef struct {
  JsonResultKind kind;
  Translated *value;
  ExportError error;
} TranslateResult;
/* Result<Export.Error, Unit>, Result<Export.Error, String>, Result<Export.Error, List<...>> */
typedef struct {
  JsonResultKind kind;
  ExportError error;
} TranslateCheck;
typedef struct {
  JsonResultKind kind;
  Str value;
  ExportError error;
} TranslateText;
typedef struct {
  JsonResultKind kind;
  Vec value;
  ExportError error;
} TranslateList;
TranslateIdSet translate_empty_set(Heap *heap);
TranslateIdSet translate_set_add(Heap *heap, TranslateIdSet set, Str key);
bool translate_set_has(const TranslateIdSet *set, Str key);
/* set_keys(set): Str* items */
Vec translate_set_keys(Heap *heap, const TranslateIdSet *set);
TranslateIdSet translate_set_insert_all(Heap *heap, const Vec *keys, TranslateIdSet set);
TranslateIdSet translate_union(Heap *heap, TranslateIdSet a, TranslateIdSet b);
TranslateSummary translate_empty_summary(Heap *heap);
Big translate_max_big(Cmp order, Big a, Big b);
TranslateSummary translate_merge(Heap *heap, TranslateSummary a, TranslateSummary b);
Big translate_decrement(Heap *heap, Cmp order, Big number);
TranslateSummary translate_under_binder(Heap *heap, TranslateSummary summary);
/* empty(): NULL when the allocation failed (heap->oom set) */
Translated *translate_empty(Heap *heap);
TranslateCheck translate_ensure(bool ok, uint32_t line, Str message);
TranslateText translate_named(Heap *heap, const Export *source, uint32_t line, Str id);
TranslateText translate_canonical(Heap *heap, const Export *source, uint32_t line, Str id);
Str translate_join(Heap *heap, const Vec *items, Str separator);
/* named_all(ids, source, line): Str* items */
TranslateList translate_named_all(Heap *heap, const Vec *ids, const Export *source, uint32_t line);
Table *translate_type_nodes(Translated *value);
Table *translate_level_nodes(Translated *value);
Table *translate_summaries(Translated *value);
Table *translate_level_params(Translated *value);
Vec translate_decl_rows(const Translated *value);
Big translate_summary_binders(TranslateSummary value);
TranslateIdSet translate_summary_params(TranslateSummary value);
TranslateIdSet translate_summary_constants(TranslateSummary value);
TranslateResult translate_translate(Heap *heap, const Export *source);
/* rows(table): TranslateRow* items in source order */
Vec translate_rows(const Translated *table);
/* nodes(table), levels(table): TableIndexed* items (value TranslateNode*, TranslateLevel*) in sort_keys order */
Vec translate_nodes(Heap *heap, Translated *table);
Vec translate_levels(Heap *heap, Translated *table);

/* ===== render.bend: the JSON rendering of a translated export (the import drivers and the report).
 * A NULL result is a failed allocation (heap->oom set) ===== */
/* number(value), string(value) */
Json *render_number(Heap *heap, Str value);
Json *render_string(Heap *heap, Str value);
/* number_items(values), numbers(values), string_items(values), strings(values): values are Str* items */
Vec render_number_items(Heap *heap, const Vec *values);
Json *render_numbers(Heap *heap, const Vec *values);
Vec render_string_items(Heap *heap, const Vec *values);
Json *render_strings(Heap *heap, const Vec *values);
/* binder_info(info) */
Str render_binder_info(ExprBinderInfo info);
/* binder(value) */
Json *render_binder(Heap *heap, const TranslateBinder *value);
/* node_field(value), node_record(row), node_records(rows): TableIndexed* items with TranslateNode* values */
JsonField *render_node_field(Heap *heap, const TranslateNode *value);
Json *render_node_record(Heap *heap, const TableIndexed *row);
Vec render_node_records(Heap *heap, const Vec *rows);
/* level_field(level), level_record(row), level_records(rows): TableIndexed* items with TranslateLevel* values */
JsonField *render_level_field(Heap *heap, const TranslateLevel *level);
Json *render_level_record(Heap *heap, const TableIndexed *row);
Vec render_level_records(Heap *heap, const Vec *rows);
/* name_field(value), name_record(row): a TableIndexed with a Name* value */
JsonField *render_name_field(Heap *heap, const Name *value);
Json *render_name_record(Heap *heap, const TableIndexed *row);
/* parameter(value), parameters(values): TranslateParameter* items */
Json *render_parameter(Heap *heap, const TranslateParameter *value);
Vec render_parameters(Heap *heap, const Vec *values);
/* declaration_id(value) */
Str render_declaration_id(const DeclEntry *value);
/* row_summary(value), row_summaries(values): TranslateRow* items */
Json *render_row_summary(Heap *heap, const TranslateRow *value);
Vec render_row_summaries(Heap *heap, const Vec *values);
/* scoped_summary(table) */
Json *render_scoped_summary(Heap *heap, Translated *table);

/* ===== lower.bend: lowers a translated declaration type to a kernel term. Comp(A) = State -> State &
 * Result<Error, A> is a LowerState pointer threaded in/out plus a LowerResult (payload per function in
 * its comment, kernel convention). The work counter is bounded by its start value 100000. ===== */
/* Error: Deferred{message}, Unsupported{message}, KernelError{error} */
typedef enum { LOWER_DEFERRED, LOWER_UNSUPPORTED, LOWER_KERNEL_ERROR } LowerErrorKind;
typedef struct {
  LowerErrorKind kind;
  Str message; /* LOWER_DEFERRED, LOWER_UNSUPPORTED */
  Error error; /* LOWER_KERNEL_ERROR */
} LowerError;
/* Resolver{call: String -> List<Level.T> -> Resolver & Result<String, Term.T>}: a handler returns its
 * successor; ctx is the handler's own state */
typedef struct LowerResolver LowerResolver;
typedef struct LowerResolved LowerResolved;
struct LowerResolver {
  LowerResolved (*call)(Heap *heap, void *ctx, Str name, const Vec *levels);
  void *ctx;
};
/* Resolver & Result<String, Term.T>: levels are const LevelVar * items */
struct LowerResolved {
  LowerResolver next;
  bool ok;
  Str message;      /* Fail */
  const Term *term; /* Done */
};
/* State{work, budget, resolver} */
typedef struct {
  Nat work;
  Budget budget;
  LowerResolver resolver;
} LowerState;
/* Result<Error, A> */
typedef enum { LOWER_DONE, LOWER_FAIL } LowerResultKind;
typedef struct {
  LowerResultKind kind;
  void *value; /* LOWER_DONE */
  LowerError error; /* LOWER_FAIL */
} LowerResult;
/* BoundContext{domain, ctx} */
typedef struct {
  const Term *domain;
  Ctx ctx;
} LowerBoundContext;
/* FunctionDomain{quantity, name, value} */
typedef struct {
  Quantity quantity;
  Str name;
  const Value *value;
} LowerFunctionDomain;
/* message(error) */
Str lower_message(Heap *heap, LowerError error);
/* resolver_call(resolver, name, levels) */
LowerResolved lower_resolver_call(Heap *heap, LowerResolver resolver, Str name, const Vec *levels);
/* missing(prefix), fixed(term) */
LowerResolver lower_missing(Heap *heap, Str prefix);
LowerResolver lower_fixed(const Term *term);
/* lower_row(row, globals, budget, table): payload const Term * */
LowerResult lower_row(Heap *heap, LowerState *state, const TranslateRow *row, Globals globals, Budget budget,
                      Translated *table);
/* lower_type(resolver, globals, table, row, budget), lower_default(resolver, globals, table, row) */
LowerResult lower_type(Heap *heap, LowerResolver resolver, Globals globals, Translated *table,
                       const TranslateRow *row, Budget budget);
LowerResult lower_default(Heap *heap, LowerResolver resolver, Globals globals, Translated *table,
                          const TranslateRow *row);

/* ===== report.bend: the parity report over a translated export (prepare lowers every row with the
 * missing-prelude resolver; files renders manifest.json, types.ndjson and summary.txt) ===== */
/* Outcome: Kernel{term}, Deferred{detail}, Unsupported{detail}, Failed{detail} */
typedef enum { REPORT_KERNEL, REPORT_DEFERRED, REPORT_UNSUPPORTED, REPORT_FAILED } ReportOutcomeKind;
typedef struct {
  ReportOutcomeKind kind;
  const Term *term; /* REPORT_KERNEL */
  Str detail;       /* REPORT_DEFERRED, REPORT_UNSUPPORTED, REPORT_FAILED */
} ReportOutcome;
/* Entry{row, outcome} */
typedef struct {
  const TranslateRow *row;
  ReportOutcome outcome;
} ReportEntry;
/* T: Report{source, types, entries}: entries are ReportEntry* items */
typedef struct {
  const Export *source;
  Translated *types;
  Vec entries;
} Report;
/* OutputFile{name, contents} */
typedef struct {
  Str name;
  Str contents;
} ReportOutputFile;
/* Result<Export.Error, T> */
typedef struct {
  JsonResultKind kind;
  Report value;      /* JSON_DONE */
  ExportError error; /* JSON_FAIL */
} ReportResult;
/* outcome(value): the LOWER_DONE payload is a const Term * */
ReportOutcome report_outcome(Heap *heap, LowerResult value);
/* prepare_entries(rows, table): rows are TranslateRow* items; ReportEntry* items out */
Vec report_prepare_entries(Heap *heap, const Vec *rows, Translated *table);
/* prepare(source) */
ReportResult report_prepare(Heap *heap, const Export *source);
/* namespace(name, prefix), external_name(name) */
bool report_namespace(Heap *heap, Str name, Str prefix);
bool report_external_name(Heap *heap, Str name);
/* canonical_constant(value, names), constant(node, source, names), constants(rows, source, names),
 * constant_names(source): rows are TableIndexed* items over Expr values */
TranslateIdSet report_canonical_constant(Heap *heap, ExportText value, TranslateIdSet names);
TranslateIdSet report_constant(Heap *heap, const Expr *node, const Export *source, TranslateIdSet names);
TranslateIdSet report_constants(Heap *heap, const Vec *rows, const Export *source, TranslateIdSet names);
TranslateIdSet report_constant_names(Heap *heap, const Export *source);
/* status(outcome), kind(entry), entry_status(entry) */
Str report_status(ReportOutcome outcome);
Str report_kind(const ReportEntry *entry);
Str report_entry_status(const ReportEntry *entry);
/* increment(yes, n), count_status(wanted, entries) */
size_t report_increment(bool yes, size_t n);
size_t report_count_status(Str wanted, const Vec *entries);
/* keep(yes, entry, kept), filter_kind(wanted, entries): ReportEntry* items in source order */
Vec report_keep(Heap *heap, bool yes, ReportEntry *entry, Vec kept);
Vec report_filter_kind(Heap *heap, Str wanted, const Vec *entries);
/* external_display(value), count_referenced(ids, source), count_declared(entries): ids are Str* items */
bool report_external_display(Heap *heap, ExportText value);
size_t report_count_referenced(Heap *heap, const Vec *ids, const Export *source);
size_t report_count_declared(Heap *heap, const Vec *entries);
/* grammar(counts), declaration_count(counts) */
Str report_grammar(Heap *heap, ExportCounts counts);
size_t report_declaration_count(ExportCounts counts);
/* kind_summary(name, entries), kind_summaries(kinds, entries), summary(report): Str* items out */
Str report_kind_summary(Heap *heap, Str name, const Vec *entries);
Vec report_kind_summaries(Heap *heap, const Vec *kinds, const Vec *entries);
Vec report_summary(Heap *heap, const Report *report);
/* detail(outcome), entry_record(entry), entry_records(entries), name_records(names): Json* items out */
JsonField *report_detail(Heap *heap, ReportOutcome outcome);
Json *report_entry_record(Heap *heap, const ReportEntry *entry);
Vec report_entry_records(Heap *heap, const Vec *entries);
Vec report_name_records(Heap *heap, const Vec *names);
/* json_lines(values): every Json* item rendered and followed by "\n" */
Str report_json_lines(Heap *heap, const Vec *values);
/* manifest(meta, count) */
Json *report_manifest(Heap *heap, ExportMetadata meta, size_t count);
/* files(report): ReportOutputFile* items manifest.json, types.ndjson, summary.txt */
Vec report_files(Heap *heap, const Report *report);

/* ===== mapping.bend: the external-constant inventory of an export (a NAME_ONLY, UNMAPPED or NEVER
 * verdict per referenced external constant, the ratified NEVER exception rows, the foundation
 * target table and the two TSV renders). Lists are Vecs of heap-boxed rows in source order ===== */
/* Verdict: NameOnly{}, Unmapped{}, Never{} */
typedef enum { MAPPING_NAME_ONLY, MAPPING_UNMAPPED, MAPPING_NEVER } MappingVerdict;
/* Row{lean_name, target, verdict}: target present false = None{} */
typedef struct {
  Str lean_name;
  ExportText target;
  MappingVerdict verdict;
} MappingRow;
/* ExceptionRow{lean_name, ratification, milestone} */
typedef struct {
  Str lean_name;
  Str ratification;
  Str milestone;
} MappingExceptionRow;
/* Target{lean_name, target} */
typedef struct {
  Str lean_name;
  Str target;
} MappingTarget;
/* T: Inventory{rows, never, external_declared, const_names}: rows are MappingRow* items, never are
 * MappingExceptionRow* items */
typedef struct {
  Vec rows;
  Vec never;
  size_t external_declared;
  size_t const_names;
} MappingInventory;
/* Result<String, Translate.IdSet> */
typedef struct {
  JsonResultKind kind;
  TranslateIdSet value; /* JSON_DONE */
  Str message;          /* JSON_FAIL */
} MappingNames;
/* Result<String, Table.T<String>>: the table rows are boxed Str* targets */
typedef struct {
  JsonResultKind kind;
  Table value; /* JSON_DONE */
  Str message; /* JSON_FAIL */
} MappingTargets;
/* Result<String, T> */
typedef struct {
  JsonResultKind kind;
  MappingInventory value; /* JSON_DONE */
  Str message;            /* JSON_FAIL */
} MappingResult;
/* verdict_name(verdict), omega_reflection(name) */
Str mapping_verdict_name(MappingVerdict verdict);
bool mapping_omega_reflection(Heap *heap, Str name);
/* clean_char(ch), clean_text(text), cell(value), guard(ok, message) */
bool mapping_clean_char(char ch);
bool mapping_clean_text(Str text);
bool mapping_cell(Str value);
TableCheck mapping_guard(bool ok, Str message);
/* add_external(yes, name, names), external_names(ids, source, names): ids are Str* items */
TranslateIdSet mapping_add_external(Heap *heap, bool yes, Str name, TranslateIdSet names);
MappingNames mapping_external_names(Heap *heap, const Vec *ids, const Export *source, TranslateIdSet names);
/* targets(rows, names, found): rows are MappingTarget* items; found is updated in place */
MappingTargets mapping_targets(Heap *heap, const Vec *rows, const TranslateIdSet *names, Table found);
/* merge_names(left, right), sort_names(names): Str* items in codepoint order */
Vec mapping_merge_names(Heap *heap, const Vec *left, const Vec *right);
Vec mapping_sort_names(Heap *heap, const Vec *names);
/* mapped_row(target, name), classify(never, name, targets), make_rows(names, targets): names are
 * Str* items, MappingRow* items out */
MappingRow *mapping_mapped_row(Heap *heap, TableGet target, Str name);
MappingRow *mapping_classify(Heap *heap, bool never, Str name, const Table *targets);
Vec mapping_make_rows(Heap *heap, const Vec *names, const Table *targets);
/* never_row(row, rest), never_rows(rows): MappingExceptionRow* items out */
Vec mapping_never_row(Heap *heap, const MappingRow *row, Vec rest);
Vec mapping_never_rows(Heap *heap, const Vec *rows);
/* count_declared(declarations, source): declarations are DeclEntry* items */
size_t mapping_count_declared(Heap *heap, const Vec *declarations, const Export *source);
/* inventory(source, target_rows): target_rows are MappingTarget* items */
MappingResult mapping_inventory(Heap *heap, const Export *source, const Vec *target_rows);
/* add_display(value, names), displays(ids, source, names): ids are Str* items */
TranslateIdSet mapping_add_display(Heap *heap, ExportText value, TranslateIdSet names);
TranslateIdSet mapping_displays(Heap *heap, const Vec *ids, const Export *source, TranslateIdSet names);
/* keep_target(found, target, rest), present_targets(targets, names), foundation_targets(source):
 * MappingTarget* items */
Vec mapping_keep_target(Heap *heap, bool found, MappingTarget *target, Vec rest);
Vec mapping_present_targets(Heap *heap, const Vec *targets, const TranslateIdSet *names);
Vec mapping_foundation_targets(Heap *heap, const Export *source);
/* target_text(target), map_lines(rows), never_lines(rows), map_tsv(inventory), never_tsv(inventory) */
Str mapping_target_text(ExportText target);
Str mapping_map_lines(Heap *heap, const Vec *rows);
Str mapping_never_lines(Heap *heap, const Vec *rows);
Str mapping_map_tsv(Heap *heap, const MappingInventory *inventory);
Str mapping_never_tsv(Heap *heap, const MappingInventory *inventory);

#endif
