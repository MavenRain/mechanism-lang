/* A <dir>/<file>.bend path in a comment names bend2/<dir>/<file>.bend in commit 8c90b64,
 * the last tree with the Bend sources. */
/* Mechanism C runtime: arena allocation, byte strings, vectors, maps, big naturals,
 * host effects and the large-stack main wrapper. One header for the whole component. */
#ifndef MECH_RT_H
#define MECH_RT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ---- arena ---- */
typedef struct ArenaChunk ArenaChunk;
typedef struct {
  ArenaChunk *head;
  size_t chunk_size;
  size_t reserved;
} Arena;

Arena arena_new(size_t chunk_size);
/* Returns NULL when the host is out of memory. Memory is zeroed. */
void *arena_alloc(Arena *arena, size_t size);
void arena_free(Arena *arena);
/* Bytes of chunk storage the arena holds now (the sum of its chunk sizes; 0 after arena_free). */
size_t arena_reserved(const Arena *arena);

/* ---- strings: immutable byte slices ---- */
typedef struct {
  const char *data;
  size_t len;
} Str;

typedef enum { STR_INDEX_OK, STR_INDEX_OUT_OF_RANGE } StrIndexStatus;
typedef struct {
  StrIndexStatus status;
  unsigned char value;
} StrAt;
typedef struct {
  StrIndexStatus status;
  Str value;
} StrSlice;
typedef struct {
  bool found;
  size_t index;
} StrFind;

Str str_lit(const char *cstr);
Str str_n(const char *data, size_t len);
Str str_empty(void);
bool str_eq(Str a, Str b);
int str_cmp(Str a, Str b);
bool str_starts_with(Str s, Str prefix);
bool str_ends_with(Str s, Str suffix);
StrFind str_find(Str s, Str needle);
StrAt str_at(Str s, size_t index);
/* Bytes [start, end) when 0 <= start <= end <= len. */
StrSlice str_slice(Str s, size_t start, size_t end);
Str str_cat(Arena *arena, Str a, Str b);
Str str_copy(Arena *arena, Str s);
/* NUL-terminated copy for host calls; NULL when the string holds a NUL byte or on OOM. */
char *str_cstr(Arena *arena, Str s);
Str str_of_u64(Arena *arena, uint64_t value);
Str str_of_i64(Arena *arena, int64_t value);

typedef struct {
  Arena *arena;
  char *data;
  size_t len;
  size_t cap;
} StrBuf;

StrBuf strbuf_new(Arena *arena);
bool strbuf_push(StrBuf *buf, Str s);
bool strbuf_push_byte(StrBuf *buf, char c);
bool strbuf_push_u64(StrBuf *buf, uint64_t value);
Str strbuf_str(const StrBuf *buf);

/* ---- vectors of pointers ---- */
typedef struct {
  Arena *arena;
  void **items;
  size_t len;
  size_t cap;
} Vec;

typedef enum { VEC_GET_OK, VEC_GET_OUT_OF_RANGE } VecGetStatus;
typedef struct {
  VecGetStatus status;
  void *value;
} VecGet;

Vec vec_new(Arena *arena);
bool vec_push(Vec *vec, void *item);
VecGet vec_get(const Vec *vec, size_t index);
VecGet vec_last(const Vec *vec);
Vec vec_map(Arena *arena, const Vec *vec, void *(*f)(void *ctx, void *item), void *ctx);
Vec vec_filter(Arena *arena, const Vec *vec, bool (*keep)(void *ctx, void *item), void *ctx);
void *vec_fold(const Vec *vec, void *init, void *(*f)(void *ctx, void *acc, void *item), void *ctx);
Vec vec_concat(Arena *arena, const Vec *a, const Vec *b);
Vec vec_reverse(Arena *arena, const Vec *vec);

/* ---- maps: Str keys to pointers, insertion ordered ---- */
typedef struct MapEntry MapEntry;
typedef struct {
  Arena *arena;
  MapEntry **buckets;
  size_t bucket_count;
  size_t len;
  MapEntry *first;
  MapEntry *last;
} Map;

typedef enum { MAP_PUT_INSERTED, MAP_PUT_REPLACED, MAP_PUT_OOM } MapPutStatus;
typedef struct {
  bool found;
  void *value;
} MapGet;

