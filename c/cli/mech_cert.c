/* Port of mech_cert.bend: the mech_cert program. The arguments (argv without the program name)
 * go to Cert.arguments (c/cli/cert.c), whose result is the exit code. This file holds main, so the
 * Makefile keeps it out of C_CLI_SOURCES (C_CLI_MAINS) and links it only into _tcc/bin/mech_cert. */
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

/* main (4-7) */
static int entry(int argc, char **argv) {
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  Vec args = vec_new(heap.arena);
  boxed_args(&heap, &args, argc, argv, 1);
  int code = heap.oom ? cli_host_failure() : cert_arguments(&heap, &args);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, entry); }
