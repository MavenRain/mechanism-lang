/* A <dir>/<file>.bend path in a comment names bend2/<dir>/<file>.bend in commit 8c90b64,
 * the last tree with the Bend sources. */
/* Mechanism kernel: C port of the kernel .bend files, one .c per .bend file and one header for
 * the component. Every function that allocates takes the Heap it allocates from. Library code
 * never aborts: a failed allocation sets heap->oom and returns a well formed placeholder, and
 * the program boundary (test drivers, CLI) checks heap->oom before it prints any result. */
#ifndef MECH_KERNEL_H
#define MECH_KERNEL_H

#include "rt.h"

/* ---- common (kernel/common.bend) ---- */
#define HEAP_SMALL_CLASSES 32
/* The sharing memo of one compact pass (host only: c/kernel/compact.c, compact_memo_open). */
typedef struct CompactMemo CompactMemo;
typedef struct {
  Arena *arena;
  bool oom;
  /* Free lists of small one-shot records (budget.c steps and continuation contexts), one per
   * 8-byte size class up to HEAP_SMALL_CLASSES * 8 bytes. */
  void *small_blocks[HEAP_SMALL_CLASSES];
  /* NULL outside a compact pass; inside one, compact_term returns the same copy for the same source. */
  CompactMemo *memo;
  /* D-261a (host only): true on the compact-path work heaps of c/surface/elab.c. There, under an
   * unlimited budget, check_engine_ensure runs each conversion query in a scratch arena that it
   * frees after the query. heap_new sets it false, so the Bend path does not change. */
  bool conv_scratch;
} Heap;

Heap heap_new(Arena *arena);
/* Preserve allocation failures from vector operations in the heap error flag. */
Vec heap_vec_concat(Heap *heap, const Vec *a, const Vec *b);
Vec heap_vec_reverse(Heap *heap, const Vec *items);
/* Zeroed memory, or NULL after setting heap->oom. */
void *heap_alloc(Heap *heap, size_t size);
/* Zeroed memory from the size-class free list when a block is available, else from the arena
 * (rounded up to the class size). Sizes above HEAP_SMALL_CLASSES * 8 go to heap_alloc. */
void *heap_alloc_small(Heap *heap, size_t size);
/* Returns a block of heap_alloc_small (or of heap_alloc with a size that is a multiple of 8)
 * to its free list. The caller must not touch the block again. Oversized blocks stay in the arena. */
void heap_release_small(Heap *heap, void *block, size_t size);
Str heap_str_cat(Heap *heap, Str a, Str b);
/* Nat.show */
Str heap_nat_show(Heap *heap, Nat n);
Nat heap_nat_of_u64(Heap *heap, uint64_t value);
Nat heap_nat_add(Heap *heap, Nat a, Nat b);
Nat heap_nat_succ(Heap *heap, Nat n);
/* The one value of Unit. */
void *kernel_unit(void);
/* Bool as a Comp or Result value: the address of one of two static bools. */
void *kernel_bool_box(bool value);
bool kernel_bool_unbox(const void *boxed);

/* Base Cmp */
typedef enum { CMP_LT, CMP_EQ, CMP_GT } Cmp;

typedef struct {
  void *first;
  void *second;
} Pair;

/* List.get: the item at index, or out of range. */
VecGet common_at(const Vec *xs, Nat index);

/* ---- bignum (kernel/bignum.bend): signed canonical base 32768 limbs, least significant first ---- */
typedef struct {
  const uint32_t *items;
  size_t len;
} Limbs;

typedef struct {
  bool negative;
  Limbs digits;
} Big;

typedef struct {
  bool some;
  Big value;
} MaybeBig;
typedef struct {
  bool some;
  uint32_t value;
} MaybeU32;
typedef struct {
  bool some;
  Nat value;
} MaybeNat;
typedef struct {
  bool some;
  Limbs value;
} MaybeLimbs;
/* Common.Pair<List<U32>, U32> of div_small */
typedef struct {
  Limbs quotient;
  uint32_t remainder;
} DivSmall;

Big big_zero(void);
Big big_one(void);
Limbs big_trim(Limbs xs);
Big big_canonical(Limbs xs, bool negative);
Big big_make(Heap *heap, bool negative, Limbs digits);
Limbs big_digits_u32(Heap *heap, uint32_t n);
Big big_of_u32(Heap *heap, uint32_t n);
Cmp big_cmp_low(Cmp high, uint32_t a, uint32_t b);
Cmp big_cmp_digits(Limbs a, Limbs b);
Cmp big_reverse_cmp(Cmp c);
Cmp big_compare(Big a, Big b);
bool big_cmp_eq(Cmp c);
bool big_cmp_le(Cmp c);
bool big_equal(Big a, Big b);
bool big_le(Big a, Big b);
Cmp big_sign(Big x);
bool big_positive(Big x);
Limbs big_add_digits(Heap *heap, Limbs a, Limbs b, uint32_t carry);
Limbs big_sub_digits(Heap *heap, Limbs a, Limbs b, uint32_t carry);
Big big_add_signed(Heap *heap, Cmp c, bool negative, Limbs a, Limbs b);
Big big_add(Heap *heap, Big a, Big b);
Big big_neg(Big x);
Big big_sub_keep(Heap *heap, Cmp c, Big a, Big b);
/* Saturating: zero when a <= b. */
Big big_sub(Heap *heap, Big a, Big b);
Limbs big_mul_digit(Heap *heap, Limbs xs, uint32_t digit, uint32_t carry);
Limbs big_mul_digits(Heap *heap, Limbs a, Limbs b);
Big big_mul(Heap *heap, Big a, Big b);
Big big_maximum(Big a, Big b);
MaybeBig big_read_digit(Heap *heap, bool valid, uint32_t c, Big value);
MaybeBig big_read_chars(Heap *heap, Str s, MaybeBig value);
/* Unsigned decimal digits only; empty or any other byte gives none. */
MaybeBig big_of_decimal(Heap *heap, Str s);
DivSmall big_div_small(Heap *heap, Limbs xs, uint32_t divisor);
DivSmall big_div_small_put_trim(DivSmall result);
Str big_show_digits(Heap *heap, DivSmall state, Str suffix);
Str big_to_string(Heap *heap, Big x);
MaybeLimbs big_limbs15(Big x);
MaybeU32 big_narrow_put(MaybeU32 rest, uint32_t digit);
MaybeU32 big_narrow_digits(Limbs xs);
MaybeU32 big_to_u32(Big value);
MaybeU32 big_i31_select(MaybeU32 value);
MaybeU32 big_to_i31(Big value);
Limbs big_digits_nat(Heap *heap, Nat n);
Big big_of_nat(Heap *heap, Nat n);
MaybeNat big_bounded_nat(Heap *heap, bool inside, Big value);
MaybeNat big_to_nat_bounded(Heap *heap, Big value, Nat upper);
VecGet big_at_narrowed(const Vec *xs, MaybeNat index);
VecGet big_at(Heap *heap, const Vec *xs, Big index);

/* ---- error (kernel/error.bend) ---- */
typedef enum {
  ERROR_PARSE,
  ERROR_NOT_YET,
  ERROR_CARRY,
  ERROR_UNBOUND,
  ERROR_MISMATCH,
  ERROR_UNIVERSE,
  ERROR_QUANTITY,
  ERROR_WRONG_LEG,
  ERROR_MISSING_BRANCH,
  ERROR_OVERFLOW,
  ERROR_CANNOT_INFER,
  ERROR_BUDGET_EXHAUSTED,
  ERROR_INDEX_NOT_ZERO,
  ERROR_INDEX_ABOVE_UNIVERSE,
  ERROR_TERMINATION
} ErrorKind;

typedef struct {
  ErrorKind kind;
  Str message;
  Nat line;   /* ERROR_PARSE only */
  Nat column; /* ERROR_PARSE only */
} Error;

Error error_make(ErrorKind kind, Str message);
Error error_parse(Str message, Nat line, Nat column);
/* Placeholder carried by library code after a failed allocation (heap->oom is set too). */
Error error_host_oom(void);
Str error_termination_msg(Heap *heap, Str name);
Str error_message(Heap *heap, Error e);
Str error_to_string(Heap *heap, Error e);

/* ---- budget (kernel/budget.bend) ---- */
typedef enum { BUDGET_UNLIMITED, BUDGET_POLLS } BudgetKind;
typedef struct {
  BudgetKind kind;
  Nat remaining; /* BUDGET_POLLS only */
} Budget;
typedef struct {
  bool some;
  Budget value;
} MaybeBudget;

/* Result<Error.T, A> with A erased to a pointer. */
typedef enum { RESULT_DONE, RESULT_FAIL } ResultKind;
typedef struct {
  ResultKind kind;
  void *value; /* RESULT_DONE */
  Error error; /* RESULT_FAIL */
} Result;

typedef struct Step Step;
/* Bool -> Step */
typedef struct {
  Step *(*fn)(Heap *heap, void *ctx, bool spent);
  void *ctx;
} ResumeK;
/* Result -> Step */
typedef struct {
  Step *(*fn)(Heap *heap, void *ctx, Result result);
  void *ctx;
} DoneK;
typedef enum { STEP_FINISHED, STEP_POLL } StepKind;
struct Step {
  StepKind kind;
  Result result;  /* STEP_FINISHED */
  ResumeK resume; /* STEP_POLL */
};

/* Continuation(A): (Result -> Step) -> Step */
typedef struct {
  Step *(*fn)(Heap *heap, void *ctx, DoneK done);
  void *ctx;
} LaterK;
typedef enum { COMP_READY, COMP_ERROR, COMP_LATER } CompKind;
typedef struct {
  CompKind kind;
  void *value;   /* COMP_READY */
  Error error;   /* COMP_ERROR */
  LaterK action; /* COMP_LATER */
} Comp;
/* A -> Comp(B) */
typedef struct {
  Comp (*fn)(Heap *heap, void *ctx, void *value);
  void *ctx;
} NextK;
/* Error.T -> Comp(A) */
typedef struct {
  Comp (*fn)(Heap *heap, void *ctx, Error error);
  void *ctx;
} HandlerK;

/* Common.Pair<T, Result> */
typedef struct {
  Budget budget;
  Result result;
} BudgetState;

typedef struct Callback Callback;
typedef struct CallbackPoll CallbackPoll;
/* Callback{poll: Unit -> Callback & Bool}; poll returns NULL only when the heap is out of memory. */
struct Callback {
  CallbackPoll *(*poll)(Heap *heap, void *ctx);
  void *ctx;
};
struct CallbackPoll {
  Callback next;
  bool spent;
};
typedef struct {
  Callback callback;
  Result result;
} CallbackState;

typedef struct IOCallback IOCallback;
typedef struct IOCallbackPoll IOCallbackPoll;
/* IOCallback{poll: Unit -> IO(IOCallback & Bool)}: the poll runs its effects directly. */
struct IOCallback {
  IOCallbackPoll *(*poll)(Heap *heap, void *ctx);
  void *ctx;
};
struct IOCallbackPoll {
  IOCallback next;
  bool spent;
};
typedef struct {
  IOCallback callback;
  Result result;
} IOCallbackState;

Budget budget_unlimited(void);
Budget budget_of_polls(Nat remaining);
MaybeBudget budget_poll(Heap *heap, Budget budget);
bool budget_exhausted(Budget budget);
Result budget_result_done(void *value);
Result budget_result_fail(Error error);
Step *budget_start_comp(Heap *heap, Comp action, DoneK done);
Comp budget_pure(void *value);
Comp budget_failed(Error error);
Step *budget_bind_result(Heap *heap, Result first, NextK next, DoneK done);
Comp budget_bind(Heap *heap, Comp first, NextK next);
Step *budget_tick_result(Heap *heap, bool spent, Str message, DoneK done);
Comp budget_tick_with(Heap *heap, Str message);
Comp budget_tick(Heap *heap);
Comp budget_lift(Result result);
BudgetState budget_run_step(Heap *heap, Step *step, Budget budget);
BudgetState budget_run_state(Heap *heap, Comp action, Budget budget);
Result budget_discard(BudgetState state);
Result budget_run(Heap *heap, Comp action, Budget budget);
Comp budget_comp_pure(void *value);
Comp budget_comp_bind(Heap *heap, Comp first, NextK next);
Step *budget_recover_result(Heap *heap, Result result, HandlerK handler, DoneK done);
Comp budget_recover(Heap *heap, Comp action, HandlerK handler);
/* Comp(Result<Error.T, A>): the value of the ready result is a heap allocated Result. */
Comp budget_attempt(Heap *heap, Comp action);
Callback budget_of_poll(CallbackPoll *(*poll)(Heap *heap, void *ctx), void *ctx);
CallbackState budget_resume_poll(Heap *heap, ResumeK resume, CallbackPoll *response);
CallbackState budget_run_poll_step(Heap *heap, Step *step, Callback callback);
CallbackState budget_run_poll_state(Heap *heap, Comp action, Callback callback);
Result budget_discard_poll(CallbackState state);
Result budget_run_poll(Heap *heap, Comp action, Callback callback);
IOCallback budget_of_poll_io(IOCallbackPoll *(*poll)(Heap *heap, void *ctx), void *ctx);
IOCallbackState budget_resume_io(Heap *heap, ResumeK resume, IOCallbackPoll *response);
IOCallbackState budget_run_io_step(Heap *heap, Step *step, IOCallback callback);
IOCallbackState budget_run_io_state(Heap *heap, Comp action, IOCallback callback);
Result budget_discard_io(IOCallbackState state);
Result budget_run_io(Heap *heap, Comp action, IOCallback callback);

/* ---- quantity (kernel/quantity.bend) ---- */
typedef enum { QUANTITY_ZERO, QUANTITY_ONE, QUANTITY_MANY } Quantity;

typedef struct {
  Quantity lo;
  Quantity hi;
} Interval;

/* Common.Pair<Nat, Interval>: one item of a usage list. */
typedef struct {
  Nat level;
  Interval value;
} UsageRow;
/* Maybe<List<Common.Pair<Nat, Interval>>>: rows holds UsageRow pointers in list order. */
typedef struct {
  bool some;
  Vec rows;
} MaybeUsageRows;
typedef struct {
  MaybeUsageRows paths;
  Vec reads; /* List<Common.Pair<Nat, Interval>> as UsageRow pointers */
} Usage;

uint32_t quantity_rank(Quantity q);
bool quantity_equal(Quantity a, Quantity b);
Str quantity_to_string(Quantity q);
Quantity quantity_runtime(Quantity q);
Quantity quantity_mul(Quantity a, Quantity b);
Quantity quantity_add(Quantity a, Quantity b);
Quantity quantity_minimum(Quantity a, Quantity b);
Quantity quantity_maximum(Quantity a, Quantity b);
Usage quantity_empty(Heap *heap);
Usage quantity_unreachable(Heap *heap);
Interval quantity_interval(const Vec *xs, Nat level);
Vec quantity_occurrence_reads(Heap *heap, Nat level, Quantity q);
Usage quantity_occurrence(Heap *heap, Nat level, Quantity q);
/* Mode 0 adds counts; 1 takes minimum; 2 takes maximum; 3 keeps left. */
Quantity quantity_combine_mark(uint32_t mode, Quantity a, Quantity b);
Interval quantity_combine_interval(Interval a, Interval b, uint32_t lo_mode, uint32_t hi_mode);
/* List<Nat> as a Vec of Nat pointers into the rows. */
Vec quantity_keys(Heap *heap, const Vec *xs);
bool quantity_contains(const Vec *xs, Nat key);
Vec quantity_unique(Heap *heap, const Vec *xs, Vec seen);
Vec quantity_combine_rows(Heap *heap, const Vec *rows, const Vec *a, const Vec *b, uint32_t lo_mode,
                          uint32_t hi_mode);
Vec quantity_combine(Heap *heap, const Vec *a, const Vec *b, uint32_t lo_mode, uint32_t hi_mode);
MaybeUsageRows quantity_sequence_paths(Heap *heap, MaybeUsageRows a, MaybeUsageRows b);
Usage quantity_sequence(Heap *heap, Usage a, Usage b);
MaybeUsageRows quantity_alternative_paths(Heap *heap, MaybeUsageRows a, MaybeUsageRows b);
Usage quantity_alternative(Heap *heap, Usage a, Usage b);
Vec quantity_scale_map(Heap *heap, const Vec *xs, Quantity lo, Quantity hi);
MaybeUsageRows quantity_scale_paths(Heap *heap, MaybeUsageRows paths, Quantity lo, Quantity hi);
Usage quantity_scale_range(Heap *heap, Interval bounds, Usage usage);
Usage quantity_scale(Heap *heap, Quantity q, Usage usage);
Interval quantity_get_paths(MaybeUsageRows paths, Interval reads, Nat level);
Interval quantity_get(Usage usage, Nat level);
Vec quantity_remove_map(Heap *heap, const Vec *xs, Nat level);
MaybeUsageRows quantity_remove_paths(Heap *heap, MaybeUsageRows paths, Nat level);
Usage quantity_remove(Heap *heap, Usage usage, Nat level);
bool quantity_interval_once(Interval value);
bool quantity_exactly_once(Nat level, Usage usage);
bool quantity_interval_used(Interval value);
bool quantity_used(Nat level, Usage usage);
bool quantity_interval_affine(Interval value);
bool quantity_at_most_once(Nat level, Usage usage);
Vec quantity_capture_paths(Heap *heap, MaybeUsageRows paths, const Vec *reads);
Usage quantity_captures(Heap *heap, Usage usage);

/* ---- shape (kernel/shape.bend): T<A> with the payload type A erased to a pointer ---- */
typedef enum {
  SHAPE_PI,
  SHAPE_ARG,
  SHAPE_COLL,
  SHAPE_PAR,
  SHAPE_MU,
  SHAPE_NU,
  SHAPE_ZK,
  SHAPE_FHC,
  SHAPE_MPC
} ShapeKind;
typedef struct {
  ShapeKind kind;
  Quantity quantity; /* SHAPE_PI, SHAPE_ARG, SHAPE_ZK */
  Str name;          /* SHAPE_PI, SHAPE_ARG, SHAPE_MU, SHAPE_NU, SHAPE_ZK */
  void *domain;      /* SHAPE_PI, SHAPE_ZK */
  Big count;         /* SHAPE_COLL */
  void *left;        /* SHAPE_PAR */
  void *right;       /* SHAPE_PAR */
  Vec args;          /* SHAPE_MU, SHAPE_NU: List<A> as pointers */
  void *level;       /* SHAPE_FHC */
  void *parties;     /* SHAPE_MPC */
  void *access;      /* SHAPE_MPC */
} Shape;
typedef struct {
  bool some;
  Str value;
} MaybeStr;
typedef struct {
  bool some;
  void *value;
} MaybePtr;

/* The constructors SPi .. SMpc. */
Shape shape_spi(Quantity quantity, Str name, void *domain);
Shape shape_sarg(Quantity quantity, Str name);
Shape shape_scoll(Big count);
Shape shape_spar(void *left, void *right);
Shape shape_smu(Str name, Vec args);
Shape shape_snu(Str name, Vec args);
Shape shape_szk(Quantity quantity, Str name, void *domain);
Shape shape_sfhc(void *level);
Shape shape_smpc(void *parties, void *access);
Shape shape_coll(Heap *heap, Nat count);
/* List<A> as a Vec of the payload pointers. */
Vec shape_payload(Heap *heap, Shape s);
MaybeStr shape_family(Shape s);
MaybePtr shape_point_dom(Shape s);
bool shape_is_pi(Shape s);
Str shape_name(Shape shape);
/* List<String> as a Vec of Str pointers. */
Vec shape_declared(Heap *heap);

/* ---- level_var (kernel/level_var.bend) ---- */
typedef enum { LEVEL_VAR_ZERO, LEVEL_VAR_SUCC, LEVEL_VAR_MAX, LEVEL_VAR_IMAX, LEVEL_VAR_VAR } LevelVarKind;
typedef struct LevelVar LevelVar;
struct LevelVar {
  LevelVarKind kind;
  Big amount;            /* LEVEL_VAR_SUCC */
  const LevelVar *base;  /* LEVEL_VAR_SUCC */
  const LevelVar *left;  /* LEVEL_VAR_MAX, LEVEL_VAR_IMAX */
  const LevelVar *right; /* LEVEL_VAR_MAX, LEVEL_VAR_IMAX */
  Nat index;             /* LEVEL_VAR_VAR */
};
typedef struct {
  bool some;
  const LevelVar *value;
} MaybeLevelVar;

/* The constructors LZero .. LVar; the allocating ones return LZero after a failed allocation
 * (heap->oom set). */
