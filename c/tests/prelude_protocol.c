/* Port of tests/prelude_protocol.bend (oracle label prelude-protocol): `ROOT` checks
 * prelude/init.mech with no postulates or primitives, the NAME_ONLY rows of map/prelude.map.tsv,
 * the client fixtures, the prelude families and the test/neg/prelude negatives, one PASS or FAIL line
 * per case, then PRELUDE-OK or a die with the failure count; `--audit FILE` checks one source the
 * same way and prints PRELUDE-AXIOMS OK. Definitions keep the Bend order and names. */
#include "cli.h"
#include "driver.h"
#include "surface.h"

#include <stdio.h>

/* Result<String, Unit> */
typedef enum { OUTCOME_DONE, OUTCOME_FAIL } OutcomeKind;
typedef struct {
  OutcomeKind kind;
  Str message; /* OUTCOME_FAIL */
} Outcome;

/* Result<String, G.T> */
typedef struct {
  OutcomeKind kind;
  Str message;     /* OUTCOME_FAIL */
  Globals globals; /* OUTCOME_DONE */
} Checked;

/* An IO step: IO_STEP_GO carries the value, IO_STEP_EXIT the exit code of IO.die or a host failure. */
typedef enum { IO_STEP_GO, IO_STEP_EXIT } IoStepKind;
typedef struct {
  IoStepKind kind;
  int code;
  size_t failed; /* IO(Nat) */
} Tally;
typedef struct {
  IoStepKind kind;
  int code;
  Str text; /* IO(String) */
} Text;
typedef struct {
  IoStepKind kind;
  int code;
  Globals globals; /* IO(G.T) */
} Loaded;

static Outcome outcome_done(void) {
  Outcome o;
  o.kind = OUTCOME_DONE;
  o.message = str_empty();
  return o;
}

static Outcome outcome_fail(Str message) {
  Outcome o;
  o.kind = OUTCOME_FAIL;
  o.message = message;
  return o;
}

static Checked checked_done(Globals globals) {
  Checked c;
  c.kind = OUTCOME_DONE;
  c.message = str_empty();
  c.globals = globals;
  return c;
}

static Checked checked_fail(Str message) {
  Checked c;
  c.kind = OUTCOME_FAIL;
  c.message = message;
  c.globals = global_empty();
  return c;
}

static Tally tally_go(size_t failed) {
  Tally t;
  t.kind = IO_STEP_GO;
  t.code = 0;
  t.failed = failed;
  return t;
}

static Tally tally_exit(int code) {
  Tally t;
  t.kind = IO_STEP_EXIT;
  t.code = code;
  t.failed = 0;
  return t;
}

static Text text_go(Str text) {
  Text t;
  t.kind = IO_STEP_GO;
  t.code = 0;
  t.text = text;
  return t;
}

static Text text_exit(int code) {
  Text t;
  t.kind = IO_STEP_EXIT;
  t.code = code;
  t.text = str_empty();
  return t;
}

static Loaded loaded_go(Globals globals) {
  Loaded l;
  l.kind = IO_STEP_GO;
  l.code = 0;
  l.globals = globals;
  return l;
}

static Loaded loaded_exit(int code) {
  Loaded l;
  l.kind = IO_STEP_EXIT;
  l.code = code;
  l.globals = global_empty();
  return l;
}

static Str cat(Heap *heap, Str a, Str b) {
  Str s = str_cat(heap->arena, a, b);
  if (s.len != a.len + b.len) heap->oom = true;
  return s;
}

static Str cat3(Heap *heap, Str a, Str b, Str c) { return cat(heap, cat(heap, a, b), c); }

/* IO.die(A, code, message) and IO.print after an out-of-memory heap: the host failure exit. */
static int die_text(Heap *heap, int code, Str message) {
  if (heap->oom) return driver_host_failure();
  return driver_die_text(code, message);
}

static int print_line(Heap *heap, Str text) {
  if (heap->oom) return driver_host_failure();
  return driver_print(text);
}

/* require (10-11) */
static Outcome require(bool ok, Str message) {
  if (ok) return outcome_done();
  return outcome_fail(message);
}

/* checked_result (13-16): Elab.check_in's Done value is an ElabChecked * (globals, rows). */
static Checked checked_result(Heap *heap, Result r) {
  switch (r.kind) {
  case RESULT_FAIL: return checked_fail(error_to_string(heap, r.error));
  case RESULT_DONE: return checked_done(((const ElabChecked *)r.value)->globals);
  }
  return checked_fail(error_to_string(heap, r.error));
}

