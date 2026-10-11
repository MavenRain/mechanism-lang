#include "rt.h"

#include <string.h>

struct MapEntry {
  Str key;
  void *value;
  uint64_t hash;
  MapEntry *chain;
  MapEntry *order;
};

static uint64_t map_hash(Str key) {
  uint64_t h = 1469598103934665603ull;
  for (size_t i = 0; i < key.len; i++) {
    h ^= (unsigned char)key.data[i];
    h *= 1099511628211ull;
  }
  return h;
}

Map map_new(Arena *arena) {
  Map map;
  map.arena = arena;
  map.buckets = NULL;
  map.bucket_count = 0;
  map.len = 0;
  map.first = NULL;
  map.last = NULL;
  return map;
}

static MapEntry *map_lookup(const Map *map, Str key, uint64_t hash) {
  if (map->bucket_count == 0) return NULL;
  MapEntry *entry = map->buckets[hash % map->bucket_count];
  while (entry != NULL) {
    if (entry->hash == hash && str_eq(entry->key, key)) return entry;
    entry = entry->chain;
  }
  return NULL;
}

static bool map_rehash(Map *map, size_t bucket_count) {
  MapEntry **buckets = arena_alloc(map->arena, bucket_count * sizeof(MapEntry *));
  if (buckets == NULL) return false;
  MapEntry *entry = map->first;
  while (entry != NULL) {
    size_t slot = entry->hash % bucket_count;
    entry->chain = buckets[slot];
    buckets[slot] = entry;
    entry = entry->order;
  }
  map->buckets = buckets;
  map->bucket_count = bucket_count;
  return true;
}

MapPutStatus map_put(Map *map, Str key, void *value) {
  uint64_t hash = map_hash(key);
  MapEntry *found = map_lookup(map, key, hash);
  if (found != NULL) {
    found->value = value;
    return MAP_PUT_REPLACED;
  }
  bool grow = map->bucket_count == 0 || map->len + 1 > map->bucket_count * 3 / 4;
  if (grow && !map_rehash(map, map->bucket_count == 0 ? 16 : map->bucket_count * 2)) return MAP_PUT_OOM;
  MapEntry *entry = arena_alloc(map->arena, sizeof(MapEntry));
  if (entry == NULL) return MAP_PUT_OOM;
  Str owned_key = key.len == 0 ? str_empty() : str_copy(map->arena, key);
  if (owned_key.len != key.len) return MAP_PUT_OOM;
  entry->key = owned_key;
  entry->value = value;
  entry->hash = hash;
  entry->order = NULL;
  size_t slot = hash % map->bucket_count;
  entry->chain = map->buckets[slot];
  map->buckets[slot] = entry;
  if (map->last == NULL) {
    map->first = entry;
  } else {
    map->last->order = entry;
  }
  map->last = entry;
  map->len += 1;
  return MAP_PUT_INSERTED;
}

MapGet map_get(const Map *map, Str key) {
  MapGet out;
  MapEntry *entry = map_lookup(map, key, map_hash(key));
  out.found = entry != NULL;
  out.value = entry == NULL ? NULL : entry->value;
  return out;
}

bool map_has(const Map *map, Str key) { return map_get(map, key).found; }

void map_each(const Map *map, void (*f)(void *ctx, Str key, void *value), void *ctx) {
  MapEntry *entry = map->first;
  while (entry != NULL) {
    f(ctx, entry->key, entry->value);
    entry = entry->order;
  }
}

Vec map_keys(Arena *arena, const Map *map) {
  Vec out = vec_new(arena);
  MapEntry *entry = map->first;
  while (entry != NULL) {
    Str *key = arena_alloc(arena, sizeof(Str));
    if (key == NULL) return vec_new(arena);
    *key = entry->key;
    if (!vec_push(&out, key)) return vec_new(arena);
    entry = entry->order;
  }
  return out;
}