const LevelVar *level_var_lzero(void);
const LevelVar *level_var_lsucc(Heap *heap, Big amount, const LevelVar *base);
const LevelVar *level_var_lmax(Heap *heap, const LevelVar *left, const LevelVar *right);
const LevelVar *level_var_limax(Heap *heap, const LevelVar *left, const LevelVar *right);
const LevelVar *level_var_lvar(Heap *heap, Nat index);
const LevelVar *level_var_offset_positive(Heap *heap, const LevelVar *level, Big amount);
const LevelVar *level_var_offset_select(Heap *heap, bool positive, Big amount, const LevelVar *level);
const LevelVar *level_var_offset(Heap *heap, Big amount, const LevelVar *level);
bool level_var_in_scope(const LevelVar *level, Nat arity);
bool level_var_well_formed(const LevelVar *level);
MaybeLevelVar level_var_subst_succ(Heap *heap, MaybeLevelVar value, Big amount);
MaybeLevelVar level_var_subst_pair(Heap *heap, MaybeLevelVar a, MaybeLevelVar b, bool imax);
/* args: List<T> as a Vec of LevelVar pointers. */
MaybeLevelVar level_var_subst_visit(Heap *heap, const LevelVar *level, const Vec *args);
MaybeLevelVar level_var_subst_valid(Heap *heap, bool valid, const Vec *args, const LevelVar *level);
MaybeLevelVar level_var_subst(Heap *heap, const Vec *args, const LevelVar *level);
bool level_var_structural_equal(const LevelVar *a, const LevelVar *b);

/* ---- level_eq (kernel/level_eq.bend) ---- */
/* List<Nat> as a cons list; NULL is Nil. */
typedef struct NatList NatList;
struct NatList {
  Nat head;
  const NatList *tail;
};
/* List<Common.Pair<Nat, Bignum.T>> as a cons list; NULL is Nil. */
typedef struct ArmList ArmList;
struct ArmList {
  Nat key;
  Big amount;
  const ArmList *rest;
};
typedef struct {
  Big constant;
  const ArmList *arms;
} Normal;
/* Cons cells (host helpers): after a failed allocation they return the tail (heap->oom set). */
const NatList *level_eq_cons(Heap *heap, Nat head, const NatList *tail);
const ArmList *level_eq_arm_cons(Heap *heap, Nat key, Big amount, const ArmList *rest);
bool level_eq_member(const NatList *xs, Nat index);
const NatList *level_eq_insert(Heap *heap, const NatList *xs, Nat index);
const NatList *level_eq_variables(Heap *heap, const LevelVar *level, const NatList *found);
MaybeBig level_eq_find_arm(const ArmList *xs, Nat index);
const ArmList *level_eq_merge_arm(Heap *heap, const ArmList *xs, Nat index, Big amount);
const ArmList *level_eq_merge_arms(Heap *heap, const ArmList *xs, const ArmList *acc);
Normal level_eq_maximum(Heap *heap, Normal a, Normal b);
const ArmList *level_eq_shift_arms(Heap *heap, const ArmList *xs, Big amount);
Normal level_eq_shift(Heap *heap, Normal value, Big amount);
Normal level_eq_imax_combine(Heap *heap, Normal right, Normal left, Big amount);
Normal level_eq_normal(Heap *heap, const LevelVar *level, const NatList *positive, Big amount);
Big level_eq_minimum_value(Heap *heap, const ArmList *xs, Big acc);
bool level_eq_arm_below(MaybeBig other, Big amount);
bool level_eq_arms_below(const ArmList *xs, const ArmList *ys);
bool level_eq_below(Heap *heap, Normal a, Normal b);
bool level_eq_cases(Heap *heap, const NatList *remaining, const NatList *positive, const LevelVar *left,
                    const LevelVar *right);
bool level_eq_le(Heap *heap, const LevelVar *left, const LevelVar *right);
bool level_eq_equal(Heap *heap, const LevelVar *left, const LevelVar *right);
bool level_eq_always_zero(const LevelVar *level);
bool level_eq_always_positive(const LevelVar *level);
Big level_eq_constant(Normal value);
MaybeBig level_eq_closed_value(Heap *heap, const LevelVar *level);
/* Budgeted relation. Comp values: const NatList * (List<Nat>), Normal *, Big *, kernel_bool_box. */
Comp level_eq_validate_offset(Heap *heap, bool valid, const LevelVar *base, const NatList *found);
Comp level_eq_variables_node(Heap *heap, const LevelVar *level, const NatList *found);
Comp level_eq_variables_m(Heap *heap, const LevelVar *level, const NatList *found);
Comp level_eq_merge_m(Heap *heap, const ArmList *xs, const ArmList *acc);
Comp level_eq_merge_node(Heap *heap, const ArmList *xs, const ArmList *acc);
Comp level_eq_maximum_m(Heap *heap, Normal a, Normal b);
Comp level_eq_shift_arms_node(Heap *heap, const ArmList *xs, Big amount);
Comp level_eq_shift_arms_m(Heap *heap, const ArmList *xs, Big amount);
Comp level_eq_shift_m(Heap *heap, Normal value, Big amount);
Comp level_eq_imax_nonzero(Heap *heap, bool zero, Normal right, const LevelVar *left, const NatList *positive,
                           Big amount);
bool level_eq_normal_zero(Normal value);
Comp level_eq_imax_helper(Heap *heap, Normal right, const LevelVar *left, const NatList *positive, Big amount);
Comp level_eq_normal_node(Heap *heap, const LevelVar *level, const NatList *positive, Big amount);
Comp level_eq_normal_m(Heap *heap, const LevelVar *level, const NatList *positive, Big amount);
Comp level_eq_minimum_node(Heap *heap, const ArmList *xs, Big acc);
Comp level_eq_minimum_m(Heap *heap, const ArmList *xs, Big acc);
Comp level_eq_arms_below_node(Heap *heap, const ArmList *xs, const ArmList *ys, bool same);
Comp level_eq_arms_below_m(Heap *heap, const ArmList *xs, const ArmList *ys, bool same);
Comp level_eq_below_m(Heap *heap, Normal a, Normal b);
Comp level_eq_relation_reverse(Heap *heap, bool holds, bool equality, Normal left, Normal right);
Comp level_eq_compare_normal(Heap *heap, Normal left, Normal right, bool equality);
Comp level_eq_cases_continue(Heap *heap, bool holds, const NatList *remaining, const NatList *positive,
                             const LevelVar *left, const LevelVar *right, bool equality);
Comp level_eq_cases_node(Heap *heap, const NatList *remaining, const NatList *positive, const LevelVar *left,
                         const LevelVar *right, bool equality);
Comp level_eq_cases_m(Heap *heap, const NatList *remaining, const NatList *positive, const LevelVar *left,
                      const LevelVar *right, bool equality);
Comp level_eq_decide_m(Heap *heap, const LevelVar *left, const LevelVar *right, bool equality);
/* Result<Error.T, Bool>: the done value is a kernel bool box. */
Result level_eq_le_budget(Heap *heap, Budget budget, const LevelVar *left, const LevelVar *right);
Result level_eq_equal_budget(Heap *heap, Budget budget, const LevelVar *left, const LevelVar *right);

/* ---- level (kernel/level.bend): T = LevelVar ---- */
const LevelVar *level_zero(void);
const LevelVar *level_one(Heap *heap);
const LevelVar *level_succ(Heap *heap, const LevelVar *level);
const LevelVar *level_max(Heap *heap, const LevelVar *left, const LevelVar *right);
const LevelVar *level_imax(Heap *heap, const LevelVar *left, const LevelVar *right);
const LevelVar *level_of_u32(Heap *heap, uint32_t value);
const LevelVar *level_var(Heap *heap, Nat index);
bool level_seen_var(const NatList *seen, Nat index);
/* levels: List<T> as a Vec of LevelVar pointers. */
bool level_distinct_vars_from(Heap *heap, const Vec *levels, const NatList *seen);
bool level_distinct_vars(Heap *heap, const Vec *levels);
bool level_in_scope(Nat arity, const LevelVar *level);
MaybeLevelVar level_subst(Heap *heap, const Vec *args, const LevelVar *level);
bool level_equal(Heap *heap, const LevelVar *left, const LevelVar *right);
bool level_le(Heap *heap, const LevelVar *left, const LevelVar *right);
Result level_equal_budget(Heap *heap, Budget budget, const LevelVar *left, const LevelVar *right);
Result level_le_budget(Heap *heap, Budget budget, const LevelVar *left, const LevelVar *right);
bool level_always_zero(const LevelVar *level);
bool level_always_positive(const LevelVar *level);
Str level_render(Heap *heap, const LevelVar *level);
Str level_show_closed(Heap *heap, MaybeBig closed, const LevelVar *level);
Str level_to_string(Heap *heap, const LevelVar *level);

/* ---- literal (kernel/literal.bend) ---- */
typedef enum { LITERAL_STRING, LITERAL_INT } LiteralKind;
typedef struct {
  LiteralKind kind;
  Str string;  /* LITERAL_STRING */
  Big integer; /* LITERAL_INT */
} Literal;
Literal literal_lstring(Str value);
Literal literal_lint(Big value);
bool literal_equal(Literal a, Literal b);

/* ---- term (kernel/term.bend) ---- */
/* The generic containers Addr<A>, Leg<A>, Motive<A> and ElimData<A> are only ever instantiated
 * at T across  (rg: Term.Addr<Term.T>, Term.Leg<Term.T>, Term.Motive<Term.T>), so the
 * payload A is const Term *. Shape<T> payload pointers inside a Term are const Term * too. */
typedef struct Term Term;
typedef enum { ADDR_PT, ADDR_LEG, ADDR_CTOR } AddrKind;
typedef struct {
  AddrKind kind;
  Quantity quantity; /* ADDR_PT */
  const Term *arg;   /* ADDR_PT */
  Big index;         /* ADDR_LEG */
  Str name;          /* ADDR_CTOR */
} Addr;
/* Common.Pair<Quantity.T, String>: one binder of a leg. */
typedef struct {
  Quantity quantity;
  Str name;
} Binder;
/* binders: List<Common.Pair<Quantity.T, String>> as a Vec of Binder pointers. */
typedef struct {
  Vec binders;
  const Term *body;
} Leg;
/* indices: List<String> as a Vec of Str pointers. */
typedef struct {
  MaybeStr ind;
  Vec indices;
  Str self;
  const Term *body;
} Motive;
typedef struct {
  bool some;
  Motive value;
} MaybeMotive;
/* Common.Pair<Addr<A>, Leg<A>>: one branch of an eliminator. */
typedef struct {
  Addr addr;
  Leg leg;
} Branch;
/* branches: List<Common.Pair<Addr<A>, Leg<A>>> as a Vec of Branch pointers. */
typedef struct {
  Shape shape;
  const Term *scrut;
  Quantity scrut_q;
  MaybeMotive motive;
  Vec branches;
} ElimData;
typedef enum {
  TERM_VAR,
  TERM_UNIV,
  TERM_LAN,
  TERM_RAN,
  TERM_IN,
  TERM_ELIM,
  TERM_SEC,
  TERM_OUT,
  TERM_LET,
  TERM_ANN,
  TERM_GLOBAL,
  TERM_LIT,
  TERM_AUTO
} TermKind;
/* The payloads overlay in anonymous unions (decision 154 precedent for Value and Spine): each
 * constructor in term.c sets only the fields of its own kind, every reader switches on the kind,
 * and compact_term copies per kind. Fields that never share a kind share storage: shape with the
 * LET/GLOBAL/ANN name/ty/value group, body with the IN/OUT addr group and with legs, args with head.
 * sizeof(Term) is 224 bytes against 384 for the flat record. */
struct Term {
  TermKind kind;
  union {
    Nat index;             /* TERM_VAR */
    const LevelVar *level; /* TERM_UNIV */
    const ElimData *data;  /* TERM_ELIM, never NULL */
    Literal lit;           /* TERM_LIT */
    struct {
      union {
        Shape shape;       /* TERM_LAN, TERM_RAN, TERM_IN, TERM_SEC, TERM_OUT */
        struct {
          Str name;        /* TERM_LET, TERM_GLOBAL */
          const Term *ty;  /* TERM_LET, TERM_ANN */
          const Term *value; /* TERM_LET */
        };
      };
      union {
        const Term *body;  /* TERM_LAN, TERM_RAN, TERM_LET, TERM_ANN */
        struct {
          Addr addr;       /* TERM_IN, TERM_OUT */
          union {
            Vec args;      /* TERM_IN: List<T> as Term pointers */
            const Term *head; /* TERM_OUT */
          };
        };
        Vec legs;          /* TERM_SEC: List<Leg<T>> as Leg pointers */
      };
    };
  };
};
/* Maybe<Common.Pair<Quantity.T, A>> of as_apt. */
typedef struct {
  bool some;
  Quantity quantity;
  const Term *arg;
} MaybeApt;

/* The constructors APt, ALeg, ACtor, Leg, Motive, ElimData and the Maybe<Motive> cases. */
Addr term_addr_pt(Quantity quantity, const Term *arg);
Addr term_addr_leg(Big index);
Addr term_addr_ctor(Str name);
Binder term_binder(Quantity quantity, Str name);
Leg term_leg(Vec binders, const Term *body);
Motive term_motive(MaybeStr ind, Vec indices, Str self, const Term *body);
MaybeMotive term_motive_some(Motive motive);
MaybeMotive term_motive_none(void);
Branch term_branch(Addr addr, Leg leg);
ElimData term_elim_data(Shape shape, const Term *scrut, Quantity scrut_q, MaybeMotive motive, Vec branches);
/* Host helpers: list items boxed for a Vec; NULL after a failed allocation (heap->oom set). */
Binder *term_binder_new(Heap *heap, Quantity quantity, Str name);
Leg *term_leg_new(Heap *heap, Vec binders, const Term *body);
Branch *term_branch_new(Heap *heap, Addr addr, Leg leg);
/* The constructors Var .. Auto; the allocating ones return Auto after a failed allocation
 * (heap->oom set). */
const Term *term_var(Heap *heap, Nat index);
const Term *term_univ(Heap *heap, const LevelVar *level);
const Term *term_lan(Heap *heap, Shape shape, const Term *body);
const Term *term_ran(Heap *heap, Shape shape, const Term *body);
const Term *term_in(Heap *heap, Shape shape, Addr addr, Vec args);
const Term *term_elim(Heap *heap, ElimData data);
const Term *term_sec(Heap *heap, Shape shape, Vec legs);
const Term *term_out(Heap *heap, Shape shape, Addr addr, const Term *head);
const Term *term_let(Heap *heap, Str name, const Term *ty, const Term *value, const Term *body);
const Term *term_ann(Heap *heap, const Term *body, const Term *ty);
const Term *term_global(Heap *heap, Str name);
const Term *term_lit(Heap *heap, Literal value);
const Term *term_auto(void);
Addr term_aleg(Heap *heap, Nat index);
MaybeApt term_as_apt(Addr addr);
MaybeBig term_as_aleg(Addr addr);
MaybeStr term_as_actor(Addr addr);
/* Children lists: List<T> as a Vec of Term pointers, in the Bend order. */
Vec term_addr_children(Heap *heap, Addr addr);
Vec term_leg_children(Heap *heap, const Vec *legs);
Vec term_motive_children(Heap *heap, MaybeMotive motive);
Vec term_branch_children(Heap *heap, const Vec *branches);
Vec term_children(Heap *heap, const Term *term);
/* names: List<String> as a Vec of Str pointers. */
bool term_member(const Vec *names, Str name);
bool term_family_member(MaybeStr name, const Vec *names);
bool term_node_name(const Term *term, bool families, const Vec *names);
/* The Bend worklist recursion run as a loop over an explicit stack (same pre-order visit). */
bool term_exists_walk(Heap *heap, Vec todo, bool families, const Vec *names);
bool term_exists_name(Heap *heap, bool include_families, const Vec *names, const Term *term);
bool term_equal_list(const Vec *a, const Vec *b);
bool term_equal_strings(const Vec *a, const Vec *b);
bool term_equal_option_string(MaybeStr a, MaybeStr b);
bool term_equal_binders(const Vec *a, const Vec *b);
bool term_equal_shape(Shape a, Shape b);
bool term_equal_addr(Addr a, Addr b);
bool term_equal_leg(Leg a, Leg b);
bool term_equal_legs(const Vec *a, const Vec *b);
bool term_equal_motive(MaybeMotive a, MaybeMotive b);
bool term_equal_branches(const Vec *a, const Vec *b);
bool term_equal_elim(const ElimData *a, const ElimData *b);
bool term_equal(const Term *a, const Term *b);
/* List<String> as a Vec of Str pointers. */
Vec term_formers(Heap *heap);
Vec term_schema(Heap *heap);

/* ---- value (kernel/value.bend) ---- */
/* The generic containers Closure<A>, VLeg<A>, VAddr<A>, StuckElim<A> and Spine<A> are only ever
 * instantiated at T across , so the payload A is const Value *. */
typedef struct Value Value;
typedef enum { HEAD_LOCAL, HEAD_GLOBAL } HeadKind;
typedef struct {
  HeadKind kind;
  Nat level; /* HEAD_LOCAL */
  Str name;  /* HEAD_GLOBAL */
} Head;
/* env: List<A> as a Vec of Value pointers. */
typedef struct {
  Vec env;
  const Term *body;
} Closure;
/* binders: a Vec of Binder pointers. */
typedef struct {
  Vec binders;
  Closure clo;
} VLeg;
typedef enum { VADDR_PT, VADDR_LEG, VADDR_CTOR } VAddrKind;
typedef struct {
  VAddrKind kind;
  Quantity quantity; /* VADDR_PT */
  const Value *arg;  /* VADDR_PT */
  Big index;         /* VADDR_LEG */
  Str name;          /* VADDR_CTOR */
} VAddr;
/* branches: a Vec of Branch pointers (Term.Addr<Term.T>, Term.Leg<Term.T>); env: Value pointers. */
typedef struct {
  Shape shape;
  Quantity scrut_q;
  MaybeMotive motive;
  Vec branches;
  Vec env;
} StuckElim;
typedef enum { SPINE_OUT, SPINE_ELIM } SpineKind;
/* The two constructors share storage: a Spine is built once by value_sout or value_selim and
 * read only through its kind, so the payloads overlay (288 B instead of 480 B; hop 101). */
typedef struct {
  SpineKind kind;
  union {
    struct {
      Shape shape; /* SPINE_OUT */
      VAddr addr;  /* SPINE_OUT */
    };
    StuckElim data; /* SPINE_ELIM */
  };
} Spine;
typedef enum {
  VALUE_UNIV,
  VALUE_LAN,
  VALUE_RAN,
  VALUE_IN,
  VALUE_SEC,
  VALUE_LIT,
  VALUE_NEUTRAL
} ValueKind;
/* Values are built once by the value_v* constructors in value.c (each sets only its own
 * fields) and read only through the kind, so the per-constructor payloads overlay in one
 * union (224 B instead of 440 B; hop 101). The shape stays outside the union: it is read for
 * every shaped kind and is zero for the others. */
struct Value {
  ValueKind kind;
  Shape shape; /* VALUE_LAN, VALUE_RAN, VALUE_IN, VALUE_SEC; payload pointers are const Value * */
  union {
    const LevelVar *level; /* VALUE_UNIV */
    struct {
      Closure diagram;            /* VALUE_LAN, VALUE_RAN */
      MaybeLevelVar former_level; /* VALUE_LAN, VALUE_RAN: the Maybe<Level.T> field named level */
    };
    struct {
      VAddr addr; /* VALUE_IN */
      Vec args;   /* VALUE_IN: List<T> as Value pointers */
    };
    Vec legs;    /* VALUE_SEC: List<VLeg<T>> as VLeg pointers */
    Literal lit; /* VALUE_LIT */
    struct {
      Head head;  /* VALUE_NEUTRAL */
      Vec spine;  /* VALUE_NEUTRAL: List<Spine<T>> as Spine pointers */
    };
  };
};

/* The constructors HLocal .. SElim. */
Head value_hlocal(Nat level);
Head value_hglobal(Str name);
Closure value_closure(Vec env, const Term *body);
VLeg value_vleg(Vec binders, Closure clo);
VAddr value_vaddr_pt(Quantity quantity, const Value *arg);
VAddr value_vaddr_leg(Big index);
VAddr value_vaddr_ctor(Str name);
StuckElim value_stuck_elim(Shape shape, Quantity scrut_q, MaybeMotive motive, Vec branches, Vec env);
Spine value_sout(Shape shape, VAddr addr);
Spine value_selim(StuckElim data);
/* Host helpers: list items boxed for a Vec; NULL after a failed allocation (heap->oom set). */
VLeg *value_vleg_new(Heap *heap, Vec binders, Closure clo);
Spine *value_spine_new(Heap *heap, Spine spine);
/* The constructors VUniv .. VNeutral; they return VLit{LString{""}} after a failed allocation
 * (heap->oom set). */
const Value *value_vuniv(Heap *heap, const LevelVar *level);
const Value *value_vlan(Heap *heap, Shape shape, Closure diagram, MaybeLevelVar level);
const Value *value_vran(Heap *heap, Shape shape, Closure diagram, MaybeLevelVar level);
const Value *value_vin(Heap *heap, Shape shape, VAddr addr, Vec args);
const Value *value_vsec(Heap *heap, Shape shape, Vec legs);
const Value *value_vlit(Heap *heap, Literal value);
const Value *value_vneutral(Heap *heap, Head head, Vec spine);
VAddr value_valeg(Heap *heap, Nat index);
const Value *value_var(Heap *heap, Nat level);
Closure value_close(Vec env, const Term *body);
MaybeLevelVar value_as_univ(const Value *value);
bool value_head_equal(Head a, Head b);

