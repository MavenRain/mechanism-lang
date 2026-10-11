/* C port of bend2/tests/kernel_global_index.bend: global name index boundaries (duplicates, replacement,
 * family removal, unicode and NUL keys, 1000 stress keys and the runtime keys from the arguments). */
#include "global_lookup_common.h"

#include <string.h>

/* List<String> as a Str array */
typedef struct {
  const Str *items;
  size_t len;
} StrList;

static StrList str_list(const Str *items, size_t len) {
  StrList list;
  list.items = items;
  list.len = len;
  return list;
}

#define NAMES(...) str_list((Str[]){__VA_ARGS__}, sizeof((Str[]){__VA_ARGS__}) / sizeof(Str))

static Str bytes(const char *data, size_t len) {
  Str s;
  s.data = data;
  s.len = len;
  return s;
}

/* require(ok, label): IO.die(1, label) on a failed check; the host failure comes first when the heap
 * ran out, because the result is then meaningless. */
static int require(Heap *heap, bool ok, const char *label) {
  if (heap->oom) return driver_host_failure();
  if (ok) return 0;
  return driver_die(1, label);
}

#define REQUIRE(heap, ok, label) \
  do { \
    int rc_ = require((heap), (ok), (label)); \
    if (rc_ != 0) return rc_; \
  } while (0)

#define RUN(call) \
  do { \
    int rc_ = (call); \
    if (rc_ != 0) return rc_; \
  } while (0)

/* names + same_names over the entry list */
static bool same_entry_names(const EntryRows *rows, StrList expected) {
  size_t i = 0;
  const EntryRows *row = rows;
  for (; row != NULL && i < expected.len; row = row->rest, i++) {
    if (!str_eq(row->name, expected.items[i])) return false;
  }
  return row == NULL && i == expected.len;
}

/* names + same_names over the family list */
static bool same_family_names(const FamilyRows *rows, StrList expected) {
  size_t i = 0;
  const FamilyRows *row = rows;
  for (; row != NULL && i < expected.len; row = row->rest, i++) {
    if (!str_eq(row->name, expected.items[i])) return false;
  }
  return row == NULL && i == expected.len;
}

static bool inventory(Globals globals, StrList expected, StrList expected_families) {
  return same_entry_names(globals.entries, expected) && same_family_names(globals.families, expected_families);
}

/* keys(): "", "a", "ab", "abc", "b", e acute, e + combining acute, grinning face, zhong, NUL, NUL a,
 * a + grinning face, grinning face + a */
static const Str fixed_keys[13] = {
    {"", 0},
    {"a", 1},
    {"ab", 2},
    {"abc", 3},
    {"b", 1},
    {"\xC3\xA9", 2},
    {"e\xCC\x81", 3},
    {"\xF0\x9F\x98\x80", 4},
    {"\xE4\xB8\xAD", 3},
    {"\0", 1},
    {"\0a", 2},
    {"a\xF0\x9F\x98\x80", 5},
    {"\xF0\x9F\x98\x80"
     "a",
     5},
};

static StrList keys(void) { return str_list(fixed_keys, 13u); }

static Globals populate(Heap *heap, StrList ks, Globals globals) {
  Globals acc = globals;
  for (size_t i = 0; i < ks.len; i++) {
    Str key = ks.items[i];
    acc = global_add_family(heap, key, lookup_family(heap, key), global_add(heap, key, lookup_entry(heap, key), acc));
  }
  return acc;
}

static int verify_keys(Heap *heap, StrList ks, Globals globals) {
  for (size_t i = 0; i < ks.len; i++) {
    Str key = ks.items[i];
    REQUIRE(heap, lookup_entry_is(global_find(key, globals), key), "entry key mismatch");
    REQUIRE(heap, lookup_family_is(global_find_family(key, globals), key), "family key mismatch");
  }
  return 0;
}

/* stress_keys(count) = [Nat.show(count - 1), ..., Nat.show(0)] */
static StrList stress_keys(Heap *heap, uint64_t count) {
  if (count == 0) return str_list(NULL, 0);
  Str *items = heap_alloc(heap, (size_t)count * sizeof(Str));
  if (items == NULL) return str_list(NULL, 0);
  for (uint64_t i = 0; i < count; i++) {
    items[i] = heap_nat_show(heap, heap_nat_of_u64(heap, count - 1u - i));
  }
  return str_list(items, (size_t)count);
}

