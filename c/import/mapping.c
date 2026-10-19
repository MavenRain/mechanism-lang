/* Port of import/mapping.bend: the external-constant inventory of an export. Every referenced
 * external constant gets a NAME_ONLY (a mapping target exists), UNMAPPED or NEVER (ratified
 * Lean.Omega reflection) verdict, the NEVER rows become exception rows, and map_tsv and never_tsv
 * render the two TSV files. Every definition keeps its Bend name with the mapping_ prefix, in Bend
 * order; the Bend list recursions are index walkers with an accumulator (Vec items in order). A
 * NULL item is a failed allocation (heap->oom set) and is not stored, like the other import
 * modules. */
#include "import.h"

/* out with item appended; a NULL item is a failed allocation (heap->oom set) and is not stored */
static Vec with_item(Heap *heap, Vec out, void *item) {
  if (item == NULL) return out;
  return json_vec_with(heap, out, item);
}

/* a ++ b; a short result is a failed allocation */
static Str cat(Heap *heap, Str a, Str b) {
  Str out = str_cat(heap->arena, a, b);
  if (out.len < a.len + b.len) heap->oom = true;
  return out;
}

/* String.concat(parts) from INDEX */
static Str concat_from(Heap *heap, const Str *parts, size_t count, size_t index, Str acc) {
  if (index >= count) return acc;
  return concat_from(heap, parts, count, index + 1, cat(heap, acc, parts[index]));
}

/* the Str of a boxed Str* (empty for NULL) */
static Str unbox_text(const void *box) { return box == NULL ? str_empty() : *(const Str *)box; }

/* the items of xs from INDEX appended to acc */
static Vec append_from(Heap *heap, Vec acc, const Vec *xs, size_t index) {
  VecGet got = vec_get(xs, index);
  if (got.status != VEC_GET_OK) return acc;
  return append_from(heap, json_vec_with(heap, acc, got.value), xs, index + 1);
}

static MappingRow *box_row(Heap *heap, MappingRow value) {
  MappingRow *box = heap_alloc(heap, sizeof(MappingRow));
  if (box == NULL) return NULL;
  *box = value;
  return box;
}

static MappingExceptionRow *box_exception(Heap *heap, MappingExceptionRow value) {
  MappingExceptionRow *box = heap_alloc(heap, sizeof(MappingExceptionRow));
  if (box == NULL) return NULL;
  *box = value;
  return box;
}

static MappingTarget *box_target(Heap *heap, MappingTarget value) {
  MappingTarget *box = heap_alloc(heap, sizeof(MappingTarget));
  if (box == NULL) return NULL;
  *box = value;
  return box;
}

/* verdict_name(verdict) */
Str mapping_verdict_name(MappingVerdict verdict) {
  switch (verdict) {
    case MAPPING_NAME_ONLY: return str_lit("NAME_ONLY");
    case MAPPING_UNMAPPED: return str_lit("UNMAPPED");
    case MAPPING_NEVER: return str_lit("NEVER");
  }
  return str_lit("NEVER");
}

/* omega_reflection(name) */
bool mapping_omega_reflection(Heap *heap, Str name) { return report_namespace(heap, name, str_lit("Lean.Omega")); }

/* clean_char(ch): NUL, tab, newline and carriage return are the unclean characters (each is one
 * UTF-8 byte, so the byte test equals the Bend codepoint test) */
bool mapping_clean_char(char ch) { return ch != 0 && ch != 9 && ch != 10 && ch != 13; }

/* clean_text(text) from byte INDEX */
static bool clean_from(Str text, size_t index) {
  if (index >= text.len) return true;
  if (!mapping_clean_char(text.data[index])) return false;
  return clean_from(text, index + 1);
}

/* clean_text(text) */
bool mapping_clean_text(Str text) { return clean_from(text, 0); }

/* cell(value): nonempty, not "-", and clean */
bool mapping_cell(Str value) {
  if (value.len == 0) return false;
  if (str_eq(value, str_lit("-"))) return false;
  return mapping_clean_text(value);
}

/* guard(ok, message) */
TableCheck mapping_guard(bool ok, Str message) {
  TableCheck out;
  out.kind = ok ? JSON_DONE : JSON_FAIL;
  out.message = ok ? str_empty() : message;
  return out;
}

/* add_external(yes, name, names) */
TranslateIdSet mapping_add_external(Heap *heap, bool yes, Str name, TranslateIdSet names) {
  if (yes) return translate_set_add(heap, names, name);
  return names;
}

static MappingNames names_done(TranslateIdSet value) {
  MappingNames out;
  out.kind = JSON_DONE;
  out.value = value;
  out.message = str_empty();
  return out;
}