/* ---- prim (kernel/prim.bend) ---- */
typedef enum { PRIM_NAT_ADD, PRIM_NAT_SUB, PRIM_NAT_MUL, PRIM_NAT_EQ, PRIM_NAT_LT } Prim;
typedef struct {
  bool some;
  Prim value;
} MaybePrim;
typedef struct {
  bool some;
  const Value *value;
} MaybeValue;

Str prim_name(Prim p);
/* List<T> as a Vec of Prim pointers (one boxed enum per item). */
Vec prim_catalog(Heap *heap);
Nat prim_arity(Heap *heap, Prim p);
MaybePrim prim_find(const Vec *ps, Str n);
MaybePrim prim_of_name(Heap *heap, Str n);
const Term *prim_nat_ty(Heap *heap);
const Term *prim_unit_ty(Heap *heap);
const Term *prim_bool_ty(Heap *heap);
const Term *prim_arrow(Heap *heap, Quantity q, Str n, const Term *dom, const Term *cod);
const Term *prim_result_ty(Heap *heap, Prim p);
const Term *prim_ty(Heap *heap, Prim p);
const Value *prim_bool_value(Heap *heap, bool b);
const Value *prim_apply_pair(Heap *heap, Prim p, Big a, Big b);
/* args: List<Literal.T> as a Vec of Literal pointers. */
MaybeValue prim_apply(Heap *heap, Prim p, const Vec *args);

/* ---- positivity (kernel/positivity.bend) ---- */
/* Binder{quantity, name, ty}: one telescope entry (the two field Binder of term.bend is another
 * type, so this one is TeleBinder). Telescope = List<Binder> as a Vec of TeleBinder pointers. */
typedef struct {
  Quantity quantity;
  Str name;
  const Term *ty;
} TeleBinder;
typedef enum { CTOR_STATUS_PROVISIONAL, CTOR_STATUS_BUILTIN, CTOR_STATUS_COMPLETE } CtorStatusKind;
typedef struct {
  CtorStatusKind kind;
  Vec names; /* CTOR_STATUS_COMPLETE: List<String> as Str pointers */
} CtorStatus;
/* res_idx: List<Term.T> as Term pointers. */
typedef struct {
  Str name;
  Vec args;
  Vec res_idx;
  Nat full_arity;
  bool self_rec;
} Ctor;
/* ctors: List<Ctor> as Ctor pointers. */
typedef struct {
  Str name;
  Vec params;
  Vec indices;
  const LevelVar *level;
  CtorStatus status;
  Vec ctors;
  bool positive;
} Family;
typedef struct {
  bool some;
  Ctor value;
} MaybeCtor;

/* The constructors Binder, Provisional, Builtin, Complete, Ctor and Family. */
TeleBinder positivity_binder(Quantity quantity, Str name, const Term *ty);
CtorStatus positivity_provisional(void);
CtorStatus positivity_builtin(void);
CtorStatus positivity_complete(Vec names);
Ctor positivity_ctor(Str name, Vec args, Vec res_idx, Nat full_arity, bool self_rec);
Family positivity_family(Str name, Vec params, Vec indices, const LevelVar *level, CtorStatus status, Vec ctors,
                         bool positive);
/* Host helpers: list items boxed for a Vec; NULL after a failed allocation (heap->oom set). */
TeleBinder *positivity_binder_new(Heap *heap, Quantity quantity, Str name, const Term *ty);
Ctor *positivity_ctor_new(Heap *heap, Ctor ctor);
MaybeCtor positivity_find_ctor(const Vec *ctors, Str name);
MaybeCtor positivity_ctor_of(Str name, Family family);
/* names: List<String> as a Vec of Str pointers. */
bool positivity_occurs(Heap *heap, const Vec *names, const Term *term);
bool positivity_absent(Heap *heap, const Vec *names, const Term *term);
bool positivity_absent_list(Heap *heap, const Vec *terms, const Vec *names);
/* Maybe<Term.T>: the value is a const Term pointer. */
MaybePtr positivity_point_dom(Shape s);
bool positivity_positive_ran(Heap *heap, MaybePtr dom, const Term *cod, const Term *whole, const Vec *names);
bool positivity_positive_out(Heap *heap, MaybePtr dom, Addr addr, const Term *head, const Term *whole,
                             const Vec *names);
bool positivity_positive(Heap *heap, const Term *term, const Vec *names);
bool positivity_fields_positive(Heap *heap, const Vec *args, const Vec *names);
/* Result<Error.T, Unit>: RESULT_DONE carries kernel_unit(). */
Result positivity_ctor_fields(Heap *heap, const Vec *names, const Vec *args);
bool positivity_self_rec(Heap *heap, const Vec *args, const Vec *names);
bool positivity_same_telescope(const Vec *a, const Vec *b);
bool positivity_same_status(CtorStatus a, CtorStatus b);
bool positivity_same_bool(bool a, bool b);
bool positivity_same_ctors(const Vec *a, const Vec *b, bool ignore_self_rec);
bool positivity_same_family_except(Family a, Family b, bool ignore_level, bool ignore_self_rec);
bool positivity_same_family(Family a, Family b);

/* ---- global (kernel/global.bend) ---- */
/* DefEntry{ty, body, reducible, rec_arg, partial} */
typedef struct {
  const Term *ty;
  const Term *body;
  bool reducible;
  MaybeNat rec_arg;
  bool partial;
} DefEntry;
typedef enum { ENTRY_DEF, ENTRY_AXIOM, ENTRY_PRIM } EntryKind;
typedef struct {
  EntryKind kind;
  DefEntry def;   /* ENTRY_DEF */
  const Term *ty; /* ENTRY_AXIOM, ENTRY_PRIM */
  Prim prim;      /* ENTRY_PRIM */
} Entry;
typedef struct {
  bool some;
  Entry value;
} MaybeEntry;
typedef struct {
  bool some;
  DefEntry value;
} MaybeDefEntry;
typedef struct {
  bool some;
  Family value;
} MaybeFamily;
/* List<Common.Pair<String, Entry>> and List<Common.Pair<String, Positivity.Family>> as persistent cons
 * lists (NULL = Nil): add prepends and every earlier Globals value stays valid, as in Bend. */
typedef struct EntryRows EntryRows;
struct EntryRows {
  Str name;
  Entry entry;
  const EntryRows *rest;
};
typedef struct FamilyRows FamilyRows;
struct FamilyRows {
  Str name;
  Family family;
  const FamilyRows *rest;
};
/* Map<String, Maybe<A>> as a persistent hash trie (set and del copy one path); a NULL root is empty. */
typedef struct IndexNode IndexNode;
typedef struct {
  const IndexNode *root;
} Index;
/* Globals{entries, families, entry_index, family_index} */
typedef struct {
  const EntryRows *entries;
  const FamilyRows *families;
  Index entry_index;
  Index family_index;
} Globals;

Entry global_entry_def(DefEntry value);
Entry global_entry_axiom(const Term *ty);
Entry global_entry_prim(const Term *ty, Prim prim);
/* Cons cells; on allocation failure heap->oom is set and rest is returned. */
const EntryRows *global_entry_rows(Heap *heap, Str name, Entry entry, const EntryRows *rest);
const FamilyRows *global_family_rows(Heap *heap, Str name, Family family, const FamilyRows *rest);
Globals global_empty(void);
MaybeEntry global_find_entry(const EntryRows *entries, Str name);
MaybeEntry global_find(Str name, Globals globals);
const EntryRows *global_remove_entry(Heap *heap, const EntryRows *entries, Str name);
Globals global_add(Heap *heap, Str name, Entry entry, Globals globals);
MaybeFamily global_find_family_entry(const FamilyRows *families, Str name);
MaybeFamily global_find_family(Str name, Globals globals);
const FamilyRows *global_remove_family(Heap *heap, const FamilyRows *families, Str name);
Globals global_add_family(Heap *heap, Str name, Family family, Globals globals);
const Term *global_entry_ty(Entry e);
MaybeDefEntry global_def_of(Entry e);
MaybeDefEntry global_find_def_result(MaybeEntry e);
MaybeDefEntry global_find_def(Str name, Globals globals);
MaybePrim global_find_prim_result(MaybeEntry e);
MaybePrim global_find_prim(Str name, Globals globals);
/* ps: List<Prim.T> as Prim pointers */
Globals global_add_prims(Heap *heap, const Vec *ps, Globals globals);
Globals global_initial(Heap *heap);
Index global_index_entry_rows(Heap *heap, const EntryRows *rows);
Index global_index_family_rows(Heap *heap, const FamilyRows *rows);
Globals global_of_lists(Heap *heap, const EntryRows *entries, const FamilyRows *families);
Globals global_without_family(Heap *heap, Str name, Globals globals);
bool global_same_rec_arg(MaybeNat a, MaybeNat b);
bool global_same_entry(Entry a, Entry b);
bool global_entry_kept(Entry entry, MaybeEntry found);
bool global_entries_kept(const EntryRows *entries, Globals later);
bool global_family_kept(Family family, MaybeFamily found);
bool global_families_kept(const FamilyRows *families, Globals later);
bool global_extended_by(Globals earlier, Globals later);


/* ---- eval (kernel/eval.bend) ---- */
/* Names: Bend <name> is eval_<name>, so the Bend `eval` is eval_eval and the Bend `eval_whnf` is
 * eval_eval_whnf. Result<Error.T, Value.T> and Result<Error.T, Term.T> are the generic Result with
 * a const Value * or const Term * payload; the other payload types get an inline result struct
 * (kind, value, error as in Result). Lists are Vecs of pointers in the Bend order. */
typedef struct {
  ResultKind kind;
  Shape value;
  Error error;
} ShapeResult;
typedef struct {
  ResultKind kind;
  VAddr value;
  Error error;
} VAddrResult;
typedef struct {
  ResultKind kind;
  Addr value;
  Error error;
} AddrResult;
typedef struct {
  ResultKind kind;
  Vec value;
  Error error;
} VecResult;
typedef struct {
  ResultKind kind;
  Leg value;
  Error error;
} LegResult;
typedef struct {
  ResultKind kind;
  MaybeMotive value;
  Error error;
} MaybeMotiveResult;
typedef struct {
  ResultKind kind;
  MaybeValue value;
  Error error;
} MaybeValueResult;
/* Maybe<List<A>> as a Vec of pointers. */
typedef struct {
  bool some;
  Vec value;
} MaybeVec;
typedef struct {
  ResultKind kind;
  MaybeVec value;
  Error error;
} MaybeVecResult;
/* Maybe<Term.Leg<Term.T>> of find_branch: a pointer into the matching Branch. */
typedef struct {
  bool some;
  const Leg *value;
} MaybeLegPtr;
typedef enum { READBACK_QUOTE, READBACK_NORMAL } Readback;

Result eval_local_value(Heap *heap, MaybeValue value, Nat index);
/* terms: Term pointers; env and the DONE payload: Value pointers. */
VecResult eval_eval_list(Heap *heap, const Vec *terms, Globals globals, Vec env);
ShapeResult eval_eval_shape(Heap *heap, Shape shape, Globals globals, Vec env);
VAddrResult eval_eval_addr(Heap *heap, Globals globals, Vec env, Addr addr);
/* legs: Leg pointers -> VLeg pointers. */
Vec eval_close_legs(Heap *heap, const Vec *legs, Vec env);
Result eval_open_closure(Heap *heap, Globals globals, Closure closure, Vec args);
Result eval_eval_global_entry(Heap *heap, MaybeEntry entry, Str name);
MaybeLevelVar eval_annotation_level(Result result);
Result eval_eval_whnf(Heap *heap, Globals globals, Vec env, const Term *term);
MaybeLevelVar eval_fill_level(Heap *heap, MaybeLevelVar level, Globals globals, Vec env, const Term *ty);
const Value *eval_annotate(Heap *heap, const Value *value, Globals globals, Vec env, const Term *ty);
Result eval_eval(Heap *heap, Globals globals, Vec env, const Term *term);
Result eval_stuck_out(Heap *heap, const Value *value, Shape shape, VAddr addr);
/* leg: Maybe<VLeg> as the VecGet of Bignum.at. */
Result eval_selected_leg(Heap *heap, VecGet leg, Globals globals);
Result eval_out_point(Heap *heap, const Value *value, Globals globals, Shape shape, VAddr addr);
Result eval_out_coll(Heap *heap, const Value *value, Globals globals, Shape shape, VAddr addr);
Result eval_out_value(Heap *heap, Globals globals, Shape shape, VAddr addr, const Value *value);
bool eval_branch_match(Addr addr, VAddr target);
/* branches: Branch pointers. */
MaybeLegPtr eval_find_branch(const Vec *branches, VAddr addr);
Result eval_stuck_elim(Heap *heap, const Value *value, StuckElim data);
Result eval_selected_branch(Heap *heap, MaybeLegPtr branch, Globals globals, Vec env, Vec args);
Result eval_elim_zk(Heap *heap, bool zk, Globals globals, Vec env, const Value *fibre, const Value *arg,
                    const Term *body);
Result eval_elim_point(Heap *heap, const Value *value, const Vec *branches, Globals globals, Vec env,
                       StuckElim data, bool zk);
Result eval_elim_named(Heap *heap, const Value *value, const Vec *branches, Globals globals, Vec env,
                       StuckElim data);
Result eval_elim_value(Heap *heap, Globals globals, StuckElim data, const Value *value);
/* spine: Spine pointers; acc and the result: Literal pointers. */
MaybeVec eval_spine_literals(Heap *heap, const Vec *spine, Vec acc);
bool eval_canonical_ctor(const Value *value);
MaybeVec eval_prepend_frame(Heap *heap, const Spine *frame, MaybeVec tail);
/* frames: Spine pointers in the application order (the reversed spine). */
MaybeVecResult eval_guarded_frames(Heap *heap, const Vec *frames, Nat index, Globals globals);
VecResult eval_force_frames(Heap *heap, const Vec *frames, Globals globals);
Result eval_unfold_guarded(Heap *heap, MaybeVec ready, Globals globals, const Term *body, const Value *value);
Result eval_unfold_entry(Heap *heap, MaybeDefEntry entry, Globals globals, const Vec *frames, const Value *value);
Result eval_whnf_neutral(Heap *heap, const Value *value, Globals globals);
Result eval_prim_after(Heap *heap, MaybeValue result, Globals globals, const Value *fallback);
Result eval_prim_args(Heap *heap, MaybeVec args, Prim p, Globals globals, const Value *fallback);
Result eval_whnf_global(Heap *heap, MaybeEntry entry, Globals globals, Str name, Vec spine);
Result eval_whnf_core(Heap *heap, Globals globals, const Value *value);
Result eval_replay(Heap *heap, Globals globals, const Value *value, const Vec *frames);
MaybeValueResult eval_unfold_step_entry(Heap *heap, MaybeDefEntry entry, Globals globals, const Vec *spine);
MaybeValueResult eval_unfold_step(Heap *heap, Globals globals, const Value *value);
Result eval_whnf_unfolded(Heap *heap, MaybeValue step, Globals globals, const Value *core);
Result eval_whnf(Heap *heap, Globals globals, const Value *value);
/* values: Value pointers -> Term pointers. */
VecResult eval_quote_list(Heap *heap, Readback mode, const Vec *values, Globals globals, Nat size);
ShapeResult eval_quote_shape(Heap *heap, Readback mode, Shape shape, Globals globals, Nat size);
Vec eval_fresh_vars(Heap *heap, Nat count, Nat size);
AddrResult eval_quote_addr(Heap *heap, Readback mode, Globals globals, Nat size, VAddr addr);
LegResult eval_quote_leg(Heap *heap, Readback mode, Globals globals, Nat size, VLeg leg);
/* legs: VLeg pointers -> Leg pointers. */
VecResult eval_quote_legs(Heap *heap, Readback mode, const Vec *legs, Globals globals, Nat size);
Nat eval_diagram_arity(Heap *heap, Shape shape);
const Term *eval_quote_annotate(Heap *heap, MaybeLevelVar level, const Term *body);
const Term *eval_build_former(Heap *heap, bool lan, Shape shape, const Term *body);
Result eval_quote_former(Heap *heap, Readback mode, Globals globals, Nat size, Shape shape, Closure clo,
                         MaybeLevelVar level, bool lan);
MaybeMotiveResult eval_quote_motive(Heap *heap, Readback mode, MaybeMotive motive, Globals globals, Vec env,
                                    Nat size);
/* branches: Branch pointers -> Branch pointers. */
VecResult eval_quote_branches(Heap *heap, Readback mode, const Vec *branches, Globals globals, Vec env,
                              Nat size);
Result eval_quote_frames(Heap *heap, Readback mode, const Vec *frames, Globals globals, Nat size,
                         const Term *head);
const Term *eval_quote_head(Heap *heap, Head head, Nat size);
Result eval_read_back(Heap *heap, Readback mode, Globals globals, Nat size, const Value *value);
Result eval_read_back_reduce(Heap *heap, Readback mode, Globals globals, const Value *value);
Result eval_read_back_value(Heap *heap, Readback mode, Globals globals, Nat size, const Value *value);
Result eval_quote(Heap *heap, Globals globals, Nat size, const Value *value);
Result eval_normalize(Heap *heap, Globals globals, Nat size, const Value *value);

/* ---- pp (kernel/pp.bend): the kernel term printer ---- */
/* names: List<String> as a Vec of Str pointers, index 0 = the innermost binder. */
Str pp_decimal_escape(Heap *heap, uint32_t n);
Str pp_escape_byte(Heap *heap, uint32_t n);
Str pp_escape_char(Heap *heap, uint32_t n);
Str pp_escaped(Heap *heap, Str s);
Str pp_literal(Heap *heap, Literal lit);
/* xs: List<String> as Str pointers. */
Str pp_join(Heap *heap, const Vec *xs, Str sep);
/* binders: Binder pointers; the result holds Str pointers. */
Vec pp_binder_names(Heap *heap, const Vec *binders);
/* List.append(List.reverse(added), names) */
Vec pp_push(Heap *heap, const Vec *names, const Vec *added);
/* terms: Term pointers; the result holds Str pointers. */
Vec pp_terms(Heap *heap, const Vec *terms, const Vec *names);
Str pp_shape(Heap *heap, const Vec *names, Shape shape);
Vec pp_under(Heap *heap, Shape shape, const Vec *names);
Str pp_addr(Heap *heap, const Vec *names, Addr addr);
Str pp_leg(Heap *heap, const Vec *names, const Leg *leg);
/* legs: Leg pointers; the result holds Str pointers. */
Vec pp_legs(Heap *heap, const Vec *legs, const Vec *names);
Str pp_motive(Heap *heap, MaybeMotive motive, const Vec *names);
/* branches: Branch pointers. */
Str pp_branches(Heap *heap, const Vec *branches, const Vec *names);
Str pp_nth_name(Heap *heap, MaybeStr result, Nat index);
Str pp_term(Heap *heap, const Vec *names, const Term *term);

/* ---- level_scope (kernel/level_scope.bend) ---- */
bool level_scope_valid_node(const Term *term, Nat arity);
/* todo: List<Term.T> as Term pointers; the Bend worklist recursion run as a loop. */
bool level_scope_valid_walk(Heap *heap, Vec todo, Nat arity);
/* Result<Error.T, Unit>: RESULT_DONE carries kernel_unit(). */
Result level_scope_check(Heap *heap, Nat arity, const Term *term);

/* ---- context (kernel/context.bend): the typing context ---- */
typedef struct {
  Str name;
  Quantity quantity;
  const Value *ty;
} Local;
typedef enum { LTREE_LEAF, LTREE_NODE } LTreeKind;
typedef struct LTree LTree;
struct LTree {
  LTreeKind kind;
  Local x;
  const LTree *l; /* LTREE_NODE */
  const LTree *r; /* LTREE_NODE */
};
/* Locals: LCons cells in the Bend order, NULL = LNil. */
typedef struct Locals Locals;
struct Locals {
  Nat w;
  const LTree *t;
  const Locals *rest;
};
typedef enum { LVIEW_NIL, LVIEW_CONS } LViewKind;
typedef struct {
  LViewKind kind;
  Local head;         /* LVIEW_CONS */
  const Locals *tail; /* LVIEW_CONS */
} LView;
typedef struct {
  bool some;
  Local value;
} MaybeLocal;
/* Ctx{globals, env, locals, size, budget, level_arity, levels_checked};
 * env: List<Value.T> as Value pointers, index 0 = the innermost binding. */
typedef struct {
  Globals globals;
  Vec env;
  const Locals *locals;
  Nat size;
  Budget budget;
  Nat level_arity;
  bool levels_checked;
} Ctx;

Local context_local(Str name, Quantity quantity, const Value *ty);
/* Tree nodes: NULL after a failed allocation (heap->oom set). */
const LTree *context_lleaf(Heap *heap, Local x);
const LTree *context_lnode(Heap *heap, Local x, const LTree *l, const LTree *r);
/* Cons cells; on allocation failure heap->oom is set and rest is returned. */
const Locals *context_lcons_cell(Heap *heap, Nat w, const LTree *t, const Locals *rest);
const Locals *context_lcons_pair(Heap *heap, bool same, Local x, Nat w1, const LTree *t1, Nat w2,
                                 const LTree *t2, const Locals *rest);