static int fixed(Heap *heap) {
  Str x = str_lit("x");
  Str y = str_lit("y");
  Globals start = global_of_lists(heap, lookup_entries(heap), lookup_families(heap));
  Globals changed = global_add(heap, x, lookup_entry(heap, str_lit("replacement")), start);
  Globals family_changed = global_add_family(heap, x, lookup_family(heap, str_lit("replacement")), changed);
  Globals removed = global_without_family(heap, x, family_changed);
  Globals unicode = populate(heap, keys(), global_empty());
  StrList stress_list = stress_keys(heap, 1000u);
  Globals stress = populate(heap, stress_list, global_empty());
  REQUIRE(heap, lookup_entry_is(global_find(x, start), str_lit("first")), "raw duplicate first entry");
  REQUIRE(heap, lookup_family_is(global_find_family(x, start), str_lit("first")), "raw duplicate first family");
  REQUIRE(heap, lookup_entry_is(global_find(x, changed), str_lit("replacement")), "replacement lookup");
  REQUIRE(heap, inventory(changed, NAMES(x, y), NAMES(x, y, x)), "replacement ordering");
  REQUIRE(heap, lookup_entry_is(global_find(x, start), str_lit("first")), "entry persistence");
  REQUIRE(heap, lookup_family_is(global_find_family(x, family_changed), str_lit("replacement")), "family replacement");
  REQUIRE(heap, inventory(family_changed, NAMES(x, y), NAMES(x, y)), "family ordering");
  REQUIRE(heap, lookup_family_is(global_find_family(x, changed), str_lit("first")), "family persistence");
  REQUIRE(heap, lookup_missing_family(global_find_family(x, removed)), "family removal");
  REQUIRE(heap, lookup_entry_is(global_find(x, removed), str_lit("replacement")), "namespace separation");
  REQUIRE(heap, inventory(removed, NAMES(x, y), NAMES(y)), "removal ordering");
  REQUIRE(heap, lookup_missing_entry(global_find(str_lit("absent"), unicode)), "missing entry");
  REQUIRE(heap, lookup_missing_family(global_find_family(str_lit("absent"), unicode)), "missing family");
  RUN(verify_keys(heap, keys(), unicode));
  RUN(verify_keys(heap, stress_list, stress));
  return 0;
}

/* SCon{Chr{0}, key} when before, key ++ SCon{Chr{0}, SNil{}} otherwise */
static Str with_nul(Heap *heap, Str key, bool before) {
  char *data = heap_alloc(heap, key.len + 1u);
  if (data == NULL) return str_empty();
  char *body = before ? data + 1 : data;
  if (key.len > 0) memcpy(body, key.data, key.len);
  if (before) {
    data[0] = '\0';
  } else {
    data[key.len] = '\0';
  }
  return bytes(data, key.len + 1u);
}

/* dynamic_keys(args): for each argument, the key, NUL ++ key and key ++ NUL */
static StrList dynamic_keys(Heap *heap, int argc, char **argv) {
  size_t count = argc > 1 ? (size_t)(argc - 1) * 3u : 0u;
  if (count == 0) return str_list(NULL, 0);
  Str *items = heap_alloc(heap, count * sizeof(Str));
  if (items == NULL) return str_list(NULL, 0);
  for (size_t i = 1; i < (size_t)argc; i++) {
    Str key = str_lit(argv[i]);
    items[(i - 1u) * 3u] = key;
    items[(i - 1u) * 3u + 1u] = with_nul(heap, key, true);
    items[(i - 1u) * 3u + 2u] = with_nul(heap, key, false);
  }
  return str_list(items, count);
}

