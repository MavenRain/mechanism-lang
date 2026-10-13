/* A failed certificate binding copy must not produce a successful report. */
#include "cli.h"
#include <stdio.h>

void *rt_test_arena_alloc_real(Arena *arena, size_t size);
static size_t until_failure = SIZE_MAX;
static bool injected;

void *arena_alloc(Arena *arena, size_t size) {
  if (until_failure == 0) {
    until_failure = SIZE_MAX;
    injected = true;
    return NULL;
  }
  if (until_failure != SIZE_MAX) until_failure -= 1;
  return rt_test_arena_alloc_real(arena, size);
}

static int check_prepend(size_t failure) {
  Arena arena = arena_new(4096);
  Heap heap = heap_new(&arena);
  CertBinding bindings[9];
  Vec tail = vec_new(&arena);
  for (size_t i = 0; i < 9; i++) {
    bindings[i].name = str_lit("value");
    bindings[i].value = str_lit("7");
    if (i > 0 && !vec_push(&tail, &bindings[i])) return 70;
  }
  CertBindingsResult rest = {IO_DONE, tail, str_empty()};
  injected = false;
  until_failure = failure;
  CertBindingsResult result = cert_prepend(&heap, rest, &bindings[0]);
  until_failure = SIZE_MAX;
  bool ok;
  if (failure == SIZE_MAX) {
    ok = !heap.oom && result.status == IO_DONE && result.bindings.len == 9;
    for (size_t i = 0; ok && i < 9; i++) {
      VecGet item = vec_get(&result.bindings, i);
      ok = item.status == VEC_GET_OK && item.value == &bindings[i];
    }
  } else {
    int code = cert_report(&heap, result);
    ok = injected && heap.oom && code == 70;
    if (!ok) fprintf(stderr, "FAIL binding allocation %zu: injected=%d oom=%d count=%zu exit=%d\n",
                     failure, injected, heap.oom, result.bindings.len, code);
  }
  arena_free(&arena);
  return ok ? 0 : 1;
}

int main(void) {
  if (check_prepend(SIZE_MAX) != 0) return 1;
  int failed = 0;
  for (size_t i = 0; i < 3; i++) failed |= check_prepend(i);
  if (!failed) puts("CLI-ALLOCATION-OK");
  return failed;
}
