/* Port of bend2/tests/wasm_wide_metadata.bend (oracle label driver_wasm-wide-metadata, manifest mode
 * wasm-wide-metadata, marker WIDE-BACKEND 19/19): raw signed and huge collection widths and leg
 * addresses through check, eval, erase, the erase_repr leg types, the circuit depth and the wasm
 * emitter. Each of the 19 probes prints one PASS or FAIL line; any failure dies with exit 1.
 * Result<String, A> is Out (value boxed as a pointer); the local require and kernel mirror the Bend
 * defs of the same names. Nested calls that take a by-value struct are hoisted into locals (the tcc
 * ldp miscompile, PROGRESS decision 274). */
#include <string.h>

#include "driver.h"
#include "wasm.h"

typedef struct {
  bool ok;
  const void *value; /* ok */
  Str error;         /* !ok */
} Out;

static const int unit_value = 0;

static Out out_done(const void *value) {
  Out o;
  o.ok = true;
  o.value = value;
  o.error = str_lit("");
  return o;
}

static Out out_fail(Str message) {
  Out o;
  o.ok = false;
  o.value = NULL;
  o.error = message;
  return o;
}

static Vec no_items(Heap *heap) { return vec_new(heap->arena); }

static Vec one_item(Heap *heap, const void *item) {
  Vec xs = vec_new(heap->arena);
  if (!vec_push(&xs, (void *)item)) heap->oom = true;
  return xs;
}

static Vec two_items(Heap *heap, const void *a, const void *b) {
  Vec xs = vec_new(heap->arena);
  if (!vec_push(&xs, (void *)a) || !vec_push(&xs, (void *)b)) heap->oom = true;
  return xs;
}

/* Bend 23-24: Big{False{}, [0,0,0,8]} = 2^48 = 281474976710656. A failed parse leaves zero, which the
 * flags probe reports as "wide fixture no longer exceeds the Nat payload limit". */
static Big huge(Heap *heap) {
  MaybeBig parsed = big_of_decimal(heap, str_lit("281474976710656"));
  if (parsed.some) return parsed.value;
  return big_zero();
}

/* Bend 26-27: Big{True{}, [1]} = -1. */
static Big negative(void) { return big_neg(big_one()); }

static const Term *zero(Heap *heap) {
  Literal lit = literal_lint(big_zero());
  return term_lit(heap, lit);
}

static const Term *injection(Heap *heap, Big width, Big index) {
  Shape shape = shape_scoll(width);
  Addr addr = term_addr_leg(index);
  Vec args = one_item(heap, zero(heap));
  return term_in(heap, shape, addr, args);
}

static const Term *projection(Heap *heap, Big width, Big index) {
  Shape shape = shape_scoll(width);
  Addr addr = term_addr_leg(index);
  Leg *leg = term_leg_new(heap, no_items(heap), zero(heap));
  Nat one = heap_nat_of_u64(heap, 1);
  Shape coll = shape_coll(heap, one);
  Vec legs = one_item(heap, leg);
  const Term *sec = term_sec(heap, coll, legs);
  Vec fields = one_item(heap, prim_nat_ty(heap));
  const Term *ty = rules_prod_ty(heap, &fields);
  const Term *ann = term_ann(heap, sec, ty);
  return term_out(heap, shape, addr, ann);
}

static Out require(bool ok, Str message) {
  if (ok) return out_done(&unit_value);
  return out_fail(message);
}

static Out kernel(Heap *heap, Result value) {
  switch (value.kind) {
    case RESULT_DONE:
      return out_done(value.value);
    case RESULT_FAIL:
      return out_fail(error_to_string(heap, value.error));
  }
  return out_fail(str_lit(""));
}

/* Rules.sum_ty([Prim.nat_ty()]) evaluated in the initial globals. */
static Out sum_nat(Heap *heap) {
  Vec fields = one_item(heap, prim_nat_ty(heap));
  const Term *ty = rules_sum_ty(heap, &fields);
  Globals globals = global_initial(heap);
  Vec env = no_items(heap);
  Result value = eval_eval(heap, globals, env, ty);
  return kernel(heap, value);
}

static Ctx make_ctx(Heap *heap) {
  Globals globals = global_initial(heap);
  Budget budget = budget_unlimited();
  Nat arity = nat_zero();
  return check_make(heap, globals, budget, arity);
}

