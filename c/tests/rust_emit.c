/* C port of tests/rust_emit.bend. Driver: `rust_emit sample <init|second>` or
 * `classify <file.mech> [<file.mech>]`.
 * `sample` prints a module from a hand-built IR. Each item of the text must be equal to the same
 * item of the hand-written target crate.
 * `classify` checks the files in order and prints one line for each declaration: its class and,
 * for a runtime declaration, its Rust form.
 * The other modes: `emit`, `crate` (cli/rust_out run), `values`, `probes` and `bin`
 * (tests/rust_emit_oracle). Each IO(Unit) of the Bend file is an int here: the exit code, after the
 * output is written. A failed allocation sets heap->oom; the builders then return placeholders and
 * the driver exits with driver_host_failure. */
#include "driver.h"
#include "rust.h"
#include "cli.h"
#include "rust_emit_oracle.h"

static const Str re_str_placeholder = {0};

static void re_push(Heap *heap, Vec *acc, const void *item) {
  if (!vec_push(acc, (void *)item)) heap->oom = true;
}

/* A Bend list literal: the pointers of `items` up to the NULL that ends the array. */
static Vec re_list(Heap *heap, const void *const *items) {
  Vec v = vec_new(heap->arena);
  for (size_t i = 0; items[i] != NULL; i++) re_push(heap, &v, items[i]);
  return v;
}

#define RE_LIST(heap, ...) re_list((heap), (const void *const[]){__VA_ARGS__, NULL})
#define RE_NIL(heap) vec_new((heap)->arena)

/* A String element of a list (the generics of an item). */
static const Str *re_name(Heap *heap, const char *text) {
  Str *s = heap_alloc(heap, sizeof(Str));
  if (s == NULL) return &re_str_placeholder;
  *s = str_lit(text);
  return s;
}

static const RirTy *re_con(Heap *heap, const char *name) { return rir_tcon(heap, str_lit(name), RE_NIL(heap)); }

static const RirTy *re_nat(Heap *heap) { return re_con(heap, "MechNat"); }

static const RirTy *re_rnat(Heap *heap) { return rir_tref(heap, re_nat(heap)); }

static const RirEx *re_v(Heap *heap, const char *name) { return rir_xvar(heap, str_lit(name)); }

static const RirParam *re_p(Heap *heap, const char *name, const RirTy *t) { return rir_param(heap, str_lit(name), t); }

static RirPat re_unit(Heap *heap, const char *family, const char *ctor) {
  return rir_pctor(str_lit(family), str_lit(ctor), RE_NIL(heap));
}

static const RirEx *re_small(Heap *heap, const char *n) {
  return rir_xcall(heap, str_lit("nat_small"), RE_NIL(heap), RE_LIST(heap, rir_xint(heap, str_lit(n))));
}

/* `&impl Fn(&A) -> C` */
static const RirTy *re_branch(Heap *heap, const char *a) {
  return rir_tref(heap, rir_tfn(heap, RE_LIST(heap, rir_tref(heap, re_con(heap, a))), re_con(heap, "C")));
}

static const RirItem *re_mech_nat(Heap *heap) {
  return rir_ienum(heap, str_lit("MechNat"), RE_NIL(heap), false,
                   RE_LIST(heap, rir_variant(heap, str_lit("MechZero"), RE_NIL(heap)),
                           rir_variant(heap, str_lit("MechSucc"), RE_LIST(heap, rir_tbox(heap, re_nat(heap))))));
}

static const RirItem *re_mech_sum(Heap *heap) {
  return rir_ienum(heap, str_lit("MechSum"), RE_LIST(heap, re_name(heap, "A"), re_name(heap, "B")), false,
                   RE_LIST(heap, rir_variant(heap, str_lit("MechInl"), RE_LIST(heap, re_con(heap, "A"))),
                           rir_variant(heap, str_lit("MechInr"), RE_LIST(heap, re_con(heap, "B")))));
}

static const RirItem *re_mech_empty_elim(Heap *heap) {
  return rir_ifn(heap, str_lit("mech_empty_elim"), RE_LIST(heap, re_name(heap, "A")),
                 RE_LIST(heap, re_p(heap, "e", re_con(heap, "MechEmpty"))), re_con(heap, "A"),
                 rir_xmatch(heap, re_v(heap, "e"), RE_NIL(heap)));
}