/* checked (18-19): Elab.check_in under budget_unlimited, run as elab_check_in_compact (the same globals
 * and error text by decision 156, one arena reset per declaration). The Bend bundle reclaimed each
 * declaration's checker garbage by GC; without the reset `--audit` of a compatibility projection
 * fixture grew past 3.8 GB (hop 381, D-381a). */
static Checked checked(Heap *heap, Globals globals, Str source) {
  return checked_result(heap, elab_check_in_compact(heap, globals, source));
}

/* entry_le (21-23): String.is_le on the names (byte order = code point order in UTF-8). */
static bool entry_le(const EntryRows *a, const EntryRows *b) { return str_cmp(a->name, b->name) <= 0; }

/* no_entries (25-30) over the sorted rows: the first Axiom or Prim row fails; Def rows pass. The
 * scan keeps the least non-Def row under entry_le (the first one among equal names), which is the
 * first non-Def row of the stable List.sort order the Bend walks. */
static Outcome no_entries(Heap *heap, const EntryRows *rows) {
  const EntryRows *least = NULL;
  const EntryRows *row = rows;
  while (row != NULL) {
    switch (row->entry.kind) {
    case ENTRY_DEF: break;
    case ENTRY_AXIOM:
    case ENTRY_PRIM:
      if (least == NULL || !entry_le(least, row)) least = row;
      break;
    }
    row = row->rest;
  }
  if (least == NULL) return outcome_done();
  switch (least->entry.kind) {
  case ENTRY_DEF: return outcome_done();
  case ENTRY_AXIOM: return outcome_fail(cat(heap, str_lit("prelude postulate: "), least->name));
  case ENTRY_PRIM: return outcome_fail(cat(heap, str_lit("prelude primitive: "), least->name));
  }
  return outcome_done();
}

/* no_postulates (32-34) */
static Outcome no_postulates(Heap *heap, Globals globals) { return no_entries(heap, globals.entries); }

/* audit (36-40) */
static Checked audit(Heap *heap, Str source) {
  Checked c = checked(heap, global_empty(), source);
  if (c.kind == OUTCOME_FAIL) return c;
  Outcome clean = no_postulates(heap, c.globals);
  if (clean.kind == OUTCOME_FAIL) return checked_fail(clean.message);
  return c;
}

/* accepted (42-45) */
static Outcome accepted(Checked r) {
  switch (r.kind) {
  case OUTCOME_FAIL: return outcome_fail(r.message);
  case OUTCOME_DONE: return outcome_done();
  }
  return outcome_fail(r.message);
}

/* clean (47-50) */
static Outcome clean(Heap *heap, Checked r) {
  switch (r.kind) {
  case OUTCOME_FAIL: return outcome_fail(r.message);
  case OUTCOME_DONE: return no_postulates(heap, r.globals);
  }
  return outcome_fail(r.message);
}

/* refused (52-55) */
static Outcome refused(Heap *heap, Checked r, Str expected) {
  switch (r.kind) {
  case OUTCOME_DONE: return outcome_fail(str_lit("negative source was accepted"));
  case OUTCOME_FAIL: return require(str_eq(r.message, expected), cat(heap, str_lit("wrong diagnostic: "), r.message));
  }
  return outcome_fail(str_lit("negative source was accepted"));
}

/* some (57-60) is the .some field of the Maybe structs. ctor_named (62-65) */
static bool ctor_named(const FamilyRows *families, Str name) {
  const FamilyRows *row = families;
  while (row != NULL) {
    if (positivity_ctor_of(name, row->family).some) return true;
    row = row->rest;
  }
  return false;
}

/* declared (67-70) */
static bool declared(Globals globals, Str name) {
  return global_find_entry(globals.entries, name).some || global_find_family_entry(globals.families, name).some ||
         ctor_named(globals.families, name);
}

/* C.Pair<Nat, Bool> of map_cells, map_combine and map_lines. */
typedef struct {
  size_t rows;
  bool ok;
} MapCount;

static MapCount map_count(size_t rows, bool ok) {
  MapCount m;
  m.rows = rows;
  m.ok = ok;
  return m;
}

