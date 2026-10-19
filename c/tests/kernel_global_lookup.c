/* C port of tests/kernel_global_lookup.bend: first-match lookup boundaries of the global entry
 * list, the family list and a constructor list. */
#include "global_lookup_common.h"

static Ctor *ctor(Heap *heap, const char *name, uint64_t full_arity) {
  Ctor *boxed = heap_alloc(heap, sizeof(Ctor));
  if (boxed == NULL) return NULL;
  boxed->name = str_lit(name);
  boxed->args = vec_new(heap->arena);
  boxed->res_idx = vec_new(heap->arena);
  boxed->full_arity = heap_nat_of_u64(heap, full_arity);
  boxed->self_rec = false;
  return boxed;
}

/* ctors() = [x: 1, y: 2, x: 3] as Ctor pointers */
static Vec ctors(Heap *heap) {
  Vec xs = vec_new(heap->arena);
  Ctor *items[3] = {ctor(heap, "x", 1u), ctor(heap, "y", 2u), ctor(heap, "x", 3u)};
  for (size_t i = 0; i < 3u; i++) {
    if (items[i] == NULL || !vec_push(&xs, items[i])) heap->oom = true;
  }
  return xs;
}

/* ctor_is: Some{Ctor{..., arity, ...}} with the expected arity */
static bool ctor_is(Heap *heap, MaybeCtor result, uint64_t expected) {
  return result.some && nat_cmp(result.value.full_arity, heap_nat_of_u64(heap, expected)) == 0;
}

static bool missing_ctor(MaybeCtor result) { return !result.some; }

static int verify(bool ok) {
  if (ok) return driver_print(str_lit("PASS global, family and constructor first-match lookup boundaries"));
  return driver_die(1, "FAIL first-match lookup boundaries");
}

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  const EntryRows *es = lookup_entries(&heap);
  const FamilyRows *fs = lookup_families(&heap);
  Vec cs = ctors(&heap);
  Vec none = vec_new(heap.arena);
  bool ok = lookup_entry_is(global_find_entry(es, str_lit("x")), str_lit("first")) &&
            lookup_entry_is(global_find_entry(es, str_lit("y")), str_lit("tail")) &&
            lookup_missing_entry(global_find_entry(es, str_lit("absent"))) &&
            lookup_missing_entry(global_find_entry(NULL, str_lit("x"))) &&
            lookup_family_is(global_find_family_entry(fs, str_lit("x")), str_lit("first")) &&
            lookup_family_is(global_find_family_entry(fs, str_lit("y")), str_lit("tail")) &&
            lookup_missing_family(global_find_family_entry(fs, str_lit("absent"))) &&
            lookup_missing_family(global_find_family_entry(NULL, str_lit("x"))) &&
            ctor_is(&heap, positivity_find_ctor(&cs, str_lit("x")), 1u) &&
            ctor_is(&heap, positivity_find_ctor(&cs, str_lit("y")), 2u) &&
            missing_ctor(positivity_find_ctor(&cs, str_lit("absent"))) &&
            missing_ctor(positivity_find_ctor(&none, str_lit("x")));
  int code = heap.oom ? driver_host_failure() : verify(ok);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
