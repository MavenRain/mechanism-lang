/* Port of tests/relational_oracle_native_probe.bend (25 lines): prints the four refusal
 * oracle answers of relational_negatives for one prefix. The Bend driver has no JS build, no
 * _bend2 wrapper and no harness call, so there is no oracle record: c-test runs it with the
 * prefixes "quantity: alpha" and "mismatch:" and compares the output with
 * c/tests/relational_oracle_native_probe.expected. Usage: one prefix argument. */
#include "driver.h"
#include "relational_negatives.h"

/* Base String.length counts characters: every byte of the UTF-8 text except the continuation bytes. */
static size_t char_length(Str s) {
  size_t count = 0;
  for (size_t i = 0; i < s.len; i++) {
    if (((unsigned char)s.data[i] & 0xC0) != 0x80) count++;
  }
  return count;
}

/* show(label, value) */
static int show(Heap *heap, const char *label, bool value) {
  Str text = heap_str_cat(heap, heap_str_cat(heap, str_lit(label), str_lit(": ")), str_lit(value ? "true" : "false"));
  if (heap->oom) return driver_host_failure();
  return driver_print(text);
}

/* probe(prefix) */
static int probe(Heap *heap, Str prefix) {
  Pin pin = {str_lit("erased-object"), str_empty(), prefix, str_empty()};
  Pin other = {str_lit("other"), str_empty(), str_lit("mismatch: other"), str_empty()};
  Vec all = vec_new(heap->arena);
  if (!vec_push(&all, &pin) || !vec_push(&all, &other)) return driver_host_failure();
  int code = show(heap, "length", char_length(prefix) > 9);
  if (code == 0) code = show(heap, "distinct", negatives_distinct_from(&pin, &all, false));
  if (code == 0) code = show(heap, "not common", !negatives_prefix_of_all(prefix, &all));
  if (code == 0) code = show(heap, "oracle", negatives_oracle_valid(&pin, &all, negatives_prefix(9, true, false)));
  return code;
}

/* arguments(args) */
static int arguments(Heap *heap, int argc, char **argv) {
  if (argc == 2) return probe(heap, str_lit(argv[1]));
  return driver_die(2, "expected prefix");
}

static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  int code = arguments(&heap, argc, argv);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