static const RirArm *re_sum_arm(Heap *heap, const char *ctor, const char *x, const char *f) {
  return rir_arm(heap, rir_pctor(str_lit("MechSum"), str_lit(ctor), RE_LIST(heap, rir_bvar(heap, str_lit(x)))),
                 rir_xcall(heap, str_lit(f), RE_NIL(heap), RE_LIST(heap, re_v(heap, x))));
}

static const RirItem *re_mech_sum_rec(Heap *heap) {
  return rir_ifn(
      heap, str_lit("mech_sum_rec"), RE_LIST(heap, re_name(heap, "A"), re_name(heap, "B"), re_name(heap, "C")),
      RE_LIST(heap, re_p(heap, "left", re_branch(heap, "A")), re_p(heap, "right", re_branch(heap, "B")),
              re_p(heap, "s",
                   rir_tref(heap, rir_tcon(heap, str_lit("MechSum"), RE_LIST(heap, re_con(heap, "A"), re_con(heap, "B")))))),
      re_con(heap, "C"),
      rir_xmatch(heap, re_v(heap, "s"),
                 RE_LIST(heap, re_sum_arm(heap, "MechInl", "a", "left"), re_sum_arm(heap, "MechInr", "b", "right"))));
}

static RirModule re_sample_init(Heap *heap) {
  return rir_module(str_lit("init.mech"),
                    RE_LIST(heap, re_mech_nat(heap), rir_iunit(heap, str_lit("MechUnit"), str_lit("mech_unit")),
                            rir_ienum(heap, str_lit("MechEmpty"), RE_NIL(heap), true, RE_NIL(heap)), re_mech_sum(heap),
                            re_mech_empty_elim(heap), re_mech_sum_rec(heap)));
}

static const RirItem *re_auction_choice(Heap *heap) {
  return rir_ienum(heap, str_lit("AuctionChoice"), RE_NIL(heap), true,
                   RE_LIST(heap, rir_variant(heap, str_lit("AuctionLose"), RE_NIL(heap)),
                           rir_variant(heap, str_lit("AuctionWin"), RE_NIL(heap))));
}

static const RirItem *re_auction_payment(Heap *heap) {
  return rir_ifn(
      heap, str_lit("auction_payment"), RE_NIL(heap),
      RE_LIST(heap, re_p(heap, "price", re_rnat(heap)), re_p(heap, "choice", re_con(heap, "AuctionChoice"))), re_nat(heap),
      rir_xmatch(heap, re_v(heap, "choice"),
                 RE_LIST(heap,
                         rir_arm(heap, re_unit(heap, "AuctionChoice", "AuctionLose"),
                                 rir_xctor(heap, str_lit("MechNat"), str_lit("MechZero"), RE_NIL(heap))),
                         rir_arm(heap, re_unit(heap, "AuctionChoice", "AuctionWin"), rir_xclone(heap, re_v(heap, "price"))))));
}

static const RirEx *re_add_step(Heap *heap) {
  return rir_xctor(heap, str_lit("MechNat"), str_lit("MechSucc"),
                   RE_LIST(heap, rir_xbox(heap, rir_xcall(heap, str_lit("auction_add"), RE_NIL(heap),
                                                          RE_LIST(heap, re_v(heap, "previous"), re_v(heap, "right"))))));
}

static const RirItem *re_auction_add(Heap *heap) {
  return rir_ifn(
      heap, str_lit("auction_add"), RE_NIL(heap), RE_LIST(heap, re_p(heap, "left", re_rnat(heap)), re_p(heap, "right", re_rnat(heap))),
      re_nat(heap),
      rir_xmatch(heap, re_v(heap, "left"),
                 RE_LIST(heap, rir_arm(heap, re_unit(heap, "MechNat", "MechZero"), rir_xclone(heap, re_v(heap, "right"))),
                         rir_arm(heap,
                                 rir_pctor(str_lit("MechNat"), str_lit("MechSucc"),
                                           RE_LIST(heap, rir_bvar(heap, str_lit("previous")))),
                                 re_add_step(heap)))));
}

