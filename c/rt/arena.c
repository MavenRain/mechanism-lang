#include "rt.h"

#include <stdlib.h>
#include <string.h>

struct ArenaChunk {
  ArenaChunk *next;
  size_t used;
  size_t size;
  char *bytes;
};

Arena arena_new(size_t chunk_size) {
  Arena arena;
  arena.head = NULL;
  arena.chunk_size = chunk_size < 4096 ? 4096 : chunk_size;
  arena.reserved = 0;
  return arena;
}

size_t arena_reserved(const Arena *arena) {
  return arena->reserved;
}

static size_t arena_round(size_t size) {
  size_t align = sizeof(void *) * 2;
  return (size + align - 1) / align * align;
}

static ArenaChunk *arena_grow(Arena *arena, size_t need) {
  size_t size = need > arena->chunk_size ? need : arena->chunk_size;
  ArenaChunk *chunk = malloc(sizeof(ArenaChunk));
  if (chunk == NULL) return NULL;
  chunk->bytes = calloc(1, size);
  if (chunk->bytes == NULL) {
    free(chunk);
    return NULL;
  }
  chunk->next = arena->head;
  chunk->used = 0;
  chunk->size = size;
  arena->head = chunk;
  arena->reserved = size > SIZE_MAX - arena->reserved ? SIZE_MAX : arena->reserved + size;
  return chunk;
}

void *arena_alloc(Arena *arena, size_t size) {
  if (size > SIZE_MAX - (sizeof(void *) * 2 - 1)) return NULL;
  size_t need = arena_round(size == 0 ? 1 : size);
  ArenaChunk *chunk = arena->head;
  bool fits = chunk != NULL && chunk->size - chunk->used >= need;
  ArenaChunk *target = fits ? chunk : arena_grow(arena, need);
  if (target == NULL) return NULL;
  void *out = target->bytes + target->used;
  target->used += need;
  return out;
}

void arena_free(Arena *arena) {
  ArenaChunk *chunk = arena->head;
  while (chunk != NULL) {
    ArenaChunk *next = chunk->next;
    free(chunk->bytes);
    free(chunk);
    chunk = next;
  }
  arena->head = NULL;
  arena->reserved = 0;
}