const Locals *context_lcons_one(Heap *heap, Local x, Nat w1, const LTree *t1, const Locals *rest);
const Locals *context_lcons(Heap *heap, Local x, const Locals *xs);
MaybeLocal context_ltree_side(Heap *heap, bool left, Nat half, const LTree *l, const LTree *r, Nat p);
MaybeLocal context_ltree_node(Heap *heap, bool root, Nat half, Local x, const LTree *l, const LTree *r,
                              Nat p);
MaybeLocal context_ltree_get(Heap *heap, Nat w, const LTree *t, Nat i);
MaybeLocal context_lget_step(Heap *heap, bool inside, Nat w, const LTree *t, const Locals *rest, Nat i);
MaybeLocal context_lget(Heap *heap, const Locals *xs, Nat i);
LView context_lview_tree(Heap *heap, Nat w, const LTree *t, const Locals *rest);
LView context_lview(Heap *heap, const Locals *xs);
/* List<Local> as Local pointers, in the Bend order. */
Vec context_ltree_list(Heap *heap, const LTree *t, const Vec *acc);
Vec context_to_list(Heap *heap, const Locals *xs);
const Locals *context_from_list(Heap *heap, const Vec *xs);
Ctx context_make(Heap *heap, Globals globals, Budget budget, Nat level_arity);
Globals context_globals_of(Ctx ctx);
Vec context_env_of(Ctx ctx);
Nat context_size_of(Ctx ctx);
/* locals: Local pointers; the result holds Str pointers. */
Vec context_names(Heap *heap, const Vec *locals);
Vec context_names_of(Heap *heap, Ctx ctx);
Ctx context_bind(Heap *heap, Str name, Quantity q, const Value *ty, Ctx ctx);
Ctx context_define(Heap *heap, Str name, Quantity q, const Value *ty, const Value *value, Ctx ctx);
bool context_readable(Quantity mode, Quantity q);

/* ---- rules (kernel/rules.bend): the shared typing rule helpers and the eta table ---- */
/* Result<Error.T, Positivity.Family> */
typedef struct {
  ResultKind kind;
  Family value;
  Error error;
} FamilyResult;
/* Result<Error.T, Bignum.T> */
typedef struct {
  ResultKind kind;
  Big value;
  Error error;
} BigResult;
typedef struct {
  bool ran;
  bool lan;
} EtaRow;
typedef struct {
  bool some;
  EtaRow value;
} MaybeEtaRow;
/* Common.Pair<String, EtaRow>: one row of the eta table. */
typedef struct {
  Str name;
  EtaRow eta;
} EtaEntry;

const Term *rules_arrow(Heap *heap, Quantity q, Str name, const Term *dom, const Term *cod);
/* items: Term pointers; the result holds Leg pointers. */
Vec rules_term_legs(Heap *heap, const Vec *items);
const Term *rules_diagram_of(Heap *heap, const Vec *items);
const Term *rules_sum_ty(Heap *heap, const Vec *items);
const Term *rules_prod_ty(Heap *heap, const Vec *items);
const Term *rules_unit_ty(Heap *heap, const LevelVar *level);
const Term *rules_unit_val(Heap *heap);
const Term *rules_bool_ty(Heap *heap);
const Value *rules_bool_value(Heap *heap, bool value);
/* DONE payload: VLeg pointers. */
VecResult rules_coll_legs(Heap *heap, const Value *value);
VecResult rules_coll_legs_of(Heap *heap, Ctx ctx, Closure clo);
/* leg: Maybe<VLeg> as the VecGet of Bignum.at; Result<Error.T, Value.T>. */
Result rules_coll_leg_ty_found(Heap *heap, VecGet leg, Ctx ctx);
Result rules_coll_leg_ty(Heap *heap, Ctx ctx, const Vec *legs, Big index);
/* legs: VLeg pointers; DONE payload: Value pointers. */
VecResult rules_force_legs(Heap *heap, const Vec *legs, Ctx ctx);
VecResult rules_mu_param_env(Heap *heap, Ctx ctx, Closure clo);
FamilyResult rules_family_ready(Family family);
FamilyResult rules_family_found(Heap *heap, MaybeFamily family, Str name);
FamilyResult rules_mu_family(Heap *heap, Ctx ctx, Str name);
/* acc and the result: Term pointers; the Bend tail recursion run as a loop. */
Vec rules_mpc_fields(Heap *heap, Cmp sign, Big count, const Vec *acc);
/* Result<Error.T, Term.T> */
Result rules_mpc_sub_count(Heap *heap, Cmp sign, Big count);
Result rules_mpc_sub_ty(Heap *heap, Big count);
Result rules_mpc_pred_ty(Heap *heap, Big count);
/* Result<Error.T, Unit>: RESULT_DONE carries kernel_unit(). */
Result rules_mpc_party_count(Big count);
Result rules_mpc_party_value(const Value *value);
Result rules_mpc_party(Heap *heap, Ctx ctx, const Value *value);
/* legs: VLeg pointers. */
Result rules_mpc_party_legs(Heap *heap, const Vec *legs, Ctx ctx);
BigResult rules_mpc_parties_value(Heap *heap, const Value *value, Ctx ctx);
BigResult rules_mpc_parties(Heap *heap, Ctx ctx, const Value *parties);
/* indices: Value pointers; Result<Error.T, Value.T>. */
Result rules_mu_result(Heap *heap, Ctx ctx, Motive motive, const Vec *indices, const Value *self);
Result rules_elim_result(Heap *heap, Ctx ctx, MaybeMotive motive, MaybeValue expected, const Value *self);
MaybeEtaRow rules_eta_of(Shape shape);
/* Shape pointers with const Term * payloads. */
Vec rules_samples(Heap *heap);
/* EtaEntry pointers. */
Vec rules_eta_entry(Heap *heap, MaybeEtaRow row, Str name);
Vec rules_eta_rows(Heap *heap, const Vec *shapes);
Vec rules_eta_table(Heap *heap);
/* rows: EtaEntry pointers; the result holds Str pointers. */
Vec rules_admitted_names(Heap *heap, const Vec *rows);
Vec rules_admitted(Heap *heap);
Vec rules_named_declared(Heap *heap);
Vec rules_named_present(Heap *heap);

/* ---- circuit (kernel/circuit.bend): the depth certificate of a kernel term ---- */
/* Result<String, Bignum.T>: a depth, or a plain message. */
typedef struct {
  ResultKind kind;
  Big value; /* RESULT_DONE */
  Str error; /* RESULT_FAIL */
} CircuitBigResult;
/* Maybe<Term.T> */
typedef struct {
  bool some;
  const Term *value;
} MaybeTermPtr;
/* Common.Pair<String, Term.T>: globals are Vecs of CircuitGlobal pointers. */
typedef struct {
  Str name;
  const Term *term;
} CircuitGlobal;
typedef enum { CIRCUIT_VD, CIRCUIT_VCLO } CircuitValueKind;
typedef struct {
  CircuitValueKind kind;
  Big depth; /* CIRCUIT_VD */
  Leg leg;   /* CIRCUIT_VCLO */
  Vec env;   /* CIRCUIT_VCLO: List<Value> as CircuitValue pointers */
  Vec seen;  /* CIRCUIT_VCLO: List<String> as Str pointers */
} CircuitValue;
typedef struct {
  bool some;
  CircuitValue value;
} MaybeCircuitValue;
/* Result<String, Value> */
typedef struct {
  ResultKind kind;
  CircuitValue value; /* RESULT_DONE */
  Str error;          /* RESULT_FAIL */
} CircuitValueResult;
/* Common.Pair<String, Bignum.T>: memo lists are Vecs of CircuitMemoRow pointers. */
typedef struct {
  Str name;
  Big value;
} CircuitMemoRow;
typedef struct {
  Big value;
  Vec memo;
} CircuitBoundAnswer;
/* Result<String, BoundAnswer> */
typedef struct {
  ResultKind kind;
  CircuitBoundAnswer value; /* RESULT_DONE */
  Str error;                /* RESULT_FAIL */
} CircuitBoundResult;
typedef enum { CIRCUIT_BTERM, CIRCUIT_BARGS, CIRCUIT_BGLOBAL } CircuitBoundTaskKind;
typedef struct {
  CircuitBoundTaskKind kind;
  const Term *term;  /* CIRCUIT_BTERM */
  Vec args;          /* CIRCUIT_BARGS: Term pointers */
  bool cycle;        /* CIRCUIT_BGLOBAL */
  MaybeBig cached;   /* CIRCUIT_BGLOBAL */
  MaybeTermPtr body; /* CIRCUIT_BGLOBAL */
  Str name;          /* CIRCUIT_BGLOBAL */
  Vec seen;          /* List<String> as Str pointers */
} CircuitBoundTask;
typedef struct {
  Nat depth;
  const Term *term;
} CircuitOccur;
/* Common.Pair<Term.T, List<Term.T>> of spine. */
typedef struct {
  const Term *head;
  Vec args;
} CircuitSpine;
typedef enum {
  CIRCUIT_QUERY_EVAL,
  CIRCUIT_QUERY_ARGS,
  CIRCUIT_QUERY_LEGS,
  CIRCUIT_QUERY_BRANCHES,
  CIRCUIT_QUERY_ADDRESS,
  CIRCUIT_QUERY_APPLY,
  CIRCUIT_QUERY_CAPP,
  CIRCUIT_QUERY_GLOBAL,
  CIRCUIT_QUERY_PRIMITIVE,
  CIRCUIT_QUERY_UNWRAP
} CircuitQueryKind;
typedef struct {
  CircuitQueryKind kind;
  const Term *term;   /* EVAL; PRIMITIVE head; CAPP spine head */
  Vec args;           /* ARGS, APPLY, CAPP (spine args), PRIMITIVE: Term pointers */
  Vec legs;           /* LEGS: Leg pointers */
  Vec branches;       /* BRANCHES: Branch pointers */
  Addr addr;          /* ADDRESS */
  CircuitValue value; /* APPLY, UNWRAP */
  bool cycle;         /* GLOBAL */
  Str name;           /* GLOBAL */
  MaybeTermPtr body;  /* GLOBAL */
  MaybeBig cost;      /* PRIMITIVE */
  Vec env;            /* all but GLOBAL and UNWRAP: CircuitValue pointers */
  Vec seen;           /* all but UNWRAP: Str pointers */
} CircuitQuery;
Str circuit_word(Heap *heap, Str head);
MaybeBig circuit_cost_case(bool yes);
MaybeBig circuit_cost(Str name);
Big circuit_choose(bool yes, Big a, Big b);
MaybeBig circuit_choose_maybe(bool yes, MaybeBig a, MaybeBig b);
Big circuit_max(Big a, Big b);
CircuitBigResult circuit_bound_result(bool ok, Big n);
CircuitBigResult circuit_bounded(Big n);
MaybeTermPtr circuit_lookup_hit(bool hit, const Term *value, MaybeTermPtr rest);
MaybeTermPtr circuit_lookup(const Vec *rows, Str name);
bool circuit_member(const Vec *xs, Str name);
CircuitValue circuit_vd(Big depth);
CircuitValue circuit_vclo(Leg leg, Vec env, Vec seen);
CircuitGlobal *circuit_global_new(Heap *heap, Str name, const Term *term);
Big circuit_vd_of(CircuitValue v);
CircuitValue circuit_nth_default(MaybeCircuitValue v);
Vec circuit_push(Heap *heap, size_t n, CircuitValue v, const Vec *env);
MaybeStr circuit_shape_error(Shape s);
CircuitValueResult circuit_type_result(MaybeStr e);
MaybeBig circuit_memo_lookup(const Vec *rows, Str name);
Big circuit_bound_value(CircuitBoundAnswer a);
Vec circuit_bound_memo(CircuitBoundAnswer a);
CircuitBoundTask circuit_bterm(const Term *term, Vec seen);
CircuitBoundTask circuit_bargs(Vec args, Vec seen);
CircuitBoundTask circuit_bglobal(bool cycle, MaybeBig cached, MaybeTermPtr body, Str name, Vec seen);
CircuitBoundResult circuit_literal_bound_go(Heap *heap, CircuitBoundTask task, const Vec *globals, Str family,
                                            Vec memo);
CircuitBigResult circuit_literal_bound(Heap *heap, CircuitBoundTask task, const Vec *globals, Str family);
/* The worklists are stacks (Bend list head LAST): the prepending helpers push onto `rest`. */
void circuit_addr_terms(Heap *heap, Addr a, Vec *rest);
void circuit_addr_occurs(Heap *heap, Addr a, Nat depth, Vec *rest);
void circuit_terms_occurs(Heap *heap, const Vec *ts, Nat depth, Vec *rest);
void circuit_legs_occurs(Heap *heap, const Vec *ls, Nat depth, Vec *rest);
void circuit_branches_occurs(Heap *heap, const Vec *bs, Nat depth, Vec *rest);
/* tasks and ts are in Bend list order (head first). */
bool circuit_reads_fields(Heap *heap, const Vec *tasks, Nat count);
bool circuit_branches_read(Heap *heap, const Vec *bs);
CircuitSpine circuit_spine(Heap *heap, const Term *t, Vec args);
void circuit_shape_terms(Heap *heap, Shape s, Vec *rest);
void circuit_legs_terms(Heap *heap, const Vec *ls, Vec *rest);
void circuit_branches_terms(Heap *heap, const Vec *bs, Vec *rest);
void circuit_motive_terms(Heap *heap, MaybeMotive m, Vec *rest);
bool circuit_names_family(Heap *heap, const Vec *ts, Str fam);
CircuitValueResult circuit_certificate_result(CircuitBigResult cert, bool recursive, CircuitValue v);
CircuitQuery circuit_global_query(Heap *heap, MaybeBig c, Str name, Vec seen, MaybeTermPtr body);
Big circuit_mu_depth(Heap *heap, bool read, Big n, Big best);
CircuitValueResult circuit_eval(Heap *heap, CircuitQuery query, const Vec *globals);
CircuitBigResult circuit_depth(Heap *heap, const Vec *globals, const Term *t);



/* ---- check_engine (kernel/check_engine.bend): the bidirectional type checker ---- */
/* Comp values: Inferred *, Former *, Point *, Usage *, Ctx *, Big *, const Value *, const Term *,
 * const LevelVar *, kernel_unit() and the kernel bool box. Ported through Bend line 600 (S1.9 hop
 * 22); the prototypes of the later definitions join this section as their hops port them. */
typedef struct {
  const Value *ty;
  Usage uses;
} Inferred;
typedef struct {
  const Value *whole;
  Shape shape;
  Closure diagram;
  MaybeLevelVar level;
} Former;
typedef struct {
  Quantity quantity;
  Str name;
  const Value *domain;
} Point;
/* Maybe<Value.T>: the expected type of a checked node. */
typedef struct {
  bool some;
  const Value *value;
} ExpectedValue;
/* Unit -> Error.T: the error is built only when the check fails. */
typedef struct {
  Error (*fn)(Heap *heap, void *ctx);
  void *ctx;
} ErrorThunk;

/* Bend lines 601-845 (S1.9 hop 23): the point and collection laws, the Mu views. */
typedef struct {
  Str name;
  Vec indices; /* Value pointers */
} Mu;
typedef struct {
  Vec env; /* Value pointers */
  Usage uses;
} EnvUses;
typedef struct {
  Ctx ctx;
  Vec env;    /* Value pointers */
  Vec values; /* Value pointers */
} Fields;
Comp check_engine_point_form(Heap *heap, Ctx ctx, bool left, Quantity q, Str name, const Term *dom,
                             const Term *diagram);
/* legs: Leg pointers; args: Term pointers. */
Comp check_engine_point_intro_sec(Heap *heap, Ctx ctx, Quantity mode, const Vec *legs, const Value *expected);
Comp check_engine_point_intro_in(Heap *heap, Ctx ctx, Quantity mode, Addr addr, const Vec *args,
                                 const Value *expected);
Comp check_engine_point_elim_out(Heap *heap, Ctx ctx, Quantity mode, Addr addr, const Term *head);
Comp check_engine_elim_result(Heap *heap, MaybeMotive motive, ExpectedValue expected, Ctx ctx, const Value *self);
Comp check_engine_check_motive(Heap *heap, MaybeMotive motive, Ctx ctx, const Value *scrut_ty);
MaybeMotive check_engine_elim_motive(const ElimData *data);
const Term *check_engine_elim_scrut(const ElimData *data);
const Vec *check_engine_elim_branches(const ElimData *data);
Comp check_engine_infer_scrutinee(Heap *heap, Ctx ctx, Quantity mode, const ElimData *data);
/* The ready value is the Leg pointer of the branch. */
Comp check_engine_first_branch_pair(Heap *heap, const Branch *branch);
Comp check_engine_point_branch(Heap *heap, Leg leg, Ctx ctx, Quantity mode, Former former, Point point,
                               MaybeMotive motive, ExpectedValue expected);
Comp check_engine_point_elim_elim(Heap *heap, Ctx ctx, Quantity mode, const ElimData *data, ExpectedValue expected);
/* legs: Leg pointers (list view by value, precedent scope_walk). */
Comp check_engine_coll_levels(Heap *heap, Vec legs, Ctx ctx, const LevelVar *level);
const LevelVar *check_engine_expected_level(MaybeLevelVar expected);
Comp check_engine_coll_form(Heap *heap, Ctx ctx, Big count, const Term *diagram, MaybeLevelVar expected);
/* dlegs: VLeg pointers. */
Comp check_engine_coll_check_legs(Heap *heap, Vec legs, Vec dlegs, Ctx ctx, Quantity mode);
Comp check_engine_coll_intro_sec(Heap *heap, Ctx ctx, Quantity mode, Big count, const Vec *legs,
                                 const Value *expected);
Comp check_engine_coll_intro_in(Heap *heap, Ctx ctx, Quantity mode, Addr addr, const Vec *args, const Value *expected);
Comp check_engine_coll_elim_out(Heap *heap, Ctx ctx, Quantity mode, Addr addr, const Term *head);
Comp check_engine_coll_branch(Heap *heap, Ctx ctx, Quantity mode, const ElimData *data, Shape shape, Vec dlegs,
                              ExpectedValue expected, Nat index);
Comp check_engine_coll_branches(Heap *heap, Nat count, Nat index, Ctx ctx, Quantity mode, const ElimData *data,
                                Shape shape, Vec dlegs, ExpectedValue expected, Usage uses);
Comp check_engine_coll_elim_elim(Heap *heap, Ctx ctx, Quantity mode, const ElimData *data, ExpectedValue expected);
Comp check_engine_mu_view(Heap *heap, Shape shape);
Str check_engine_mu_name(Mu value);
Vec check_engine_mu_index_values(Mu value);

/* Bend lines 846-1111 (S1.9 hop 24): the Positivity accessors and the mu laws up to mu_cover. */
Vec check_engine_fam_params(Family f);
Vec check_engine_fam_indices(Family f);
const LevelVar *check_engine_fam_level(Family f);
Vec check_engine_ctor_args(Ctor c);
Vec check_engine_ctor_indices(Ctor c);
Vec check_engine_env_values(EnvUses value);
Usage check_engine_env_uses(EnvUses value);
Ctx check_engine_fields_ctx(Fields value);
Vec check_engine_fields_env(Fields value);
Vec check_engine_fields_values(Fields value);
/* The ready value is a heap boxed Vec of Term pointers. */
Comp check_engine_params_of(Heap *heap, const Term *diagram);
/* tele: TeleBinder pointers; args: Term pointers; env: Value pointers (list views by value). Ready: EnvUses *. */
Comp check_engine_telescope_loop(Heap *heap, Vec tele, Vec args, Ctx ctx, Quantity mode, Vec env, Usage uses);
Comp check_engine_telescope_uses(Heap *heap, Ctx ctx, Quantity mode, Str what, const Vec *tele, const Vec *args,
                                 Vec env);
/* indices: Term pointers. */
Comp check_engine_mu_form(Heap *heap, Ctx ctx, Str name, const Vec *indices, const Term *diagram);
Comp check_engine_index_equal_result(Heap *heap, bool same, Ctx ctx, Str ctor, Str name, const Value *got,
                                     const Value *want);
/* tele: TeleBinder pointers; results: Term pointers; wants, env, index_env: Value pointers (list views by value). */
Comp check_engine_index_loop(Heap *heap, Vec tele, Vec results, Vec wants, Ctx ctx, Str name, Str ctor, Vec env,
                             Vec index_env);
Comp check_engine_mu_indices(Heap *heap, Ctx ctx, Str name, Str ctor, const Vec *tele, const Vec *results, Vec penv,
                             Vec wants, Vec env);