static Out accepted_in(Heap *heap, Big width) {
  Out expected = sum_nat(heap);
  if (!expected.ok) return expected;
  Big index = big_zero();
  const Term *term = injection(heap, width, index);
  Ctx ctx = make_ctx(heap);
  Result checked = check_check(heap, ctx, QUANTITY_MANY, term, (const Value *)expected.value);
  return kernel(heap, checked);
}

static Out accepted_out(Heap *heap, Big width) {
  Big index = big_zero();
  const Term *term = projection(heap, width, index);
  Globals globals = global_initial(heap);
  Budget budget = budget_unlimited();
  Result inferred = check_infer_term(heap, globals, budget, term);
  Out ty = kernel(heap, inferred);
  if (!ty.ok) return ty;
  return out_done(&unit_value);
}

static bool is_zero(const Value *value) {
  if (value->kind != VALUE_LIT) return false;
  switch (value->lit.kind) {
    case LITERAL_INT:
      return big_equal(value->lit.integer, big_zero());
    case LITERAL_STRING:
      return false;
  }
  return false;
}

static Out evaluated_out(Heap *heap, Big width) {
  Big index = big_zero();
  const Term *term = projection(heap, width, index);
  Globals globals = global_initial(heap);
  Vec env = no_items(heap);
  Result evaluated = eval_eval(heap, globals, env, term);
  Out value = kernel(heap, evaluated);
  if (!value.ok) return value;
  return require(is_zero((const Value *)value.value), str_lit("projection did not evaluate to zero"));
}

static Out wrong_leg(Heap *heap, Result value) {
  switch (value.kind) {
    case RESULT_DONE:
      return out_fail(str_lit("out-of-range leg was accepted"));
    case RESULT_FAIL:
      if (value.error.kind == ERROR_WRONG_LEG) return out_done(&unit_value);
      return out_fail(heap_str_cat(heap, str_lit("wrong refusal: "), error_to_string(heap, value.error)));
  }
  return out_fail(str_lit(""));
}

static Out rejected_in(Heap *heap, Big index) {
  Out expected = sum_nat(heap);
  if (!expected.ok) return expected;
  Big width = big_one();
  const Term *term = injection(heap, width, index);
  Ctx ctx = make_ctx(heap);
  Result checked = check_check(heap, ctx, QUANTITY_MANY, term, (const Value *)expected.value);
  return wrong_leg(heap, checked);
}

static Out rejected_out(Heap *heap, Big index) {
  Big width = big_one();
  const Term *term = projection(heap, width, index);
  Globals globals = global_initial(heap);
  Budget budget = budget_unlimited();
  Result inferred = check_infer_term(heap, globals, budget, term);
  return wrong_leg(heap, inferred);
}

static Entry entry(const Term *ty, const Term *body) {
  DefEntry def;
  def.ty = ty;
  def.body = body;
  def.reducible = true;
  def.rec_arg.some = false;
  def.rec_arg.value = nat_zero();
  def.partial = false;
  return global_entry_def(def);
}

/* Common.Pair<String, Global.Entry> as an arena boxed CheckRow. */
static CheckRow *check_row(Heap *heap, const char *name, Entry value) {
  CheckRow *row = heap_alloc(heap, sizeof *row);
  if (row == NULL) {
    heap->oom = true;
    return NULL;
  }
  row->name = str_lit(name);
  row->entry = value;
  return row;
}

/* Done value: the erased rows as a Vec * of EraseRow pointers. */
static Out rows(Heap *heap, Big width) {
  Vec sum_fields = one_item(heap, prim_nat_ty(heap));
  const Term *sum = rules_sum_ty(heap, &sum_fields);
  Big index = big_zero();
  const Term *tag_body = injection(heap, width, index);
  Entry tag_entry = entry(sum, tag_body);
  CheckRow *tag = check_row(heap, "tag", tag_entry);
  Branch *branch = heap_alloc(heap, sizeof *branch);
  if (branch == NULL) {
    heap->oom = true;
    return out_fail(str_lit(""));
  }
  branch->addr = term_aleg(heap, nat_zero());
  Binder *binder = term_binder_new(heap, QUANTITY_MANY, str_lit("value"));
  Vec binders = one_item(heap, binder);
  const Term *var = term_var(heap, nat_zero());
  branch->leg = term_leg(binders, var);
  ElimData data;
  memset(&data, 0, sizeof data);
  Nat one = heap_nat_of_u64(heap, 1);
  data.shape = shape_coll(heap, one);
  data.scrut = term_global(heap, str_lit("tag"));
  data.scrut_q = QUANTITY_MANY;
  data.motive.some = false;
  data.branches = one_item(heap, branch);
  const Term *body = term_elim(heap, data);
  Entry main_entry = entry(prim_nat_ty(heap), body);
  CheckRow *main_row = check_row(heap, "main", main_entry);
  Vec program = two_items(heap, tag, main_row);
  Globals globals = global_initial(heap);
  Result erased = erase_program(heap, globals, &program);
  return kernel(heap, erased);
}

