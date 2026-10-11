/* Port of bend2/kernel/spec_count.bend: the specification count report of the kernel (formers,
 * schema constructors, shapes declared and admitted, named rules, eta rows). Same definitions in
 * the same order. The List<String> values are Vecs of Str pointers, as term_formers, term_schema,
 * shape_declared and the rules_* lists build them; the eta table is a Vec of EtaEntry pointers.
 * Every call that returns a 16-byte Str or Nat lands in a local before it feeds another call
 * (the tcc AArch64 argument clobber, PROGRESS Traps). */
#include "kernel.h"

/* The Str at index of a List<String> Vec, or "" past the end (total indexing). */
static Str item(const Vec *items, size_t index) {
  VecGet got = vec_get(items, index);
  if (got.status == VEC_GET_OK && got.value != NULL) return *(const Str *)got.value;
  return str_empty();
}

/* The EtaEntry at index of the eta table, or NULL past the end. */
static const EtaEntry *row_at(const Vec *rows, size_t index) {
  VecGet got = vec_get(rows, index);
  if (got.status == VEC_GET_OK) return got.value;
  return NULL;
}

static void push_str(Heap *heap, Vec *xs, Str text) {
  Str *boxed = heap_alloc(heap, sizeof(Str));
  if (boxed == NULL) return;
  *boxed = text;
  if (!vec_push(xs, boxed)) heap->oom = true;
}

/* join(items): Nil -> ""; [x] -> x; x :: xs -> x ++ " " ++ join(xs). */
Str spec_count_join(Heap *heap, const Vec *items) {
  Str out = str_empty();
  Str space = str_lit(" ");
  size_t i;
  for (i = 0; i < items->len; i++) {
    Str x = item(items, i);
    if (i == 0) {
      out = x;
    } else {
      Str spaced = heap_str_cat(heap, out, space);
      out = heap_str_cat(heap, spaced, x);
    }
  }
  return out;
}

/* pick(index, items): the item at index; "?" when the list runs out first. */
Str spec_count_pick(Nat index, const Vec *items) {
  NatToU64 n = nat_to_u64(index);
  if (n.status == NAT_TO_U64_OK && n.value < (uint64_t)items->len) return item(items, (size_t)n.value);
  return str_lit("?");
}

/* eta_names(rows, keep): per row, "Ran-<name>" when ran == keep (Bool.not(Bool.xor(ran, keep)))
 * followed by "Lan-<name>" when lan == keep, in table order; Term.formers() is read per row as
 * in the Bend source. */
Vec spec_count_eta_names(Heap *heap, const Vec *rows, bool keep) {
  Vec out = vec_new(heap->arena);
  Str dash = str_lit("-");
  size_t i;
  for (i = 0; i < rows->len; i++) {
    const EtaEntry *row = row_at(rows, i);
    Vec formers = term_formers(heap);
    if (row != NULL) {
      if (row->eta.ran == keep) {
        Nat one = heap_nat_of_u64(heap, 1);
        Str former = spec_count_pick(one, &formers);
        Str dashed = heap_str_cat(heap, former, dash);
        Str named = heap_str_cat(heap, dashed, row->name);
        push_str(heap, &out, named);
      }
      if (row->eta.lan == keep) {
        Nat zero = heap_nat_of_u64(heap, 0);
        Str former = spec_count_pick(zero, &formers);
        Str dashed = heap_str_cat(heap, former, dash);
        Str named = heap_str_cat(heap, dashed, row->name);
        push_str(heap, &out, named);
      }
    }
  }
  return out;
}

/* row(label, items): label ++ " " ++ Nat.show(length items) ++ ": " ++ join(items) ++ "\n". */
Str spec_count_row(Heap *heap, Str label, const Vec *items) {
  Nat length = heap_nat_of_u64(heap, (uint64_t)items->len);
  Str count = heap_nat_show(heap, length);
  Str joined = spec_count_join(heap, items);
  Str space = str_lit(" ");
  Str colon = str_lit(": ");
  Str newline = str_lit("\n");
  Str out = heap_str_cat(heap, label, space);
  out = heap_str_cat(heap, out, count);
  out = heap_str_cat(heap, out, colon);
  out = heap_str_cat(heap, out, joined);
  out = heap_str_cat(heap, out, newline);
  return out;
}

/* print(): the eight rows in the Bend order. */
Str spec_count_print(Heap *heap) {
  Vec formers = term_formers(heap);
  Vec schema = term_schema(heap);
  Vec declared = shape_declared(heap);
  Vec admitted = rules_admitted(heap);
  Vec named_declared = rules_named_declared(heap);
  Vec named_present = rules_named_present(heap);
  Vec eta_table = rules_eta_table(heap);
  Vec eta_table_again = rules_eta_table(heap);
  Vec eta = spec_count_eta_names(heap, &eta_table, true);
  Vec no_eta = spec_count_eta_names(heap, &eta_table_again, false);
  Str out = spec_count_row(heap, str_lit("formers"), &formers);
  Str line = spec_count_row(heap, str_lit("schema constructors"), &schema);
  out = heap_str_cat(heap, out, line);
  line = spec_count_row(heap, str_lit("shapes declared"), &declared);
  out = heap_str_cat(heap, out, line);
  line = spec_count_row(heap, str_lit("shapes admitted"), &admitted);
  out = heap_str_cat(heap, out, line);
  line = spec_count_row(heap, str_lit("named rules declared"), &named_declared);
  out = heap_str_cat(heap, out, line);
  line = spec_count_row(heap, str_lit("named rules present"), &named_present);
  out = heap_str_cat(heap, out, line);
  line = spec_count_row(heap, str_lit("eta rows"), &eta);
  out = heap_str_cat(heap, out, line);
  line = spec_count_row(heap, str_lit("no eta"), &no_eta);
  out = heap_str_cat(heap, out, line);
  return out;
}
