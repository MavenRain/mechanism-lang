/* Port of bend2/tests/surface_foundation_common.bend (103 lines): level builds a successor level, contract
 * and check_contract check the frozen contract source of an instance, install_families and install_terms
 * instantiate prelude families and polymorphic terms in list order, negatives checks each control and
 * refuses each negative source, exhausted/countdown are finite budget callbacks, budget_refusal expects the
 * exhausted budget refusal, zero/one/ones build MechNat values and audit is Audit.audit. Library only. */
#include "driver.h"
#include "surface_foundation_common.h"
#include "surface_prelude_poly.h"

static Str cat(Heap *heap, Str a, Str b) { return str_cat(heap->arena, a, b); }

/* level(index): 0n is L.zero(), 1n+p is L.succ(level(p)). */
const LevelVar *surface_foundation_common_level(Heap *heap, uint64_t index) {
  const LevelVar *level = level_zero();
  for (uint64_t i = 0; i < index && level != NULL; i++) level = level_succ(heap, level);
  if (level == NULL) heap->oom = true;
  return level;
}

/* contract(contracts, name): the source of the first contract whose key equals name. */
Task surface_foundation_common_contract(Heap *heap, const FoundationContract *contracts, size_t count, Str name,
                                        Str *source) {
  for (size_t i = 0; i < count; i++) {
    if (str_eq(str_lit(contracts[i].name), name)) {
      *source = str_lit(contracts[i].source);
      return task_pure();
    }
  }
  return task_fail(cat(heap, str_lit("missing frozen contract: "), name));
}

/* check_contract(globals, name, contracts): an empty name keeps globals; otherwise R.globals of the checked
 * contract source. */
Task surface_foundation_common_check_contract(Heap *heap, Globals globals, Str name, const FoundationContract *contracts,
                                              size_t count, Globals *out) {
  if (name.len == 0) {
    *out = globals;
    return task_pure();
  }
  Str source = str_empty();
  const ElabChecked *checked = NULL;
  RELATIONAL_TRY(surface_foundation_common_contract(heap, contracts, count, name, &source));
  RELATIONAL_TRY(relational_checked(heap, globals, source, &checked));
  *out = relational_globals(checked);
  return task_pure();
}

/* install_families(instances, catalog, contracts, globals): Family.instantiate(globals, catalog, B.unlimited(),
 * name, levels, alias, [], None{}) then check_contract, per instance in list order. */
Task surface_foundation_common_install_families(Heap *heap, const FoundationInstance *instances, size_t len,
                                                FamilyCatalog catalog, const FoundationContract *contracts,
                                                size_t count, Globals globals, Globals *out) {
  Globals current = globals;
  for (size_t i = 0; i < len; i++) {
    Vec reuse = vec_new(heap->arena);
    FamilyMaybeVec exports;
    exports.some = false;
    exports.value = vec_new(heap->arena);
    void *value = NULL;
    RELATIONAL_TRY(relational_kernel(heap,
                                     family_instantiate(heap, current, catalog, budget_unlimited(), instances[i].name,
                                                        &instances[i].levels, instances[i].alias, &reuse, exports),
                                     &value));
    if (value == NULL) return task_fail(str_lit("instantiate returned no globals"));
    RELATIONAL_TRY(surface_foundation_common_check_contract(heap, *(const Globals *)value, instances[i].contract,
                                                            contracts, count, &current));
  }
  *out = current;
  return task_pure();
}

/* entry_count(globals) = R.length(entries) */
uint64_t surface_foundation_common_entry_count(Globals globals) {
  return (uint64_t)relational_entry_rows_length(globals.entries);
}

/* instance_globals(instance) */
Globals surface_foundation_common_instance_globals(PolyInstance instance) { return instance.globals; }

/* install_terms(instances, catalog, contracts, globals, added): Poly.instantiate(globals, catalog, B.unlimited(),
 * name, levels, alias) per instance; added grows by the entries the instance added (Nat subtraction saturates). */
Task surface_foundation_common_install_terms(Heap *heap, const FoundationInstance *instances, size_t len,
                                             PolyCatalog catalog, const FoundationContract *contracts, size_t count,
                                             Globals globals, uint64_t added, Globals *out_globals,
                                             uint64_t *out_added) {
  Globals current = globals;
  uint64_t total = added;
  for (size_t i = 0; i < len; i++) {
    void *value = NULL;
    RELATIONAL_TRY(relational_kernel(heap,
                                     poly_instantiate(heap, current, catalog, budget_unlimited(), instances[i].name,
                                                      instances[i].levels, instances[i].alias),
                                     &value));
    if (value == NULL) return task_fail(str_lit("instantiate returned no instance"));
    Globals installed = surface_foundation_common_instance_globals(*(const PolyInstance *)value);
    uint64_t after = surface_foundation_common_entry_count(installed);
    uint64_t before = surface_foundation_common_entry_count(current);
    total += after > before ? after - before : 0;
    RELATIONAL_TRY(surface_foundation_common_check_contract(heap, installed, instances[i].contract, contracts, count,
                                                            &current));
  }
  *out_globals = current;
  *out_added = total;
  return task_pure();
}

