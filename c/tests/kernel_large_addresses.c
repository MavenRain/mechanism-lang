/* Port of tests/kernel_large_addresses.bend (oracle label unit-kernel-large-addresses). */
#include "driver.h"
#include "levels_protocol.h"

static Vec no_items(Heap *heap) { return vec_new(heap->arena); }

static Vec one_item(Heap *heap, const void *item) {
  Vec xs = vec_new(heap->arena);
  if (!vec_push(&xs, (void *)item)) heap->oom = true;
  return xs;
}

static MaybeStr some_text(const char *text) {
  MaybeStr m;
  m.some = true;
  m.value = str_lit(text);
  return m;
}

static const Term *zero(Heap *heap) { return term_lit(heap, literal_lint(big_zero())); }

static const Term *nat(Heap *heap) { return term_global(heap, str_lit("Nat")); }

static const Term *injection(Heap *heap, Big width, Big index) {
  return term_in(heap, shape_scoll(width), term_addr_leg(index), one_item(heap, zero(heap)));
}

static const Term *tuple(Heap *heap) {
  Leg *leg = term_leg_new(heap, no_items(heap), zero(heap));
  const Term *sec = term_sec(heap, shape_coll(heap, heap_nat_of_u64(heap, 1)), one_item(heap, leg));
  Vec fields = one_item(heap, nat(heap));
  return term_ann(heap, sec, rules_prod_ty(heap, &fields));
}

static const Term *projection(Heap *heap, Big width, Big index) {
  return term_out(heap, shape_scoll(width), term_addr_leg(index), tuple(heap));
}

static Result check_injection(Heap *heap, Big width, Big index) {
  Vec fields = one_item(heap, nat(heap));
  Result ty = eval_eval(heap, global_initial(heap), no_items(heap), rules_sum_ty(heap, &fields));
  if (ty.kind == RESULT_FAIL) return ty;
  return check_check_term(heap, global_initial(heap), budget_unlimited(), injection(heap, width, index),
                          (const Value *)ty.value);
}

static Result infer_projection(Heap *heap, Big width, Big index) {
  return check_infer_term(heap, global_initial(heap), budget_unlimited(), projection(heap, width, index));
}

static TR roundtrip(Heap *heap, Big width) {
  const Term *term = injection(heap, width, big_zero());
  TR value = test_kernel(heap, eval_eval(heap, global_initial(heap), no_items(heap), term));
  if (!value.ok) return value;
  TR quoted = test_kernel(heap, eval_quote(heap, global_initial(heap), heap_nat_of_u64(heap, 0),
                                           (const Value *)value.value));
  if (!quoted.ok) return quoted;
  return test_require(term_equal(term, (const Term *)quoted.value),
                      str_lit("quote changed ignored signed or huge width"));
}

static TR accepted(Heap *heap, Big width) {
  TR ok = test_kernel(heap, check_injection(heap, width, big_zero()));
  if (!ok.ok) return ok;
  TR ty = test_kernel(heap, infer_projection(heap, width, big_zero()));
  if (!ty.ok) return ty;
  return roundtrip(heap, width);
}

static TR refused(Heap *heap, Big index) {
  TR ok = test_expect(heap, check_injection(heap, big_one(), index), str_lit("wrong leg"),
                      some_text("the leg address is outside the collection"));
  if (!ok.ok) return ok;
  return test_expect(heap, infer_projection(heap, big_one(), index), str_lit("wrong leg"),
                     some_text("the leg address is outside the collection"));
}

static TR former_refused(Heap *heap, Big width) {
  const Term *diagram = term_lan(heap, shape_scoll(width), term_sec(heap, shape_scoll(width), no_items(heap)));
  return test_expect(heap, check_infer_term(heap, global_initial(heap), budget_unlimited(), diagram),
                     str_lit("mismatch"), some_text("the diagram is not a section of the declared width"));
}

static TR party_width(Heap *heap, Big width) {
  Ctx ctx = context_make(heap, global_initial(heap), budget_unlimited(), heap_nat_of_u64(heap, 0));
  Vec empty = no_items(heap);
  MaybeLevelVar none;
  none.some = false;
  none.value = NULL;
  const Value *value =
      value_vlan(heap, shape_scoll(width), value_close(no_items(heap), rules_diagram_of(heap, &empty)), none);
  BigResult got = rules_mpc_parties(heap, ctx, value);
  if (got.kind == RESULT_FAIL) return test_fail(error_to_string(heap, got.error));
  return test_require(big_equal(got.value, width), str_lit("MPC party inspection changed the declared raw width"));
}

static bool some_nat(MaybeNat actual, Nat expected) { return actual.some && nat_cmp(actual.value, expected) == 0; }

static bool absent_nat(MaybeNat actual) { return !actual.some; }

static bool absent_get(VecGet actual) { return actual.status != VEC_GET_OK; }