static const RirEx *re_native_step(Heap *heap) {
  return rir_xclosure(heap,
                      RE_LIST(heap, re_p(heap, "_value", re_rnat(heap)), re_p(heap, "previous", rir_tref(heap, re_con(heap, "Nat")))),
                      rir_xcall(heap, str_lit("nat_add"), RE_NIL(heap),
                                RE_LIST(heap, rir_xborrow(heap, re_small(heap, "1")), re_v(heap, "previous"))));
}

static const RirItem *re_auction_to_native(Heap *heap) {
  return rir_ifn(heap, str_lit("auction_to_native"), RE_NIL(heap), RE_LIST(heap, re_p(heap, "n", re_rnat(heap))),
                 re_con(heap, "Nat"),
                 rir_xcall(heap, str_lit("mech_nat_rec"), RE_LIST(heap, re_con(heap, "Nat")),
                           RE_LIST(heap, rir_xborrow(heap, re_small(heap, "0")), rir_xborrow(heap, re_native_step(heap)),
                                   re_v(heap, "n"))));
}

static RirModule re_sample_second(Heap *heap) {
  return rir_module(str_lit("second-price.mech"),
                    RE_LIST(heap, rir_iuse(heap, str_lit("init")), rir_iuse(heap, str_lit("nat")), re_auction_choice(heap),
                            re_auction_payment(heap), re_auction_add(heap), re_auction_to_native(heap)));
}

/* The mode tag of a run: `run_args` picks it from the first two arguments and `run_md` switches on
 * it, so one run prints only the chosen module. */
typedef enum { MD_INIT, MD_SECOND, MD_CLASSIFY, MD_EMIT, MD_CRATE, MD_VALUES, MD_PROBES, MD_BIN, MD_NONE } Md;

static Str re_usage(void) {
  return str_lit("usage: rust_emit.bend sample <init|second> | classify <file.mech> [<file.mech>] | emit <file.mech> "
                 "[<file.mech>] | crate <file.mech>... <out dir> | values <file.mech>... | probes <file.mech>... | bin "
                 "<file.mech>...\n");
}

static bool re_is(Str s, const char *text) { return str_eq(s, str_lit(text)); }

static Md re_oracle_of(Str mode) {
  if (re_is(mode, "values")) return MD_VALUES;
  if (re_is(mode, "probes")) return MD_PROBES;
  if (re_is(mode, "bin")) return MD_BIN;
  return MD_NONE;
}

static Md re_which_of(Str which) {
  if (re_is(which, "init")) return MD_INIT;
  if (re_is(which, "second")) return MD_SECOND;
  return MD_NONE;
}

static Md re_mode_of(Str mode, Str which) {
  if (re_is(mode, "sample")) return re_which_of(which);
  if (re_is(mode, "classify")) return MD_CLASSIFY;
  if (re_is(mode, "emit")) return MD_EMIT;
  if (re_is(mode, "crate")) return MD_CRATE;
  return re_oracle_of(mode);
}

/* IO.write(text): the text on stdout with no newline. */
static int re_write(Heap *heap, Str text) {
  if (heap->oom) return driver_host_failure();
  if (text.len > 0) fwrite(text.data, 1, text.len, stdout);
  return 0;
}

/* IO.die: the message and a newline on stderr. */
static int re_die(int code, Str message) {
  if (message.len > 0) fwrite(message.data, 1, message.len, stderr);
  fputc('\n', stderr);
  return code;
}

/* IO(String): the text when ok, else the exit code of the IO.die that ended the run. */
typedef struct {
  bool ok;
  Str text;
  int code;
} ReText;

static ReText re_read_result(IoResult result, Str path) {
  switch (result.status) {
    case IO_DONE:
      return (ReText){true, result.value, 0};
    case IO_FAIL:
      fputs("cannot read ", stderr);
      return (ReText){false, re_str_placeholder, re_die(64, path)};
  }
  return (ReText){false, re_str_placeholder, 70};
}

static ReText re_read_file(Heap *heap, Str path) {
  IoResult result = io_read_file(heap, path);
  if (heap->oom) return (ReText){false, re_str_placeholder, driver_host_failure()};
  return re_read_result(result, path);
}