static MappingNames names_fail(Heap *heap, Str message) {
  MappingNames out;
  out.kind = JSON_FAIL;
  out.value = translate_empty_set(heap);
  out.message = message;
  return out;
}

/* external_names(ids, source, names) from INDEX: Table.require of the canonical source name */
static MappingNames external_from(Heap *heap, const Vec *ids, size_t index, const Export *source, TranslateIdSet names) {
  VecGet got = vec_get(ids, index);
  if (got.status != VEC_GET_OK) return names_done(names);
  Str id = unbox_text(got.value);
  ExportText name = export_name_string(source, id);
  if (!name.present) return names_fail(heap, cat(heap, str_lit("missing canonical source name "), id));
  bool external = report_external_name(heap, name.value);
  return external_from(heap, ids, index + 1, source, mapping_add_external(heap, external, name.value, names));
}

/* external_names(ids, source, names): ids are Str* items */
MappingNames mapping_external_names(Heap *heap, const Vec *ids, const Export *source, TranslateIdSet names) {
  return external_from(heap, ids, 0, source, names);
}

static MappingTargets targets_done(Table value) {
  MappingTargets out;
  out.kind = JSON_DONE;
  out.value = value;
  out.message = str_empty();
  return out;
}

static MappingTargets targets_fail(Heap *heap, Str message) {
  MappingTargets out;
  out.kind = JSON_FAIL;
  out.value = table_empty(heap);
  out.message = message;
  return out;
}

/* targets(rows, names, found) from INDEX: the four guards in Bend order, then put */
static MappingTargets targets_from(Heap *heap, const Vec *rows, size_t index, const TranslateIdSet *names, Table found) {
  VecGet got = vec_get(rows, index);
  if (got.status != VEC_GET_OK) return targets_done(found);
  const MappingTarget *row = got.value;
  if (row == NULL) return targets_from(heap, rows, index + 1, names, found);
  Str name = row->lean_name;
  TableCheck cells = mapping_guard(mapping_cell(name) && mapping_cell(row->target), str_lit("mapping targets require nonempty TSV cells"));
  if (cells.kind == JSON_FAIL) return targets_fail(heap, cells.message);
  TableCheck fresh = mapping_guard(!table_has(&found, name), cat(heap, str_lit("duplicate mapping target for "), name));
  if (fresh.kind == JSON_FAIL) return targets_fail(heap, fresh.message);
  TableCheck known = mapping_guard(translate_set_has(names, name), cat(heap, str_lit("mapping target is not a referenced external constant: "), name));
  if (known.kind == JSON_FAIL) return targets_fail(heap, known.message);
  TableCheck ratified = mapping_guard(!mapping_omega_reflection(heap, name), cat(heap, str_lit("mapping target conflicts with ratified NEVER: "), name));
  if (ratified.kind == JSON_FAIL) return targets_fail(heap, ratified.message);
  table_put(heap, &found, name, json_boxed_str(heap, row->target));
  return targets_from(heap, rows, index + 1, names, found);
}

/* targets(rows, names, found): rows are MappingTarget* items; the table is updated in place */
MappingTargets mapping_targets(Heap *heap, const Vec *rows, const TranslateIdSet *names, Table found) {
  return targets_from(heap, rows, 0, names, found);
}

static Vec merge_from(Heap *heap, const Vec *left, size_t li, const Vec *right, size_t ri, Vec acc);

/* merge_order(order, left, xs, right, ys): GT takes the right head first */
static Vec merge_order(Heap *heap, Cmp order, const Vec *left, size_t li, const Vec *right, size_t ri, Vec acc) {
  VecGet a = vec_get(left, li);
  VecGet b = vec_get(right, ri);
  if (order == CMP_GT) return merge_from(heap, left, li, right, ri + 1, json_vec_with(heap, acc, b.value));
  return merge_from(heap, left, li + 1, right, ri, json_vec_with(heap, acc, a.value));
}

/* merge_names(left, right) from the heads at LI and RI, by String.order */
static Vec merge_from(Heap *heap, const Vec *left, size_t li, const Vec *right, size_t ri, Vec acc) {
  VecGet a = vec_get(left, li);
  VecGet b = vec_get(right, ri);
  if (a.status != VEC_GET_OK) return append_from(heap, acc, right, ri);
  if (b.status != VEC_GET_OK) return append_from(heap, acc, left, li);
  return merge_order(heap, json_lexical(unbox_text(a.value), unbox_text(b.value)), left, li, right, ri, acc);
}

