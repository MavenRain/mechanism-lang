/* Port of bend2/kernel/eterm.bend: the erased kernel term, its declarations and their printers. */
#include "kernel.h"

/* Left to right concatenation of the parts. */
static Str cats(Heap *heap, const Str *parts, size_t count) {
  Str out = str_empty();
  size_t i;
  for (i = 0; i < count; i++) out = heap_str_cat(heap, out, parts[i]);
  return out;
}

static void buf_push(Heap *heap, StrBuf *buf, Str s) {
  if (!strbuf_push(buf, s)) heap->oom = true;
}

/* U32.show */
static Str show_u32(Heap *heap, uint32_t n) {
  Str out = str_of_u64(heap->arena, n);
  if (out.len == 0) heap->oom = true;
  return out;
}

/* String.from_list([Char.from_u32(n)]) for a printable ASCII code point. */
static Str one_char(Heap *heap, uint32_t n) {
  char *bytes = heap_alloc(heap, 2);
  if (bytes == NULL) return str_empty();
  bytes[0] = (char)n;
  bytes[1] = 0;
  return str_n(bytes, 1);
}

/* ---- constructors ---- */

Tid eterm_tid(Str name) {
  Tid t;
  t.name = name;
  return t;
}

Fid eterm_fid(Str name) {
  Fid f;
  f.name = name;
  return f;
}

static Repr repr_of(ReprKind kind, Tid tid) {
  Repr r;
  r.kind = kind;
  r.tid = tid;
  return r;
}

Repr eterm_ri31(void) {
  Str empty = str_empty();
  Tid none = eterm_tid(empty);
  return repr_of(REPR_I31, none);
}

Repr eterm_rstruct(Tid tid) {
  return repr_of(REPR_STRUCT, tid);
}

Repr eterm_runion(Tid tid) {
  return repr_of(REPR_UNION, tid);
}

Repr eterm_rfunc(Tid tid) {
  return repr_of(REPR_FUNC, tid);
}

Repr eterm_rthunk(Tid tid) {
  return repr_of(REPR_THUNK, tid);
}

KtmBranch eterm_branch(Nat tag, Nat arity, const Ktm *body) {
  KtmBranch b;
  b.tag = tag;
  b.arity = arity;
  b.body = body;
  return b;
}

Kdecl eterm_kfun(Fid fid, Vec params, Repr result, const Ktm *body) {
  Kdecl d;
  d.kind = KDECL_FUN;
  d.fid = fid;
  d.params = params;
  d.result = result;
  d.body = body;
  d.tids = params;
  d.tids.len = 0;
  return d;
}

Kdecl eterm_krec(Vec tids) {
  Kdecl d;
  d.kind = KDECL_REC;
  d.fid = eterm_fid(str_empty());
  d.params = tids;
  d.params.len = 0;
  d.result = eterm_ri31();
  d.body = eterm_kerased();
  d.tids = tids;
  return d;
}

Tid *eterm_tid_new(Heap *heap, Str name) {
  Tid *t = heap_alloc(heap, sizeof(Tid));
  if (t == NULL) return NULL;
  t->name = name;
  return t;
}

Repr *eterm_repr_new(Heap *heap, Repr repr) {
  Repr *r = heap_alloc(heap, sizeof(Repr));
  if (r == NULL) return NULL;
  *r = repr;
  return r;
}

KtmBranch *eterm_branch_new(Heap *heap, Nat tag, Nat arity, const Ktm *body) {
  KtmBranch *b = heap_alloc(heap, sizeof(KtmBranch));
  if (b == NULL) return NULL;
  b->tag = tag;
  b->arity = arity;
  b->body = body;
  return b;
}

Kdecl *eterm_kdecl_new(Heap *heap, Kdecl decl) {
  Kdecl *d = heap_alloc(heap, sizeof(Kdecl));
  if (d == NULL) return NULL;
  *d = decl;
  return d;
}