/* args: Term pointers. */
Comp check_engine_mu_intro_in(Heap *heap, Ctx ctx, Quantity mode, Addr addr, const Vec *args, const Value *expected);
Vec check_engine_motive_indices(Motive motive);
const Term *check_engine_motive_body(Motive motive);
Str check_engine_motive_self(Motive motive);
MaybeStr check_engine_motive_ind(Motive motive);
/* indices: Value pointers. The ready value is a heap boxed Motive. */
Comp check_engine_mu_motive_of(Heap *heap, Str name, Family fam, const Vec *indices, const ElimData *data);
/* indices: Value pointers. */
Comp check_engine_mu_result(Heap *heap, Ctx ctx, Motive motive, Vec indices, const Value *self);
/* tele: TeleBinder pointers; names: Str pointers; env, values: Value pointers (list views by value). Ready: Fields *. */
Comp check_engine_motive_bind_indices(Heap *heap, Vec tele, Vec names, Ctx ctx, Vec env, Vec values, Str family);
MaybeLevelVar check_engine_former_level(Former f);
/* penv: Value pointers. */
Comp check_engine_mu_motive_level(Heap *heap, Ctx ctx, Str name, Family fam, Motive motive, Former former, Vec penv);
/* tele: TeleBinder pointers (list view by value). */
bool check_engine_erased_fields(Vec tele);
bool check_engine_zero_ctor(MaybeCtor ctor);
bool check_engine_mu_zero_eliminable(Family fam);
Comp check_engine_mu_large(Heap *heap, Str name, Family fam, const LevelVar *level);
/* The ready value is a heap boxed Vec of Str pointers. */
Comp check_engine_mu_ctor_names(Heap *heap, Family fam, Str name);
/* branches: Branch pointers (list view by value). The ready value is a heap boxed Vec of Str pointers. */
Comp check_engine_branch_keys(Heap *heap, Vec branches, Str name);
/* keys: Str pointers (list view by value). */
Nat check_engine_count_name(Heap *heap, Vec keys, Str name);
Comp check_engine_cover_count(Heap *heap, Nat count, Str name, Str ctor);
/* names: Str pointers (list view by value); keys: Str pointers. */
Comp check_engine_cover_names(Heap *heap, Vec names, const Vec *keys, Str family);
/* keys: Str pointers (list view by value); names: Str pointers. */
Comp check_engine_cover_known(Heap *heap, Vec keys, const Vec *names, Str family);
/* names: Str pointers; branches: Branch pointers. */
Comp check_engine_mu_cover(Heap *heap, Str name, const Vec *names, const Vec *branches);

/* Bend lines 1113-1392 (S1.9 hop 26): the mu elimination, the circuit bridge, the zk and fhc laws. */
/* ProofArgs{fibre, uses}: the relation leg of a proof and the uses of its instance. */
typedef struct {
  const Term *fibre;
  Usage uses;
} ProofArgs;
/* Mpc{parties, access}: the party set value and the access value of a share former. */
typedef struct {
  const Value *parties;
  const Value *access;
} Mpc;
Comp check_engine_fhc_fragment(Heap *heap, Ctx ctx, const Value *ty);
/* tele: TeleBinder pointers; binders: Binder pointers; env, values: Value pointers (list views by value). */
Comp check_engine_branch_bind_fields(Heap *heap, Vec tele, Vec binders, Ctx ctx, Vec env, Vec values);
/* penv: Value pointers; branches: Branch pointers. */
Comp check_engine_mu_branch(Heap *heap, Ctx ctx, Quantity mode, Str name, Family fam, Motive motive, Vec penv,
                            const Vec *branches, Str ctor);
/* names: Str pointers (list view by value). */
Comp check_engine_mu_branches(Heap *heap, Vec names, Ctx ctx, Quantity mode, Str name, Family fam, Motive motive,
                              Vec penv, const Vec *branches, Usage uses);
Comp check_engine_mu_elim_elim(Heap *heap, Ctx ctx, Quantity mode, const ElimData *data);
/* The rows are CircuitGlobal pointers in entry order. */
Vec check_engine_definition_rows(Heap *heap, const EntryRows *entries);
Vec check_engine_globals_rows(Heap *heap, Globals globals);
Comp check_engine_circuit_result(Heap *heap, CircuitBigResult result);
Comp check_engine_circuit(Heap *heap, Ctx ctx, const Term *term);
Comp check_engine_zk_form(Heap *heap, Ctx ctx, Quantity q, Str name, const Term *dom, const Term *diagram);
/* args: Term pointers. */
Comp check_engine_proof_args(Heap *heap, Vec args, Ctx ctx, Quantity mode);
Comp check_engine_zk_prove(Heap *heap, ProofArgs args, Ctx ctx, Quantity mode, const Value *dom, Closure clo,
                           const Term *point);
Comp check_engine_zk_intro_in(Heap *heap, Ctx ctx, Quantity mode, Addr addr, const Vec *args, const Value *expected);
Comp check_engine_fhc_shape(Shape shape, Str message);
Comp check_engine_literal_level(Heap *heap, const Value *value);
Comp check_engine_fhc_level(Heap *heap, Ctx ctx, const Value *value);
/* legs: VLeg pointers (list view by value). */
Comp check_engine_fragment_legs(Heap *heap, Vec legs, Ctx ctx);
Comp check_engine_fragment_former(Heap *heap, Shape shape, Closure clo, Ctx ctx);
Comp check_engine_fragment_value(Heap *heap, const Value *value, Ctx ctx);
Comp check_engine_fhc_form(Heap *heap, Ctx ctx, const Term *level, const Term *diagram);
Comp check_engine_fhc_eval(Heap *heap, Ctx ctx, Quantity mode, Big out_level, const Value *out_ty, const Value *fty,
                           Usage f_uses, const Term *function, const Term *cipher);
Comp check_engine_fhc_const_pi(Heap *heap, Ctx ctx, const Value *dom, const Value *cod);
Comp check_engine_fhc_cipher_first(Heap *heap, Ctx ctx, Quantity mode, Big out_level, const Value *out_ty,
                                   const Term *function, const Term *cipher);
bool check_engine_is_function(const Value *value);
Comp check_engine_fhc_head_function(Heap *heap, bool is_fun, Ctx ctx, Quantity mode, Big out_level,
                                    const Value *out_ty, const Value *fw, Usage uses, const Term *first,
                                    const Term *second);
/* result: the boxed Result of a budget_attempt over infer_uses (an Inferred pointer when done). */
Comp check_engine_fhc_head_result(Heap *heap, const Result *result, Ctx ctx, Quantity mode, Big out_level,
                                  const Value *out_ty, const Term *first, const Term *second);
/* legs: Leg pointers. */
Comp check_engine_fhc_legs(Heap *heap, const Vec *legs, Ctx ctx, Quantity mode, Big out_level, const Value *out_ty);
Comp check_engine_fhc_intro_sec(Heap *heap, Ctx ctx, Quantity mode, const Vec *legs, const Value *expected);
Comp check_engine_fhc_elim_out(Heap *heap, Ctx ctx, Quantity mode, Addr addr, const Term *head);
/* Bend lines 1394-1642 (S1.9 hop 28): the mpc laws and the shape dispatch. */
Comp check_engine_mpc_view(Heap *heap, Shape shape, Str message);
const Value *check_engine_mpc_parties(Mpc value);
const Value *check_engine_mpc_access(Mpc value);
Comp check_engine_mpc_form(Heap *heap, Ctx ctx, const Term *parties, const Term *access, const Term *diagram);
Comp check_engine_mpc_share_ty(Heap *heap, Ctx ctx, const Value *pv, const Value *av, const Value *ty);
/* shares: Term pointers (list view by value). */
Comp check_engine_mpc_shares(Heap *heap, Vec shares, Ctx ctx, Quantity mode, Mpc mpc, const Value *fty);
/* shares: Term pointers (list view by value). */
Comp check_engine_mpc_join(Heap *heap, Ctx ctx, Quantity mode, Mpc mpc, const Value *out_ty, const Term *ps,
                           const Term *function, Vec shares);
/* legs: Leg pointers (list view by value); the result is a boxed Vec of Term pointers. */
Comp check_engine_mpc_args(Heap *heap, Vec legs);
/* args: Term pointers (list view by value). */
Comp check_engine_mpc_intro_args(Heap *heap, Vec args, Ctx ctx, Quantity mode, Mpc mpc, const Value *out_ty);
Comp check_engine_mpc_intro_sec(Heap *heap, Ctx ctx, Quantity mode, const Vec *legs, const Value *expected);
/* The result is a boxed Pair {qset, proof} of Term pointers. */
Comp check_engine_mpc_address(Heap *heap, const Term *point);
Comp check_engine_mpc_open_pair(Heap *heap, Pair pair, Ctx ctx, Quantity mode, const Term *head);
Comp check_engine_mpc_elim_out(Heap *heap, Ctx ctx, Quantity mode, Addr addr, const Term *head);
Comp check_engine_form_dispatch(Heap *heap, Shape shape, bool left, Ctx ctx, const Term *diagram,
                                MaybeLevelVar expected);
/* ---- conv (Bend 1645-2115): the conversion checker ---- */
/* ConvStep{argument: Maybe<Value.T>, next: Value.T}: one spine step of a neutral comparison. */
typedef struct {
  ExpectedValue argument;
  const Value *next;
} ConvStep;
const Value *check_engine_conv_placeholder(Heap *heap);
Comp check_engine_conv_and(bool flag, Comp next);
Comp check_engine_conv_supported(Shape s);
Ctx check_engine_conv_grow(Heap *heap, Nat n, Ctx ctx);
Vec check_engine_conv_fresh(Heap *heap, Nat n, Nat size);
Comp check_engine_conv_list(Heap *heap, Ctx ctx, Vec xs, Vec ys);
Comp check_engine_conv_shape(Heap *heap, Ctx ctx, Shape a, Shape b);
Comp check_engine_conv_at_type(Heap *heap, Ctx ctx, ExpectedValue ty, const Value *a, const Value *b);
Comp check_engine_conv_addr(Heap *heap, Ctx ctx, ExpectedValue ty, VAddr a, VAddr b);
Comp check_engine_conv_closure_same(Heap *heap, bool same, Ctx ctx, Nat arity, Closure x, Closure y);
Comp check_engine_conv_closures(Heap *heap, Ctx ctx, Nat arity, Closure x, Nat other, Closure y);
Comp check_engine_conv_legs(Heap *heap, Ctx ctx, Vec xs, Vec ys);
Comp check_engine_conv_branch_addr(Heap *heap, Ctx ctx, Vec env1, Vec env2, Addr a, Addr b);
Comp check_engine_conv_branches(Heap *heap, Ctx ctx, Vec env1, Vec env2, Vec bs1, Vec bs2);
Comp check_engine_conv_motives(Heap *heap, Ctx ctx, Vec env1, Vec env2, MaybeMotive m1, MaybeMotive m2);
Comp check_engine_conv_stuck(Heap *heap, Ctx ctx, StuckElim m1, StuckElim m2);
ConvStep check_engine_conv_step_default(Heap *heap);
ExpectedValue check_engine_conv_step_arg(ConvStep s);
const Value *check_engine_conv_step_next(ConvStep s);
/* Maybe<VLeg<Value.T>> as (some, leg). */
Comp check_engine_conv_spine_leg(Heap *heap, Ctx ctx, bool some, const VLeg *leg);
const Value *check_engine_conv_head_dom(Shape ws, const Value *dom);
Comp check_engine_conv_spine_former(Heap *heap, Ctx ctx, const Value *w, Shape s, VAddr addr);
Comp check_engine_conv_spine_step(Heap *heap, Ctx ctx, const Value *hty, Shape s, VAddr a);
Comp check_engine_conv_frame_shape(Heap *heap, Ctx ctx, Shape a, Shape b);
Comp check_engine_conv_spines(Heap *heap, Ctx ctx, const Value *hty, Vec xs, Vec ys);
const LevelVar *check_engine_conv_level_default(MaybeLevelVar u);
Comp check_engine_conv_annotation_width(Heap *heap, bool zero, MaybeLevelVar u1, MaybeLevelVar u2);
Comp check_engine_conv_annotations(Heap *heap, Shape s, MaybeLevelVar u1, MaybeLevelVar u2);
Comp check_engine_conv_diagram(Heap *heap, Ctx ctx, Shape s, Closure d1, Closure d2);
Comp check_engine_conv_former_annotations(Heap *heap, bool same, Ctx ctx, Shape s, Closure d1, MaybeLevelVar u1,
                                          Closure d2, MaybeLevelVar u2);
Comp check_engine_conv_formers(Heap *heap, Ctx ctx, Shape s1, Closure d1, MaybeLevelVar u1, Shape s2, Closure d2,
                               MaybeLevelVar u2);
Comp check_engine_conv_neutrals(Heap *heap, bool same, Ctx ctx, Head h, Vec sp1, Vec sp2);
Comp check_engine_conv_structural_whnf(Heap *heap, Ctx ctx, const Value *a, const Value *b);
const Value *check_engine_unfolded_or(ExpectedValue step, const Value *fallback);
Comp check_engine_conv_delta_unfold(Heap *heap, Ctx ctx, ExpectedValue ua, ExpectedValue ub, const Value *a,
                                    const Value *b);
Comp check_engine_conv_delta_retry(Heap *heap, bool same, Ctx ctx, const Value *a, const Value *b);
Comp check_engine_conv_structural(Heap *heap, Ctx ctx, const Value *a, const Value *b);
Comp check_engine_conv_prop_probe(Heap *heap, Ctx ctx, const Value *ty);
Comp check_engine_conv_prop_failure(Error error);
Comp check_engine_conv_is_prop(Heap *heap, Ctx ctx, const Value *ty);
bool check_engine_conv_fields_zero(Vec xs);
bool check_engine_conv_ctor_zero(MaybeCtor c);
bool check_engine_conv_family_zero(Family f);
bool check_engine_conv_family_probe(FamilyResult r);
Comp check_engine_conv_subsingleton(Heap *heap, Ctx ctx, const Value *w);
Comp check_engine_conv_out(Heap *heap, Ctx ctx, Shape s, VAddr a, const Value *v);
Vec check_engine_conv_proj_branches(Heap *heap, Quantity q, Nat which);
Comp check_engine_conv_project(Heap *heap, Ctx ctx, Shape s, Quantity q, Quantity scrut_q, MaybeMotive mo, Nat which,
                               const Value *v);
Comp check_engine_conv_proj_motive_body(Heap *heap, Nat which, Ctx ctx, Shape s, Closure d, Quantity q,
                                        const Value *dom);
/* Comp(Maybe<Term.Motive<Term.T>>): the ready value is a heap allocated MaybeMotive. */
Comp check_engine_conv_proj_motive(Heap *heap, Ctx ctx, Shape s, Closure d, Quantity q, Str n, const Value *dom,
                                   Nat which);
Comp check_engine_conv_eta_pair_second(Heap *heap, bool first, Ctx ctx, Shape s, Closure d, Quantity q, Str n,
                                       const Value *dom, const Value *a1, const Value *a, const Value *b);
Comp check_engine_conv_eta_pair(Heap *heap, Ctx ctx, Shape s, Closure d, Quantity q, Str n, const Value *dom,
                                const Value *a, const Value *b);
Comp check_engine_conv_eta_coll(Heap *heap, Cmp sign, Big n, Big index, Ctx ctx, Shape s, Vec legs, const Value *a,
                                const Value *b);
Comp check_engine_conv_eta_or_structural(Heap *heap, Ctx ctx, const Value *w, const Value *a, const Value *b);
Comp check_engine_conv_after_sub(Heap *heap, bool sub, Ctx ctx, const Value *w, const Value *a, const Value *b);
Comp check_engine_conv_after_prop(Heap *heap, bool prop, Ctx ctx, const Value *ty, const Value *a, const Value *b);
Comp check_engine_conv_quick(Heap *heap, Ctx ctx, const Value *a, const Value *b);
Comp check_engine_conv_after_quick(Heap *heap, bool quick, Ctx ctx, const Value *w, const Value *a, const Value *b);
Comp check_engine_conv_whnf_ty(Heap *heap, Ctx ctx, const Value *w, const Value *a, const Value *b);

/* The laws: mutually recursive entry points. */
Comp check_engine_infer_uses(Heap *heap, Ctx ctx, Quantity mode, const Term *term);
Comp check_engine_check_uses(Heap *heap, Ctx ctx, Quantity mode, const Term *term, const Value *expected);
Comp check_engine_infer_univ(Heap *heap, Ctx ctx, const Term *term);
Comp check_engine_conv(Heap *heap, Ctx ctx, const Value *ty, const Value *a, const Value *b);
Comp check_engine_conv_type(Heap *heap, Ctx ctx, const Value *a, const Value *b);
/* D-269a (host only, no Bend counterpart): the top-level conversion entries. On a compact-path
 * heap (conv_scratch) under an unlimited budget they run the query in a scratch arena (D-261a);
 * otherwise they are check_engine_conv and check_engine_conv_type. The value is a bool box. */
Comp check_engine_conv_entry(Heap *heap, Ctx ctx, const Value *ty, const Value *a, const Value *b);
Comp check_engine_conv_type_entry(Heap *heap, Ctx ctx, const Value *a, const Value *b);
Comp check_engine_form(Heap *heap, Ctx ctx, bool left, Shape shape, const Term *diagram, MaybeLevelVar expected);
Comp check_engine_intro_sec(Heap *heap, Ctx ctx, Quantity mode, Shape shape, const Vec *legs, const Value *expected);
Comp check_engine_intro_in(Heap *heap, Ctx ctx, Quantity mode, Shape shape, Addr addr, const Vec *args,
                           const Value *expected);
Comp check_engine_elim_out(Heap *heap, Ctx ctx, Quantity mode, Shape shape, Addr addr, const Term *head);
Comp check_engine_elim_elim(Heap *heap, Ctx ctx, Quantity mode, const ElimData *data, ExpectedValue expected);
/* todo: Term pointers. */
Comp check_engine_scope_walk(Heap *heap, Vec todo, Nat arity);
Comp check_engine_let_body(Heap *heap, Ctx ctx, Quantity mode, Str name, const Term *ty, const Term *value,
                           const Term *body, ExpectedValue expected);

Comp check_engine_require(bool valid, Error error);
/* Maybe<A> as (some, value). */
Comp check_engine_optional(bool some, void *value, Error error);
Comp check_engine_require_by(Heap *heap, bool valid, ErrorThunk error);
Comp check_engine_optional_by(Heap *heap, bool some, void *value, ErrorThunk error);
const Value *check_engine_inferred_ty(Inferred value);
Usage check_engine_inferred_uses(Inferred value);
Comp check_engine_eval(Heap *heap, Ctx ctx, const Term *term);
Comp check_engine_whnf(Heap *heap, Ctx ctx, const Value *value);
Comp check_engine_whnf_core(Heap *heap, Ctx ctx, const Value *value);
/* Comp(Maybe<Value.T>): the ready value is a heap allocated ExpectedValue. */
Comp check_engine_unfold_step(Heap *heap, Ctx ctx, const Value *value);
Comp check_engine_open(Heap *heap, Ctx ctx, Closure clo, Vec args);
Comp check_engine_quote(Heap *heap, Ctx ctx, const Value *value);
/* result: Result<Error.T, Term.T>; names: Str pointers. */
Str check_engine_pp_result(Heap *heap, Result result, const Vec *names);
Str check_engine_pp_value(Heap *heap, Ctx ctx, const Value *value);
Comp check_engine_check_scope_node(Heap *heap, const Term *term, Nat arity);
Comp check_engine_scope(Heap *heap, Ctx ctx, const Term *term);
bool check_engine_close_valid(bool is_outer, Quantity mode, Quantity q, bool affine, Nat level, Usage uses);
Comp check_engine_close_local(Heap *heap, bool valid, Str name, bool affine);
Comp check_engine_close_continue(Heap *heap, bool outside, const Locals *rest, Nat level, Nat outer, Quantity mode,
                                 bool affine, Usage uses, Str name, Quantity q);
Comp check_engine_close_loop(Heap *heap, LView locals, Nat size, Nat outer, Quantity mode, bool affine, Usage uses);
Comp check_engine_close(Heap *heap, Ctx ctx, Nat outer, Quantity mode, Usage uses, bool affine);
Comp check_engine_head_local(MaybeLocal value);
Comp check_engine_head_global(Heap *heap, MaybeEntry entry, Globals globals, Str name);
Comp check_engine_head_local_bounded(Heap *heap, bool in_bounds, const Locals *locals, Nat size, Nat level);
Comp check_engine_head_ty(Heap *heap, Ctx ctx, Head head);
Comp check_engine_universe_value(Heap *heap, const Value *value, Ctx ctx);
Comp check_engine_local_infer(Heap *heap, MaybeLocal local, Nat index, Nat size, Quantity mode);
Comp check_engine_infer_var(Heap *heap, Ctx ctx, Quantity mode, Nat index);
Comp check_engine_infer_ann(Heap *heap, Ctx ctx, Quantity mode, const Term *body, const Value *ty);
Comp check_engine_infer_node(Heap *heap, Ctx ctx, Quantity mode, const Term *term);
Comp check_engine_infer_scoped(Heap *heap, Ctx ctx, Quantity mode, const Term *term);
Comp check_engine_ensure_result(Heap *heap, bool same, Ctx ctx, const Value *got, const Value *expected);
Comp check_engine_ensure(Heap *heap, Ctx ctx, const Value *got, const Value *expected);
Comp check_engine_check_inferred(Heap *heap, Inferred got, Ctx ctx, const Value *expected);
Comp check_engine_former_equal_result(Heap *heap, bool same, const LevelVar *got, const LevelVar *expected);
Comp check_engine_check_former_equal(Heap *heap, const LevelVar *got, const LevelVar *expected);
Comp check_engine_check_former_level(Heap *heap, const LevelVar *level, Ctx ctx, bool left, Shape shape,
                                     const Term *diagram);
