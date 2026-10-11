/* C unit driver for the runtime (make c-test). Prints RT-OK checks=N on success. */
#include "rt.h"

#include <stdio.h>
#include <string.h>

typedef struct {
  int checks;
  int failures;
} RtTally;

static void expect(RtTally *tally, bool ok, const char *name) {
  tally->checks += 1;
  if (!ok) {
    tally->failures += 1;
    printf("FAIL %s\n", name);
  }
}

static void *double_item(void *ctx, void *item) {
  Arena *arena = ctx;
  uint64_t *out = arena_alloc(arena, sizeof(uint64_t));
  *out = *(uint64_t *)item * 2;
  return out;
}

static bool is_even(void *ctx, void *item) {
  (void)ctx;
  return *(uint64_t *)item % 2 == 0;
}

static void *sum_items(void *ctx, void *acc, void *item) {
  (void)ctx;
  *(uint64_t *)acc += *(uint64_t *)item;
  return acc;
}

static void count_entry(void *ctx, Str key, void *value) {
  (void)key;
  (void)value;
  *(int *)ctx += 1;
}

static Str nat_text(Arena *arena, Str decimal, Nat (*op)(Arena *, Nat, Nat), Str other) {
  NatParse a = nat_parse_decimal(arena, decimal);
  NatParse b = nat_parse_decimal(arena, other);
  return nat_to_decimal(arena, op(arena, a.value, b.value));
}