static TR bounded(Heap *heap, Big huge) {
  Nat two = heap_nat_of_u64(heap, 2);
  uint32_t seven = 7;
  uint32_t eight = 8;
  Vec pair = no_items(heap);
  if (!vec_push(&pair, &seven) || !vec_push(&pair, &eight)) heap->oom = true;
  TR ok = test_require(absent_nat(big_to_nat_bounded(heap, huge, two)), str_lit("huge address narrowed into Nat"));
  if (!ok.ok) return ok;
  ok = test_require(absent_nat(big_to_nat_bounded(heap, big_neg(big_one()), two)),
                    str_lit("negative address narrowed into Nat"));
  if (!ok.ok) return ok;
  ok = test_require(some_nat(big_to_nat_bounded(heap, big_of_u32(heap, 2), two), two),
                    str_lit("inclusive concrete bound refused"));
  if (!ok.ok) return ok;
  ok = test_require(absent_nat(big_to_nat_bounded(heap, big_of_u32(heap, 3), two)),
                    str_lit("address exceeded concrete bound"));
  if (!ok.ok) return ok;
  ok = test_require(absent_get(big_at(heap, &pair, huge)), str_lit("huge list lookup wrapped"));
  if (!ok.ok) return ok;
  return test_require(absent_get(big_at(heap, &pair, big_neg(big_one()))), str_lit("negative list lookup wrapped"));
}

static TR native_max(Heap *heap, NatParse value) {
  if (value.status != NAT_PARSE_OK) return test_fail(str_lit("native Nat maximum did not parse"));
  return test_require(str_eq(big_to_string(heap, big_of_nat(heap, value.value)), str_lit("281474976710655")),
                      str_lit("Nat to Big lost high bits"));
}

static TR mpc_constructors(Heap *heap) {
  Vec none = no_items(heap);
  TR empty = test_kernel(heap, rules_mpc_sub_ty(heap, big_zero()));
  if (!empty.ok) return empty;
  TR ok = test_require(term_equal((const Term *)empty.value, rules_prod_ty(heap, &none)),
                       str_lit("empty subset type changed"));
  if (!ok.ok) return ok;
  TR sub = test_kernel(heap, rules_mpc_sub_ty(heap, big_of_u32(heap, 3)));
  if (!sub.ok) return sub;
  Vec three = no_items(heap);
  if (!vec_push(&three, (void *)nat(heap)) || !vec_push(&three, (void *)nat(heap)) ||
      !vec_push(&three, (void *)nat(heap))) {
    heap->oom = true;
  }
  ok = test_require(term_equal((const Term *)sub.value, rules_prod_ty(heap, &three)),
                    str_lit("subset field count changed"));
  if (!ok.ok) return ok;
  ok = test_expect(heap, rules_mpc_sub_ty(heap, big_neg(big_one())), str_lit("mismatch"),
                   some_text("an mpc subset width must be nonnegative"));
  if (!ok.ok) return ok;
  return test_expect(heap, rules_mpc_pred_ty(heap, big_neg(big_one())), str_lit("mismatch"),
                     some_text("an mpc subset width must be nonnegative"));
}

static TR run_big(Heap *heap, Big huge) {
  TR ok = accepted(heap, huge);
  if (!ok.ok) return ok;
  ok = accepted(heap, big_neg(big_one()));
  if (!ok.ok) return ok;
  ok = accepted(heap, big_zero());
  if (!ok.ok) return ok;
  ok = refused(heap, huge);
  if (!ok.ok) return ok;
  ok = refused(heap, big_neg(big_one()));
  if (!ok.ok) return ok;
  ok = former_refused(heap, huge);
  if (!ok.ok) return ok;
  ok = former_refused(heap, big_neg(big_one()));
  if (!ok.ok) return ok;
  ok = party_width(heap, huge);
  if (!ok.ok) return ok;
  ok = party_width(heap, big_neg(big_one()));
  if (!ok.ok) return ok;
  ok = bounded(heap, huge);
  if (!ok.ok) return ok;
  ok = native_max(heap, nat_parse_decimal(heap->arena, str_lit("281474976710655")));
  if (!ok.ok) return ok;
  return mpc_constructors(heap);
}

static TR parsed(Heap *heap, MaybeBig value) {
  if (!value.some) return test_fail(str_lit("huge signed-width fixture did not parse"));
  return run_big(heap, value.value);
}

static TR run(Heap *heap) { return parsed(heap, big_of_decimal(heap, str_lit("281474976710656"))); }

static int finish(TR result) {
  if (!result.ok) return driver_die_text(1, result.error);
  return driver_print(str_lit("PASS signed large kernel widths and addresses"));
}

/* arguments(args) ignores args; main() = finish(run()). */
int kernel_large_addresses_run(Heap *heap) { return finish(run(heap)); }

#ifndef MECH_DRIVER_LIBRARY
static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = kernel_large_addresses_run(&heap);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
#endif
