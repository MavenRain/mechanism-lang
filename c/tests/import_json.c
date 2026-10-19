/* Port of tests/import_json.bend (oracle label import-json): parses the one JSON source
 * argument and prints Json.to_string of the value; dies with 1 and the parse error text, or with
 * 64 and the usage text on any other argument count. */
#include "driver.h"
#include "import.h"

/* result(value) */
static int result(Heap *heap, JsonResult value) {
  if (value.kind == JSON_DONE) return driver_print(json_to_string(heap, value.value));
  return driver_die_text(1, value.message);
}

/* arguments(args): the arguments after the program name. */
static int arguments(Heap *heap, int argc, char **argv) {
  if (argc == 2) return result(heap, json_parse(heap, str_lit(argv[1])));
  return driver_die(64, "usage: import_json JSON");
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