static Str view(Str s, size_t start, size_t end) {
  StrSlice slice = str_slice(s, start, end);
  if (slice.status == STR_INDEX_OK) return slice.value;
  return str_empty();
}

/* The end of the String.split piece that starts at START: the next SEP byte or the text end. */
static size_t piece_end(Str text, size_t start, unsigned char sep) {
  size_t end = start;
  while (end < text.len && !(str_at(text, end).status == STR_INDEX_OK && str_at(text, end).value == sep)) end++;
  return end;
}

/* map_cells (72-75) over String.split(line, Chr{9}): exactly three cells whose third is NAME_ONLY
 * count one row, declared when the second cell names a checked prelude entity. */
static MapCount map_cells(Str line, Globals globals) {
  size_t first_end = piece_end(line, 0, 9);
  if (first_end >= line.len) return map_count(0, true);
  size_t second_end = piece_end(line, first_end + 1, 9);
  if (second_end >= line.len) return map_count(0, true);
  size_t third_end = piece_end(line, second_end + 1, 9);
  if (third_end < line.len) return map_count(0, true);
  if (!str_eq(view(line, second_end + 1, third_end), str_lit("NAME_ONLY"))) return map_count(0, true);
  return map_count(1, declared(globals, view(line, first_end + 1, second_end)));
}

/* map_combine (77-79) */
static MapCount map_combine(MapCount a, MapCount b) { return map_count(a.rows + b.rows, a.ok && b.ok); }

/* map_lines (81-84) over String.split(map, Chr{10}): every piece, the empty last one included. */
static MapCount map_lines(Str text, Globals globals) {
  MapCount total = map_count(0, true);
  size_t start = 0;
  while (start <= text.len) {
    size_t end = piece_end(text, start, 10);
    total = map_combine(total, map_cells(view(text, start, end), globals));
    start = end + 1;
  }
  return total;
}

/* map_checked (86-88) */
static Outcome map_checked(MapCount r) {
  return require(r.rows > 0 && r.ok, str_lit("NAME_ONLY target is not a checked prelude name, or no NAME_ONLY rows"));
}

/* empty (90-93) */
static bool empty(Globals globals) { return globals.entries == NULL && globals.families == NULL; }

/* family_result (95-99) */
static Outcome family_result(Heap *heap, MaybeFamily m, Str name) {
  if (!m.some) return outcome_fail(cat(heap, str_lit("missing checked family: "), name));
  switch (m.value.status.kind) {
  case CTOR_STATUS_COMPLETE: return require(m.value.positive, cat(heap, str_lit("nonpositive family: "), name));
  case CTOR_STATUS_PROVISIONAL:
  case CTOR_STATUS_BUILTIN: return outcome_fail(cat(heap, str_lit("family was not defined by the prelude: "), name));
  }
  return outcome_fail(cat(heap, str_lit("family was not defined by the prelude: "), name));
}

/* report (106-113) */
static Tally report(Heap *heap, Str name, Outcome r) {
  switch (r.kind) {
  case OUTCOME_DONE: {
    int code = print_line(heap, cat(heap, str_lit("PASS prelude "), name));
    return code == 0 ? tally_go(0) : tally_exit(code);
  }
  case OUTCOME_FAIL: {
    int code = print_line(heap, cat(heap, cat3(heap, str_lit("FAIL prelude "), name, str_lit(": ")), r.message));
    return code == 0 ? tally_go(1) : tally_exit(code);
  }
  }
  return tally_exit(driver_host_failure());
}

/* One step of reports (115-120): the row is reported and its count added unless an earlier step exited. */
static Tally reports_row(Heap *heap, Tally failed, const char *name, Outcome r) {
  if (failed.kind == IO_STEP_EXIT) return failed;
  Tally bad = report(heap, str_lit(name), r);
  if (bad.kind == IO_STEP_EXIT) return bad;
  return tally_go(failed.failed + bad.failed);
}

/* families (101-104) reported through reports (115-120). */
static Tally families(Heap *heap, const char *const *names, size_t count, Globals globals, Tally failed) {
  Tally total = failed;
  size_t i = 0;
  while (i < count && total.kind == IO_STEP_GO) {
    Str name = str_lit(names[i]);
    Tally bad = report(heap, cat(heap, str_lit("family-"), name), family_result(heap, global_find_family(name, globals), name));
    total = bad.kind == IO_STEP_EXIT ? bad : tally_go(total.failed + bad.failed);
    i++;
  }
  return total;
}

