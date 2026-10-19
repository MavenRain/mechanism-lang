/* Port of tests/io_driver.bend: one file effect of io.bend (c/cli/io.c) per run, chosen
 * by the arguments. Success prints OK or the text; a failure dies with exit 1 and the message; any
 * other argument list dies with exit 64 and the usage line. dev/c-io-check.py drives it. */
#include "cli.h"
#include "driver.h"

#include <string.h>

static int unit_result(Heap *heap, IoResult result) {
  if (heap->oom) return driver_host_failure();
  switch (result.status) {
  case IO_DONE: return driver_print(str_lit("OK"));
  case IO_FAIL: return driver_die_text(1, result.value);
  }
  return driver_die_text(1, result.value);
}

static int text_result(Heap *heap, IoResult result) {
  if (heap->oom) return driver_host_failure();
  switch (result.status) {
  case IO_DONE: return driver_print(result.value);
  case IO_FAIL: return driver_die_text(1, result.value);
  }
  return driver_die_text(1, result.value);
}

static IoFile *io_file_new(Heap *heap, const char *name, const char *contents) {
  IoFile *file = heap_alloc(heap, sizeof(IoFile));
  if (file == NULL) return NULL;
  file->name = str_lit(name);
  file->contents = str_lit(contents);
  return file;
}

/* [Common.Pair{first}, Common.Pair{second}]; heap->oom when a cell fails. */
static Vec two_files(Heap *heap, IoFile *first, IoFile *second) {
  Vec files = vec_new(heap->arena);
  if (first == NULL || second == NULL || !vec_push(&files, first) || !vec_push(&files, second)) heap->oom = true;
  return files;
}

static int publish(Heap *heap, Str directory, IoFile *first, IoFile *second) {
  Vec files = two_files(heap, first, second);
  if (heap->oom) return driver_host_failure();
  return unit_result(heap, io_publish(heap, directory, &files));
}

static bool command(int argc, char **argv, int count, const char *name) {
  return argc == count + 1 && strcmp(argv[1], name) == 0;
}

static int arguments(Heap *heap, int argc, char **argv) {
  static const char bytes[] = "\x00\xff\x80\n";
  if (command(argc, argv, 2, "read")) return text_result(heap, io_read_file(heap, str_lit(argv[2])));
  if (command(argc, argv, 3, "write")) return unit_result(heap, io_write_file(heap, str_lit(argv[2]), str_lit(argv[3])));
  if (command(argc, argv, 2, "bytes")) return unit_result(heap, io_write_bytes(heap, str_lit(argv[2]), str_n(bytes, 4)));
  if (command(argc, argv, 2, "publish"))
    return publish(heap, str_lit(argv[2]), io_file_new(heap, "first.json", "{\"value\":42}\n"),
                   io_file_new(heap, "second.tsv", "name\tvalue\n"));
  if (command(argc, argv, 2, "publish-fail"))
    return publish(heap, str_lit(argv[2]), io_file_new(heap, "first", "written"), io_file_new(heap, "absent/second", "failure"));
  if (command(argc, argv, 1, "temp")) return text_result(heap, io_temp_file(heap, str_lit("mechanism-io-"), str_lit(".wasm")));
  return driver_die(64, "usage: io_driver read|write|bytes|publish|publish-fail PATH [CONTENTS] | temp");
}

static int entry(int argc, char **argv) {
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  int code = arguments(&heap, argc, argv);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, entry); }
