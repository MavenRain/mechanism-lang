/* Port of kernel/bignum.bend: signed, canonical base 32768 limbs, least significant first.
 * Bend lists become arrays; list recursion becomes the same arithmetic over indices, with the
 * untrimmed shapes of add_digits, sub_digits and mul_digits kept. trim_put, div_small_put and
 * read_chars' per-character step are folded into their callers. */
#include "kernel.h"

#include <string.h>

#define BIG_BASE 32768u

/* Mutable limb buffer; results are frozen into Limbs views. */
typedef struct {
  uint32_t *items;
  size_t len;
  size_t cap;
} LimbBuf;

static LimbBuf limbbuf_new(Heap *heap, size_t cap) {
  LimbBuf buf;
  buf.items = cap == 0 ? NULL : heap_alloc(heap, cap * sizeof(uint32_t));
  buf.len = 0;
  buf.cap = buf.items == NULL ? 0 : cap;
  return buf;
}

static void limbbuf_push(LimbBuf *buf, uint32_t limb) {
  if (buf->len < buf->cap) {
    buf->items[buf->len] = limb;
    buf->len += 1;
  }
}

static Limbs limbbuf_freeze(LimbBuf buf) {
  Limbs out;
  out.items = buf.items;
  out.len = buf.len;
  return out;
}

static Limbs limbs_empty(void) {
  Limbs out;
  out.items = NULL;
  out.len = 0;
  return out;
}

static uint32_t limb_at(Limbs xs, size_t index) { return index < xs.len ? xs.items[index] : 0; }

static Big big_of_limbs(bool negative, Limbs digits) {
  Big b;
  b.negative = negative;
  b.digits = digits;
  return b;
}

Big big_zero(void) { return big_of_limbs(false, limbs_empty()); }

static const uint32_t big_one_digits[1] = {1};

Big big_one(void) {
  Limbs digits;
  digits.items = big_one_digits;
  digits.len = 1;
  return big_of_limbs(false, digits);
}

/* def trim(xs): drop the most significant zero limbs (trim_put folded in). */
Limbs big_trim(Limbs xs) {
  Limbs out = xs;
  while (out.len > 0 && xs.items[out.len - 1] == 0) out.len -= 1;
  return out;
}

Big big_canonical(Limbs xs, bool negative) {
  if (xs.len == 0) return big_zero();
  return big_of_limbs(negative, xs);
}

Big big_make(Heap *heap, bool negative, Limbs digits) {
  (void)heap;
  return big_canonical(big_trim(digits), negative);
}

/* def digits_u32(+n: U32): digits_u32_step folded into the loop. */
Limbs big_digits_u32(Heap *heap, uint32_t n) {
  if (n == 0) return limbs_empty();
  LimbBuf buf = limbbuf_new(heap, 3);
  uint32_t rest = n;
  while (rest != 0 && buf.len < buf.cap) {
    limbbuf_push(&buf, rest % BIG_BASE);
    rest /= BIG_BASE;
  }
  return limbbuf_freeze(buf);
}

Big big_of_u32(Heap *heap, uint32_t n) { return big_of_limbs(false, big_digits_u32(heap, n)); }

static Cmp cmp_u32(uint32_t a, uint32_t b) {
  if (a < b) return CMP_LT;
  if (a > b) return CMP_GT;
  return CMP_EQ;
}

Cmp big_cmp_low(Cmp high, uint32_t a, uint32_t b) {
  switch (high) {
    case CMP_LT:
      return CMP_LT;
    case CMP_EQ:
      return cmp_u32(a, b);
    case CMP_GT:
      return CMP_GT;
  }
  return high;
}

/* def cmp_digits(a, b): a shorter list is less; equal lengths compare from the top limb down. */
Cmp big_cmp_digits(Limbs a, Limbs b) {
  if (a.len != b.len) return a.len < b.len ? CMP_LT : CMP_GT;
  Cmp high = CMP_EQ;
  for (size_t i = a.len; i > 0; i--) {
    high = big_cmp_low(high, a.items[i - 1], b.items[i - 1]);
  }
  return high;
}

Cmp big_reverse_cmp(Cmp c) {
  switch (c) {
    case CMP_LT:
      return CMP_GT;
    case CMP_EQ:
      return CMP_EQ;
    case CMP_GT:
      return CMP_LT;
  }
  return c;
}