Comp check_engine_check_former_value(Heap *heap, const Value *expected, Ctx ctx, bool left, Shape shape,
                                     const Term *diagram);
Comp check_engine_check_former(Heap *heap, Ctx ctx, bool left, Shape shape, const Term *diagram,
                               const Value *expected);
Comp check_engine_check_node(Heap *heap, Ctx ctx, Quantity mode, const Term *term, const Value *expected);
bool check_engine_linear_used(Heap *heap, LView locals, Nat size, Usage uses);
bool check_engine_has_linear(Heap *heap, Ctx ctx, Usage uses);
Comp check_engine_let_finish(Heap *heap, Inferred result, Ctx inner, Nat size, Quantity mode, Usage def_uses,
                             bool linear);
Comp check_engine_let_inner(Heap *heap, ExpectedValue expected, Ctx inner, Quantity mode, const Term *body);
Comp check_engine_let_defined(Heap *heap, Ctx ctx, Quantity mode, Str name, const Value *ty, const Value *value,
                              const Term *body, ExpectedValue expected, Usage uses);
Comp check_engine_let_typed(Heap *heap, Ctx ctx, Quantity mode, Str name, const Value *ty, const Term *value,
                            const Term *body, ExpectedValue expected);
Comp check_engine_former_value(Heap *heap, const Value *value, bool left, Str message);
Comp check_engine_former_view(Heap *heap, Ctx ctx, bool left, const Value *value, Str message);
Shape check_engine_former_shape(Former f);
Closure check_engine_former_clo(Former f);
const Value *check_engine_former_whole(Former f);
Comp check_engine_view_point(Heap *heap, Shape shape);
Quantity check_engine_point_q(Point point);
Str check_engine_point_name(Point point);
const Value *check_engine_point_dom(Point point);
Comp check_engine_view_coll(Heap *heap, Shape shape);
/* items: the payload pointers of List<A>; the ready value is the one item. */
Comp check_engine_one(const Vec *items, Error error);
Comp check_engine_binder_quantity(Heap *heap, Str subject, Str source, Str name, Quantity marked, Quantity declared);
const Term *check_engine_leg_body(Leg leg);
/* Binder pointers. */
Vec check_engine_leg_binders(Leg leg);
Quantity check_engine_pair_q(Binder pair);
Str check_engine_pair_name(Binder pair);
Comp check_engine_point_arg(Addr addr, Str message);
Comp check_engine_leg_index(Heap *heap, Addr addr, Str message);

/* ---- order (kernel/order.bend): the structural recursion certificate ---- */
typedef enum { ORDER_RULE_STRUCTURAL } OrderRule;
typedef enum { ORDER_PRINCIPAL, ORDER_SMALLER, ORDER_OTHER } OrderStatus;
typedef struct {
  bool some;
  OrderStatus value;
} MaybeOrderStatus;
/* Step{scrut, ctor} */
typedef struct {
  Nat scrut;
  MaybeStr ctor;
} OrderStep;
/* Call{callee, chain}: chain is List<Step> as a Vec of OrderStep pointers, head first. */
typedef struct {
  Str callee;
  Vec chain;
} OrderCall;
/* Row{member, calls}: calls is List<Call> as a Vec of OrderCall pointers. */
typedef struct {
  Str member;
  Vec calls;
} OrderRow;
/* T = Certificate{group, arg, rows}: group is List<String> as a Vec of Str pointers, rows a Vec of
 * OrderRow pointers. */
typedef struct {
  Vec group;
  Nat arg;
  Vec rows;
} OrderCert;
typedef struct {
  bool some;
  OrderCert value;
} MaybeOrderCert;
/* Result<Error.T, Maybe<T>> */
typedef struct {
  ResultKind kind;
  MaybeOrderCert value;
  Error error;
} OrderCertResult;
/* Result<Error.T, Unit> */
typedef struct {
  ResultKind kind;
  Error error;
} OrderUnitResult;
/* Result<Error.T, Term.T> */
typedef struct {
  ResultKind kind;
  const Term *value;
  Error error;
} OrderTermResult;
/* Common.Pair<String, Term.T>: one member of a recursive group; the member list is a Vec of
 * OrderMember pointers. */
typedef struct {
  Str name;
  const Term *body;
} OrderMember;
typedef struct {
  Str name;
  Nat formals;
  const Term *inner;
} OrderPeeled;
/* Spine{head, args, shapes}: args Term pointers and shapes Shape pointers, innermost first. */
typedef struct {
  const Term *head;
  Vec args;
  Vec shapes;
} OrderSpine;
typedef enum { ORDER_TASK_VISIT, ORDER_TASK_CHECK_CALL, ORDER_TASK_REFUSE } OrderTaskKind;
/* Visit{status, chain, term} | CheckCall{valid, callee, chain} | Refuse{}; status is List<Status> as a
 * Vec of OrderStatus pointers, head first. */
typedef struct {
  OrderTaskKind kind;
  Vec status;        /* ORDER_TASK_VISIT */
  Vec chain;         /* ORDER_TASK_VISIT, ORDER_TASK_CHECK_CALL */
  const Term *term;  /* ORDER_TASK_VISIT */
  bool valid;        /* ORDER_TASK_CHECK_CALL */
  Str callee;        /* ORDER_TASK_CHECK_CALL */
} OrderTask;
typedef enum { ORDER_SEARCH, ORDER_CANDIDATE } OrderSearchKind;
typedef struct {
  OrderSearchKind kind;
  MaybeVec rows; /* ORDER_CANDIDATE */
  Nat remaining;
  Nat index;
} OrderSearch;

/* Host helpers: boxed list items; NULL after a failed allocation (heap->oom set). */
OrderStep *order_step_new(Heap *heap, Nat scrut, MaybeStr ctor);
OrderCall *order_call_new(Heap *heap, Str callee, Vec chain);
OrderRow *order_row_new(Heap *heap, Str member, Vec calls);
OrderCert order_certificate(Vec group, Nat arg, Vec rows);
OrderMember *order_member_new(Heap *heap, Str name, const Term *body);
OrderPeeled *order_peeled_new(Heap *heap, OrderPeeled peeled);
bool order_member(const Vec *xs, Str n);
bool order_mentions(Heap *heap, const Vec *group, const Term *t);
OrderPeeled order_peel(Heap *heap, const Term *t, Nat n, Str name);
OrderSpine order_spine(Heap *heap, const Term *t, Vec args, Vec shapes);
MaybeOrderStatus order_status_at(const Vec *st, Nat ix);
bool order_is_smaller(MaybeOrderStatus s);
bool order_is_principal_or_smaller(MaybeOrderStatus s);
/* arg: Maybe<Term.T> as a MaybePtr to a Term. */
bool order_guarded_arg(MaybePtr arg, const Vec *st);
bool order_guarded_call(const Vec *st, const Vec *args, Nat k);
/* n: the binder count (every Bend caller passes a list length). */
Vec order_under(Heap *heap, size_t n, OrderStatus status, const Vec *st);
Vec order_under_point_result(Heap *heap, MaybePtr dom, const Vec *st);
Vec order_under_point(Heap *heap, Shape s, const Vec *st);
/* The Task constructors; NULL after a failed allocation. */
OrderTask *order_visit(Heap *heap, Vec status, Vec chain, const Term *term);
OrderTask *order_check_call(Heap *heap, bool valid, Str callee, Vec chain);
OrderTask *order_refuse(Heap *heap);
/* The task builders append to out in the Bend list order (the Bend `rest` is the caller's tail). */
void order_term_tasks(Heap *heap, const Vec *ts, Vec st, Vec chain, Vec *out);
void order_shape_tasks(Heap *heap, Shape s, Vec st, Vec chain, Vec *out);
void order_shapes_tasks(Heap *heap, const Vec *ss, Vec st, Vec chain, Vec *out);
void order_addr_tasks(Heap *heap, Addr a, Vec st, Vec chain, Vec *out);
void order_leg_tasks(Heap *heap, const Vec *ls, Vec st, Vec chain, Vec *out);
void order_spine_tasks(Heap *heap, OrderSpine sp, Vec st, Vec chain, const Vec *group, Nat k, Vec *out);
MaybeNat order_scrut_var(const Term *t);
bool order_special_scrut(MaybeNat v, const Vec *st);
Vec order_branch_chain(Heap *heap, MaybeNat v, bool special, Addr a, Vec chain);
void order_branch_tasks(Heap *heap, const Vec *bs, Vec st, Vec chain, MaybeNat v, bool special, Vec *out);
void order_motive_tasks(Heap *heap, MaybeMotive m, Vec st, Vec chain, Vec *out);
void order_elim_tasks(Heap *heap, const ElimData *e, Vec st, Vec chain, Vec *out);
/* tasks: OrderTask pointers in list order; the result is Maybe<List<Call>> as OrderCall pointers. */
MaybeVec order_walk(Heap *heap, Vec tasks, const Vec *group, Nat k, Vec calls);
Vec order_seed(Heap *heap, Nat n, Nat index, Nat formals, Nat k);
MaybeVec order_passes(Heap *heap, OrderRule rule, const Vec *group, Nat k, Nat formals, const Term *body);
/* ms: OrderMember pointers; the result is a Vec of Str pointers. */
Vec order_names(Heap *heap, const Vec *ms);
/* ms: OrderMember pointers; the result is a Vec of OrderPeeled pointers. */
Vec order_peeled(Heap *heap, const Vec *ms);
bool order_any_mentions(Heap *heap, const Vec *ms, const Vec *group);
MaybeNat order_min_bound(MaybeNat a, Nat n);
/* ms: OrderPeeled pointers. */
MaybeNat order_bound(Heap *heap, const Vec *ms, const Vec *group, MaybeNat acc);
Nat order_bound_default(MaybeNat b);
MaybeVec order_row_join(Heap *heap, MaybeVec cs, MaybeVec rs, Str n);
/* ms: OrderPeeled pointers; the result is Maybe<List<Row>>. */
MaybeVec order_rows_at(Heap *heap, const Vec *ms, const Vec *group, Nat k);
Error order_refusal(const Vec *ms);
OrderSearch order_search(Nat remaining, Nat index);
OrderSearch order_candidate(MaybeVec rows, Nat remaining, Nat index);
OrderCertResult order_first_fit(Heap *heap, OrderSearch s, const Vec *ps, const Vec *group, Error error);
OrderCertResult order_certify_used(Heap *heap, bool used, const Vec *ms);
OrderCertResult order_certify(Heap *heap, const Vec *members);
Nat order_argument(OrderCert cert);
Str order_one_elim_msg(void);
Str order_motive_msg(void);
Str order_ctor_key_msg(void);
OrderUnitResult order_valid_calls(const Vec *cs, Str name);
OrderUnitResult order_valid_rows(const Vec *rs);
OrderUnitResult order_valid_branches(const Vec *bs);
OrderUnitResult order_valid_motive(MaybeMotive m, const Vec *bs, const Vec *rows);
OrderUnitResult order_valid_elim(Heap *heap, const ElimData *e, Nat formals, Nat k, const Vec *rows);
OrderUnitResult order_translate_inner(Heap *heap, OrderPeeled p, Nat k, const Vec *rows);
OrderTermResult order_translate_used(Heap *heap, bool used, OrderCert cert, const Term *body);
OrderTermResult order_translate(Heap *heap, OrderCert cert, const Term *body);

/* ---- totality (kernel/totality.bend): the budgeted termination guard ---- */
Str totality_budget_msg(void);
/* terms: Term pointers, the worklist head first; Result<Error.T, Budget.T> with the budget boxed. */
Result totality_spend(Heap *heap, MaybeBudget state, Vec terms);
/* ms: OrderMember pointers; the result is a Vec of Term pointers. */
Vec totality_bodies(Heap *heap, const Vec *ms);
OrderCertResult totality_guard_group(Heap *heap, Budget budget, Globals globals, const Vec *members);
MaybeNat totality_guarded_argument(MaybeOrderCert cert);
/* Result<Error.T, Maybe<Nat>> */
typedef struct {
  ResultKind kind;
  MaybeNat value;
  Error error;
} MaybeNatResult;
MaybeNatResult totality_guard(Heap *heap, Budget budget, Globals globals, Str name, const Term *ty, const Term *body);
Comp totality_tick_comp(Heap *heap);
Comp totality_spend_comp(Heap *heap, Vec terms);
/* Comp(Maybe<Order.T>): the ready value is a heap allocated MaybeOrderCert. */
Comp totality_guard_group_comp(Heap *heap, Globals globals, const Vec *members);
/* Comp(Maybe<Nat>): the ready value is a heap allocated MaybeNat. */
Comp totality_guard_comp(Heap *heap, Globals globals, Str name, const Term *ty, const Term *body);

/* ---- conv (kernel/conv.bend) ---- */
/* Result<Error.T, Bool> */
typedef struct {
  ResultKind kind;
  bool value;  /* RESULT_DONE */
  Error error; /* RESULT_FAIL */
} BoolResult;
Budget conv_budget_of(Ctx ctx);
BoolResult conv_conv(Heap *heap, Ctx ctx, const Value *ty, const Value *a, const Value *b);
BoolResult conv_conv_type(Heap *heap, Ctx ctx, const Value *a, const Value *b);

/* ---- check (kernel/check.bend) ---- */
/* Comp values: Entry *, Ctx *, Globals *, Ctor *, Vec * (List<Ctor> as Ctor pointers, List<Pair<String, Entry>> as
 * CheckRow pointers), kernel_unit() and the kernel bool box. Results carry the same payloads; Result<Error.T, Value.T>
 * a const Value *, Result<Error.T, Level.T> a const LevelVar *, Result<Error.T, Inferred> an Inferred *. */
typedef enum { DECL_DEFINITION, DECL_POSTULATE } DeclKind;
/* Decl{name, kind, ty, body} */
typedef struct {
  Str name;
  DeclKind kind;
  const Term *ty;
  MaybeTermPtr body;
} Decl;
/* FamilyDecl{name, params, indices, level}: params and indices are Telescopes (TeleBinder pointers). */
typedef struct {
  Str name;
  Vec params;
  Vec indices;
  const LevelVar *level;
} FamilyDecl;
/* CtorDecl{name, args, res_params, res_idx}: args a Telescope, res_params and res_idx Term pointers. */
typedef struct {
  Str name;
  Vec args;
  Vec res_params;
  Vec res_idx;
} CtorDecl;
/* Common.Pair<String, Global.Entry> */
typedef struct {
  Str name;
  Entry entry;
} CheckRow;
/* Local and Ctx are Context.Local and Context.Ctx. */
Ctx check_make(Heap *heap, Globals globals, Budget budget, Nat level_arity);
Globals check_globals_of(Ctx ctx);
Vec check_env_of(Ctx ctx);
Nat check_size_of(Ctx ctx);
/* locals: Local pointers (context_to_list); the result holds Str pointers into them. */
Vec check_names(Heap *heap, const Vec *locals);
Vec check_names_of(Heap *heap, Ctx ctx);
MaybeNat check_name_index(Heap *heap, LView view, Str name, Nat index);
MaybeNat check_index_of_name(Heap *heap, Ctx ctx, Str name);
Ctx check_bind(Heap *heap, Str name, Quantity q, const Value *ty, Ctx ctx);
Ctx check_define(Heap *heap, Str name, Quantity q, const Value *ty, const Value *value, Ctx ctx);
bool check_readable(Quantity mode, Quantity q);
Budget check_context_budget(Ctx ctx);
Result check_infer_result(Result result);
Result check_infer(Heap *heap, Ctx ctx, Quantity mode, const Term *term);
Result check_infer_node(Heap *heap, Ctx ctx, Quantity mode, const Term *term);
Result check_check_result(Result result);
Result check_check(Heap *heap, Ctx ctx, Quantity mode, const Term *term, const Value *expected);
Result check_infer_univ(Heap *heap, Ctx ctx, const Term *term);
Comp check_type_valued(Heap *heap, Globals globals, Nat size, const Value *value);
Comp check_type_valued_node(Heap *heap, Globals globals, Nat size, const Value *value);
Entry check_checked_entry(const Term *ty, const Term *body);
Comp check_check_definition(Heap *heap, Ctx ctx, Str name, const Term *ty, MaybeTermPtr body, const Value *tyv);
Comp check_check_decl_kind(Heap *heap, DeclKind kind, Ctx ctx, Str name, const Term *ty, MaybeTermPtr body,
                           const Value *tyv);
Comp check_check_decl_comp(Heap *heap, Globals globals, Nat arity, Decl decl);
Result check_check_decl_at(Heap *heap, Globals globals, Budget budget, Nat arity, Decl decl);
Result check_check_decl(Heap *heap, Globals globals, Budget budget, Decl decl);
Comp check_check_scheme_comp(Heap *heap, Globals globals, Nat arity, Decl decl);
Result check_check_scheme(Heap *heap, Globals globals, Budget budget, Nat arity, Decl decl);
Str check_decl_name(Decl decl);
bool check_absent_entry(MaybeEntry value);
/* decls: Decl pointers. */
Comp check_check_decls_comp(Heap *heap, Globals globals, Vec decls);
Result check_check_decls(Heap *heap, Globals globals, Budget budget, Vec decls);
Comp check_check_telescope(Heap *heap, Vec tele, Ctx ctx);
Comp check_index_rules(Heap *heap, Ctx ctx, Str family, const LevelVar *level, const TeleBinder *binder);
Comp check_check_index_telescope(Heap *heap, Vec tele, Ctx ctx, Str name, const LevelVar *level);
bool check_absent_family(MaybeFamily value);
Comp check_declare_family_comp(Heap *heap, Globals globals, Nat arity, FamilyDecl decl);
Result check_declare_family_at(Heap *heap, Globals globals, Budget budget, Nat arity, FamilyDecl decl);
Result check_declare_family(Heap *heap, Globals globals, Budget budget, FamilyDecl decl);
bool check_parameter_at(const Term *term, Nat index);
bool check_parameters_at(Heap *heap, const Vec *terms, Nat index);
Comp check_ctor_fields(Heap *heap, Vec tele, Ctx ctx, Str name, const LevelVar *level);
/* group: List<String> as Str pointers. */
Comp check_check_ctor_comp(Heap *heap, Ctx ctx, Family family, const Vec *group, CtorDecl decl);
bool check_provisional(Family family);
/* ctors: CtorDecl pointers. */
Comp check_check_ctors_comp(Heap *heap, Vec ctors, Ctx ctx, Family family, const Vec *group);
Vec check_ctor_names(Heap *heap, const Vec *ctors);
Family check_completed(Heap *heap, Family family, Vec ctors);
Comp check_define_ctors_comp(Heap *heap, Globals globals, Nat arity, const Vec *group, Str name, Vec ctors);
Result check_define_ctors_at(Heap *heap, Globals globals, Budget budget, Nat arity, const Vec *group, Str name,
                             Vec ctors);
Result check_define_ctors(Heap *heap, Globals globals, Budget budget, const Vec *group, Str name, Vec ctors);
Str check_family_decl_name(FamilyDecl decl);
const LevelVar *check_family_decl_level(FamilyDecl decl);
Comp check_check_family_scheme_comp(Heap *heap, Globals globals, Nat arity, FamilyDecl decl, Vec ctors);
Result check_check_family_scheme(Heap *heap, Globals globals, Budget budget, Nat arity, FamilyDecl decl, Vec ctors);
Result check_infer_term(Heap *heap, Globals globals, Budget budget, const Term *term);
Result check_check_term(Heap *heap, Globals globals, Budget budget, const Term *term, const Value *ty);

/* ---- eterm (kernel/eterm.bend): the erased kernel term, its declarations and printers ---- */
typedef struct {
  Str name;
} Tid;
typedef struct {
  Str name;
} Fid;
typedef enum { REPR_I31, REPR_STRUCT, REPR_UNION, REPR_FUNC, REPR_THUNK } ReprKind;
typedef struct {
  ReprKind kind;
  Tid tid; /* REPR_STRUCT, REPR_UNION, REPR_FUNC, REPR_THUNK */
} Repr;
typedef struct Ktm Ktm;
/* Branch<A> is only instantiated at A = Ktm across  and the name Branch belongs to the term
 * eliminator branch above, so Branch<Ktm> is KtmBranch. */