/* merge_names(left, right): Str* items, both inputs in codepoint order */
Vec mapping_merge_names(Heap *heap, const Vec *left, const Vec *right) {
  return merge_from(heap, left, 0, right, 0, vec_new(heap->arena));
}

/* Export.KeySplit{left, right}: Str* items */
typedef struct {
  Vec left;
  Vec right;
} Split;

/* Export.split_prepend(a, b, rest): a joins the left half, b the right half */
static Split split_prepend(Heap *heap, void *a, void *b, Split rest) {
  Split split;
  split.left = json_vec_with(heap, rest.left, a);
  split.right = json_vec_with(heap, rest.right, b);
  return split;
}

/* Export.split_keys(keys) from INDEX: alternate items into the two halves, keeping their order */
static Split split_from(Heap *heap, const Vec *keys, size_t index, Split acc) {
  VecGet one = vec_get(keys, index);
  VecGet two = vec_get(keys, index + 1);
  if (one.status != VEC_GET_OK) return acc;
  if (two.status != VEC_GET_OK) {
    acc.left = json_vec_with(heap, acc.left, one.value);
    return acc;
  }
  return split_from(heap, keys, index + 2, split_prepend(heap, one.value, two.value, acc));
}

/* Export.split_keys(keys) */
static Split split_keys(Heap *heap, const Vec *keys) {
  Split empty;
  empty.left = vec_new(heap->arena);
  empty.right = vec_new(heap->arena);
  return split_from(heap, keys, 0, empty);
}

/* sort_split(split) */
static Vec sort_split(Heap *heap, Split split) {
  Vec left = mapping_sort_names(heap, &split.left);
  Vec right = mapping_sort_names(heap, &split.right);
  return mapping_merge_names(heap, &left, &right);
}

/* sort_names(names): Str* items in codepoint order (merge sort); zero or one name is a copy */
Vec mapping_sort_names(Heap *heap, const Vec *names) {
  if (vec_get(names, 1).status != VEC_GET_OK) return append_from(heap, vec_new(heap->arena), names, 0);
  return sort_split(heap, split_keys(heap, names));
}

/* mapped_row(target, name): the table value is a boxed Str* */
MappingRow *mapping_mapped_row(Heap *heap, TableGet target, Str name) {
  MappingRow row;
  row.lean_name = name;
  row.target.present = target.present;
  row.target.value = target.present ? unbox_text(target.value) : str_empty();
  row.verdict = target.present ? MAPPING_NAME_ONLY : MAPPING_UNMAPPED;
  return box_row(heap, row);
}

/* classify(never, name, targets) */
MappingRow *mapping_classify(Heap *heap, bool never, Str name, const Table *targets) {
  if (never) {
    MappingRow row;
    row.lean_name = name;
    row.target.present = false;
    row.target.value = str_empty();
    row.verdict = MAPPING_NEVER;
    return box_row(heap, row);
  }
  return mapping_mapped_row(heap, table_get(targets, name), name);
}

/* make_rows(names, targets) from INDEX */
static Vec rows_from(Heap *heap, const Vec *names, size_t index, const Table *targets, Vec out) {
  VecGet got = vec_get(names, index);
  if (got.status != VEC_GET_OK) return out;
  Str name = unbox_text(got.value);
  MappingRow *row = mapping_classify(heap, mapping_omega_reflection(heap, name), name, targets);
  return rows_from(heap, names, index + 1, targets, with_item(heap, out, row));
}

/* make_rows(names, targets): names are Str* items; MappingRow* items out */
Vec mapping_make_rows(Heap *heap, const Vec *names, const Table *targets) {
  return rows_from(heap, names, 0, targets, vec_new(heap->arena));
}

/* never_row(row, rest): a NEVER row becomes the ratified exception row, appended to the kept prefix
 * (the Bend conses onto the filtered tail; the output order is the row order either way) */
Vec mapping_never_row(Heap *heap, const MappingRow *row, Vec rest) {
  switch (row->verdict) {
    case MAPPING_NEVER: {
      MappingExceptionRow exception;
      exception.lean_name = row->lean_name;
      exception.ratification = str_lit("R-V4:omega-reflection");
      exception.milestone = str_lit("NEVER");
      return with_item(heap, rest, box_exception(heap, exception));
    }
    case MAPPING_NAME_ONLY: return rest;
    case MAPPING_UNMAPPED: return rest;
  }
  return rest;
}

/* never_rows(rows) from INDEX */
static Vec never_from(Heap *heap, const Vec *rows, size_t index, Vec out) {
  VecGet got = vec_get(rows, index);
  if (got.status != VEC_GET_OK) return out;
  const MappingRow *row = got.value;
  if (row == NULL) return never_from(heap, rows, index + 1, out);
  return never_from(heap, rows, index + 1, mapping_never_row(heap, row, out));
}