/* List<U32> bytes are the encoded module as a Str (emit_program Done value, decision 267). */
static bool bytes_same(const Str *a, const Str *b) { return str_eq(*a, *b); }

static Out emitted_in(Heap *heap, Big width) {
  Out wide = rows(heap, width);
  if (!wide.ok) return wide;
  Big one = big_one();
  Out small = rows(heap, one);
  if (!small.ok) return small;
  Str wide_text = erase_print(heap, (const Vec *)wide.value);
  Str small_text = erase_print(heap, (const Vec *)small.value);
  Out same = require(str_eq(wide_text, small_text), str_lit("ignored injection metadata changed erasure"));
  if (!same.ok) return same;
  Result wide_bytes = emit_program(heap, (const Vec *)wide.value, str_lit("main"));
  Out actual = kernel(heap, wide_bytes);
  if (!actual.ok) return actual;
  Result small_bytes = emit_program(heap, (const Vec *)small.value, str_lit("main"));
  Out expected = kernel(heap, small_bytes);
  if (!expected.ok) return expected;
  return require(bytes_same((const Str *)actual.value, (const Str *)expected.value),
                 str_lit("ignored injection metadata changed Wasm bytes"));
}

static Out erased_out(Heap *heap, Big width) {
  Big index = big_zero();
  const Term *body = projection(heap, width, index);
  Entry main_entry = entry(prim_nat_ty(heap), body);
  Globals globals = global_initial(heap);
  Result erased = erase_entry(heap, globals, str_lit("main"), main_entry);
  return wrong_leg(heap, erased);
}

static Closure diagram(Heap *heap) {
  Leg *leg = term_leg_new(heap, no_items(heap), prim_nat_ty(heap));
  Nat one = heap_nat_of_u64(heap, 1);
  Shape coll = shape_coll(heap, one);
  Vec legs = one_item(heap, leg);
  const Term *sec = term_sec(heap, coll, legs);
  return value_close(no_items(heap), sec);
}

static EraseCtx ec(Heap *heap) {
  Ctx check = make_ctx(heap);
  Vec slots = no_items(heap);
  return erase_repr_ctx(check, slots, str_lit("wide"));
}

static Result leg_types(Heap *heap, Big width) {
  EraseCtx ctx = ec(heap);
  Closure clo = diagram(heap);
  Comp action = erase_repr_leg_types(heap, ctx, width, clo);
  Budget budget = budget_unlimited();
  return budget_run(heap, action, budget);
}

static Out width_refused(Heap *heap, Big width) {
  Result types = leg_types(heap, width);
  return wrong_leg(heap, types);
}

static Out finite_diagram(Heap *heap) {
  Big one = big_one();
  Result run = leg_types(heap, one);
  Out types = kernel(heap, run);
  if (!types.ok) return types;
  return require(((const Vec *)types.value)->len == 1, str_lit("finite diagram lost a leg"));
}

static Out circuit_zero(Heap *heap, const Term *term) {
  Vec globals = no_items(heap);
  CircuitBigResult depth = circuit_depth(heap, &globals, term);
  switch (depth.kind) {
    case RESULT_DONE:
      return require(big_equal(depth.value, big_zero()), str_lit("ignored metadata changed circuit depth"));
    case RESULT_FAIL:
      return out_fail(depth.error);
  }
  return out_fail(str_lit(""));
}

static bool flags_equal(Heap *heap, Big count, uint64_t expected) {
  Vec flags = vec_new(heap->arena);
  if (!vec_push(&flags, kernel_bool_box(true)) || !vec_push(&flags, kernel_bool_box(false)) ||
      !vec_push(&flags, kernel_bool_box(true))) {
    heap->oom = true;
  }
  Nat got = erase_flags_before(heap, &flags, count);
  Nat want = heap_nat_of_u64(heap, expected);
  return nat_cmp(got, want) == 0;
}

