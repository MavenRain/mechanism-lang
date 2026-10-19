/* A <dir>/<file>.bend path in a comment names bend2/<dir>/<file>.bend in commit 8c90b64,
 * the last tree with the Bend sources. */
/* Mechanism CLI component: C port of host_fs.bend, io.bend and the cli .bend
 * files, one .c per .bend file and one header for the component. A Bend Result<String, String> of
 * the file protocol is an IoResult: the text on Done, the error message on Fail; a
 * Result<String, Unit> is an IoResult whose Done value is empty. A Bend byte list
 * (List<&2, U32> whose items are bytes) is a Str, as in c/wasm/wasm.h. Every function that
 * allocates takes the Heap it allocates from and never aborts or exits: after a failed allocation
 * (heap->oom) no further file effect runs, and the program boundary checks heap->oom before it
 * prints. cli_exit returns the exit status that the program boundary returns from its entry. */
#ifndef MECH_CLI_H
#define MECH_CLI_H

#include "kernel.h"

/* ---- host_fs (host_fs.bend): the POSIX file effect of effects/fs.c ---- */
typedef enum { IO_DONE, IO_FAIL } IoStatus;
/* The operation numbers of effects/fs.c (mech_fs_call). */
typedef enum {
  HOST_FS_READ = 0,
  HOST_FS_STAGE = 1,
  HOST_FS_RENAME = 2,
  HOST_FS_UNLINK = 3,
  HOST_FS_RMDIR = 4,
  HOST_FS_EXISTS = 5,
  HOST_FS_WRITE = 6,
  HOST_FS_TEMP = 7
} HostFsOp;
/* Result<U32 & String, String>: Done carries value; Fail carries the errno and its strerror text
 * (EILSEQ for a NUL byte inside a path, ENOMEM when memory runs out). */
typedef struct {
  IoStatus status;
  uint32_t code;
  Str message;
  Str value;
} HostFsResult;

HostFsResult mechanism_fs(Heap *heap, HostFsOp operation, Str path, Str other);
HostFsResult mechanism_write_bytes(Heap *heap, Str path, Str bytes);

/* ---- io (io.bend) ---- */
typedef struct {
  IoStatus status;
  Str value;
} IoResult;
/* Common.Pair<String, String> of write_files, cleanup_files and publish: a file name and its
 * contents. The file lists are Vecs of IoFile pointers. */
typedef struct {
  Str name;
  Str contents;
} IoFile;

Str io_error_text(Heap *heap, uint32_t code, Str message, Str path);
IoResult io_convert(Heap *heap, HostFsResult result, Str path);
IoResult io_fs(Heap *heap, HostFsOp operation, Str path, Str other);
IoResult io_unit_result(IoResult result);
IoResult io_fs_unit(Heap *heap, HostFsOp operation, Str path, Str other);
IoResult io_read_file(Heap *heap, Str path);
IoResult io_write_file(Heap *heap, Str path, Str contents);
IoResult io_remove(Heap *heap, Str path);
IoResult io_remove_dir(Heap *heap, Str path);
bool io_exists_result(IoResult result);
bool io_exists(Heap *heap, Str path);
IoResult io_temp_file(Heap *heap, Str prefix, Str suffix);
IoResult io_write_bytes(Heap *heap, Str path, Str bytes);
/* strip_slashes, target_name and normalize_target: the Bend code strips the leading slashes of
 * the reversed text; the C port strips the trailing slashes of the text (the same bytes, since a
 * slash never occurs inside a UTF-8 sequence). The results are views of the argument. */
Str io_strip_slashes(Str text);
Str io_target_name(Str trimmed, Str original);
Str io_normalize_target(Str directory);
IoResult io_remove_if_exists(Heap *heap, bool yes, Str path);
/* cleanup_files(files, directory) and write_files(files, directory) over files[index..]. */
IoResult io_cleanup_files(Heap *heap, const Vec *files, size_t index, Str directory);
IoResult io_cleanup_error(Heap *heap, IoResult result, Str message);
IoResult io_finish_publish(Heap *heap, IoResult result, const Vec *files, Str temporary);
IoResult io_write_files(Heap *heap, const Vec *files, size_t index, Str directory);
IoResult io_rename_error(Heap *heap, HostFsResult error, Str original);
IoResult io_rename_result(Heap *heap, HostFsResult result, Str original);
IoResult io_publish_rename(Heap *heap, bool present, Str temporary, Str target, Str original);
IoResult io_publish_written(Heap *heap, IoResult result, Str temporary, Str target, Str original);
IoResult io_publish_staged(Heap *heap, IoResult result, const Vec *files, Str target, Str original);
IoResult io_publish_exists(Heap *heap, bool present, Str target, Str original, const Vec *files);
IoResult io_publish(Heap *heap, Str directory, const Vec *files);

/* ---- output (cli/output.bend) ---- */
/* cli_exit(code): the exit status (code modulo 256, as the process status keeps it) with no output. */
int cli_exit(uint32_t code);
/* The IO.print and IO.die effects of the program boundary (C only; the Bend runtime provides them):
 * cli_print_parts writes the parts and a newline on stdout; cli_die writes the message and a newline
 * on stderr (also for an empty message) and returns cli_exit(code); cli_host_failure is the exit
 * after an out-of-memory heap (status 70, as c/tests/driver.h). */