typedef struct {
  Nat tag;
  Nat arity;
  const Ktm *body;
} KtmBranch;
typedef enum {
  KTM_VAR,
  KTM_LIT,
  KTM_GLOBAL,
  KTM_ERASED,
  KTM_LET,
  KTM_CLOS,
  KTM_APP,
  KTM_TAIL,
  KTM_STRUCT,
  KTM_PROJ,
  KTM_TAG,
  KTM_CASE,
  KTM_DELAY,
  KTM_FORCE
} KtmKind;
struct Ktm {
  KtmKind kind;
  Nat index;        /* KTM_VAR, KTM_PROJ */
  Literal lit;      /* KTM_LIT: value */
  Str name;         /* KTM_GLOBAL, KTM_LET */
  const Ktm *value; /* KTM_LET, KTM_PROJ, KTM_FORCE */
  const Ktm *body;  /* KTM_LET */
  Fid fid;          /* KTM_CLOS, KTM_DELAY */
  Nat arity;        /* KTM_CLOS */
  Vec captures;     /* KTM_CLOS, KTM_DELAY: List<Ktm> as Ktm pointers */
  const Ktm *head;  /* KTM_APP, KTM_TAIL */
  Vec args;         /* KTM_APP, KTM_TAIL: List<Ktm> as Ktm pointers */
  Tid tid;          /* KTM_STRUCT, KTM_PROJ, KTM_TAG, KTM_CASE */
  Vec fields;       /* KTM_STRUCT, KTM_TAG: List<Ktm> as Ktm pointers */
  Nat tag;          /* KTM_TAG */
  const Ktm *scrut; /* KTM_CASE */
  Vec branches;     /* KTM_CASE: List<Branch<Ktm>> as KtmBranch pointers */
};
typedef enum { KDECL_FUN, KDECL_REC } KdeclKind;
typedef struct {
  KdeclKind kind;
  Fid fid;         /* KDECL_FUN */
  Vec params;      /* KDECL_FUN: List<Repr> as Repr pointers */
  Repr result;     /* KDECL_FUN */
  const Ktm *body; /* KDECL_FUN */
  Vec tids;        /* KDECL_REC: List<Tid> as Tid pointers */
} Kdecl;
typedef enum { PRINT_TASK_TEXT, PRINT_TASK_TERM, PRINT_TASK_TERMS, PRINT_TASK_BRANCHES } PrintTaskKind;
typedef struct {
  PrintTaskKind kind;
  Str text;        /* PRINT_TASK_TEXT */
  const Ktm *term; /* PRINT_TASK_TERM */
  Vec terms;       /* PRINT_TASK_TERMS: Ktm pointers */
  Vec branches;    /* PRINT_TASK_BRANCHES: KtmBranch pointers */
} PrintTask;
/* The constructors Tid, Fid, RI31 .. RThunk, Branch, KFun and KRec. */
Tid eterm_tid(Str name);
Fid eterm_fid(Str name);
Repr eterm_ri31(void);
Repr eterm_rstruct(Tid tid);
Repr eterm_runion(Tid tid);
Repr eterm_rfunc(Tid tid);
Repr eterm_rthunk(Tid tid);
KtmBranch eterm_branch(Nat tag, Nat arity, const Ktm *body);
Kdecl eterm_kfun(Fid fid, Vec params, Repr result, const Ktm *body);
Kdecl eterm_krec(Vec tids);
/* Host helpers: list items boxed for a Vec; NULL after a failed allocation (heap->oom set). */
Tid *eterm_tid_new(Heap *heap, Str name);
Repr *eterm_repr_new(Heap *heap, Repr repr);
KtmBranch *eterm_branch_new(Heap *heap, Nat tag, Nat arity, const Ktm *body);
Kdecl *eterm_kdecl_new(Heap *heap, Kdecl decl);
/* The constructors KVar .. KForce; the allocating ones return KErased after a failed allocation
 * (heap->oom set). */
const Ktm *eterm_kvar(Heap *heap, Nat index);
const Ktm *eterm_klit(Heap *heap, Literal value);
const Ktm *eterm_kglobal(Heap *heap, Str name);
const Ktm *eterm_kerased(void);
const Ktm *eterm_klet(Heap *heap, Str name, const Ktm *value, const Ktm *body);
const Ktm *eterm_kclos(Heap *heap, Fid fid, Nat arity, Vec captures);
const Ktm *eterm_kapp(Heap *heap, const Ktm *head, Vec args);
const Ktm *eterm_ktail(Heap *heap, const Ktm *head, Vec args);
const Ktm *eterm_kstruct(Heap *heap, Tid tid, Vec fields);
const Ktm *eterm_kproj(Heap *heap, Tid tid, Nat index, const Ktm *value);
const Ktm *eterm_ktag(Heap *heap, Tid tid, Nat tag, Vec fields);
const Ktm *eterm_kcase(Heap *heap, Tid tid, const Ktm *scrut, Vec branches);
const Ktm *eterm_kdelay(Heap *heap, Fid fid, Vec captures);
const Ktm *eterm_kforce(Heap *heap, const Ktm *value);
Str eterm_tid_text(Tid t);
Str eterm_fid_text(Fid f);
Str eterm_decimal_escape(Heap *heap, uint32_t n);
Str eterm_ascii_escape(Heap *heap, bool printable, uint32_t n);
Str eterm_unicode_escape_four(Heap *heap, bool four, uint32_t n);
Str eterm_unicode_escape_two(Heap *heap, bool two, uint32_t n);
Str eterm_unicode_escape(Heap *heap, bool ascii, uint32_t n);
/* c: one code point. */
Str eterm_escape_char(Heap *heap, uint32_t c);
/* Bend walks the code points of String.to_list(s); the host walks the UTF-8 bytes (the S1.7 pp
 * precedent): a byte below 128 is escape_char of itself, a byte at or above 128 is decimal_escape of
 * itself, which is what escape_char yields per UTF-8 byte of a code point at or above 128. */
Str eterm_escape(Heap *heap, Str s);
Str eterm_literal_text(Heap *heap, Literal l);
Str eterm_print_repr(Heap *heap, Repr r);
/* The tasks of one term, PrintTask pointers in order. */
Vec eterm_expand(Heap *heap, const Ktm *t);
/* tasks: PrintTask pointers. */
Str eterm_print_tasks(Heap *heap, const Vec *tasks);
Str eterm_print_ktm(Heap *heap, const Ktm *t);
/* rs: Repr pointers. */
Str eterm_repr_list(Heap *heap, const Vec *rs);
/* ts: Tid pointers. */
Str eterm_tid_list(Heap *heap, const Vec *ts);
Str eterm_print_decl(Heap *heap, Kdecl d);

/* ---- erase_repr (kernel/erase_repr.bend): the erasure context and the runtime representation of a type ---- */
typedef enum { SLOT_DROP, SLOT_KEEP, SLOT_EXTRA } Slot;
/* Ctx{check, slots, self}: the name Ctx belongs to the typing context above, so erase_repr's Ctx
 * (R.Ctx in erase_util and erase) is EraseCtx. slots: List<Slot> as Slot box pointers
 * (erase_repr_slot_box), index 0 = the innermost binding. */
typedef struct {
  Ctx check;
  Vec slots;
  Str self;
} EraseCtx;
typedef struct {
  bool some;
  Repr value;
} MaybeRepr;
/* Comp values: EraseCtx *, const Value *, Nat *, Vec * (Str, Value, MaybeRepr or Tid pointers, or kernel bool
 * boxes), Str *, Repr *, Tid *, MaybeRepr *, Pair * (first = EraseCtx *, second = const Value *) and the
 * kernel bool box. */
void *erase_repr_slot_box(Slot slot);
Slot erase_repr_slot_unbox(const void *boxed);
EraseCtx erase_repr_ctx(Ctx check, Vec slots, Str self);
Ctx erase_repr_checker(EraseCtx ec);
Vec erase_repr_slots(EraseCtx ec);
Str erase_repr_self(EraseCtx ec);
Globals erase_repr_globals(EraseCtx ec);
Nat erase_repr_size(EraseCtx ec);
Vec erase_repr_env(EraseCtx ec);
EraseCtx erase_repr_bind(Heap *heap, EraseCtx ec, Str name, Quantity q, const Value *ty, Slot slot);
EraseCtx erase_repr_define(Heap *heap, EraseCtx ec, Str name, Quantity q, const Value *ty, const Value *value,
                           Slot slot);
EraseCtx erase_repr_extra(Heap *heap, EraseCtx ec);
EraseCtx erase_repr_replace_slots(EraseCtx ec, Vec slots);
EraseCtx erase_repr_fresh(Heap *heap, EraseCtx ec);
Slot erase_repr_keep_slot(bool keep);
Comp erase_repr_eval(Heap *heap, EraseCtx ec, const Term *term);
/* env: Value pointers. */
Comp erase_repr_eval_env(Heap *heap, EraseCtx ec, Vec env, const Term *term);
Comp erase_repr_whnf(Heap *heap, EraseCtx ec, const Value *value);
/* args: Value pointers. */
Comp erase_repr_open(Heap *heap, EraseCtx ec, Closure clo, Vec args);
Comp erase_repr_infer(Heap *heap, EraseCtx ec, const Term *term);
Comp erase_repr_proof_free(Heap *heap, EraseCtx ec, const Value *value);
Comp erase_repr_runtime_collection(Heap *heap, bool empty, EraseCtx ec, const Value *value);
Comp erase_repr_runtime_node(Heap *heap, EraseCtx ec, const Value *value);
Comp erase_repr_runtime_ty(Heap *heap, EraseCtx ec, const Value *ty);
Comp erase_repr_point_runtime(Heap *heap, EraseCtx ec, Quantity q, const Value *dom);
Repr erase_repr_any_repr(void);
Tid erase_repr_mu_tid(Heap *heap, Str name);
/* rs: Repr pointers. */
Str erase_repr_repr_commas(Heap *heap, const Vec *rs);
Tid erase_repr_mu_leg_tid(Heap *heap, Str name, Nat index, const Vec *rs);
/* texts: Str pointers. */
Str erase_repr_join(Heap *heap, const Vec *texts, Str sep);
/* The laws. */
Comp erase_repr_repr_of(Heap *heap, EraseCtx ec, const Value *value);
Comp erase_repr_arity_of(Heap *heap, EraseCtx ec, Nat n, Shape shape, Closure clo);
/* Comp(Pair<Ctx, Value.T>): a Pair with first = EraseCtx *, second = const Value *. */
Comp erase_repr_under_point(Heap *heap, EraseCtx ec, Str name, Quantity q, const Value *dom, Closure clo);
Comp erase_repr_repr_text(Heap *heap, EraseCtx ec, bool keep, const Value *ty);
Comp erase_repr_arity_rest(Heap *heap, EraseCtx ec, Nat n, const Value *value);
Comp erase_repr_arity_open(Heap *heap, const Pair *pair, Nat n, bool keep);
Comp erase_repr_arity_point(Heap *heap, EraseCtx ec, Nat n, Quantity q, Str name, const Value *dom, Closure clo);
/* first: Str pointers. */
Comp erase_repr_pair_second(Heap *heap, const Pair *pair, Vec first);
Comp erase_repr_pair_texts(Heap *heap, EraseCtx ec, Quantity q, Str name, const Value *dom, Closure clo);
/* legs: VLeg pointers; the ready value is a Vec of Value pointers. */
Comp erase_repr_leg_types_loop(Heap *heap, Nat n, Nat index, EraseCtx ec, Vec legs);
Comp erase_repr_bounded_leg_types(Heap *heap, MaybeNat count, EraseCtx ec, Vec legs);
Comp erase_repr_leg_types(Heap *heap, EraseCtx ec, Big count, Closure clo);
/* types: Value pointers; the ready value is a Vec of kernel bool boxes. */
Comp erase_repr_flags(Heap *heap, Vec types, EraseCtx ec);
Comp erase_repr_leg_flags(Heap *heap, EraseCtx ec, Big count, Closure clo);
Comp erase_repr_sum_text(Heap *heap, EraseCtx ec, const Value *ty, bool keep);
Comp erase_repr_sum_texts(Heap *heap, Vec types, EraseCtx ec);
Comp erase_repr_tuple_texts(Heap *heap, Vec types, EraseCtx ec);
Comp erase_repr_ran_collection(Heap *heap, bool empty, EraseCtx ec, Big count, Closure clo);
Comp erase_repr_lan_collection(Heap *heap, bool empty, EraseCtx ec, Big count, Closure clo);
Comp erase_repr_ran_repr(Heap *heap, EraseCtx ec, Shape shape, Closure clo);
Comp erase_repr_lan_repr(Heap *heap, EraseCtx ec, Shape shape, Closure clo);
Comp erase_repr_repr_node(Heap *heap, EraseCtx ec, const Value *value);
Comp erase_repr_repr_tid(Heap *heap, Repr repr);
Comp erase_repr_tid_of(Heap *heap, EraseCtx ec, const Value *value);
Comp erase_repr_optional_repr(Heap *heap, EraseCtx ec, bool keep, const Value *ty);
/* tele: TeleBinder pointers; env: Value pointers; the ready value is a Vec of MaybeRepr pointers. */
Comp erase_repr_mu_leg_reprs(Heap *heap, Vec tele, EraseCtx ec, Vec env);
Comp erase_repr_mu_parameters(Heap *heap, Vec tele, EraseCtx ec);
Comp erase_repr_mu_layout(Heap *heap, EraseCtx ec, Family family, Ctor ctor);
/* rs: MaybeRepr pointers; the result holds Repr pointers. */
Vec erase_repr_kept_reprs(Heap *heap, const Vec *rs);
/* The ready value is a Vec of Str pointers. */
Comp erase_repr_ctor_names(Heap *heap, Family family, Str name);
/* names: Str pointers; the ready value is a Vec of Tid pointers. */
Comp erase_repr_group_tids(Heap *heap, Vec names, Nat index, EraseCtx ec, Str name, Family family);
Comp erase_repr_mu_group_tids(Heap *heap, EraseCtx ec, Str name, Family family);

/* ---- erase_util (kernel/erase_util.bend): free variables, capture frames and the runtime term reindexing ---- */
/* Every definition is pure (no Comp): a failed allocation sets heap->oom and the result is best
 * effort. List<Nat> is a Vec of heap boxed Nat pointers (erase_util_nat_new); List<Slot> a Vec of
 * Slot box pointers (erase_repr_slot_box); List<FreeWork> and List<RuntimeWork> Vecs of FreeWork
 * and RuntimeWork pointers; List<T.T> and List<Ktm> Vecs of Term and Ktm pointers. */
typedef struct {
  Nat depth;
  const Term *term;
} FreeWork;
typedef struct {
  Nat depth;
  const Ktm *term;
} RuntimeWork;
typedef enum { REINDEX_SHIFT, REINDEX_PRUNE } ReindexKind;
typedef struct {
  ReindexKind kind;
  Nat amount; /* REINDEX_SHIFT */
  Nat params; /* REINDEX_PRUNE */
  Vec live;   /* REINDEX_PRUNE: Nat pointers */
} Reindex;
typedef struct {
  Vec reprs;       /* Repr pointers */
  Vec args;        /* Ktm pointers */
  const Ktm *body;
} Pruned;
/* Host helpers: list items boxed for a Vec; NULL after a failed allocation (heap->oom set). */
Nat *erase_util_nat_new(Heap *heap, Nat n);
FreeWork *erase_util_free_work_new(Heap *heap, Nat depth, const Term *term);
RuntimeWork *erase_util_runtime_work_new(Heap *heap, Nat depth, const Ktm *term);
/* The constructors Shift, Prune and Pruned. */
Reindex erase_util_shift(Nat amount);
Reindex erase_util_prune(Nat params, Vec live);
Pruned erase_util_pruned(Vec reprs, Vec args, const Ktm *body);
bool erase_util_member(Nat n, const Vec *xs);
/* insert and insert_order: the sorted insertion that drops a duplicate. */
Vec erase_util_insert(Heap *heap, Nat n, const Vec *xs);
Vec erase_util_insert_order(Heap *heap, bool before, bool same, Nat n, Nat x, const Vec *rest);
Vec erase_util_sorted(Heap *heap, const Vec *xs);
Vec erase_util_term_work(Heap *heap, const Vec *terms, Nat depth);
/* legs: Leg pointers; branches: Branch pointers. */
Vec erase_util_leg_work(Heap *heap, const Vec *legs, Nat depth);
Vec erase_util_branch_work(Heap *heap, const Vec *branches, Nat depth);
Vec erase_util_addr_work(Heap *heap, Addr addr, Nat depth);
Vec erase_util_free_children(Heap *heap, const Term *term, Nat depth);
/* The Bend worklist recursion run as a loop over an explicit stack (same visit order). */
Vec erase_util_free_walk(Heap *heap, const Vec *work);
Vec erase_util_free_vars(Heap *heap, Nat depth, const Term *term);
MaybeNat erase_util_lookup(Heap *heap, const Vec *slots, Nat index, Nat seen);
bool erase_util_is_some(MaybeNat value);
Vec erase_util_filter_caps(Heap *heap, const Vec *xs, const Vec *slots);
Vec erase_util_captures_of(Heap *heap, EraseCtx ec, const Term *term);
Vec erase_util_frame(Heap *heap, const Vec *slots, const Vec *caps, Nat index);
Vec erase_util_runtime_work(Heap *heap, const Vec *terms, Nat depth);
/* branches: KtmBranch pointers. */
Vec erase_util_runtime_branch_work(Heap *heap, const Vec *branches, Nat depth);
Vec erase_util_runtime_children(Heap *heap, const Ktm *term, Nat depth);
Vec erase_util_runtime_walk(Heap *heap, const Vec *work);
Vec erase_util_runtime_vars(Heap *heap, Nat depth, const Ktm *term);
Nat erase_util_count_before(Heap *heap, const Vec *xs, Nat index);
Nat erase_util_reindex(Heap *heap, Reindex index, Nat i);
Vec erase_util_reindex_list(Heap *heap, const Vec *terms, Reindex index, Nat depth);
Vec erase_util_reindex_branches(Heap *heap, const Vec *branches, Reindex index, Nat depth);
const Ktm *erase_util_reindex_runtime(Heap *heap, Reindex index, Nat depth, const Ktm *term);
const Ktm *erase_util_shift_runtime(Heap *heap, Nat by, Nat depth, const Ktm *term);
Vec erase_util_live_captures(Heap *heap, const Vec *xs, Nat params, Nat count);
/* xs: any list items; the kept items keep their pointers. */
Vec erase_util_filter_live(Heap *heap, const Vec *xs, const Vec *live, Nat count, Nat index);
/* ps: Repr pointers; args: Ktm pointers. */
Pruned erase_util_prune_captures(Heap *heap, const Vec *ps, const Vec *args, Nat params, const Ktm *body);

/* erase section: kernel/erase.bend (S1.12). The file's own Entry, Slot, Found, Acc, Out, Outs,
 * Chain and Signature carry the Erase prefix (R.Slot is the erase_repr Slot above, R.Ctx is EraseCtx).
 * Comp values: EraseOut *, EraseOuts *, EraseChain *, EraseSignature *, const Value *, const Term *,
 * Vec * (Term, Ktm or Repr pointers), kernel bool boxes and kernel_unit. */
