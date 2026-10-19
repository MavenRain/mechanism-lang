/* Port of tests/import_translate.bend (oracle label import-translate): reads the one NDJSON
 * source argument with Export.read_string, translates it and prints Render.scoped_summary as JSON;
 * dies with 1 and "LINE:message" on an import or translation error, or with 64 and the usage text on
 * any other argument count. */
#include "driver.h"
#include "import.h"

/* U32.show(line) ++ ":" ++ message */
static Str located(Heap *heap, ExportError error) {
  return heap_str_cat(heap, heap_str_cat(heap, str_of_u64(heap->arena, error.line), str_lit(":")), error.message);
}

/* translated(value) */
static int translated(Heap *heap, TranslateResult value) {
  if (value.kind == JSON_DONE) {
    return driver_print(json_to_string(heap, render_scoped_summary(heap, value.value)));
  }
  return driver_die_text(1, located(heap, value.error));
}

/* parsed(value) */
static int parsed(Heap *heap, ExportResult value) {
  if (value.kind == JSON_DONE) return translated(heap, translate_translate(heap, &value.value));
  return driver_die_text(1, located(heap, value.error));
}

/* arguments(args): the arguments after the program name. */
static int arguments(Heap *heap, int argc, char **argv) {
  if (argc == 2) return parsed(heap, export_read_string(heap, str_lit(argv[1])));
  return driver_die(64, "usage: import_translate NDJSON");
}

/* main() */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = arguments(&heap, argc, argv);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
