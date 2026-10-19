/* Port of tests/relational_support.bend (52 lines): the MD5 vectors of relational_digest
 * and the directory listing of relational_fs. The Bend driver has no JS build, no _bend2 wrapper
 * and no harness call, so there is no oracle record: c-test makes the fixture directory
 * _tcc/relational_support_fixture (the files "plain" and "line\nbreak"), runs the driver on it and
 * compares the output with c/tests/relational_support.expected. Usage: the fixture directory. */
#include "driver.h"
#include "relational_digest.h"
#include "relational_fs.h"

typedef struct {
  const char *text;
  uint32_t limit;
  const char *expected;
} DigestCase;

/* require(ok, message) */
static int require(bool ok, Str message) {
  if (ok) return 0;
  return driver_die_text(1, message);
}

/* missing(result) */
static bool missing(RelationalDir result) { return result.code != 0; }

/* has(names, name) */
static bool has(const Vec *names, Str name) {
  for (size_t i = 0; i < names->len; i++) {
    VecGet got = vec_get(names, i);
    if (got.status == VEC_GET_OUT_OF_RANGE || got.value == NULL) return false;
    if (str_eq(*(const Str *)got.value, name)) return true;
  }
  return false;
}

/* present(result) */
static bool present(RelationalDir result) {
  if (result.code != 0) return false;
  return has(&result.names, str_lit("plain")) && has(&result.names, str_lit("line\nbreak"));
}

/* digest(text, limit, expected) */
static int digest(Heap *heap, DigestCase item) {
  Str expected = str_lit(item.expected);
  Str actual = relational_digest(heap, str_lit(item.text), item.limit);
  Str message = str_empty();
  if (heap->oom) return driver_host_failure();
  if (str_eq(actual, expected)) return 0;
  message = heap_str_cat(heap, str_lit("digest "), expected);
  if (heap->oom) return driver_host_failure();
  return require(false, message);
}

static const DigestCase digests[] = {
  {"", 0, "d41d8cd98f00b204e9800998ecf8427e"},
  {"abc", 0, "900150983cd24fb0d6963f7d28e17f72"},
  {"abcdefghijklmnopqrstuvwxyz", 0, "c3fcd3d76192e4007dfb496cca67e13b"},
  {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 0, "d174ab98d277d9f5a5611c2c9f419d9f"},
  {"12345678901234567890123456789012345678901234567890123456789012345678901234567890", 0,
   "57edf4a22be3c955ac49da2e2107b67a"},
  {"abcdef", 3, "900150983cd24fb0d6963f7d28e17f72"},
};

/* run(path) */
static int run(Heap *heap, Str path) {
  for (size_t i = 0; i < sizeof digests / sizeof digests[0]; i++) {
    int code = digest(heap, digests[i]);
    if (code != 0) return code;
  }
  RelationalDir bad = relational_list_dir(heap, heap_str_cat(heap, path, str_lit("/absent")));
  if (heap->oom) return driver_host_failure();
  int code = require(missing(bad), str_lit("missing directory accepted"));
  if (code != 0) return code;
  RelationalDir good = relational_list_dir(heap, path);
  if (heap->oom) return driver_host_failure();
  code = require(present(good), str_lit("filenames or reused IO status changed"));
  if (code != 0) return code;
  return driver_print(str_lit("RELATIONAL-SUPPORT-OK assertions=8"));
}

/* arguments(args): the first argument is the fixture directory; the rest are ignored. */
static int arguments(Heap *heap, int argc, char **argv) {
  if (argc < 2) return driver_die(1, "expected fixture directory");
  return run(heap, str_lit(argv[1]));
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
