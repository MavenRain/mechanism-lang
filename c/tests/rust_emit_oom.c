/* A single arena allocation failure must not silently change emitted Rust. */
#include "rust.h"

#include <stdio.h>

void *rt_test_arena_alloc_real(Arena *arena, size_t size);

static size_t remaining = SIZE_MAX;
static size_t allocation_count;
static bool injected;
static size_t failures;
static size_t checks;

void *arena_alloc(Arena *arena, size_t size) {
  allocation_count += 1;
  if (remaining == 0) {
    remaining = SIZE_MAX;
    injected = true;
    return NULL;
  }
  if (remaining != SIZE_MAX) remaining -= 1;
  return rt_test_arena_alloc_real(arena, size);
}

static void expect(bool ok, const char *name, size_t index) {
  checks += 1;
  if (!ok) {
    failures += 1;
    printf("FAIL %s allocation=%zu\n", name, index);
  }
}

static RirModule let_module(Heap *heap) {
  Vec items = vec_new(heap->arena);
  const RirEx *value = rir_xbool(heap, true);
  const RirEx *body = rir_xvar(heap, str_lit("x"));
  const RirEx *let = rir_xlet(heap, str_lit("x"), rir_some_ty(rir_tbool(heap)), value, body);
  const RirItem *fn = rir_ifn(heap, str_lit("keep"), vec_new(heap->arena), vec_new(heap->arena), rir_tbool(heap), let);
  if (!vec_push(&items, (void *)fn)) heap->oom = true;
  return rir_module(str_lit("let.mech"), items);
}

static void emit_faults(void) {
  Arena base_arena = arena_new(4096);
  Heap base = heap_new(&base_arena);
  RirModule module = let_module(&base);
  allocation_count = 0;
  Str expected = emit_text(&base, &module);
  size_t count = allocation_count;
  expect(!base.oom && expected.len > 0, "healthy let emission", 0);
  for (size_t i = 0; i < count; i++) {
    Arena arena = arena_new(4096);
    Heap heap = heap_new(&arena);
    RirModule m = let_module(&heap);
    injected = false;
    remaining = i;
    Str actual = emit_text(&heap, &m);
    remaining = SIZE_MAX;
    expect(injected, "emission fault reached", i);
    expect(heap.oom || str_eq(actual, expected), "emission cannot drop the let or tail", i);
    if (!heap.oom && !str_eq(actual, expected)) printf("emitted: %.*s\n", (int)(actual.len < 512 ? actual.len : 512), actual.data);
    arena_free(&arena);
  }
  arena_free(&base_arena);
}

static void erasure_faults(void) {
  Str source = str_lit("mu Signal : Type 0 with\n"
                       "| silent : Signal\n"
                       "| ready : Signal\n"
                       "\n"
                       "def identity : Signal -> Signal :=\n"
                       "  fun (s : Signal) => s\n");
  Arena base_arena = arena_new(4096);
  Heap base = heap_new(&base_arena);
  allocation_count = 0;
  EtModuleResult result = erase_typed_emit1(&base, str_lit("signal.mech"), source);
  size_t count = allocation_count;
  Str expected = result.ok ? emit_text(&base, &result.module) : str_empty();
  expect(result.ok && !base.oom && expected.len > 0, "healthy family erasure", 0);
  for (size_t i = 0; i < count; i++) {
    Arena arena = arena_new(4096);
    Heap heap = heap_new(&arena);
    injected = false;
    remaining = i;
    EtModuleResult actual = erase_typed_emit1(&heap, str_lit("signal.mech"), source);
    remaining = SIZE_MAX;
    expect(injected, "erasure fault reached", i);
    if (actual.ok && !heap.oom) {
      Str text = emit_text(&heap, &actual.module);
      expect(heap.oom || str_eq(text, expected), "erasure cannot drop constructors or definitions", i);
      if (!heap.oom && !str_eq(text, expected)) printf("erased: %.*s\n", (int)(text.len < 512 ? text.len : 512), text.data);
    }
    arena_free(&arena);
  }
  arena_free(&base_arena);
}

int main(void) {
  emit_faults();
  erasure_faults();
  if (failures == 0) printf("RUST-EMIT-OOM-OK checks=%zu\n", checks);
  return failures == 0 ? 0 : 1;
}