Cmp big_compare(Big a, Big b) {
  if (!a.negative && !b.negative) return big_cmp_digits(a.digits, b.digits);
  if (a.negative && b.negative) return big_reverse_cmp(big_cmp_digits(a.digits, b.digits));
  if (a.negative) return CMP_LT;
  return CMP_GT;
}

bool big_cmp_eq(Cmp c) {
  switch (c) {
    case CMP_EQ:
      return true;
    case CMP_LT:
    case CMP_GT:
      return false;
  }
  return false;
}

bool big_cmp_le(Cmp c) {
  switch (c) {
    case CMP_GT:
      return false;
    case CMP_LT:
    case CMP_EQ:
      return true;
  }
  return false;
}

bool big_equal(Big a, Big b) { return big_cmp_eq(big_compare(a, b)); }

bool big_le(Big a, Big b) { return big_cmp_le(big_compare(a, b)); }

Cmp big_sign(Big x) { return big_compare(x, big_zero()); }

bool big_positive(Big x) { return !big_le(x, big_zero()); }

/* def add_digits(a, b, carry): a missing limb counts as 0; the final carry is appended as digits. */
Limbs big_add_digits(Heap *heap, Limbs a, Limbs b, uint32_t carry) {
  size_t longest = a.len > b.len ? a.len : b.len;
  LimbBuf buf = limbbuf_new(heap, longest + 3);
  if (buf.items == NULL) return limbs_empty();
  uint32_t c = carry;
  for (size_t i = 0; i < longest; i++) {
    uint32_t s = limb_at(a, i) + limb_at(b, i) + c;
    limbbuf_push(&buf, s % BIG_BASE);
    c = s / BIG_BASE;
  }
  while (c != 0 && buf.len < buf.cap) {
    limbbuf_push(&buf, c % BIG_BASE);
    c /= BIG_BASE;
  }
  return limbbuf_freeze(buf);
}

/* def sub_digits(a, b, carry): the result has the length of a; limbs of b beyond a are dropped. */
Limbs big_sub_digits(Heap *heap, Limbs a, Limbs b, uint32_t carry) {
  if (a.len == 0) return limbs_empty();
  LimbBuf buf = limbbuf_new(heap, a.len);
  if (buf.items == NULL) return limbs_empty();
  uint32_t c = carry;
  for (size_t i = 0; i < a.len; i++) {
    uint32_t s = BIG_BASE + a.items[i] - limb_at(b, i) - c;
    limbbuf_push(&buf, s % BIG_BASE);
    c = 1 - s / BIG_BASE;
  }
  return limbbuf_freeze(buf);
}

Big big_add_signed(Heap *heap, Cmp c, bool negative, Limbs a, Limbs b) {
  switch (c) {
    case CMP_EQ:
      return big_zero();
    case CMP_GT:
      return big_make(heap, negative, big_sub_digits(heap, a, b, 0));
    case CMP_LT:
      return big_make(heap, !negative, big_sub_digits(heap, b, a, 0));
  }
  return big_zero();
}

Big big_add(Heap *heap, Big a, Big b) {
  if (!a.negative && !b.negative) return big_make(heap, false, big_add_digits(heap, a.digits, b.digits, 0));
  if (a.negative && b.negative) return big_make(heap, true, big_add_digits(heap, a.digits, b.digits, 0));
  if (!a.negative) return big_add_signed(heap, big_cmp_digits(a.digits, b.digits), false, a.digits, b.digits);
  return big_add_signed(heap, big_cmp_digits(a.digits, b.digits), true, a.digits, b.digits);
}

Big big_neg(Big x) { return big_canonical(x.digits, !x.negative); }

Big big_sub_keep(Heap *heap, Cmp c, Big a, Big b) {
  switch (c) {
    case CMP_GT:
      return big_add(heap, a, big_neg(b));
    case CMP_EQ:
    case CMP_LT:
      return big_zero();
  }
  return big_zero();
}

Big big_sub(Heap *heap, Big a, Big b) { return big_sub_keep(heap, big_compare(a, b), a, b); }

/* def mul_digit(xs, +digit, carry) */
Limbs big_mul_digit(Heap *heap, Limbs xs, uint32_t digit, uint32_t carry) {
  LimbBuf buf = limbbuf_new(heap, xs.len + 3);
  if (buf.items == NULL) return limbs_empty();
  uint32_t c = carry;
  for (size_t i = 0; i < xs.len; i++) {
    uint32_t s = xs.items[i] * digit + c;
    limbbuf_push(&buf, s % BIG_BASE);
    c = s / BIG_BASE;
  }
  while (c != 0 && buf.len < buf.cap) {
    limbbuf_push(&buf, c % BIG_BASE);
    c /= BIG_BASE;
  }
  return limbbuf_freeze(buf);
}