static int boundary(Heap *heap, Str key) {
  Str other = heap_str_cat(heap, key, str_lit("/other"));
  Str missing = heap_str_cat(heap, key, str_lit("/missing"));
  const EntryRows *es = global_entry_rows(heap, key, lookup_entry(heap, str_lit("last")), NULL);
  es = global_entry_rows(heap, other, lookup_entry(heap, str_lit("other")), es);
  es = global_entry_rows(heap, key, lookup_entry(heap, str_lit("first")), es);
  const FamilyRows *fs = global_family_rows(heap, key, lookup_family(heap, str_lit("last")), NULL);
  fs = global_family_rows(heap, other, lookup_family(heap, str_lit("other")), fs);
  fs = global_family_rows(heap, key, lookup_family(heap, str_lit("first")), fs);
  Globals raw = global_of_lists(heap, es, fs);
  Globals changed = global_add(heap, key, lookup_entry(heap, str_lit("new-entry")), raw);
  Globals fchanged = global_add_family(heap, key, lookup_family(heap, str_lit("new-family")), changed);
  Globals removed = global_without_family(heap, key, fchanged);
  Globals readded = global_add_family(heap, key, lookup_family(heap, str_lit("again")), removed);
  Globals fresh = global_add(heap, missing, lookup_entry(heap, str_lit("fresh")), fchanged);
  REQUIRE(heap, lookup_entry_is(global_find(key, raw), str_lit("first")), "dynamic duplicate entry");
  REQUIRE(heap, lookup_family_is(global_find_family(key, raw), str_lit("first")), "dynamic duplicate family");
  REQUIRE(heap, inventory(raw, NAMES(key, other, key), NAMES(key, other, key)), "raw order");
  REQUIRE(heap, lookup_entry_is(global_find(key, changed), str_lit("new-entry")), "replace entry");
  REQUIRE(heap, lookup_family_is(global_find_family(key, changed), str_lit("first")), "independent namespaces");
  REQUIRE(heap, inventory(changed, NAMES(key, other), NAMES(key, other, key)), "entry replacement order");
  REQUIRE(heap, lookup_family_is(global_find_family(key, fchanged), str_lit("new-family")), "replace family");
  REQUIRE(heap, inventory(fchanged, NAMES(key, other), NAMES(key, other)), "family replacement order");
  REQUIRE(heap, lookup_missing_family(global_find_family(key, removed)), "family delete");
  REQUIRE(heap, lookup_entry_is(global_find(key, removed), str_lit("new-entry")), "delete preserves entry");
  REQUIRE(heap, lookup_family_is(global_find_family(other, removed), str_lit("other")), "delete preserves neighbor");
  REQUIRE(heap, inventory(removed, NAMES(key, other), NAMES(other)), "delete order");
  REQUIRE(heap, lookup_family_is(global_find_family(key, readded), str_lit("again")), "reinsert deleted family");
  REQUIRE(heap, inventory(readded, NAMES(key, other), NAMES(key, other)), "reinsert order");
  REQUIRE(heap, lookup_entry_is(global_find(missing, fresh), str_lit("fresh")), "fresh insert");
  REQUIRE(heap, inventory(fresh, NAMES(missing, key, other), NAMES(key, other)), "fresh insertion order");
  REQUIRE(heap, inventory(global_without_family(heap, missing, raw), NAMES(key, other, key), NAMES(key, other, key)),
          "absent delete");
  REQUIRE(heap, lookup_missing_entry(global_find(missing, raw)), "missing entry");
  REQUIRE(heap, lookup_missing_family(global_find_family(missing, raw)), "missing family");
  REQUIRE(heap, lookup_entry_is(global_find(key, raw), str_lit("first")), "entry persistence");
  REQUIRE(heap, lookup_family_is(global_find_family(key, raw), str_lit("first")), "family persistence");
  return 0;
}

static int dynamic_cases(Heap *heap, StrList ks, Globals globals) {
  for (size_t i = 0; i < ks.len; i++) {
    Str key = ks.items[i];
    Globals updated = global_add_family(heap, key, lookup_family(heap, str_lit("updated-family")),
                                        global_add(heap, key, lookup_entry(heap, str_lit("updated-entry")), globals));
    Globals removed = global_without_family(heap, key, updated);
    RUN(boundary(heap, key));
    REQUIRE(heap, lookup_entry_is(global_find(key, updated), str_lit("updated-entry")), "pooled replace entry");
    REQUIRE(heap, lookup_family_is(global_find_family(key, updated), str_lit("updated-family")), "pooled replace family");
    REQUIRE(heap, lookup_missing_family(global_find_family(key, removed)), "pooled delete family");
    REQUIRE(heap, lookup_entry_is(global_find(key, globals), key), "pooled entry persistence");
    REQUIRE(heap, lookup_family_is(global_find_family(key, globals), key), "pooled family persistence");
  }
  return 0;
}

static int arguments(Heap *heap, int argc, char **argv) {
  StrList runtime_keys = dynamic_keys(heap, argc, argv);
  Globals globals = populate(heap, runtime_keys, global_empty());
  REQUIRE(heap, runtime_keys.len != 0, "runtime keys required");
  RUN(fixed(heap));
  RUN(verify_keys(heap, runtime_keys, globals));
  RUN(dynamic_cases(heap, runtime_keys, globals));
  RUN(verify_keys(heap, runtime_keys, globals));
  StrBuf out = strbuf_new(heap->arena);
  bool ok = strbuf_push(&out, str_lit("GLOBAL-INDEX-OK boundaries=13 unicode=13 stress=1000 namespaces=2 runtime-keys=")) &&
            strbuf_push_u64(&out, (uint64_t)runtime_keys.len) && strbuf_push(&out, str_lit(" per-key=30"));
  if (!ok || heap->oom) return driver_host_failure();
  return driver_print(strbuf_str(&out));
}

/* main: IO.args() then arguments(args) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = arguments(&heap, argc, argv);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