/* never_rows(rows): rows are MappingRow* items; MappingExceptionRow* items out */
Vec mapping_never_rows(Heap *heap, const Vec *rows) { return never_from(heap, rows, 0, vec_new(heap->arena)); }

/* count_declared(declarations, source) from INDEX */
static size_t declared_from(Heap *heap, const Vec *declarations, size_t index, const Export *source, size_t acc) {
  VecGet got = vec_get(declarations, index);
  if (got.status != VEC_GET_OK) return acc;
  const DeclEntry *declaration = got.value;
  if (declaration == NULL) return declared_from(heap, declarations, index + 1, source, acc);
  bool external = report_external_display(heap, export_name_string(source, declaration->name));
  return declared_from(heap, declarations, index + 1, source, report_increment(external, acc));
}

/* count_declared(declarations, source): declarations are DeclEntry* items */
size_t mapping_count_declared(Heap *heap, const Vec *declarations, const Export *source) {
  return declared_from(heap, declarations, 0, source, 0);
}

static MappingResult result_fail(Heap *heap, Str message) {
  MappingResult out;
  out.kind = JSON_FAIL;
  out.value.rows = vec_new(heap->arena);
  out.value.never = vec_new(heap->arena);
  out.value.external_declared = 0;
  out.value.const_names = 0;
  out.message = message;
  return out;
}

/* inventory(source, target_rows): target_rows are MappingTarget* items */
MappingResult mapping_inventory(Heap *heap, const Export *source, const Vec *target_rows) {
  TranslateIdSet constants = report_constant_names(heap, source);
  Vec constant_keys = translate_set_keys(heap, &constants);
  Vec sorted_keys = export_sort_keys(heap, &constant_keys);
  MappingNames names = mapping_external_names(heap, &sorted_keys, source, translate_empty_set(heap));
  if (names.kind == JSON_FAIL) return result_fail(heap, names.message);
  MappingTargets targets = mapping_targets(heap, target_rows, &names.value, table_empty(heap));
  if (targets.kind == JSON_FAIL) return result_fail(heap, targets.message);
  Vec name_keys = translate_set_keys(heap, &names.value);
  Vec ordered = mapping_sort_names(heap, &name_keys);
  Vec declarations = export_declarations(source);
  MappingResult out;
  out.kind = JSON_DONE;
  out.value.rows = mapping_make_rows(heap, &ordered, &targets.value);
  out.value.never = mapping_never_rows(heap, &out.value.rows);
  out.value.external_declared = mapping_count_declared(heap, &declarations, source);
  out.value.const_names = translate_set_keys(heap, &constants).len;
  out.message = str_empty();
  return out;
}

/* add_display(value, names) */
TranslateIdSet mapping_add_display(Heap *heap, ExportText value, TranslateIdSet names) {
  if (value.present) return translate_set_add(heap, names, value.value);
  return names;
}

/* displays(ids, source, names) from INDEX */
static TranslateIdSet displays_from(Heap *heap, const Vec *ids, size_t index, const Export *source, TranslateIdSet names) {
  VecGet got = vec_get(ids, index);
  if (got.status != VEC_GET_OK) return names;
  ExportText value = export_name_string(source, unbox_text(got.value));
  return displays_from(heap, ids, index + 1, source, mapping_add_display(heap, value, names));
}

/* displays(ids, source, names): ids are Str* items */
TranslateIdSet mapping_displays(Heap *heap, const Vec *ids, const Export *source, TranslateIdSet names) {
  return displays_from(heap, ids, 0, source, names);
}

/* keep_target(found, target, rest): the kept target joins the prefix */
Vec mapping_keep_target(Heap *heap, bool found, MappingTarget *target, Vec rest) {
  if (found) return with_item(heap, rest, target);
  return rest;
}

/* present_targets(targets, names) from INDEX */
static Vec present_from(Heap *heap, const Vec *targets, size_t index, const TranslateIdSet *names, Vec out) {
  VecGet got = vec_get(targets, index);
  if (got.status != VEC_GET_OK) return out;
  MappingTarget *target = got.value;
  if (target == NULL) return present_from(heap, targets, index + 1, names, out);
  bool found = translate_set_has(names, target->lean_name);
  return present_from(heap, targets, index + 1, names, mapping_keep_target(heap, found, target, out));
}

/* present_targets(targets, names): MappingTarget* items in and out */
Vec mapping_present_targets(Heap *heap, const Vec *targets, const TranslateIdSet *names) {
  return present_from(heap, targets, 0, names, vec_new(heap->arena));
}

