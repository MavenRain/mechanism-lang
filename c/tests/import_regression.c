#include "driver.h"
#include "import.h"

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  const char *invalid[] = {
      "\"\x80\"", "\"\xc0\xaf\"", "\"\xe2\x82\"", "\"\xe0\x80\x80\"",
      "\"\xf4\x90\x80\x80\"", "\"\xff\"", "\"\xed\xa0\x80\"",
      "{\"\x80\":null}"};
  int failures = 0;
  for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
    if (json_parse(&heap, str_lit(invalid[i])).kind != JSON_FAIL) {
      fprintf(stderr, "FAIL invalid UTF-8 case %zu was accepted\n", i);
      ++failures;
    }
  }
  Str valid = str_lit("\"\xc2\x80\xe0\xa0\x80\xef\xbf\xbf\xf4\x8f\xbf\xbf\"");
  JsonResult parsed = json_parse(&heap, valid);
  if (parsed.kind != JSON_DONE || !str_eq(json_to_string(&heap, parsed.value), valid)) {
    fprintf(stderr, "FAIL valid UTF-8 did not round-trip\n");
    ++failures;
  }
  int code = heap.oom ? driver_host_failure() : (failures == 0 ? driver_print(str_lit("PASS import_regression")) : 1);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