static Out flags(Heap *heap) {
  Big wide = huge(heap);
  Big signed_one = negative();
  Big one = big_one();
  Str wide_text = big_to_string(heap, wide);
  Out ok = require(str_eq(wide_text, str_lit("281474976710656")),
                   str_lit("wide fixture no longer exceeds the Nat payload limit"));
  if (!ok.ok) return ok;
  Str signed_text = big_to_string(heap, signed_one);
  ok = require(str_eq(signed_text, str_lit("-1")), str_lit("signed fixture lost its sign"));
  if (!ok.ok) return ok;
  ok = require(flags_equal(heap, wide, 2), str_lit("large index wrapped in projection offset"));
  if (!ok.ok) return ok;
  ok = require(flags_equal(heap, signed_one, 0), str_lit("negative index became a runtime offset"));
  if (!ok.ok) return ok;
  return require(flags_equal(heap, one, 1), str_lit("finite projection offset changed"));
}

static uint64_t report(Heap *heap, const char *name, Out value) {
  Str label = str_lit(name);
  if (value.ok) {
    driver_print(heap_str_cat(heap, str_lit("PASS wide-backend "), label));
    return 0;
  }
  Str head = heap_str_cat(heap, str_lit("FAIL wide-backend "), label);
  Str tail = heap_str_cat(heap, str_lit(": "), value.error);
  driver_print(heap_str_cat(heap, head, tail));
  return 1;
}

static int finish(Heap *heap, uint64_t failed) {
  if (failed == 0) return driver_print(str_lit("WIDE-BACKEND 19/19"));
  Str count = str_of_u64(heap->arena, failed);
  return driver_die_text(1, heap_str_cat(heap, str_lit("WIDE-BACKEND FAIL "), count));
}

/* Bend 169-173: the four circuit_zero probes in one Result do-block; the first failure wins. */
static Out circuit_metadata(Heap *heap) {
  Big wide = huge(heap);
  Big signed_one = negative();
  Big index = big_zero();
  Out ok = circuit_zero(heap, injection(heap, wide, signed_one));
  if (!ok.ok) return ok;
  ok = circuit_zero(heap, injection(heap, signed_one, wide));
  if (!ok.ok) return ok;
  ok = circuit_zero(heap, projection(heap, wide, index));
  if (!ok.ok) return ok;
  return circuit_zero(heap, projection(heap, signed_one, index));
}

static int arguments(Heap *heap) {
  Big wide = huge(heap);
  Big signed_one = negative();
  uint64_t failed = 0;
  failed += report(heap, "huge-ignored-injection", accepted_in(heap, wide));
  failed += report(heap, "negative-ignored-injection", accepted_in(heap, signed_one));
  failed += report(heap, "huge-ignored-projection", accepted_out(heap, wide));
  failed += report(heap, "negative-ignored-projection", accepted_out(heap, signed_one));
  failed += report(heap, "huge-projection-evaluation", evaluated_out(heap, wide));
  failed += report(heap, "negative-projection-evaluation", evaluated_out(heap, signed_one));
  failed += report(heap, "huge-injection-address", rejected_in(heap, wide));
  failed += report(heap, "negative-injection-address", rejected_in(heap, signed_one));
  failed += report(heap, "huge-projection-address", rejected_out(heap, wide));
  failed += report(heap, "negative-projection-address", rejected_out(heap, signed_one));
  failed += report(heap, "huge-injection-exact-wasm", emitted_in(heap, wide));
  failed += report(heap, "negative-injection-exact-wasm", emitted_in(heap, signed_one));
  failed += report(heap, "huge-projection-backend-refusal", erased_out(heap, wide));
  failed += report(heap, "negative-projection-backend-refusal", erased_out(heap, signed_one));
  failed += report(heap, "finite-diagram", finite_diagram(heap));
  failed += report(heap, "huge-diagram-refusal", width_refused(heap, wide));
  failed += report(heap, "negative-diagram-refusal", width_refused(heap, signed_one));
  failed += report(heap, "wide-projection-offsets", flags(heap));
  failed += report(heap, "ignored-circuit-metadata", circuit_metadata(heap));
  return finish(heap, failed);
}

/* main() = arguments(IO.args()); the args are ignored. */
static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = arguments(&heap);
  int out = heap.oom ? driver_host_failure() : code;
  arena_free(&arena);
  return out;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