/* accept (122-125) for G.T */
static Loaded accept(Heap *heap, Checked r) {
  switch (r.kind) {
  case OUTCOME_DONE: return heap->oom ? loaded_exit(driver_host_failure()) : loaded_go(r.globals);
  case OUTCOME_FAIL: return loaded_exit(die_text(heap, 1, r.message));
  }
  return loaded_exit(die_text(heap, 1, r.message));
}

/* read (127-130): F.read_file, accept(String, r). */
static Text read(Heap *heap, Str path) {
  IoResult r = io_read_file(heap, path);
  switch (r.status) {
  case IO_DONE: return heap->oom ? text_exit(driver_host_failure()) : text_go(r.value);
  case IO_FAIL: return text_exit(die_text(heap, 1, r.value));
  }
  return text_exit(die_text(heap, 1, r.value));
}

/* negative_result (132-135) */
static Outcome negative_result(Heap *heap, Checked r, Str expected) {
  switch (r.kind) {
  case OUTCOME_DONE: return outcome_fail(str_lit("negative source was accepted"));
  case OUTCOME_FAIL:
    return require(expected.len > 0 && str_starts_with(r.message, expected), cat(heap, str_lit("wrong diagnostic: "), r.message));
  }
  return outcome_fail(str_lit("negative source was accepted"));
}

static bool is_space(unsigned char c) { return c == ' ' || (c >= 9 && c <= 13); }

/* String.trim: drop Char.is_space (32 and 9..13) at both ends. */
static Str trim(Str s) {
  size_t start = 0;
  size_t end = s.len;
  while (start < end && str_at(s, start).status == STR_INDEX_OK && is_space(str_at(s, start).value)) start++;
  while (end > start && str_at(s, end - 1).status == STR_INDEX_OK && is_space(str_at(s, end - 1).value)) end--;
  return view(s, start, end);
}

/* negatives (137-144) */
static Tally negatives(Heap *heap, const char *const *names, size_t count, Str root, Globals globals, Tally failed) {
  Tally total = failed;
  size_t i = 0;
  while (i < count && total.kind == IO_STEP_GO) {
    Str name = str_lit(names[i]);
    Str base = cat3(heap, root, str_lit("/test/neg/prelude/"), name);
    Text source = read(heap, cat(heap, base, str_lit(".mech")));
    if (source.kind == IO_STEP_EXIT) return tally_exit(source.code);
    Text expected = read(heap, cat(heap, base, str_lit(".err")));
    if (expected.kind == IO_STEP_EXIT) return tally_exit(expected.code);
    Tally bad = report(heap, cat(heap, str_lit("reject-"), name),
                       negative_result(heap, checked(heap, globals, source.text), trim(expected.text)));
    total = bad.kind == IO_STEP_EXIT ? bad : tally_go(total.failed + bad.failed);
    i++;
  }
  return total;
}

static size_t entry_count(const EntryRows *rows) {
  size_t n = 0;
  const EntryRows *row = rows;
  while (row != NULL) {
    n++;
    row = row->rest;
  }
  return n;
}

static size_t family_count(const FamilyRows *rows) {
  size_t n = 0;
  const FamilyRows *row = rows;
  while (row != NULL) {
    n++;
    row = row->rest;
  }
  return n;
}

/* passed (146-149) */
static int passed(Heap *heap, Globals globals) {
  char line[128];
  int n = snprintf(line, sizeof line, "PRELUDE-OK families=%zu definitions=%zu axioms=0 primitives=0",
                   family_count(globals.families), entry_count(globals.entries));
  if (n < 0 || (size_t)n >= sizeof line) return driver_host_failure();
  return print_line(heap, str_n(line, (size_t)n));
}

/* finish (151-154) */
static int finish(Heap *heap, size_t failed, Globals globals) {
  if (failed == 0) return passed(heap, globals);
  char line[64];
  int n = snprintf(line, sizeof line, "%zu prelude cases failed", failed);
  if (n < 0 || (size_t)n >= sizeof line) return driver_host_failure();
  return die_text(heap, 1, str_n(line, (size_t)n));
}

static const char *const FAMILY_NAMES[] = {"MechNat",  "MechBool",      "MechUnit", "MechEmpty", "MechFalse",
                                           "MechSum",  "MechDecidable", "MechTrue", "MechEq",    "MechProofEq"};