static const Ktm ERASED_KTM = {.kind = KTM_ERASED};

/* A zeroed term of the kind, or NULL after a failed allocation (heap->oom set). */
static Ktm *ktm_new(Heap *heap, KtmKind kind) {
  Ktm *t = heap_alloc(heap, sizeof(Ktm));
  if (t == NULL) return NULL;
  t->kind = kind;
  t->lit = literal_lint(big_zero());
  return t;
}

const Ktm *eterm_kvar(Heap *heap, Nat index) {
  Ktm *t = ktm_new(heap, KTM_VAR);
  if (t == NULL) return &ERASED_KTM;
  t->index = index;
  return t;
}

const Ktm *eterm_klit(Heap *heap, Literal value) {
  Ktm *t = ktm_new(heap, KTM_LIT);
  if (t == NULL) return &ERASED_KTM;
  t->lit = value;
  return t;
}

const Ktm *eterm_kglobal(Heap *heap, Str name) {
  Ktm *t = ktm_new(heap, KTM_GLOBAL);
  if (t == NULL) return &ERASED_KTM;
  t->name = name;
  return t;
}

const Ktm *eterm_kerased(void) {
  return &ERASED_KTM;
}

const Ktm *eterm_klet(Heap *heap, Str name, const Ktm *value, const Ktm *body) {
  Ktm *t = ktm_new(heap, KTM_LET);
  if (t == NULL) return &ERASED_KTM;
  t->name = name;
  t->value = value;
  t->body = body;
  return t;
}

const Ktm *eterm_kclos(Heap *heap, Fid fid, Nat arity, Vec captures) {
  Ktm *t = ktm_new(heap, KTM_CLOS);
  if (t == NULL) return &ERASED_KTM;
  t->fid = fid;
  t->arity = arity;
  t->captures = captures;
  return t;
}

const Ktm *eterm_kapp(Heap *heap, const Ktm *head, Vec args) {
  Ktm *t = ktm_new(heap, KTM_APP);
  if (t == NULL) return &ERASED_KTM;
  t->head = head;
  t->args = args;
  return t;
}

const Ktm *eterm_ktail(Heap *heap, const Ktm *head, Vec args) {
  Ktm *t = ktm_new(heap, KTM_TAIL);
  if (t == NULL) return &ERASED_KTM;
  t->head = head;
  t->args = args;
  return t;
}

const Ktm *eterm_kstruct(Heap *heap, Tid tid, Vec fields) {
  Ktm *t = ktm_new(heap, KTM_STRUCT);
  if (t == NULL) return &ERASED_KTM;
  t->tid = tid;
  t->fields = fields;
  return t;
}

const Ktm *eterm_kproj(Heap *heap, Tid tid, Nat index, const Ktm *value) {
  Ktm *t = ktm_new(heap, KTM_PROJ);
  if (t == NULL) return &ERASED_KTM;
  t->tid = tid;
  t->index = index;
  t->value = value;
  return t;
}

const Ktm *eterm_ktag(Heap *heap, Tid tid, Nat tag, Vec fields) {
  Ktm *t = ktm_new(heap, KTM_TAG);
  if (t == NULL) return &ERASED_KTM;
  t->tid = tid;
  t->tag = tag;
  t->fields = fields;
  return t;
}

const Ktm *eterm_kcase(Heap *heap, Tid tid, const Ktm *scrut, Vec branches) {
  Ktm *t = ktm_new(heap, KTM_CASE);
  if (t == NULL) return &ERASED_KTM;
  t->tid = tid;
  t->scrut = scrut;
  t->branches = branches;
  return t;
}

const Ktm *eterm_kdelay(Heap *heap, Fid fid, Vec captures) {
  Ktm *t = ktm_new(heap, KTM_DELAY);
  if (t == NULL) return &ERASED_KTM;
  t->fid = fid;
  t->captures = captures;
  return t;
}