int cli_print_parts(const Str *parts, size_t count);
int cli_print(Str text);
int cli_die(uint32_t code, Str message);
int cli_host_failure(void);

/* ---- cert (cli/cert.bend) ---- */
/* Binding{name, value} */
typedef struct {
  Str name;
  Str value;
} CertBinding;
/* Result<String, Binding>: binding on IO_DONE (NULL only with heap->oom), message on IO_FAIL. */
typedef struct {
  IoStatus status;
  const CertBinding *binding;
  Str message;
} CertBindingResult;
/* Result<String, List<Binding>>: a Vec of CertBinding pointers on IO_DONE, message on IO_FAIL. */
typedef struct {
  IoStatus status;
  Vec bindings;
  Str message;
} CertBindingsResult;

/* The export lists are the boxed Str items exports[index..] of the argument Vec. */
CertBindingResult cert_natural(Heap *heap, const Value *value, Str name);
CertBindingResult cert_normal(Heap *heap, Result result, Str name);
CertBindingResult cert_evaluated(Heap *heap, Result result, Globals globals, Str name);
CertBindingsResult cert_prepend(Heap *heap, CertBindingsResult result, const CertBinding *binding);
CertBindingsResult cert_next(Heap *heap, CertBindingResult result, Globals globals, const Vec *exports, size_t index);
CertBindingsResult cert_values(Heap *heap, Globals globals, const Vec *exports, size_t index);
CertBindingsResult cert_allowed(Heap *heap, const Vec *axioms, Globals globals, const Vec *exports, size_t index);
CertBindingsResult cert_checked(Heap *heap, Result result, const Vec *exports, size_t index);
int cert_show(const Vec *bindings);
int cert_report(Heap *heap, CertBindingsResult result);
int cert_read_result(Heap *heap, IoResult result, const Vec *exports, size_t index);
/* arguments(args): args = IO.args() as a Vec of boxed Str (argv without the program name). */
int cert_arguments(Heap *heap, const Vec *args);

/* ---- host (cli/host.bend) ---- */
/* T: Node{} | Wasmtime{} | Kernel{} */
typedef enum { HOST_NODE, HOST_WASMTIME, HOST_KERNEL } HostKind;
/* Outcome: Value{number} | Trap{message} | Invalid{message} */
typedef enum { HOST_OUTCOME_VALUE, HOST_OUTCOME_TRAP, HOST_OUTCOME_INVALID } HostOutcomeKind;
typedef struct {
  HostOutcomeKind kind;
  Big number;  /* HOST_OUTCOME_VALUE */
  Str message; /* HOST_OUTCOME_TRAP, HOST_OUTCOME_INVALID */
} HostOutcome;
/* Runner{launcher, relative} */
typedef struct {
  Str launcher;
  Str relative;
} HostRunner;
/* Maybe<Runner> */
typedef struct {
  bool some;
  HostRunner value;
} HostMaybeRunner;
/* Maybe<String> */
typedef struct {
  bool some;
  Str value;
} HostText;
Str host_executable(Heap *heap);
uint32_t host_process(Heap *heap, Str launcher, Str runner, Str wasm, Str export_name, Str out_path, Str err_path);
Str host_name(HostKind h);
HostMaybeRunner host_runner(HostKind h);
/* trim_slashes, parent_chars, dirname_rev and dirname_trimmed: the Bend code walks the reversed
 * character list; the C port walks the text from its end. The results are views of the argument. */
Str host_trim_slashes(Str text);
Str host_parent_chars(Str text);
Str host_dirname_rev(Str text);
Str host_dirname_trimmed(Str trimmed, Str original);
Str host_dirname(Str path);
Str host_root(Heap *heap);
Str host_join(Heap *heap, Str root, Str relative);
HostText host_missing_at(Heap *heap, HostMaybeRunner r, Str root);
HostText host_missing_runner(Heap *heap, Str root, HostKind h);
Str host_out_path(Heap *heap, Str wasm);
Str host_err_path(Heap *heap, Str wasm);
Vec host_captures(Heap *heap, Str wasm);
Str host_text_result(IoResult r);
Str host_read_text(Heap *heap, Str path);
/* first_nonblank(String.split(text, '\n')) */
Str host_first_nonblank(Str text);
Str host_first_line(Heap *heap, Str path);
Str host_strip(Str prefix, Str line);
uint32_t host_digit(uint32_t c);
MaybeBig host_number_digit(Heap *heap, bool valid, uint32_t d, uint32_t base, Big n);
MaybeBig host_number_rest(Heap *heap, Str s, uint32_t base, MaybeBig n);
MaybeBig host_number_first(Heap *heap, Str s, uint32_t base);
Big host_int_limit(Heap *heap);
Big host_int_modulus(Heap *heap);
Big host_int_max(Heap *heap);
Big host_unsigned_max(Heap *heap);
Big host_signed_wrap(Heap *heap, bool negative, Big n);
MaybeBig host_number_bound(Heap *heap, MaybeBig n, bool negative, bool unsigned_base);
MaybeBig host_number_base(Heap *heap, Str s, bool negative);
MaybeBig host_parse_number(Heap *heap, Str s);
HostOutcome host_printed(Heap *heap, MaybeBig n, Str text);
HostOutcome host_of_exit(Heap *heap, uint32_t code, Str wasm);
HostOutcome host_spawn(Heap *heap, HostRunner r, Str root, Str wasm, Str export_name);
HostOutcome host_kernel_small(Heap *heap, Big n, MaybeU32 small);
HostOutcome host_kernel_value(Heap *heap, Result result);
HostOutcome host_kernel_outcome(Heap *heap, Globals globals, Str export_name);
HostOutcome host_run_module(Heap *heap, Str root, Globals globals, HostKind h, Str wasm, Str export_name);