typedef enum { ERASE_ENTRY_DROPPED, ERASE_ENTRY_POSTULATE, ERASE_ENTRY_CODE } EraseEntryKind;
typedef struct {
  EraseEntryKind kind;
  Repr repr; /* ERASE_ENTRY_POSTULATE */
  Vec decls; /* ERASE_ENTRY_CODE: Kdecl pointers */
} EraseEntry;
typedef enum { ERASE_SLOT_DROP, ERASE_SLOT_KEEP, ERASE_SLOT_EXTRA } EraseSlot;
typedef enum { ERASE_FOUND_KEEP, ERASE_FOUND_DROP, ERASE_FOUND_OUT } EraseFoundKind;
typedef struct {
  EraseFoundKind kind;
  Nat index; /* ERASE_FOUND_KEEP */
} EraseFound;
typedef struct {
  Nat next;
  Vec lifted; /* Kdecl pointers */
  Vec groups; /* Tid pointers */
} EraseAcc;
typedef struct {
  const Ktm *value;
  EraseAcc acc;
} EraseOut;
typedef struct {
  Vec values; /* Ktm pointers */
  EraseAcc acc;
} EraseOuts;
typedef struct {
  Vec params; /* Repr pointers */
  Repr result;
  const Ktm *body;
  EraseAcc acc;
} EraseChain;
typedef struct {
  Vec params; /* Repr pointers */
  Repr result;
} EraseSignature;
/* Maybe<V.T>: the expected type of a term. */
typedef struct {
  bool some;
  const Value *ty;
} ExpectedTy;
/* Constructors; Slot values are static boxes (erase_slot_box) for a Vec. */
EraseEntry erase_entry_dropped(void);
EraseEntry erase_entry_postulate(Repr repr);
EraseEntry erase_entry_code(Vec decls);
void *erase_slot_box(EraseSlot slot);
EraseSlot erase_slot_unbox(const void *boxed);
EraseFound erase_found_keep(Nat index);
EraseFound erase_found_drop(void);
EraseFound erase_found_out(void);
EraseAcc erase_acc(Nat next, Vec lifted, Vec groups);
EraseOut erase_out(const Ktm *value, EraseAcc acc);
EraseOuts erase_outs(Vec values, EraseAcc acc);
EraseChain erase_chain_make(Vec params, Repr result, const Ktm *body, EraseAcc acc);
EraseSignature erase_signature(Vec params, Repr result);
ExpectedTy erase_expected_some(const Value *ty);
ExpectedTy erase_expected_none(void);
/* Boxed items for Comp values and Vecs; NULL after a failed allocation (heap->oom set). */
EraseOut *erase_out_new(Heap *heap, EraseOut out);
EraseOuts *erase_outs_new(Heap *heap, EraseOuts outs);
EraseChain *erase_chain_new(Heap *heap, EraseChain chain);
EraseSignature *erase_signature_new(Heap *heap, EraseSignature sig);
/* P.Pair<Maybe<V.T>, T.T> of erase_fold and tuple_jobs: first = ExpectedTy *, second = const Term *. */
Pair *erase_job_new(Heap *heap, ExpectedTy expected, const Term *t);
/* The definitions in Bend order. */
EraseFound erase_lookup(Heap *heap, const Vec *slots, Nat ix, Nat seen);
bool erase_quantity_runtime(Quantity q);
/* The laws term, chain and remaining_signature (defined after the helpers they use). */
Comp erase_term(Heap *heap, EraseCtx ec, EraseAcc ac, bool tail, ExpectedTy expected, const Term *t);
Comp erase_chain(Heap *heap, EraseCtx ec, EraseAcc ac, Vec params, const Value *ty, const Term *t);
Comp erase_remaining_signature(Heap *heap, EraseCtx ec, Shape shape, Closure clo);
/* here: Repr pointers. */
Comp erase_signature_value(Heap *heap, const Value *value, EraseCtx ec, const Value *cod, Vec here);
Comp erase_app_jobs(Heap *heap, const Vec *args, EraseCtx ec, const Value *ty);
const Ktm *erase_out_value(EraseOut out);
EraseAcc erase_out_acc(EraseOut out);
Vec erase_outs_values(EraseOuts out);
EraseAcc erase_outs_acc(EraseOuts out);
EraseAcc erase_add_lift(Heap *heap, EraseAcc ac, const Kdecl *decl);
EraseAcc erase_add_groups(Heap *heap, EraseAcc ac, const Vec *tids);
Nat erase_next(EraseAcc ac);
EraseAcc erase_advance(Heap *heap, EraseAcc ac);
Comp erase_position_ty(Heap *heap, EraseCtx ec, ExpectedTy expected, const Term *t);
Comp erase_var_found(Heap *heap, MaybeNat found, EraseAcc ac);
Nat erase_kernel_slots(Heap *heap, const Vec *slots);
Comp erase_var_arm(Heap *heap, EraseCtx ec, EraseAcc ac, Nat ix);
/* erase_fold: jobs are erase_job_new pairs. */
Comp erase_erase_fold(Heap *heap, const Vec *jobs, EraseCtx ec, EraseAcc ac);
MaybeLegPtr erase_as_lam(const Term *t);
bool erase_function_node(const Value *value);
Comp erase_function_result(Heap *heap, EraseCtx ec, const Value *ty);
Comp erase_host_arg_keep(Heap *heap, bool keep, EraseCtx ec, EraseAcc ac, const Value *ty, const Term *t);
Comp erase_host_arg(Heap *heap, EraseCtx ec, EraseAcc ac, const Term *t);
Comp erase_host_args(Heap *heap, const Vec *ts, EraseCtx ec, EraseAcc ac);
Comp erase_host_call(Heap *heap, EraseCtx ec, EraseAcc ac, Str name, const Vec *ts);
Comp erase_leg_arg(const Leg *leg, Str message);
Comp erase_fhc_pick(Heap *heap, bool is_eval, EraseCtx ec, EraseAcc ac, const Term *level, const Term *first, const Term *second);
Comp erase_fhc_sec(Heap *heap, EraseCtx ec, EraseAcc ac, const Term *level, const Vec *legs);
Comp erase_leg_args(Heap *heap, const Vec *legs, Str message);
Comp erase_mpc_args(Heap *heap, const Vec *args, EraseCtx ec, EraseAcc ac);
Comp erase_mpc_sec(Heap *heap, EraseCtx ec, EraseAcc ac, const Vec *legs);
Comp erase_zk_args(Heap *heap, const Vec *args, EraseCtx ec, EraseAcc ac, const Term *point);
Comp erase_zk_in(Heap *heap, EraseCtx ec, EraseAcc ac, Addr addr, const Vec *args);
Vec erase_tuple_jobs(Heap *heap, const Vec *legs, const Vec *types, const Vec *flags);
/* Quarter 2: tuple_finish .. ctor_indices. fields: Ktm pointers; legs: Leg pointers. */
Comp erase_tuple_finish(Heap *heap, const Vec *fields, EraseAcc ac, EraseCtx ec, const Value *ty);
Comp erase_tuple_type(Heap *heap, const Value *value, EraseCtx ec, EraseAcc ac, const Value *ty, Big count,
                      const Vec *legs);
Comp erase_tuple_arm(Heap *heap, EraseCtx ec, EraseAcc ac, const Value *ty, Big count, const Vec *legs);
/* caps: Nat pointers; the ready value is a Vec of Repr pointers. */
Comp erase_capture_reprs(Heap *heap, const Vec *caps, EraseCtx ec);
/* caps: Nat pointers; the ready value is a Vec of Ktm pointers. */
Comp erase_capture_args(Heap *heap, const Vec *caps, EraseCtx ec, EraseAcc ac);
/* params, reprs: Repr pointers; args: Ktm pointers. */
EraseOut erase_lift_pruned(Heap *heap, Pruned pruned, Fid fid, const Vec *params, Repr ret, EraseAcc ac);
EraseOut erase_lift_finish(Heap *heap, EraseChain result, const Vec *reprs, const Vec *args, Fid fid);
Comp erase_lift_arm(Heap *heap, EraseCtx ec, EraseAcc ac, const Value *ty, const Term *t);
Comp erase_add_param(Heap *heap, EraseCtx ec, bool keep, const Value *ty, const Vec *params);
Comp erase_chain_stop(Heap *heap, EraseCtx ec, EraseAcc ac, Vec params, const Value *ty, const Term *t);
Comp erase_chain_point(Heap *heap, EraseCtx ec, EraseAcc ac, Vec params, Quantity q, Str name, const Value *dom,
                       Closure clo, const Leg *leg);
/* pair: first = EraseCtx *, second = const Value *. */
Comp erase_signature_rest(Heap *heap, const Pair *pair, Vec here);
EraseSignature erase_append_signature(Heap *heap, EraseSignature sig, const Vec *here);
Comp erase_signature_point(Heap *heap, EraseCtx ec, Quantity q, Str name, const Value *dom, Closure clo);
Vec erase_eta_args(Heap *heap, Nat n);
EraseChain erase_eta_finish(Heap *heap, EraseOut out, EraseSignature sig, const Vec *params);
Comp erase_eta_chain(Heap *heap, EraseCtx ec, EraseAcc ac, Vec params, const Value *ty, const Term *t, Shape s,
                     Closure d);
Comp erase_chain_shape(Heap *heap, MaybeLegPtr leg, Shape shape, EraseCtx ec, EraseAcc ac, Vec params,
                       const Value *ty, const Term *t, Closure clo);
Comp erase_chain_value(Heap *heap, const Value *value, EraseCtx ec, EraseAcc ac, Vec params, const Value *ty,
                       const Term *t);
/* args: Term pointers. */
Comp erase_one_term(const Vec *args, Str message);
Comp erase_point_term(Addr addr, Str message);
Comp erase_under_runtime(Heap *heap, const Pair *pair);
Vec erase_job(Heap *heap, bool keep, const Value *ty, const Term *t);
Comp erase_pair_intro(Heap *heap, EraseCtx ec, EraseAcc ac, const Value *ty, Quantity q, Str x, const Value *dom,
                      Closure clo, Addr addr, const Vec *args);
Comp erase_leg_index(Heap *heap, Addr addr, Str message);
Comp erase_tag_intro(Heap *heap, EraseCtx ec, EraseAcc ac, const Value *ty, Big count, Closure clo, Addr addr,
                     const Vec *args);
Comp erase_in_type(Heap *heap, const Value *value, EraseCtx ec, EraseAcc ac, const Value *ty, Addr addr,
                   const Vec *args);
Comp erase_in_typed(Heap *heap, EraseCtx ec, EraseAcc ac, const Value *ty, Addr addr, const Vec *args);
/* names: Str pointers. */
MaybeNat erase_position(Heap *heap, const Vec *names, Str name, Nat index);
Comp erase_constructor(Heap *heap, Addr addr);
bool erase_field_kept(const MaybeRepr *field);
Comp erase_optional_field(Heap *heap, bool keep, EraseCtx ec, EraseAcc ac, const Value *ty, const Term *t);
/* tele: TeleBinder pointers; args: Term pointers; layout: MaybeRepr pointers; env: Value pointers. */
Comp erase_mu_fields(Heap *heap, const Vec *tele, const Vec *args, const Vec *layout, EraseCtx ec, EraseAcc ac,
                     Vec env);
Vec erase_ctor_args(Ctor ctor);
Vec erase_ctor_indices(Ctor ctor);
/* Quarter 3 (Bend 525-798): mu_tag .. eval_terms. */
/* P.Pair<T.T, List<T.T>>: the head and the point arguments of an application spine. */
typedef struct {
  const Term *head;
  Vec args; /* Term pointers */
} EraseSpine;
/* Maybe<T.Leg<T.T>> as a pointer into the branch that holds the leg. */
typedef struct {
  bool some;
  const Leg *value;
} EraseMaybeLeg;
/* BranchOut{branch, acc} */
typedef struct {
  const KtmBranch *branch;
  EraseAcc acc;
} EraseBranchOut;
/* Branches{branches, acc} */
typedef struct {
  Vec branches; /* KtmBranch pointers */
  EraseAcc acc;
} EraseBranches;
/* MuBinders{ctx, env, values, arity} */
typedef struct {
  EraseCtx ctx;
  Vec env;    /* Value pointers */
  Vec values; /* Value pointers */
  Nat arity;
} EraseMuBinders;
Comp erase_mu_tag(Heap *heap, EraseCtx ec, EraseAcc ac, Str name, Closure clo, Addr addr, const Vec *args);
Comp erase_mu_intro_type(Heap *heap, const Value *value, EraseCtx ec, EraseAcc ac, Addr addr, const Vec *args);
Comp erase_mu_intro(Heap *heap, EraseCtx ec, EraseAcc ac, const Value *ty, Addr addr, const Vec *args);
EraseSpine erase_spine(Heap *heap, const Term *t, Vec args);
Comp erase_app_point(Heap *heap, EraseCtx ec, Quantity q, const Value *dom, Closure clo, const Term *arg,
                     const Vec *rest);
Comp erase_app_type(Heap *heap, const Value *value, EraseCtx ec, const Term *arg, const Vec *rest);
Comp erase_app_bare(Heap *heap, const Vec *args, EraseCtx ec, const Value *ty);
EraseOut erase_app_result(Heap *heap, bool bare, bool tail, const Ktm *head, Vec args, EraseAcc ac);
Comp erase_app_spine(Heap *heap, EraseSpine parts, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty);
Comp erase_app_arm(Heap *heap, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty, const Term *t);
Nat erase_flags_prefix(Heap *heap, const Vec *flags, Big limit, Nat index);
Nat erase_flags_before(Heap *heap, const Vec *flags, Big count);
Comp erase_proj_type(Heap *heap, const Value *value, EraseCtx ec, EraseAcc ac, const Value *sty, Big n, Big index,
                     const Term *head);
Comp erase_proj_arm(Heap *heap, EraseCtx ec, EraseAcc ac, Big n, Addr addr, const Term *head);
Comp erase_let_runtime(Heap *heap, bool keep, EraseCtx ec, EraseCtx next, EraseAcc ac, bool tail, const Value *ty,
                       Str name, const Value *lty, const Term *value, const Term *body);
Comp erase_let_arm(Heap *heap, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty, Str name, const Term *lty,
                   const Term *value, const Term *body);
Shape erase_elim_shape(const ElimData *data);
Comp erase_pair_branch(const Vec *branches);
Comp erase_second_binder(Heap *heap, const Leg *leg);
const Term *erase_leg_body(const Leg *leg);
const Ktm *erase_project_let(Heap *heap, bool keep, Str name, Tid tid, Nat index, const Ktm *body);
Comp erase_pair_elim(Heap *heap, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty, const Value *sty,
                     const ElimData *data, Quantity q, Str name, const Value *dom, Closure clo);
const KtmBranch *erase_branch_value(EraseBranchOut out);
EraseAcc erase_branch_acc(EraseBranchOut out);
Vec erase_branches_values(EraseBranches out);
EraseAcc erase_branches_acc(EraseBranches out);
EraseMaybeLeg erase_find_leg(Heap *heap, const Vec *branches, Nat index);
Comp erase_case_leg(Heap *heap, const Leg *leg, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty,
                    const ElimData *data, Big count, Nat index, const Value *lty);
Comp erase_case_branches(Heap *heap, const Vec *types, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty,
                         const ElimData *data, Big count, Nat index);
Comp erase_case_elim(Heap *heap, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty, const Value *sty,
                     const ElimData *data, Big count, Closure clo);
Comp erase_elim_type(Heap *heap, const Value *value, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty,
                     const Value *sty, const ElimData *data);
Comp erase_elim_typed(Heap *heap, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty, const ElimData *data);
Comp erase_mu_binders(Heap *heap, const Vec *tele, const Vec *binders, const Vec *layout, EraseCtx ec, Vec env,
                      Vec values, Nat arity);
EraseMaybeLeg erase_find_ctor_branch(const Vec *branches, Str name);
Vec erase_leg_binders(const Leg *leg);
Comp erase_eval_terms(Heap *heap, const Vec *terms, EraseCtx ec, Vec env);

/* Quarter 4a: Bend lines 799-923. */
Comp erase_mu_branch_body(Heap *heap, const EraseMuBinders *bound, EraseCtx ec, EraseAcc ac, bool tail, Str name,
                          Motive motive, Nat index, Str ctor, const Vec *indices, const Term *body);
Comp erase_mu_branch_of(Heap *heap, EraseCtx ec, EraseAcc ac, bool tail, Str name, Family family, Motive motive,
                        Vec penv, const Vec *branches, Nat index, Str ctor);
Comp erase_mu_branches(Heap *heap, const Vec *names, Nat index, EraseCtx ec, EraseAcc ac, bool tail, Str name,
                       Family family, Motive motive, Vec penv, const Vec *branches);
Comp erase_mu_case(Heap *heap, EraseCtx ec, EraseAcc ac, bool tail, const Value *sty, const ElimData *data, Str name,
                   Vec idx, Closure clo);
Comp erase_mu_elim_type(Heap *heap, const Value *value, EraseCtx ec, EraseAcc ac, bool tail, const Value *sty,
                        const ElimData *data);
Comp erase_mu_elim(Heap *heap, EraseCtx ec, EraseAcc ac, bool tail, const ElimData *data);
Comp erase_sec_arm(Heap *heap, Shape shape, EraseCtx ec, EraseAcc ac, const Value *ty, const Vec *legs,
                   const Term *t);
Comp erase_in_arm(Heap *heap, Shape shape, EraseCtx ec, EraseAcc ac, const Value *ty, Addr addr, const Vec *args);
Comp erase_out_arm(Heap *heap, Shape shape, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty, Addr addr,
                   const Term *head, const Term *t);
Comp erase_elim_arm(Heap *heap, Shape shape, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty,
                    const ElimData *data);
Comp erase_node(Heap *heap, const Term *t, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty);
Comp erase_term_runtime(Heap *heap, bool keep, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty, const Term *t);

/* Quarter 4b: Bend lines 924-1049. */
/* P.Pair<String, Entry> of program and print: a Vec of EraseRow pointers. The input rows of program
 * are CheckRow pointers (Common.Pair<String, Global.Entry>). */
typedef struct {
  Str name;
  EraseEntry entry;
} EraseRow;
EraseEntry *erase_entry_new(Heap *heap, EraseEntry entry);
EraseRow *erase_row_new(Heap *heap, Str name, EraseEntry entry);
/* Tid lists are Tid pointers; work is RuntimeWork pointers; reprs Repr pointers; decls Kdecl pointers. */
Vec erase_tids_repr(Heap *heap, Repr repr);
Vec erase_tids_reprs(Heap *heap, const Vec *reprs);
Vec erase_node_tid(Heap *heap, const Ktm *term);
Vec erase_tids_walk(Heap *heap, const Vec *work);
Vec erase_tids_ktm(Heap *heap, const Ktm *t);
Vec erase_tids_decl(Heap *heap, Kdecl decl);
Vec erase_tids_decls(Heap *heap, const Vec *decls);
bool erase_member_tid(const Vec *tids, Tid tid);
Vec erase_dedup_tids(Heap *heap, const Vec *tids, Vec seen);
EraseEntry erase_def_finish(Heap *heap, EraseChain result, Str name);
Comp erase_def_code(Heap *heap, EraseCtx ec, Str name, const Value *ty, const Term *body);
Comp erase_axiom_runtime(Heap *heap, bool keep, EraseCtx ec, const Value *ty);
Comp erase_def_runtime(Heap *heap, bool keep, EraseCtx ec, Str name, const Value *ty, const Term *body);
Comp erase_entry_kind(Heap *heap, Entry entry, EraseCtx ec, Str name);
Comp erase_entry_comp(Heap *heap, Globals globals, Str name, Entry entry);
/* Result<Error.T, Entry>: an EraseEntry *. */
Result erase_decl(Heap *heap, Globals globals, Budget budget, Str name, Entry entry);
Result erase_entry(Heap *heap, Globals globals, Str name, Entry entry);
/* Comp and Result<Error.T, List<Pair<String, Entry>>>: a Vec * of EraseRow pointers. */
Comp erase_program_comp(Heap *heap, const Vec *rows, Globals globals);
Result erase_program_budget(Heap *heap, Globals globals, Budget budget, const Vec *rows);
Result erase_program(Heap *heap, Globals globals, const Vec *rows);
Str erase_print_decls(Heap *heap, const Vec *decls);
Str erase_print_entry(Heap *heap, Str name, EraseEntry entry);
Str erase_print(Heap *heap, const Vec *rows);

/* ---- spec_count (kernel/spec_count.bend) ---- */
/* items: a List<String> as a Vec of Str pointers. */
Str spec_count_join(Heap *heap, const Vec *items);
Str spec_count_pick(Nat index, const Vec *items);
/* rows: EtaEntry pointers (rules_eta_table); the result holds Str pointers. */
Vec spec_count_eta_names(Heap *heap, const Vec *rows, bool keep);
Str spec_count_row(Heap *heap, Str label, const Vec *items);
Str spec_count_print(Heap *heap);

/* ---- compact (host only, no Bend counterpart: c/kernel/compact.c, index_copy in global.c) ----
 * Deep copies into heap->arena of the state that survives a declaration (decisions 156, 157).
 * Shape payloads are copied as Terms (the only instantiation inside Globals). */
typedef void *(*CompactItem)(Heap *heap, const void *item);
typedef const void *(*IndexRemap)(void *ctx, Str key, const void *old);
Index index_copy(Heap *heap, Index ix, IndexRemap remap, void *ctx);
Nat compact_nat(Heap *heap, Nat n);
MaybeNat compact_maybe_nat(Heap *heap, MaybeNat n);
Big compact_big(Heap *heap, Big b);
Str compact_str(Heap *heap, Str s);
MaybeStr compact_maybe_str(Heap *heap, MaybeStr s);
Vec compact_vec_with(Heap *heap, const Vec *vec, CompactItem f);
Vec compact_str_vec(Heap *heap, const Vec *vec);
Vec compact_term_vec(Heap *heap, const Vec *vec);
const LevelVar *compact_level(Heap *heap, const LevelVar *level);
Shape compact_shape_term(Heap *heap, Shape shape);
Addr compact_addr(Heap *heap, Addr addr);
Leg compact_leg(Heap *heap, Leg leg);
Motive compact_motive(Heap *heap, Motive motive);
const ElimData *compact_elim(Heap *heap, const ElimData *data);
Literal compact_literal(Heap *heap, Literal lit);
/* Opens the sharing memo of one compact pass on heap (D-240a): until compact_memo_close, compact_term
 * returns the same copy for the same source Term, so a copied DAG stays a DAG. Returns true when this
 * call opened the memo; false when one is already open (the outer pass owns it) or when the table could
 * not be allocated (heap->oom is then set). The memo must not outlive the pass: the source arena is
 * freed after it and its addresses are reused. */
bool compact_memo_open(Heap *heap);
/* Frees the memo when opened is true (the result of the matching compact_memo_open). */
void compact_memo_close(Heap *heap, bool opened);
const Term *compact_term(Heap *heap, const Term *term);
MaybeTermPtr compact_maybe_term(Heap *heap, MaybeTermPtr term);
Vec compact_tele(Heap *heap, const Vec *binders);
Vec compact_ctors(Heap *heap, const Vec *ctors);
CtorStatus compact_ctor_status(Heap *heap, CtorStatus status);
Family compact_family(Heap *heap, Family family);
Entry compact_entry(Heap *heap, Entry entry);
Decl compact_decl(Heap *heap, Decl decl);
Vec compact_check_rows(Heap *heap, const Vec *rows);
Error compact_error(Heap *heap, Error error);
const EntryRows *compact_entry_rows(Heap *heap, const EntryRows *rows);
const FamilyRows *compact_family_rows(Heap *heap, const FamilyRows *rows);
Globals compact_globals(Heap *heap, Globals globals);
/* D-265a: the erased-term copies (c/tests/runtime_reachable.c copies each erased entry out of its
 * step arena). Inside a pass with the memo open, a shared Ktm keeps one copy. */
const Ktm *compact_ktm(Heap *heap, const Ktm *term);
Kdecl compact_kdecl(Heap *heap, Kdecl decl);
EraseEntry compact_erase_entry(Heap *heap, EraseEntry entry);

#endif
