#include "rt.h"

#include <string.h>

/* Mutable working buffer; results are frozen into Nat views over arena memory. */
typedef struct {
  uint32_t *limbs;
  size_t len;
} NatBuf;

static NatBuf natbuf_new(Arena *arena, size_t cap) {
  NatBuf buf;
  buf.limbs = arena_alloc(arena, (cap == 0 ? 1 : cap) * sizeof(uint32_t));
  buf.len = buf.limbs == NULL ? 0 : cap;
  return buf;
}

static Nat natbuf_freeze(NatBuf buf) {
  Nat n;
  size_t len = buf.len;
  while (len > 0 && buf.limbs[len - 1] == 0) len -= 1;
  n.limbs = buf.limbs;
  n.len = len;
  return n;
}

Nat nat_zero(void) {
  Nat n;
  n.limbs = NULL;
  n.len = 0;
  return n;
}

Nat nat_of_u64(Arena *arena, uint64_t value) {
  NatBuf buf = natbuf_new(arena, 2);
  if (buf.limbs == NULL) return nat_zero();
  buf.limbs[0] = (uint32_t)value;
  buf.limbs[1] = (uint32_t)(value >> 32);
  return natbuf_freeze(buf);
}

NatToU64 nat_to_u64(Nat n) {
  NatToU64 out;
  if (n.len > 2) {
    out.status = NAT_TO_U64_OVERFLOW;
    out.value = 0;
    return out;
  }
  out.status = NAT_TO_U64_OK;
  out.value = (n.len > 0 ? (uint64_t)n.limbs[0] : 0) | (n.len > 1 ? (uint64_t)n.limbs[1] << 32 : 0);
  return out;
}

bool nat_is_zero(Nat n) { return n.len == 0; }

int nat_cmp(Nat a, Nat b) {
  if (a.len != b.len) return a.len < b.len ? -1 : 1;
  for (size_t i = a.len; i > 0; i--) {
    if (a.limbs[i - 1] != b.limbs[i - 1]) return a.limbs[i - 1] < b.limbs[i - 1] ? -1 : 1;
  }
  return 0;
}

Nat nat_add(Arena *arena, Nat a, Nat b) {
  size_t len = (a.len > b.len ? a.len : b.len) + 1;
  NatBuf buf = natbuf_new(arena, len);
  if (buf.limbs == NULL) return nat_zero();
  uint64_t carry = 0;
  for (size_t i = 0; i < len; i++) {
    uint64_t sum = carry + (i < a.len ? a.limbs[i] : 0) + (i < b.len ? b.limbs[i] : 0);
    buf.limbs[i] = (uint32_t)sum;
    carry = sum >> 32;
  }
  return natbuf_freeze(buf);
}

Nat nat_succ(Arena *arena, Nat a) { return nat_add(arena, a, nat_of_u64(arena, 1)); }

NatSub nat_sub(Arena *arena, Nat a, Nat b) {
  NatSub out;
  if (nat_cmp(a, b) < 0) {
    out.status = NAT_SUB_UNDERFLOW;
    out.value = nat_zero();
    return out;
  }
  NatBuf buf = natbuf_new(arena, a.len);
  if (buf.limbs == NULL && a.len > 0) {
    out.status = NAT_SUB_OOM;
    out.value = nat_zero();
    return out;
  }
  int64_t borrow = 0;
  for (size_t i = 0; i < a.len; i++) {
    int64_t diff = (int64_t)a.limbs[i] - (int64_t)(i < b.len ? b.limbs[i] : 0) - borrow;
    borrow = diff < 0 ? 1 : 0;
    buf.limbs[i] = (uint32_t)(diff + (borrow != 0 ? ((int64_t)1 << 32) : 0));
  }
  out.status = NAT_SUB_OK;
  out.value = natbuf_freeze(buf);
  return out;
}

Nat nat_mul(Arena *arena, Nat a, Nat b) {
  if (a.len == 0 || b.len == 0) return nat_zero();
  NatBuf buf = natbuf_new(arena, a.len + b.len);
  if (buf.limbs == NULL) return nat_zero();
  memset(buf.limbs, 0, buf.len * sizeof(uint32_t));
  for (size_t i = 0; i < a.len; i++) {
    uint64_t carry = 0;
    for (size_t j = 0; j < b.len; j++) {
      uint64_t cur = (uint64_t)a.limbs[i] * b.limbs[j] + buf.limbs[i + j] + carry;
      buf.limbs[i + j] = (uint32_t)cur;
      carry = cur >> 32;
    }
    size_t k = i + b.len;
    while (carry != 0 && k < buf.len) {
      uint64_t cur = (uint64_t)buf.limbs[k] + carry;
      buf.limbs[k] = (uint32_t)cur;
      carry = cur >> 32;
      k += 1;
    }
  }
  return natbuf_freeze(buf);
}

