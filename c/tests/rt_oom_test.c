/* The real arena allocator is renamed only in its object file for fault injection. */
#include "rt.h"

#include <stdio.h>

void *rt_test_arena_alloc_real(Arena *arena, size_t size);

static size_t until_failure = SIZE_MAX;
static size_t alloc_calls;
static int failures;
static int checks;

void *arena_alloc(Arena *arena, size_t size) {
  alloc_calls += 1;
  if (until_failure == 0) {
    until_failure = SIZE_MAX;
    return NULL;
  }
  if (until_failure != SIZE_MAX) until_failure -= 1;
  return rt_test_arena_alloc_real(arena, size);
}

static void expect(bool ok, const char *name) {
  checks += 1;
  if (!ok) {
    failures += 1;
    printf("FAIL %s\n", name);
  }
}

static void reset_fault(void) { until_failure = SIZE_MAX; }

int main(void) {
  Arena arena = arena_new(4096);
  Nat a = nat_of_u64(&arena, 100);
  Nat b = nat_of_u64(&arena, 3);
  until_failure = 0;
  NatSub sub = nat_sub(&arena, a, b);
  expect(sub.status != NAT_SUB_OK && sub.status != NAT_SUB_UNDERFLOW, "nat sub OOM is distinct");
  for (size_t i = 0; i < 2; i++) {
    until_failure = i;
    NatDiv div = nat_divmod(&arena, a, b);
    expect(div.status != NAT_DIV_OK && div.status != NAT_DIV_BY_ZERO, "nat div OOM is distinct");
  }
  until_failure = 0;
  NatParse parsed = nat_parse_decimal(&arena, str_lit("123"));
  expect(parsed.status != NAT_PARSE_OK && parsed.status != NAT_PARSE_EMPTY &&
         parsed.status != NAT_PARSE_BAD_DIGIT, "nat parse OOM is distinct");
  reset_fault();

  StrBuf buf = strbuf_new(&arena);
  strbuf_push(&buf, str_lit("value="));
  until_failure = 0;
  bool pushed = strbuf_push_u64(&buf, 42);
  expect(!pushed || str_eq(strbuf_str(&buf), str_lit("value=42")), "strbuf cannot lose numeric append");
  reset_fault();

  for (size_t i = 0; i < 2; i++) {
    Arena map_arena = arena_new(4096);
    Map map = map_new(&map_arena);
    int value = 7;
    map_put(&map, str_lit("seed"), &value);
    until_failure = i;
    MapPutStatus put = map_put(&map, str_lit("new"), &value);
    reset_fault();
    expect(put == MAP_PUT_OOM && map.len == 1 && map_has(&map, str_lit("seed")) &&
           !map_has(&map, str_lit("new")), "map insertion is atomic on OOM");
    arena_free(&map_arena);
  }

  FsResult temp = fs_temp_dir(&arena, str_lit("mech-rt-oom-"));
  if (temp.status != FS_OK) return 2;
  Str path = str_cat(&arena, temp.value, str_lit("/data"));
  expect(fs_write(path, str_lit("payload")).status == FS_OK, "seed OOM file");
  for (size_t i = 0; i < 2; i++) {
    until_failure = i;
    FsResult read = fs_read(&arena, path);
    reset_fault();
    expect(read.status == FS_OOM, "fs read reports allocation failure");
  }

  for (int kind = 0; kind < 3; kind++) {
    alloc_calls = 0;
    FsResult healthy = kind == 0 ? fs_stage(&arena, path) :
                       kind == 1 ? fs_temp_dir(&arena, str_lit("mech-rt-fault-")) :
                                   fs_temp_file(&arena, str_lit("mech-rt-fault-"), str_lit(".txt"));
    size_t count = alloc_calls;
    expect(healthy.status == FS_OK, "healthy temp creation");
    if (healthy.status == FS_OK) {
      if (kind == 2) fs_unlink(healthy.value); else fs_rmdir(healthy.value);
    }
    for (size_t i = 0; i < count; i++) {
      until_failure = i;
      FsResult result = kind == 0 ? fs_stage(&arena, path) :
                        kind == 1 ? fs_temp_dir(&arena, str_lit("mech-rt-fault-")) :
                                    fs_temp_file(&arena, str_lit("mech-rt-fault-"), str_lit(".txt"));
      reset_fault();
      char name[80];
      snprintf(name, sizeof name, "temp creation OOM kind=%d allocation=%zu", kind, i);
      expect(result.status == FS_OOM, name);
      if (result.status == FS_OK) {
        if (kind == 2) fs_unlink(result.value); else fs_rmdir(result.value);
      }
    }
  }
  expect(fs_unlink(path).status == FS_OK && fs_rmdir(temp.value).status == FS_OK, "OOM cleanup");
  arena_free(&arena);
  if (failures == 0) printf("RT-OOM-OK checks=%d\n", checks);
  return failures == 0 ? 0 : 1;
}