const Ktm *eterm_kforce(Heap *heap, const Ktm *value) {
  Ktm *t = ktm_new(heap, KTM_FORCE);
  if (t == NULL) return &ERASED_KTM;
  t->value = value;
  return t;
}

/* ---- the Bend definitions, in order ---- */

Str eterm_tid_text(Tid t) {
  return t.name;
}

Str eterm_fid_text(Fid f) {
  return f.name;
}

Str eterm_decimal_escape(Heap *heap, uint32_t n) {
  Str parts[4];
  parts[0] = str_lit("\\");
  parts[1] = show_u32(heap, n / 100);
  parts[2] = show_u32(heap, (n / 10) % 10);
  parts[3] = show_u32(heap, n % 10);
  return cats(heap, parts, 4);
}

Str eterm_ascii_escape(Heap *heap, bool printable, uint32_t n) {
  if (printable) return one_char(heap, n);
  return eterm_decimal_escape(heap, n);
}

Str eterm_unicode_escape_four(Heap *heap, bool four, uint32_t n) {
  Str parts[4];
  if (four) {
    parts[0] = eterm_decimal_escape(heap, 240 | (n >> 18));
    parts[1] = eterm_decimal_escape(heap, 128 | ((n >> 12) & 63));
    parts[2] = eterm_decimal_escape(heap, 128 | ((n >> 6) & 63));
    parts[3] = eterm_decimal_escape(heap, 128 | (n & 63));
    return cats(heap, parts, 4);
  }
  parts[0] = eterm_decimal_escape(heap, 224 | (n >> 12));
  parts[1] = eterm_decimal_escape(heap, 128 | ((n >> 6) & 63));
  parts[2] = eterm_decimal_escape(heap, 128 | (n & 63));
  return cats(heap, parts, 3);
}

Str eterm_unicode_escape_two(Heap *heap, bool two, uint32_t n) {
  Str parts[2];
  if (two) {
    parts[0] = eterm_decimal_escape(heap, 192 | (n >> 6));
    parts[1] = eterm_decimal_escape(heap, 128 | (n & 63));
    return cats(heap, parts, 2);
  }
  return eterm_unicode_escape_four(heap, n >= 65536, n);
}

Str eterm_unicode_escape(Heap *heap, bool ascii, uint32_t n) {
  if (ascii) return eterm_ascii_escape(heap, n >= 32 && n < 127, n);
  return eterm_unicode_escape_two(heap, n < 2048, n);
}

Str eterm_escape_char(Heap *heap, uint32_t c) {
  if (c == 10) return str_lit("\\n");
  if (c == 13) return str_lit("\\r");
  if (c == 9) return str_lit("\\t");
  if (c == 8) return str_lit("\\b");
  if (c == 34) return str_lit("\\\"");
  if (c == 92) return str_lit("\\\\");
  return eterm_unicode_escape(heap, c < 128, c);
}

/* One UTF-8 byte of the walked string: see the kernel.h note on eterm_escape. */
static Str escape_byte(Heap *heap, uint32_t b) {
  if (b < 128) return eterm_escape_char(heap, b);
  return eterm_decimal_escape(heap, b);
}

Str eterm_escape(Heap *heap, Str s) {
  StrBuf buf = strbuf_new(heap->arena);
  size_t i;
  for (i = 0; i < s.len; i++) {
    StrAt c = str_at(s, i);
    if (c.status == STR_INDEX_OK) {
      Str piece = escape_byte(heap, c.value);
      buf_push(heap, &buf, piece);
    }
  }
  return strbuf_str(&buf);
}

Str eterm_literal_text(Heap *heap, Literal l) {
  Str parts[3];
  switch (l.kind) {
  case LITERAL_STRING:
    parts[0] = str_lit("\"");
    parts[1] = eterm_escape(heap, l.string);
    parts[2] = str_lit("\"");
    return cats(heap, parts, 3);
  case LITERAL_INT:
    return big_to_string(heap, l.integer);
  }
  return str_empty();
}