static int re_classify1_io(Heap *heap, Str path) {
  ReText source = re_read_file(heap, path);
  if (!source.ok) return source.code;
  return re_write(heap, erase_typed_classify1(heap, source.text));
}

static int re_classify2_io(Heap *heap, Str first, Str second) {
  ReText a = re_read_file(heap, first);
  if (!a.ok) return a.code;
  ReText b = re_read_file(heap, second);
  if (!b.ok) return b.code;
  return re_write(heap, erase_typed_classify2(heap, a.text, b.text));
}

/* `rest` = argv[from..argc): the arguments after `which`. */
static int re_classify_io(Heap *heap, Str first, int argc, char **argv, int from) {
  if (from >= argc) return re_classify1_io(heap, first);
  return re_classify2_io(heap, first, str_lit(argv[from]));
}

static int re_emit_out(Heap *heap, EtModuleResult r) {
  if (heap->oom) return driver_host_failure();
  if (r.ok) return re_write(heap, emit_text(heap, &r.module));
  return re_die(65, r.why);
}

static int re_emit1_io(Heap *heap, Str path) {
  ReText source = re_read_file(heap, path);
  if (!source.ok) return source.code;
  return re_emit_out(heap, erase_typed_emit1(heap, path, source.text));
}

static int re_emit2_io(Heap *heap, Str first, Str second) {
  ReText a = re_read_file(heap, first);
  if (!a.ok) return a.code;
  ReText b = re_read_file(heap, second);
  if (!b.ok) return b.code;
  return re_emit_out(heap, erase_typed_emit2(heap, first, a.text, second, b.text));
}

static int re_emit_io(Heap *heap, Str first, int argc, char **argv, int from) {
  if (from >= argc) return re_emit1_io(heap, first);
  return re_emit2_io(heap, first, str_lit(argv[from]));
}

/* `which <> rest` = argv[from..argc) as a Vec of Str pointers. */
static Vec re_paths(Heap *heap, int argc, char **argv, int from) {
  Vec v = vec_new(heap->arena);
  for (int i = from; i < argc; i++) {
    Str *s = heap_alloc(heap, sizeof(Str));
    if (s == NULL) return v;
    *s = str_lit(argv[i]);
    re_push(heap, &v, s);
  }
  return v;
}

static int re_run_md(Heap *heap, Md m, int argc, char **argv) {
  Str which = str_lit(argv[2]);
  Vec paths;
  switch (m) {
    case MD_INIT: {
      RirModule module = re_sample_init(heap);
      if (heap->oom) return driver_host_failure();
      return re_write(heap, emit_text(heap, &module));
    }
    case MD_SECOND: {
      RirModule module = re_sample_second(heap);
      if (heap->oom) return driver_host_failure();
      return re_write(heap, emit_text(heap, &module));
    }
    case MD_CLASSIFY:
      return re_classify_io(heap, which, argc, argv, 3);
    case MD_EMIT:
      return re_emit_io(heap, which, argc, argv, 3);
    case MD_CRATE:
      paths = re_paths(heap, argc, argv, 2);
      if (heap->oom) return driver_host_failure();
      return rust_out_run(heap, &paths);
    case MD_VALUES:
      paths = re_paths(heap, argc, argv, 2);
      if (heap->oom) return driver_host_failure();
      return rust_emit_oracle_values(heap, &paths);
    case MD_PROBES:
      paths = re_paths(heap, argc, argv, 2);
      if (heap->oom) return driver_host_failure();
      return rust_emit_oracle_probes(heap, &paths);
    case MD_BIN:
      paths = re_paths(heap, argc, argv, 2);
      if (heap->oom) return driver_host_failure();
      return rust_emit_oracle_bin(heap, &paths);
    case MD_NONE:
      return re_write(heap, re_usage());
  }
  return driver_host_failure();
}

static int re_run_args(Heap *heap, int argc, char **argv) {
  if (argc < 3) return re_write(heap, re_usage());
  return re_run_md(heap, re_mode_of(str_lit(argv[1]), str_lit(argv[2])), argc, argv);
}

static int re_entry(int argc, char **argv) {
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  int code = re_run_args(&heap, argc, argv);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, re_entry); }
