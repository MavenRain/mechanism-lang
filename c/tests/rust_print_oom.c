/* Printer allocation failures must not silently remove method or field chains. */
#include <stdio.h>
#include "rust.h"

void *rt_test_arena_alloc_real(Arena *arena, size_t size);
static size_t review_allocations;
static size_t review_until_failure = SIZE_MAX;
static bool review_injected;

void *arena_alloc(Arena *arena, size_t size) {
  review_allocations += 1;
  if (review_until_failure == 0) {
    review_until_failure = SIZE_MAX;
    review_injected = true;
    return NULL;
  }
  if (review_until_failure != SIZE_MAX) review_until_failure -= 1;
  return rt_test_arena_alloc_real(arena, size);
}

typedef struct {
  const char *name;
  const char *source;
  const char *expected;
} ReviewCase;

static const ReviewCase review_cases[] = {
    {"short chain", "fn f() { x.foo().bar() }", "fn f() {\n    x.foo().bar()\n}\n"},
    {"wrapped chain",
     "fn f() { x.aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa()"
     ".bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb() }",
     "fn f() {\n    x.aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa()\n"
     "        .bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb()\n}\n"},
};

static size_t review_print(const ReviewCase *test, size_t fail_at, bool *ok) {
  Arena arena = arena_new(4096);
  Heap heap = heap_new(&arena);
  RustSrcFileResult parsed = rust_parser_file(&heap, str_lit(test->source));
  if (!parsed.ok || heap.oom) {
    *ok = false;
    arena_free(&arena);
    return 0;
  }
  review_allocations = 0;
  review_injected = false;
  review_until_failure = fail_at;
  Str got = print_file(&heap, &parsed.file);
  size_t calls = review_allocations;
  review_until_failure = SIZE_MAX;
  bool same = str_eq(got, str_lit(test->expected));
  bool lost = review_injected && !heap.oom && !same;
  if (lost) {
    printf("FAIL print %s allocation=%zu oom=%d text=%.*s", test->name, fail_at, heap.oom,
           (int)got.len, got.data);
  }
  *ok = !lost && (fail_at != SIZE_MAX || (!heap.oom && same));
  arena_free(&arena);
  return calls;
}

int main(void) {
  bool ok = false;
  size_t failed = 0;
  size_t total = 0;
  for (size_t t = 0; t < sizeof review_cases / sizeof review_cases[0]; t++) {
    size_t allocations = review_print(&review_cases[t], SIZE_MAX, &ok);
    if (!ok) return 1;
    total += allocations;
    for (size_t i = 0; i < allocations; i++) {
      review_print(&review_cases[t], i, &ok);
      if (!ok) failed += 1;
    }
  }
  printf("rust_print_oom allocation-cases=%zu failed=%zu\n", total, failed);
  return failed == 0 ? 0 : 1;
}