Str eterm_print_repr(Heap *heap, Repr r) {
  Str parts[2];
  switch (r.kind) {
  case REPR_I31:
    return str_lit("i31");
  case REPR_STRUCT:
    parts[0] = str_lit("struct ");
    parts[1] = eterm_tid_text(r.tid);
    return cats(heap, parts, 2);
  case REPR_UNION:
    parts[0] = str_lit("union ");
    parts[1] = eterm_tid_text(r.tid);
    return cats(heap, parts, 2);
  case REPR_FUNC:
    parts[0] = str_lit("func ");
    parts[1] = eterm_tid_text(r.tid);
    return cats(heap, parts, 2);
  case REPR_THUNK:
    parts[0] = str_lit("thunk ");
    parts[1] = eterm_tid_text(r.tid);
    return cats(heap, parts, 2);
  }
  return str_empty();
}

/* ---- the print tasks ---- */

static PrintTask *task_new(Heap *heap, PrintTaskKind kind) {
  PrintTask *task = heap_alloc(heap, sizeof(PrintTask));
  if (task == NULL) return NULL;
  task->kind = kind;
  return task;
}

static void push_task(Heap *heap, Vec *out, PrintTask *task) {
  if (task == NULL) return;
  if (!vec_push(out, task)) heap->oom = true;
}

static void push_text(Heap *heap, Vec *out, Str text) {
  PrintTask *task = task_new(heap, PRINT_TASK_TEXT);
  if (task != NULL) task->text = text;
  push_task(heap, out, task);
}

static void push_term(Heap *heap, Vec *out, const Ktm *term) {
  PrintTask *task = task_new(heap, PRINT_TASK_TERM);
  if (task != NULL) task->term = term;
  push_task(heap, out, task);
}

static void push_terms(Heap *heap, Vec *out, Vec terms) {
  PrintTask *task = task_new(heap, PRINT_TASK_TERMS);
  if (task != NULL) task->terms = terms;
  push_task(heap, out, task);
}

static void push_branches(Heap *heap, Vec *out, Vec branches) {
  PrintTask *task = task_new(heap, PRINT_TASK_BRANCHES);
  if (task != NULL) task->branches = branches;
  push_task(heap, out, task);
}