/* the foundation target table of foundation_targets, in Bend order */
static const char *const foundation_table[][2] = {
  {"Nat", "MechNat"},
  {"Nat.zero", "mechZero"},
  {"Nat.succ", "mechSucc"},
  {"Bool", "MechBool"},
  {"Bool.false", "mechFalse"},
  {"Bool.true", "mechTrue"},
  {"Unit", "MechUnit"},
  {"Unit.unit", "mechUnit"},
  {"Empty", "MechEmpty"},
  {"False", "MechFalse"},
  {"Sum", "MechSum"},
  {"Sum.inl", "mechInl"},
  {"Sum.inr", "mechInr"},
  {"Decidable", "MechDecidable"},
  {"Decidable.isFalse", "mechIsFalse"},
  {"Decidable.isTrue", "mechIsTrue"},
  {"Eq", "MechEq"},
  {"Eq.refl", "mechRefl"},
  {"Eq.rec", "mechJ"},
  {"Eq.ndrec", "mechTransport"},
  {"Eq.symm", "mechSymm"},
  {"Eq.trans", "mechTrans"},
  {"congrArg", "mechCongr"},
};

/* the foundation table as MappingTarget* items from INDEX */
static Vec foundation_from(Heap *heap, size_t index, Vec out) {
  size_t count = sizeof(foundation_table) / sizeof(foundation_table[0]);
  if (index >= count) return out;
  MappingTarget target;
  target.lean_name = str_lit(foundation_table[index][0]);
  target.target = str_lit(foundation_table[index][1]);
  return foundation_from(heap, index + 1, with_item(heap, out, box_target(heap, target)));
}

/* foundation_targets(source): the foundation targets whose display name the export references */
Vec mapping_foundation_targets(Heap *heap, const Export *source) {
  TranslateIdSet constants = report_constant_names(heap, source);
  Vec ids = translate_set_keys(heap, &constants);
  TranslateIdSet names = mapping_displays(heap, &ids, source, translate_empty_set(heap));
  Vec table = foundation_from(heap, 0, vec_new(heap->arena));
  return mapping_present_targets(heap, &table, &names);
}

/* target_text(target) */
Str mapping_target_text(ExportText target) {
  if (target.present) return target.value;
  return str_lit("-");
}

/* map_lines(rows) from INDEX */
static Str map_lines_from(Heap *heap, const Vec *rows, size_t index, Str acc) {
  VecGet got = vec_get(rows, index);
  if (got.status != VEC_GET_OK) return acc;
  const MappingRow *row = got.value;
  if (row == NULL) return map_lines_from(heap, rows, index + 1, acc);
  Str parts[6];
  parts[0] = row->lean_name;
  parts[1] = str_lit("\t");
  parts[2] = mapping_target_text(row->target);
  parts[3] = str_lit("\t");
  parts[4] = mapping_verdict_name(row->verdict);
  parts[5] = str_lit("\n");
  return map_lines_from(heap, rows, index + 1, concat_from(heap, parts, 6, 0, acc));
}

/* map_lines(rows): rows are MappingRow* items */
Str mapping_map_lines(Heap *heap, const Vec *rows) { return map_lines_from(heap, rows, 0, str_empty()); }

/* never_lines(rows) from INDEX */
static Str never_lines_from(Heap *heap, const Vec *rows, size_t index, Str acc) {
  VecGet got = vec_get(rows, index);
  if (got.status != VEC_GET_OK) return acc;
  const MappingExceptionRow *row = got.value;
  if (row == NULL) return never_lines_from(heap, rows, index + 1, acc);
  Str parts[6];
  parts[0] = row->lean_name;
  parts[1] = str_lit("\t");
  parts[2] = row->ratification;
  parts[3] = str_lit("\t");
  parts[4] = row->milestone;
  parts[5] = str_lit("\n");
  return never_lines_from(heap, rows, index + 1, concat_from(heap, parts, 6, 0, acc));
}

/* never_lines(rows): rows are MappingExceptionRow* items */
Str mapping_never_lines(Heap *heap, const Vec *rows) { return never_lines_from(heap, rows, 0, str_empty()); }

/* map_tsv(inventory) */
Str mapping_map_tsv(Heap *heap, const MappingInventory *inventory) {
  return cat(heap, str_lit("lean_name\ttarget\tverdict\n"), mapping_map_lines(heap, &inventory->rows));
}

/* never_tsv(inventory) */
Str mapping_never_tsv(Heap *heap, const MappingInventory *inventory) {
  return cat(heap, str_lit("lean_name\tratification\tmilestone\n"), mapping_never_lines(heap, &inventory->never));
}
