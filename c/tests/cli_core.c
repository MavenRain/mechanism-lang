/* Port of tests/cli_core.bend: the kanon command line (c/cli/kanon.c) as a program.
 * No arguments prints the usage; otherwise the first argument is the command and the rest its
 * arguments. */
#include "cli.h"

/* IO.args() as a Vec of boxed Str: argv[index..argc]. */
static void boxed_args(Heap *heap, Vec *out, int argc, char **argv, int index) {
  if (index >= argc) return;
  Str *cell = heap_alloc(heap, sizeof(Str));
  if (cell == NULL || !vec_push(out, cell)) {
    heap->oom = true;
    return;
  }
  *cell = str_lit(argv[index]);
  boxed_args(heap, out, argc, argv, index + 1);
}

/* start (4-7) */
static int start(Heap *heap, const Vec *args) {
  VecGet got = vec_get(args, 0);
  switch (got.status) {
  case VEC_GET_OUT_OF_RANGE: return kanon_usage(heap);
  case VEC_GET_OK: return kanon_dispatch(heap, *(const Str *)got.value, args, 1);
  }
  return kanon_usage(heap);
}

/* main (9-12) */
static int entry(int argc, char **argv) {
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  Vec args = vec_new(heap.arena);
  boxed_args(&heap, &args, argc, argv, 1);
  int code = heap.oom ? cli_host_failure() : start(&heap, &args);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, entry); }