Vec eterm_expand(Heap *heap, const Ktm *t) {
  Vec out = vec_new(heap->arena);
  Str parts[5];
  switch (t->kind) {
  case KTM_VAR:
    parts[0] = str_lit("KVar ");
    parts[1] = heap_nat_show(heap, t->index);
    push_text(heap, &out, cats(heap, parts, 2));
    return out;
  case KTM_LIT:
    parts[0] = str_lit("KLit ");
    parts[1] = eterm_literal_text(heap, t->lit);
    push_text(heap, &out, cats(heap, parts, 2));
    return out;
  case KTM_GLOBAL:
    parts[0] = str_lit("KGlobal ");
    parts[1] = t->name;
    push_text(heap, &out, cats(heap, parts, 2));
    return out;
  case KTM_ERASED:
    push_text(heap, &out, str_lit("KErased"));
    return out;
  case KTM_LET:
    parts[0] = str_lit("KLet ");
    parts[1] = t->name;
    parts[2] = str_lit(" (");
    push_text(heap, &out, cats(heap, parts, 3));
    push_term(heap, &out, t->value);
    push_text(heap, &out, str_lit(") ("));
    push_term(heap, &out, t->body);
    push_text(heap, &out, str_lit(")"));
    return out;
  case KTM_CLOS:
    parts[0] = str_lit("KClos ");
    parts[1] = eterm_fid_text(t->fid);
    parts[2] = str_lit(" ");
    parts[3] = heap_nat_show(heap, t->arity);
    parts[4] = str_lit(" [");
    push_text(heap, &out, cats(heap, parts, 5));
    push_terms(heap, &out, t->captures);
    push_text(heap, &out, str_lit("]"));
    return out;
  case KTM_APP:
    push_text(heap, &out, str_lit("KApp ("));
    push_term(heap, &out, t->head);
    push_text(heap, &out, str_lit(") ["));
    push_terms(heap, &out, t->args);
    push_text(heap, &out, str_lit("]"));
    return out;
  case KTM_TAIL:
    push_text(heap, &out, str_lit("KTail ("));
    push_term(heap, &out, t->head);
    push_text(heap, &out, str_lit(") ["));
    push_terms(heap, &out, t->args);
    push_text(heap, &out, str_lit("]"));
    return out;
  case KTM_STRUCT:
    parts[0] = str_lit("KStruct ");
    parts[1] = eterm_tid_text(t->tid);
    parts[2] = str_lit(" [");
    push_text(heap, &out, cats(heap, parts, 3));
    push_terms(heap, &out, t->fields);
    push_text(heap, &out, str_lit("]"));
    return out;
  case KTM_PROJ:
    parts[0] = str_lit("KProj ");
    parts[1] = eterm_tid_text(t->tid);
    parts[2] = str_lit(" ");
    parts[3] = heap_nat_show(heap, t->index);
    parts[4] = str_lit(" (");
    push_text(heap, &out, cats(heap, parts, 5));
    push_term(heap, &out, t->value);
    push_text(heap, &out, str_lit(")"));
    return out;
  case KTM_TAG:
    parts[0] = str_lit("KTag ");
    parts[1] = eterm_tid_text(t->tid);
    parts[2] = str_lit(" ");
    parts[3] = heap_nat_show(heap, t->tag);
    parts[4] = str_lit(" [");
    push_text(heap, &out, cats(heap, parts, 5));
    push_terms(heap, &out, t->fields);
    push_text(heap, &out, str_lit("]"));
    return out;
  case KTM_CASE:
    parts[0] = str_lit("KCase ");
    parts[1] = eterm_tid_text(t->tid);
    parts[2] = str_lit(" (");
    push_text(heap, &out, cats(heap, parts, 3));
    push_term(heap, &out, t->scrut);
    push_text(heap, &out, str_lit(") ["));
    push_branches(heap, &out, t->branches);
    push_text(heap, &out, str_lit("]"));
    return out;
  case KTM_DELAY:
    parts[0] = str_lit("KDelay ");
    parts[1] = eterm_fid_text(t->fid);
    parts[2] = str_lit(" [");
    push_text(heap, &out, cats(heap, parts, 3));
    push_terms(heap, &out, t->captures);
    push_text(heap, &out, str_lit("]"));
    return out;
  case KTM_FORCE:
    push_text(heap, &out, str_lit("KForce ("));
    push_term(heap, &out, t->value);
    push_text(heap, &out, str_lit(")"));
    return out;
  }
  return out;
}

/* print_tasks runs the task list front to back; Term expands in place, Terms and Branches
 * interleave the items with "; ". The recursion here yields the same text as the Bend worklist. */
static void run_tasks(Heap *heap, StrBuf *buf, const Vec *tasks);

static void run_term(Heap *heap, StrBuf *buf, const Ktm *term) {
  Vec parts = eterm_expand(heap, term);
  run_tasks(heap, buf, &parts);
}

static void run_terms(Heap *heap, StrBuf *buf, const Vec *terms) {
  size_t i;
  for (i = 0; i < terms->len; i++) {
    VecGet item = vec_get(terms, i);
    if (item.status != VEC_GET_OK) return;
    if (i > 0) buf_push(heap, buf, str_lit("; "));
    run_term(heap, buf, item.value);
  }
}

static void run_branch(Heap *heap, StrBuf *buf, const KtmBranch *branch) {
  Str parts[5];
  parts[0] = str_lit("{");
  parts[1] = heap_nat_show(heap, branch->tag);
  parts[2] = str_lit(" ");
  parts[3] = heap_nat_show(heap, branch->arity);
  parts[4] = str_lit(" (");
  buf_push(heap, buf, cats(heap, parts, 5));
  run_term(heap, buf, branch->body);
  buf_push(heap, buf, str_lit(")}"));
}