static int rt_test_program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  RtTally tally = {0, 0};
  Arena arena = arena_new(1 << 16);

  /* arena */
  char *big = arena_alloc(&arena, 1 << 20);
  expect(&tally, big != NULL && big[0] == 0, "arena alloc beyond chunk");
  expect(&tally, arena_alloc(&arena, 0) != NULL, "arena alloc zero");

  /* strings */
  Str hello = str_lit("hello");
  expect(&tally, str_eq(str_cat(&arena, hello, str_lit(" world")), str_lit("hello world")), "str cat");
  expect(&tally, str_cmp(str_lit("abc"), str_lit("abd")) < 0 && str_cmp(hello, hello) == 0, "str cmp");
  expect(&tally, str_slice(hello, 1, 3).status == STR_INDEX_OK && str_eq(str_slice(hello, 1, 3).value, str_lit("el")), "str slice");
  expect(&tally, str_slice(hello, 3, 9).status == STR_INDEX_OUT_OF_RANGE, "str slice bounds");
  expect(&tally, str_at(hello, 9).status == STR_INDEX_OUT_OF_RANGE && str_at(hello, 4).value == 'o', "str at");
  expect(&tally, str_find(hello, str_lit("ll")).found && str_find(hello, str_lit("ll")).index == 2, "str find");
  expect(&tally, !str_find(hello, str_lit("z")).found, "str find missing");
  expect(&tally, str_starts_with(hello, str_lit("he")) && str_ends_with(hello, str_lit("lo")), "str affixes");
  expect(&tally, str_eq(str_of_u64(&arena, 18446744073709551615ull), str_lit("18446744073709551615")), "str of u64");
  expect(&tally, str_eq(str_of_i64(&arena, -42), str_lit("-42")), "str of i64");
  expect(&tally, str_cstr(&arena, str_n("a\0b", 3)) == NULL, "str cstr rejects NUL");
  StrBuf buf = strbuf_new(&arena);
  bool pushed = strbuf_push(&buf, hello) && strbuf_push_byte(&buf, '-') && strbuf_push_u64(&buf, 7);
  expect(&tally, pushed && str_eq(strbuf_str(&buf), str_lit("hello-7")), "strbuf");

  /* vectors */
  Vec vec = vec_new(&arena);
  uint64_t numbers[5] = {1, 2, 3, 4, 5};
  bool all = true;
  for (size_t i = 0; i < 5; i++) all = all && vec_push(&vec, &numbers[i]);
  expect(&tally, all && vec.len == 5, "vec push");
  expect(&tally, vec_get(&vec, 5).status == VEC_GET_OUT_OF_RANGE, "vec get bounds");
  expect(&tally, vec_get(&vec, 2).status == VEC_GET_OK && *(uint64_t *)vec_get(&vec, 2).value == 3, "vec get");
  Vec doubled = vec_map(&arena, &vec, double_item, &arena);
  Vec evens = vec_filter(&arena, &doubled, is_even, NULL);
  uint64_t total = 0;
  vec_fold(&evens, &total, sum_items, NULL);
  expect(&tally, evens.len == 5 && total == 30, "vec map filter fold");
  Vec reversed = vec_reverse(&arena, &vec);
  expect(&tally, *(uint64_t *)vec_get(&reversed, 0).value == 5 && vec_concat(&arena, &vec, &reversed).len == 10, "vec reverse concat");

  /* maps */
  Map map = map_new(&arena);
  bool inserted = true;
  for (uint64_t i = 0; i < 100; i++) {
    Str key = str_of_u64(&arena, i);
    inserted = inserted && map_put(&map, key, &numbers[i % 5]) == MAP_PUT_INSERTED;
  }
  expect(&tally, inserted && map.len == 100, "map insert 100");
  expect(&tally, map_put(&map, str_lit("7"), &numbers[0]) == MAP_PUT_REPLACED && map.len == 100, "map replace");
  expect(&tally, map_get(&map, str_lit("99")).found && !map_get(&map, str_lit("100")).found, "map get");
  int seen = 0;
  map_each(&map, count_entry, &seen);
  Vec keys = map_keys(&arena, &map);
  expect(&tally, seen == 100 && keys.len == 100 && str_eq(*(Str *)vec_get(&keys, 0).value, str_lit("0")), "map order");

  /* big naturals */
  Str big_a = str_lit("340282366920938463463374607431768211456");
  Str big_b = str_lit("18446744073709551616");
  expect(&tally, str_eq(nat_text(&arena, big_a, nat_add, big_b), str_lit("340282366920938463481821351505477763072")), "nat add");
  expect(&tally, str_eq(nat_text(&arena, big_b, nat_mul, big_b), big_a), "nat mul");
  NatParse pa = nat_parse_decimal(&arena, big_a);
  NatParse pb = nat_parse_decimal(&arena, big_b);
  NatDiv div = nat_divmod(&arena, pa.value, nat_succ(&arena, pb.value));
  expect(&tally, div.status == NAT_DIV_OK && str_eq(nat_to_decimal(&arena, div.quotient), str_lit("18446744073709551615")) && str_eq(nat_to_decimal(&arena, div.remainder), str_lit("1")), "nat divmod");
  expect(&tally, nat_divmod(&arena, pa.value, nat_zero()).status == NAT_DIV_BY_ZERO, "nat div by zero");
  NatSub sub = nat_sub(&arena, pa.value, pb.value);
  expect(&tally, sub.status == NAT_SUB_OK && str_eq(nat_to_decimal(&arena, sub.value), str_lit("340282366920938463444927863358058659840")), "nat sub");
  expect(&tally, nat_sub(&arena, pb.value, pa.value).status == NAT_SUB_UNDERFLOW, "nat sub underflow");
  expect(&tally, nat_parse_decimal(&arena, str_lit("12x")).status == NAT_PARSE_BAD_DIGIT, "nat parse bad digit");
  expect(&tally, nat_parse_decimal(&arena, str_empty()).status == NAT_PARSE_EMPTY, "nat parse empty");
  expect(&tally, str_eq(nat_to_decimal(&arena, nat_of_u64(&arena, 0)), str_lit("0")), "nat zero text");
  expect(&tally, str_eq(nat_to_decimal(&arena, nat_of_u64(&arena, 1000000000)), str_lit("1000000000")), "nat 1e9 text");
  NatToU64 back = nat_to_u64(nat_parse_decimal(&arena, str_lit("18446744073709551615")).value);
  expect(&tally, back.status == NAT_TO_U64_OK && back.value == 18446744073709551615ull, "nat to u64");
  expect(&tally, nat_to_u64(pa.value).status == NAT_TO_U64_OVERFLOW, "nat to u64 overflow");
  expect(&tally, nat_cmp(pa.value, pb.value) > 0 && nat_cmp(pb.value, pb.value) == 0, "nat cmp");

  /* effects */
  FsResult temp = fs_temp_dir(&arena, str_lit("mech-rt-test"));
  expect(&tally, temp.status == FS_OK && temp.value.len > 0, "fs temp dir");
  Str file = str_cat(&arena, temp.value, str_lit("/data.txt"));
  Str staged = str_cat(&arena, temp.value, str_lit("/staged.txt"));
  expect(&tally, fs_write(file, str_lit("payload\n")).status == FS_OK, "fs write");
  FsResult read = fs_read(&arena, file);
  expect(&tally, read.status == FS_OK && str_eq(read.value, str_lit("payload\n")), "fs read");
  expect(&tally, fs_read(&arena, str_cat(&arena, temp.value, str_lit("/missing"))).status == FS_ERRNO, "fs read missing");
  FsResult stage = fs_stage(&arena, staged);
  expect(&tally, stage.status == FS_OK && fs_exists(stage.value) && fs_rename(stage.value, staged).status == FS_OK && fs_exists(staged) && !fs_exists(stage.value), "fs stage rename");
  FsResult temp_file = fs_temp_file(&arena, str_lit("mech-rt-"), str_lit(".txt"));
  expect(&tally, temp_file.status == FS_OK && str_ends_with(temp_file.value, str_lit(".txt")) && fs_unlink(temp_file.value).status == FS_OK, "fs temp file");
  Str out_path = str_cat(&arena, temp.value, str_lit("/out.txt"));
  Str err_path = str_cat(&arena, temp.value, str_lit("/err.txt"));
  const char *const echo_argv[] = {"/bin/sh", "-c", "printf hi; exit 3", NULL};
  ProcessResult ran = host_process_run(echo_argv, 3, out_path, err_path);
  FsResult captured = fs_read(&arena, out_path);
  expect(&tally, ran.status == PROCESS_RAN && ran.code == 3 && captured.status == FS_OK && str_eq(captured.value, str_lit("hi")), "process run");
  expect(&tally, host_executable_path(&arena).len > 0, "executable path");
  expect(&tally, fs_unlink(file).status == FS_OK && fs_rmdir(staged).status == FS_OK && fs_unlink(out_path).status == FS_OK && fs_unlink(err_path).status == FS_OK && fs_rmdir(temp.value).status == FS_OK, "fs unlink rmdir");
  expect(&tally, fs_rmdir(temp.value).status == FS_ERRNO, "fs rmdir missing");

  arena_free(&arena);
  if (tally.failures == 0) printf("RT-OK checks=%d\n", tally.checks);
  return tally.failures == 0 ? 0 : 1;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, rt_test_program); }
