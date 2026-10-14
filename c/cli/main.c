/* Port of bend2/main.bend: the mechanism-driver program. The first argument selects mech
 * (c/cli/mech.c), cert (c/cli/cert.c) or kanon (c/cli/kanon.c); any other argument list dies with 64
 * and the usage text. This file holds main, so the Makefile keeps it out of C_CLI_SOURCES
 * (C_CLI_MAINS) and links it only into _tcc/bin/main. */
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

/* args[index], or the empty string past the end (callers test the length first). */
static Str main_arg(const Vec *args, size_t index) {
  VecGet got = vec_get(args, index);
  switch (got.status) {
  case VEC_GET_OK: return *(const Str *)got.value;
  case VEC_GET_OUT_OF_RANGE: return str_empty();
  }
  return str_empty();
}

/* kanon (6-11): args = the Str pointers after "kanon". */
static int main_kanon(Heap *heap, const Vec *args) {
  if (args->len == 0) return kanon_usage(heap);
  return kanon_dispatch(heap, main_arg(args, 0), args, 1);
}

/* arguments (13-22) */
static int main_arguments(Heap *heap, const Vec *args) {
  Str usage = str_lit("usage: mechanism-driver mech|cert|kanon ARGUMENTS...");
  if (args->len == 0) return cli_die(64, usage);
  Vec rest = vec_new(heap->arena);
  size_t i = 1;
  for (; i < args->len; i = i + 1) {
    VecGet got = vec_get(args, i);
    switch (got.status) {
    case VEC_GET_OK:
      if (!vec_push(&rest, got.value)) return cli_host_failure();
      break;
    case VEC_GET_OUT_OF_RANGE: return cli_host_failure();
    }
  }
  Str head = main_arg(args, 0);
  if (str_eq(head, str_lit("mech"))) return mech_arguments(heap, &rest);
  if (str_eq(head, str_lit("cert"))) return cert_arguments(heap, &rest);
  if (str_eq(head, str_lit("kanon"))) return main_kanon(heap, &rest);
  return cli_die(64, usage);
}

/* main (24-27) */
static int entry(int argc, char **argv) {
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  Vec args = vec_new(heap.arena);
  boxed_args(&heap, &args, argc, argv, 1);
  int code = heap.oom ? cli_host_failure() : main_arguments(&heap, &args);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, entry); }