/* ---- kanon (cli/kanon.bend): kanon.c links c/wasm, so only the kanon drivers link it ---- */
Str kanon_usage_text(void);
/* usage(): the usage text on stderr, exit 64. */
int kanon_usage(Heap *heap);
/* dispatch(cmd, args): args = the boxed Str items args[index..] of the argument Vec. The result is
 * the process exit code (0 when the command ran to the end). */
int kanon_dispatch(Heap *heap, Str cmd, const Vec *args, size_t index);

/* ---- mech (cli/mech.bend; S6.13): mech.c needs c/import, c/wasm and c/rust, so only
 * _tcc/bin/mech and _tcc/bin/main link it ---- */
/* usage(): the usage text on stderr, exit 64. */
int mech_usage(void);
/* arguments(args): args = the Str pointers of argv without the program name; the result is the
 * process exit code. */
int mech_arguments(Heap *heap, const Vec *args);

/* ---- rust_out (cli/rust_out.bend; S6.12) ---- */
/* Ro.run(args): the `mech rust-out` command over `args` (Str pointers, in order); writes its
 * output and returns the exit code. */
int rust_out_run(Heap *heap, const Vec *args);
/* Exported for rust/resolve.bend and cli/rust_in.bend. Strings are Str pointers in each Vec. */
/* code_is(c, n): byte c has code n (n < 128, so a byte test is the Bend Char test). */
bool rust_out_code_is(unsigned char c, uint32_t n);
/* mod_ok(s): a lower-case letter, then lower-case letters, digits, `_` or `-` (D7). */
bool rust_out_mod_ok(Str s);
/* mod_name(stem): Et.rust_name of the stem with each `-` made `_`. */
Str rust_out_mod_name(Heap *heap, Str stem);
/* reserved(): ["bin", "lib", "main", "nat"]; nat_names(): ["Nat", "nat_small", "nat_add",
 * "nat_decimal"]. */
Vec rust_out_reserved(Heap *heap);
Vec rust_out_nat_names(Heap *heap);
/* sorted(xs): the strings in byte order (R7), Bend insert(sorted(rest), x). */
Vec rust_out_sorted(Heap *heap, const Vec *xs);
/* variant_tys(vs): the fields of each variant (RirVariant pointers) as RirTy pointers. */
Vec rust_out_variant_tys(Heap *heap, const Vec *vs);
/* lines_of(s): the parts between "\n" bytes, the part after the last one included. */
Vec rust_out_lines_of(Heap *heap, Str s);
/* words(s): the parts between single spaces. chunks_of(text): the carrier chunks (a Vec of Vecs of
 * Str pointers, source order). join_lines(lines) = Et.join("\n", lines). */
Vec rust_out_words(Heap *heap, Str s);
Vec rust_out_chunks_of(Heap *heap, Str text);
Str rust_out_join_lines(Heap *heap, const Vec *lines);
/* key_line(file, prev): "-- at FILE start" for prev = "", else "-- at FILE after PREV". */
Str rust_out_key_line(Heap *heap, Str file, Str prev);
/* nat_rs(): the text of src/nat.rs (the kernel `Nat` module, Q5). */
Str rust_out_nat_rs(void);
/* IO(String) of read_file: ok = the text; else the stderr line is written and `code` is the exit
 * status (64, "mech: cannot read PATH"). */
typedef struct {
  bool ok;
  Str text;
  int code;
} RustOutText;
RustOutText rust_out_read_file(Heap *heap, Str path);
/* wrote(result): 0 for Done, else "mech: MESSAGE" on stderr and exit status 1. */
int rust_out_wrote(Heap *heap, IoResult result);
/* absent(present, out): Fail "output already exists: OUT" when present, else Done. */
IoResult rust_out_absent(Heap *heap, bool present, Str out);

/* ---- rust_in (cli/rust_in.bend; S6.13a) ---- */
/* RustIn.run(args): the `mech rust-in CRATE_DIR OUT_DIR` command over `args` (Str pointers, in
 * order); writes its output and returns the exit code. lows_of is declared in rust.h (D-226a). */
int rust_in_run(Heap *heap, const Vec *args);

#endif