Map map_new(Arena *arena);
MapPutStatus map_put(Map *map, Str key, void *value);
MapGet map_get(const Map *map, Str key);
bool map_has(const Map *map, Str key);
void map_each(const Map *map, void (*f)(void *ctx, Str key, void *value), void *ctx);
Vec map_keys(Arena *arena, const Map *map);

/* ---- big naturals: little-endian base 2^32 limbs, no leading zero limb ---- */
typedef struct {
  const uint32_t *limbs;
  size_t len;
} Nat;

typedef enum { NAT_TO_U64_OK, NAT_TO_U64_OVERFLOW } NatToU64Status;
typedef struct {
  NatToU64Status status;
  uint64_t value;
} NatToU64;
typedef enum { NAT_SUB_OK, NAT_SUB_UNDERFLOW, NAT_SUB_OOM } NatSubStatus;
typedef struct {
  NatSubStatus status;
  Nat value;
} NatSub;
typedef enum { NAT_DIV_OK, NAT_DIV_BY_ZERO, NAT_DIV_OOM } NatDivStatus;
typedef struct {
  NatDivStatus status;
  Nat quotient;
  Nat remainder;
} NatDiv;
typedef enum { NAT_PARSE_OK, NAT_PARSE_EMPTY, NAT_PARSE_BAD_DIGIT, NAT_PARSE_OOM } NatParseStatus;
typedef struct {
  NatParseStatus status;
  Nat value;
} NatParse;

Nat nat_zero(void);
Nat nat_of_u64(Arena *arena, uint64_t value);
NatToU64 nat_to_u64(Nat n);
bool nat_is_zero(Nat n);
int nat_cmp(Nat a, Nat b);
Nat nat_add(Arena *arena, Nat a, Nat b);
Nat nat_succ(Arena *arena, Nat a);
NatSub nat_sub(Arena *arena, Nat a, Nat b);
Nat nat_mul(Arena *arena, Nat a, Nat b);
NatDiv nat_divmod(Arena *arena, Nat a, Nat b);
Str nat_to_decimal(Arena *arena, Nat n);
NatParse nat_parse_decimal(Arena *arena, Str s);

/* ---- host effects (ports of effects/fs.c and cli/effects) ---- */
typedef enum { FS_OK, FS_ERRNO, FS_OOM, FS_BAD_PATH } FsStatus;
typedef struct {
  FsStatus status;
  int code; /* errno when status == FS_ERRNO */
  Str value;
} FsResult;

FsResult fs_read(Arena *arena, Str path);
FsResult fs_write(Str path, Str data);
/* Creates a fresh sibling directory path.tmp-XXXXXX and returns its name. */
FsResult fs_stage(Arena *arena, Str path);
/* Creates a fresh temporary file /<prefix>XXXXXX<suffix> and returns its path. */
FsResult fs_temp_file(Arena *arena, Str prefix, Str suffix);
FsResult fs_rename(Str from, Str to);
FsResult fs_unlink(Str path);
FsResult fs_rmdir(Str path);
FsResult fs_mkdir(Str path);
/* Creates a fresh temporary directory under the host temp root and returns its path. */
FsResult fs_temp_dir(Arena *arena, Str prefix);
bool fs_exists(Str path);

typedef enum { PROCESS_RAN, PROCESS_INVALID_ARGUMENT, PROCESS_SPAWN_FAILED } ProcessStatus;
typedef struct {
  ProcessStatus status;
  uint32_t code; /* exit code; 127 when the process did not run */
} ProcessResult;

/* Runs argv (NULL terminated, at most 4 entries used by the Bend host) with stdout and stderr
 * redirected to the named files, as cli/effects/process.c does. */
ProcessResult host_process_run(const char *const *argv, size_t argc, Str stdout_path, Str stderr_path);
/* Path of the running executable; empty on hosts that cannot tell. */
Str host_executable_path(Arena *arena);

/* ---- main wrapper: runs program on a thread with a 1 GiB stack ---- */
int rt_run_main(int argc, char **argv, int (*program)(int argc, char **argv));

#endif
