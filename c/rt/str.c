#include "rt.h"

#include <string.h>

Str str_lit(const char *cstr) {
  Str s;
  s.data = cstr;
  s.len = cstr == NULL ? 0 : strlen(cstr);
  return s;
}

Str str_n(const char *data, size_t len) {
  Str s;
  s.data = data;
  s.len = len;
  return s;
}

Str str_empty(void) { return str_n("", 0); }

bool str_eq(Str a, Str b) {
  return a.len == b.len && (a.len == 0 || memcmp(a.data, b.data, a.len) == 0);
}

int str_cmp(Str a, Str b) {
  size_t n = a.len < b.len ? a.len : b.len;
  int head = n == 0 ? 0 : memcmp(a.data, b.data, n);
  if (head != 0) return head < 0 ? -1 : 1;
  if (a.len == b.len) return 0;
  return a.len < b.len ? -1 : 1;
}

bool str_starts_with(Str s, Str prefix) {
  return s.len >= prefix.len && (prefix.len == 0 || memcmp(s.data, prefix.data, prefix.len) == 0);
}

bool str_ends_with(Str s, Str suffix) {
  return s.len >= suffix.len &&
         (suffix.len == 0 || memcmp(s.data + (s.len - suffix.len), suffix.data, suffix.len) == 0);
}

StrFind str_find(Str s, Str needle) {
  StrFind out;
  out.found = false;
  out.index = 0;
  if (needle.len > s.len) return out;
  size_t last = s.len - needle.len;
  for (size_t i = 0; i <= last; i++) {
    if (needle.len == 0 || memcmp(s.data + i, needle.data, needle.len) == 0) {
      out.found = true;
      out.index = i;
      return out;
    }
  }
  return out;
}

StrAt str_at(Str s, size_t index) {
  StrAt out;
  if (index >= s.len) {
    out.status = STR_INDEX_OUT_OF_RANGE;
    out.value = 0;
    return out;
  }
  out.status = STR_INDEX_OK;
  out.value = (unsigned char)s.data[index];
  return out;
}

StrSlice str_slice(Str s, size_t start, size_t end) {
  StrSlice out;
  if (start > end || end > s.len) {
    out.status = STR_INDEX_OUT_OF_RANGE;
    out.value = str_empty();
    return out;
  }
  out.status = STR_INDEX_OK;
  out.value = str_n(start == 0 ? s.data : s.data + start, end - start);
  return out;
}

Str str_cat(Arena *arena, Str a, Str b) {
  char *bytes = arena_alloc(arena, a.len + b.len + 1);
  if (bytes == NULL) return str_empty();
  if (a.len > 0) memcpy(bytes, a.data, a.len);
  if (b.len > 0) memcpy(bytes + a.len, b.data, b.len);
  bytes[a.len + b.len] = 0;
  return str_n(bytes, a.len + b.len);
}

Str str_copy(Arena *arena, Str s) { return str_cat(arena, s, str_empty()); }

char *str_cstr(Arena *arena, Str s) {
  if (s.len > 0 && memchr(s.data, 0, s.len) != NULL) return NULL;
  char *bytes = arena_alloc(arena, s.len + 1);
  if (bytes == NULL) return NULL;
  if (s.len > 0) memcpy(bytes, s.data, s.len);
  bytes[s.len] = 0;
  return bytes;
}

static Str str_u64_digits(char *digits, uint64_t value) {
  size_t at = 21;
  uint64_t rest = value;
  do {
    at -= 1;
    digits[at] = (char)('0' + (int)(rest % 10));
    rest /= 10;
  } while (rest > 0);
  return str_n(digits + at, 21 - at);
}

Str str_of_u64(Arena *arena, uint64_t value) {
  char digits[21];
  return str_copy(arena, str_u64_digits(digits, value));
}

Str str_of_i64(Arena *arena, int64_t value) {
  if (value >= 0) return str_of_u64(arena, (uint64_t)value);
  uint64_t magnitude = (uint64_t)(-(value + 1)) + 1u;
  return str_cat(arena, str_lit("-"), str_of_u64(arena, magnitude));
}

StrBuf strbuf_new(Arena *arena) {
  StrBuf buf;
  buf.arena = arena;
  buf.data = NULL;
  buf.len = 0;
  buf.cap = 0;
  return buf;
}

static bool strbuf_reserve(StrBuf *buf, size_t extra) {
  size_t need = buf->len + extra + 1;
  if (need <= buf->cap) return true;
  size_t cap = buf->cap == 0 ? 64 : buf->cap;
  while (cap < need) cap *= 2;
  char *data = arena_alloc(buf->arena, cap);
  if (data == NULL) return false;
  if (buf->len > 0) memcpy(data, buf->data, buf->len);
  buf->data = data;
  buf->cap = cap;
  return true;
}

bool strbuf_push(StrBuf *buf, Str s) {
  if (!strbuf_reserve(buf, s.len)) return false;
  if (s.len > 0) memcpy(buf->data + buf->len, s.data, s.len);
  buf->len += s.len;
  buf->data[buf->len] = 0;
  return true;
}

bool strbuf_push_byte(StrBuf *buf, char c) { return strbuf_push(buf, str_n(&c, 1)); }

bool strbuf_push_u64(StrBuf *buf, uint64_t value) {
  char digits[21];
  return strbuf_push(buf, str_u64_digits(digits, value));
}

Str strbuf_str(const StrBuf *buf) { return str_n(buf->data == NULL ? "" : buf->data, buf->len); }