/* 0 <> xs */
static Limbs limbs_shift(Heap *heap, Limbs xs) {
  LimbBuf buf = limbbuf_new(heap, xs.len + 1);
  if (buf.items == NULL) return limbs_empty();
  limbbuf_push(&buf, 0);
  if (xs.len > 0) memcpy(buf.items + 1, xs.items, xs.len * sizeof(uint32_t));
  buf.len = xs.len + 1;
  return limbbuf_freeze(buf);
}

/* def mul_digits(a, +b): Con{x, xs} -> add_digits(mul_digit(b, x, 0), 0 <> mul_digits(xs, b), 0),
 * evaluated from the most significant limb of a inward, the same shape as the recursion. */
Limbs big_mul_digits(Heap *heap, Limbs a, Limbs b) {
  Limbs acc = limbs_empty();
  for (size_t i = a.len; i > 0; i--) {
    Limbs shifted = limbs_shift(heap, acc);
    acc = big_add_digits(heap, big_mul_digit(heap, b, a.items[i - 1], 0), shifted, 0);
  }
  return acc;
}

Big big_mul(Heap *heap, Big a, Big b) {
  return big_make(heap, a.negative != b.negative, big_mul_digits(heap, a.digits, b.digits));
}

Big big_maximum(Big a, Big b) { return big_le(a, b) ? b : a; }

MaybeBig big_read_digit(Heap *heap, bool valid, uint32_t c, Big value) {
  MaybeBig out;
  if (!valid) {
    out.some = false;
    out.value = big_zero();
    return out;
  }
  out.some = true;
  out.value = big_add(heap, big_mul(heap, value, big_of_u32(heap, 10)), big_of_u32(heap, c - 48));
  return out;
}

static bool is_digit_code(uint32_t code) { return code >= 48 && code <= 57; }

/* def read_chars(s, value): a none value ends the scan at once, as the Bend match does. */
MaybeBig big_read_chars(Heap *heap, Str s, MaybeBig value) {
  MaybeBig acc = value;
  for (size_t i = 0; i < s.len && acc.some; i++) {
    uint32_t code = (unsigned char)s.data[i];
    acc = big_read_digit(heap, is_digit_code(code), code, acc.value);
  }
  return acc;
}

MaybeBig big_of_decimal(Heap *heap, Str s) {
  if (s.len == 0) {
    MaybeBig none;
    none.some = false;
    none.value = big_zero();
    return none;
  }
  uint32_t code = (unsigned char)s.data[0];
  StrSlice rest = str_slice(s, 1, s.len);
  return big_read_chars(heap, rest.value, big_read_digit(heap, is_digit_code(code), code, big_zero()));
}

/* def div_small(xs, +divisor): from the most significant limb down (div_small_put folded in). */
DivSmall big_div_small(Heap *heap, Limbs xs, uint32_t divisor) {
  DivSmall out;
  out.quotient = limbs_empty();
  out.remainder = 0;
  if (xs.len == 0 || divisor == 0) return out;
  LimbBuf buf = limbbuf_new(heap, xs.len);
  if (buf.items == NULL) return out;
  uint32_t rem = 0;
  for (size_t i = xs.len; i > 0; i--) {
    uint32_t n = rem * BIG_BASE + xs.items[i - 1];
    buf.items[i - 1] = n / divisor;
    rem = n % divisor;
  }
  buf.len = xs.len;
  out.quotient = limbbuf_freeze(buf);
  out.remainder = rem;
  return out;
}

DivSmall big_div_small_put_trim(DivSmall result) {
  DivSmall out;
  out.quotient = big_trim(result.quotient);
  out.remainder = result.remainder;
  return out;
}

/* def show_digits(state, suffix): the Bend callers always pass Some; each step prepends the
 * remainder digit and divides the trimmed quotient by 10 until it is empty. */
Str big_show_digits(Heap *heap, DivSmall state, Str suffix) {
  size_t cap = state.quotient.len * 5 + 1 + suffix.len;
  char *bytes = heap_alloc(heap, cap + 1);
  if (bytes == NULL) return str_empty();
  if (suffix.len > 0) memcpy(bytes + cap - suffix.len, suffix.data, suffix.len);
  size_t pos = cap - suffix.len;
  DivSmall cur = state;
  bool more = true;
  while (more && pos > 0) {
    pos -= 1;
    bytes[pos] = (char)(48 + cur.remainder);
    more = cur.quotient.len > 0 && !heap->oom;
    if (more) cur = big_div_small_put_trim(big_div_small(heap, cur.quotient, 10));
  }
  return str_n(bytes + pos, cap - pos);
}