/* negatives(cases, globals): each control checks, then Audit.refusal(Elab.check_in(globals, B.unlimited(),
 * source), expected, name ++ ": expected refusal: " ++ expected). */
Task surface_foundation_common_negatives(Heap *heap, const FoundationNegative *cases, size_t len, Globals globals) {
  for (size_t i = 0; i < len; i++) {
    const ElabChecked *accepted = NULL;
    Str expected = str_lit(cases[i].expected);
    RELATIONAL_TRY(relational_checked(heap, globals, str_lit(cases[i].control), &accepted));
    RELATIONAL_TRY(surface_prelude_poly_refusal(
        heap, elab_check_in(heap, globals, budget_unlimited(), str_lit(cases[i].source)), expected,
        cat(heap, cat(heap, str_lit(cases[i].name), str_lit(": expected refusal: ")), expected)));
  }
  return task_pure();
}

/* countdown(remaining): 0n answers exhausted(), 1n+rest answers (countdown(rest), False). */
typedef struct {
  uint64_t remaining;
} Countdown;

static CallbackPoll *countdown_poll(Heap *heap, void *ctx) {
  Countdown *c = ctx;
  if (c == NULL) return NULL;
  CallbackPoll *out = heap_alloc(heap, sizeof *out);
  if (out == NULL) return NULL;
  out->spent = c->remaining == 0;
  out->next = surface_foundation_common_countdown(heap, c->remaining == 0 ? 0 : c->remaining - 1);
  return out;
}

/* exhausted(): a callback that always answers (exhausted(), True), the same answers as countdown(0n). */
Callback surface_foundation_common_exhausted(Heap *heap) { return surface_foundation_common_countdown(heap, 0); }

Callback surface_foundation_common_countdown(Heap *heap, uint64_t remaining) {
  Countdown *c = heap_alloc(heap, sizeof *c);
  if (c != NULL) c->remaining = remaining;
  return budget_of_poll(countdown_poll, c);
}

/* budget_refusal(result) */
Task surface_foundation_common_budget_refusal(Heap *heap, Result result) {
  if (result.kind == RESULT_DONE) return task_fail(str_lit("budget: expected refusal"));
  Str text = error_to_string(heap, result.error);
  return relational_require(surface_prelude_poly_starts(str_lit("budget: the check budget is exhausted"), text),
                            cat(heap, str_lit("budget: wrong refusal: "), text));
}

/* zero() = In{SMu{"MechNat", []}, ACtor{"mechZero"}, []} */
const Term *surface_foundation_common_zero(Heap *heap) {
  return term_in(heap, shape_smu(str_lit("MechNat"), vec_new(heap->arena)), term_addr_ctor(str_lit("mechZero")),
                 vec_new(heap->arena));
}

/* one() = In{SMu{"MechNat", []}, ACtor{"mechSucc"}, [zero()]} */
const Term *surface_foundation_common_one(Heap *heap) {
  Vec args = vec_new(heap->arena);
  const Term *zero = surface_foundation_common_zero(heap);
  if (zero == NULL || !vec_push(&args, (void *)zero)) heap->oom = true;
  return term_in(heap, shape_smu(str_lit("MechNat"), vec_new(heap->arena)), term_addr_ctor(str_lit("mechSucc")), args);
}

/* ones(names): Pair{name, one()} per name, in order; a Vec of Computation pointers for R.computations. */
Vec surface_foundation_common_ones(Heap *heap, const char *const *names, size_t len) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < len; i++) {
    Computation *row = heap_alloc(heap, sizeof *row);
    if (row == NULL) {
      heap->oom = true;
      return out;
    }
    row->name = str_lit(names[i]);
    row->expected = surface_foundation_common_one(heap);
    if (row->expected == NULL || !vec_push(&out, row)) heap->oom = true;
  }
  return out;
}

/* audit(globals) = Audit.audit(globals) */
Task surface_foundation_common_audit(Heap *heap, Globals globals) { return surface_prelude_poly_audit(heap, globals); }