static void run_branches(Heap *heap, StrBuf *buf, const Vec *branches) {
  size_t i;
  for (i = 0; i < branches->len; i++) {
    VecGet item = vec_get(branches, i);
    if (item.status != VEC_GET_OK) return;
    if (i > 0) buf_push(heap, buf, str_lit("; "));
    run_branch(heap, buf, item.value);
  }
}

static void run_task(Heap *heap, StrBuf *buf, const PrintTask *task) {
  switch (task->kind) {
  case PRINT_TASK_TEXT:
    buf_push(heap, buf, task->text);
    return;
  case PRINT_TASK_TERM:
    run_term(heap, buf, task->term);
    return;
  case PRINT_TASK_TERMS:
    run_terms(heap, buf, &task->terms);
    return;
  case PRINT_TASK_BRANCHES:
    run_branches(heap, buf, &task->branches);
    return;
  }
}

static void run_tasks(Heap *heap, StrBuf *buf, const Vec *tasks) {
  size_t i;
  for (i = 0; i < tasks->len; i++) {
    VecGet item = vec_get(tasks, i);
    if (item.status != VEC_GET_OK) return;
    run_task(heap, buf, item.value);
  }
}

Str eterm_print_tasks(Heap *heap, const Vec *tasks) {
  StrBuf buf = strbuf_new(heap->arena);
  run_tasks(heap, &buf, tasks);
  return strbuf_str(&buf);
}

Str eterm_print_ktm(Heap *heap, const Ktm *t) {
  Vec tasks = vec_new(heap->arena);
  push_term(heap, &tasks, t);
  return eterm_print_tasks(heap, &tasks);
}

Str eterm_repr_list(Heap *heap, const Vec *rs) {
  StrBuf buf = strbuf_new(heap->arena);
  size_t i;
  for (i = 0; i < rs->len; i++) {
    VecGet item = vec_get(rs, i);
    if (item.status != VEC_GET_OK) break;
    if (i > 0) buf_push(heap, &buf, str_lit(", "));
    {
      const Repr *r = item.value;
      Str shown = eterm_print_repr(heap, *r);
      buf_push(heap, &buf, shown);
    }
  }
  return strbuf_str(&buf);
}

Str eterm_tid_list(Heap *heap, const Vec *ts) {
  StrBuf buf = strbuf_new(heap->arena);
  size_t i;
  for (i = 0; i < ts->len; i++) {
    VecGet item = vec_get(ts, i);
    if (item.status != VEC_GET_OK) break;
    if (i > 0) buf_push(heap, &buf, str_lit("; "));
    {
      const Tid *t = item.value;
      Str shown = eterm_tid_text(*t);
      buf_push(heap, &buf, shown);
    }
  }
  return strbuf_str(&buf);
}

Str eterm_print_decl(Heap *heap, Kdecl d) {
  Str parts[7];
  switch (d.kind) {
  case KDECL_FUN:
    parts[0] = str_lit("fun ");
    parts[1] = eterm_fid_text(d.fid);
    parts[2] = str_lit(" (");
    parts[3] = eterm_repr_list(heap, &d.params);
    parts[4] = str_lit(") : ");
    parts[5] = eterm_print_repr(heap, d.result);
    parts[6] = str_lit(" := ");
    {
      Str head = cats(heap, parts, 7);
      Str body = eterm_print_ktm(heap, d.body);
      return heap_str_cat(heap, head, body);
    }
  case KDECL_REC:
    parts[0] = str_lit("rec [");
    parts[1] = eterm_tid_list(heap, &d.tids);
    parts[2] = str_lit("]");
    return cats(heap, parts, 3);
  }
  return str_empty();
}