Str big_to_string(Heap *heap, Big x) {
  if (x.digits.len == 0) return str_lit("0");
  Str body = big_show_digits(heap, big_div_small_put_trim(big_div_small(heap, x.digits, 10)), str_empty());
  if (x.negative) return heap_str_cat(heap, str_lit("-"), body);
  return body;
}

MaybeLimbs big_limbs15(Big x) {
  MaybeLimbs out;
  out.some = !x.negative;
  out.value = x.negative ? limbs_empty() : x.digits;
  return out;
}

MaybeU32 big_narrow_put(MaybeU32 rest, uint32_t digit) {
  MaybeU32 out;
  out.some = false;
  out.value = 0;
  if (!rest.some) return out;
  if (rest.value <= (4294967295u - digit) / BIG_BASE) {
    out.some = true;
    out.value = rest.value * BIG_BASE + digit;
  }
  return out;
}

MaybeU32 big_narrow_digits(Limbs xs) {
  MaybeU32 acc;
  acc.some = true;
  acc.value = 0;
  for (size_t i = xs.len; i > 0; i--) {
    acc = big_narrow_put(acc, xs.items[i - 1]);
  }
  return acc;
}

MaybeU32 big_to_u32(Big value) {
  if (value.negative) {
    MaybeU32 none;
    none.some = false;
    none.value = 0;
    return none;
  }
  return big_narrow_digits(value.digits);
}

MaybeU32 big_i31_select(MaybeU32 value) {
  MaybeU32 out;
  out.some = value.some && value.value <= 1073741823u;
  out.value = out.some ? value.value : 0;
  return out;
}

MaybeU32 big_to_i31(Big value) { return big_i31_select(big_to_u32(value)); }

/* def digits_nat(n: Nat) */
Limbs big_digits_nat(Heap *heap, Nat n) {
  if (nat_is_zero(n)) return limbs_empty();
  LimbBuf buf = limbbuf_new(heap, n.len * 3 + 1);
  if (buf.items == NULL) return limbs_empty();
  Nat base = heap_nat_of_u64(heap, BIG_BASE);
  Nat cur = n;
  while (!nat_is_zero(cur) && buf.len < buf.cap) {
    NatDiv d = nat_divmod(heap->arena, cur, base);
    if (d.status != NAT_DIV_OK) {
      heap->oom = true;
      return limbs_empty();
    }
    limbbuf_push(&buf, (uint32_t)nat_to_u64(d.remainder).value);
    cur = d.quotient;
  }
  return limbbuf_freeze(buf);
}

Big big_of_nat(Heap *heap, Nat n) { return big_of_limbs(false, big_digits_nat(heap, n)); }

/* def bounded_nat(inside, value): Nat.read(to_string(value)) when inside. */
MaybeNat big_bounded_nat(Heap *heap, bool inside, Big value) {
  MaybeNat out;
  out.some = false;
  out.value = nat_zero();
  if (!inside) return out;
  NatParse parsed = nat_parse_decimal(heap->arena, big_to_string(heap, value));
  switch (parsed.status) {
    case NAT_PARSE_OK:
      out.some = true;
      out.value = parsed.value;
      return out;
    case NAT_PARSE_OOM:
      heap->oom = true;
      return out;
    case NAT_PARSE_EMPTY:
    case NAT_PARSE_BAD_DIGIT:
      return out;
  }
  return out;
}

MaybeNat big_to_nat_bounded(Heap *heap, Big value, Nat upper) {
  bool inside = big_le(big_zero(), value) && big_le(value, big_of_nat(heap, upper));
  return big_bounded_nat(heap, inside, value);
}

VecGet big_at_narrowed(const Vec *xs, MaybeNat index) {
  if (!index.some) {
    VecGet out;
    out.status = VEC_GET_OUT_OF_RANGE;
    out.value = NULL;
    return out;
  }
  return common_at(xs, index.value);
}

VecGet big_at(Heap *heap, const Vec *xs, Big index) {
  Nat length = heap_nat_of_u64(heap, (uint64_t)xs->len);
  return big_at_narrowed(xs, big_to_nat_bounded(heap, index, length));
}