/* Shift-subtract long division, one bit at a time; sizes stay bounded by the inputs. */
NatDiv nat_divmod(Arena *arena, Nat a, Nat b) {
  NatDiv out;
  if (b.len == 0) {
    out.status = NAT_DIV_BY_ZERO;
    out.quotient = nat_zero();
    out.remainder = nat_zero();
    return out;
  }
  out.status = NAT_DIV_OK;
  if (nat_cmp(a, b) < 0) {
    out.quotient = nat_zero();
    out.remainder = a;
    return out;
  }
  NatBuf q = natbuf_new(arena, a.len);
  NatBuf r = natbuf_new(arena, b.len + 1);
  if (q.limbs == NULL || r.limbs == NULL) {
    out.status = NAT_DIV_OOM;
    out.quotient = nat_zero();
    out.remainder = nat_zero();
    return out;
  }
  memset(q.limbs, 0, q.len * sizeof(uint32_t));
  memset(r.limbs, 0, r.len * sizeof(uint32_t));
  for (size_t bit = a.len * 32; bit > 0; bit--) {
    size_t at = bit - 1;
    uint32_t carry = (a.limbs[at / 32] >> (at % 32)) & 1u;
    for (size_t i = 0; i < r.len; i++) {
      uint32_t next = r.limbs[i] >> 31;
      r.limbs[i] = (r.limbs[i] << 1) | carry;
      carry = next;
    }
    Nat rv = natbuf_freeze(r);
    if (nat_cmp(rv, b) >= 0) {
      int64_t borrow = 0;
      for (size_t i = 0; i < r.len; i++) {
        int64_t diff = (int64_t)r.limbs[i] - (int64_t)(i < b.len ? b.limbs[i] : 0) - borrow;
        borrow = diff < 0 ? 1 : 0;
        r.limbs[i] = (uint32_t)(diff + (borrow != 0 ? ((int64_t)1 << 32) : 0));
      }
      q.limbs[at / 32] |= (uint32_t)1 << (at % 32);
    }
  }
  out.quotient = natbuf_freeze(q);
  out.remainder = natbuf_freeze(r);
  return out;
}

Str nat_to_decimal(Arena *arena, Nat n) {
  if (n.len == 0) return str_lit("0");
  NatBuf work = natbuf_new(arena, n.len);
  if (work.limbs == NULL) return str_lit("0");
  memcpy(work.limbs, n.limbs, n.len * sizeof(uint32_t));
  size_t cap = n.len * 10 + 1;
  char *digits = arena_alloc(arena, cap);
  if (digits == NULL) return str_lit("0");
  size_t at = cap;
  size_t len = work.len;
  while (len > 0) {
    uint64_t rem = 0;
    for (size_t i = len; i > 0; i--) {
      uint64_t cur = (rem << 32) | work.limbs[i - 1];
      work.limbs[i - 1] = (uint32_t)(cur / 1000000000u);
      rem = cur % 1000000000u;
    }
    while (len > 0 && work.limbs[len - 1] == 0) len -= 1;
    for (int k = 0; k < 9; k++) {
      if (at == 0) return str_lit("0");
      at -= 1;
      digits[at] = (char)('0' + (int)(rem % 10));
      rem /= 10;
      if (len == 0 && rem == 0) break;
    }
  }
  while (at < cap - 1 && digits[at] == '0') at += 1;
  return str_copy(arena, str_n(digits + at, cap - at));
}

NatParse nat_parse_decimal(Arena *arena, Str s) {
  NatParse out;
  out.value = nat_zero();
  if (s.len == 0) {
    out.status = NAT_PARSE_EMPTY;
    return out;
  }
  NatBuf buf = natbuf_new(arena, s.len / 9 + 2);
  if (buf.limbs == NULL) {
    out.status = NAT_PARSE_OOM;
    return out;
  }
  memset(buf.limbs, 0, buf.len * sizeof(uint32_t));
  for (size_t i = 0; i < s.len; i++) {
    unsigned char c = (unsigned char)s.data[i];
    if (c < '0' || c > '9') {
      out.status = NAT_PARSE_BAD_DIGIT;
      return out;
    }
    uint64_t carry = (uint64_t)(c - '0');
    for (size_t j = 0; j < buf.len; j++) {
      uint64_t cur = (uint64_t)buf.limbs[j] * 10 + carry;
      buf.limbs[j] = (uint32_t)cur;
      carry = cur >> 32;
    }
  }
  out.status = NAT_PARSE_OK;
  out.value = natbuf_freeze(buf);
  return out;
}