static const char *const NEGATIVE_NAMES[] = {
    "eq-relevant-index",     "eq-erased-endpoint", "eq-wrong-endpoint",     "eq-multiple-large-elim", "eq-constructor-field",
    "eq-type-index",         "nat-wrong-constructor", "nat-rec-wrong-step", "proof-eq-large-elim",    "proof-eq-wrong-endpoint",
    "sum-missing-field",     "sum-wrong-parameter", "sum-wrong-nested",     "sum-wrong-family",       "decidable-wrong-proof",
    "dependent-wrong-field", "indexed-wrong-result"};

/* suite (156-173) */
static int suite(Heap *heap, Str root) {
  Text source = read(heap, cat(heap, root, str_lit("/prelude/init.mech")));
  if (source.kind == IO_STEP_EXIT) return source.code;
  Loaded globals = accept(heap, audit(heap, source.text));
  if (globals.kind == IO_STEP_EXIT) return globals.code;
  Text map = read(heap, cat(heap, root, str_lit("/map/prelude.map.tsv")));
  if (map.kind == IO_STEP_EXIT) return map.code;
  Text client = read(heap, cat(heap, root, str_lit("/test/fixtures/prelude/client.mech")));
  if (client.kind == IO_STEP_EXIT) return client.code;
  Text equality = read(heap, cat(heap, root, str_lit("/test/fixtures/prelude/equality.mech")));
  if (equality.kind == IO_STEP_EXIT) return equality.code;
  Globals g = globals.globals;
  Tally failed = tally_go(0);
  failed = reports_row(heap, failed, "empty-environment", require(empty(global_empty()), str_lit("the initial environment is not empty")));
  failed = reports_row(heap, failed, "map-name-only-targets", map_checked(map_lines(map.text, g)));
  failed = reports_row(heap, failed, "reject-postulate",
                       refused(heap, audit(heap, cat(heap, source.text, str_lit("\naxiom CounterfeitInit : Type 0\n"))),
                               str_lit("prelude postulate: CounterfeitInit")));
  failed = reports_row(heap, failed, "reject-hidden-builtin",
                       refused(heap, checked(heap, global_empty(), str_lit("def HiddenBuiltin : Type 0 := Nat\n")), str_lit("unbound: Nat")));
  failed = reports_row(heap, failed, "duplicate-constructor-name",
                       accepted(checked(heap, g,
                                        str_lit("mu AltSum (0 A : Type 0) : Type 0 with\n| mechInl : A -> AltSum A\n\n"
                                                "def stillSum : MechSum MechNat MechUnit := mechInl mechZero\n"
                                                "def altUse : AltSum MechBool := mechInl mechTrue\n"))));
  failed = reports_row(heap, failed, "client", clean(heap, checked(heap, g, client.text)));
  failed = reports_row(heap, failed, "equality-client", clean(heap, checked(heap, g, equality.text)));
  failed = families(heap, FAMILY_NAMES, sizeof FAMILY_NAMES / sizeof FAMILY_NAMES[0], g, failed);
  failed = negatives(heap, NEGATIVE_NAMES, sizeof NEGATIVE_NAMES / sizeof NEGATIVE_NAMES[0], root, g, failed);
  if (failed.kind == IO_STEP_EXIT) return failed.code;
  return finish(heap, failed.failed, g);
}

/* audit_file (175-179) */
static int audit_file(Heap *heap, Str path) {
  Text text = read(heap, path);
  if (text.kind == IO_STEP_EXIT) return text.code;
  Loaded globals = accept(heap, audit(heap, text.text));
  if (globals.kind == IO_STEP_EXIT) return globals.code;
  return print_line(heap, str_lit("PRELUDE-AXIOMS OK"));
}

/* arguments (181-185) over IO.args() = argv[1..]. */
static int arguments(Heap *heap, int argc, char **argv) {
  if (argc == 3 && str_eq(str_lit(argv[1]), str_lit("--audit"))) return audit_file(heap, str_lit(argv[2]));
  if (argc == 2) return suite(heap, str_lit(argv[1]));
  return driver_die(1, "usage: prelude ROOT | prelude --audit FILE");
}

static int entry(int argc, char **argv) {
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  int code = arguments(&heap, argc, argv);
  arena_free(&arena);
  return code;
}

/* main (187-190) */
int main(int argc, char **argv) { return rt_run_main(argc, argv, entry); }
