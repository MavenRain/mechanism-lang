/* Port of bend2/cli/host.bend (the kanon run host): find the runner beside the executable, spawn it
 * over a Wasm module with stdout and stderr captured to WASM.out and WASM.err, and read the outcome
 * back (a printed number, a trap line or an invalid line), or evaluate the export in the kernel.
 * Strings are byte views: the Bend code walks code points, but every byte it compares is ASCII
 * ('/', '_', digits, signs, base letters), and an ASCII byte never occurs inside a UTF-8 sequence, so
 * the results are the same bytes. Every function that allocates takes the Heap it allocates from; no
 * host effect runs once heap->oom is set. */
#include "cli.h"

static HostOutcome outcome(HostOutcomeKind kind, Big number, Str message) {
  HostOutcome result = {kind, number, message};
  return result;
}

static HostOutcome outcome_value(Big number) { return outcome(HOST_OUTCOME_VALUE, number, str_empty()); }
static HostOutcome outcome_trap(Str message) { return outcome(HOST_OUTCOME_TRAP, big_zero(), message); }
static HostOutcome outcome_invalid(Str message) { return outcome(HOST_OUTCOME_INVALID, big_zero(), message); }

static HostRunner runner_of(Str launcher, Str relative) {
  HostRunner runner = {launcher, relative};
  return runner;
}

static HostText text_none(void) {
  HostText text = {false, str_empty()};
  return text;
}

static HostText text_some(Str value) {
  HostText text = {true, value};
  return text;
}

static MaybeBig big_none(void) {
  MaybeBig none = {false, big_zero()};
  return none;
}

static MaybeBig big_some(Big value) {
  MaybeBig some = {true, value};
  return some;
}

static Str view(Str s, size_t start, size_t end) {
  StrSlice slice = str_slice(s, start, end);
  return slice.status == STR_INDEX_OK ? slice.value : str_empty();
}

/* host_executable (25-27): the path of the running executable, empty when the host cannot tell. */
Str host_executable(Heap *heap) {
  if (heap->oom) return str_empty();
  return host_executable_path(heap->arena);
}

/* host_process (29-31): run LAUNCHER RUNNER WASM EXPORT with stdout and stderr captured; 127 when a
 * field holds a NUL byte (no capture file is touched) or the process does not run. */
uint32_t host_process(Heap *heap, Str launcher, Str runner, Str wasm, Str export_name, Str out_path, Str err_path) {
  if (heap->oom) return 127;
  char *launcher_c = str_cstr(heap->arena, launcher);
  char *runner_c = str_cstr(heap->arena, runner);
  char *wasm_c = str_cstr(heap->arena, wasm);
  char *export_c = str_cstr(heap->arena, export_name);
  if (launcher_c == NULL || runner_c == NULL || wasm_c == NULL || export_c == NULL) return 127;
  const char *const argv[] = {launcher_c, runner_c, wasm_c, export_c, NULL};
  ProcessResult result = host_process_run(argv, 4, out_path, err_path);
  return result.code;
}

/* name (33-37) */
Str host_name(HostKind h) {
  switch (h) {
  case HOST_NODE: return str_lit("node");
  case HOST_WASMTIME: return str_lit("wasmtime");
  case HOST_KERNEL: return str_lit("kernel");
  }
  return str_lit("kernel");
}

/* runner (39-43) */
HostMaybeRunner host_runner(HostKind h) {
  HostMaybeRunner none = {false, runner_of(str_empty(), str_empty())};
  HostMaybeRunner node = {true, runner_of(str_lit("node"), str_lit("dev/run-node.mjs"))};
  HostMaybeRunner wasmtime = {true, runner_of(str_lit("zsh"), str_lit("dev/run-wasmtime.sh"))};
  switch (h) {
  case HOST_NODE: return node;
  case HOST_WASMTIME: return wasmtime;
  case HOST_KERNEL: return none;
  }
  return none;
}

/* trim_slashes (45-48) on the reversed text: the text without its trailing slashes. */
Str host_trim_slashes(Str text) {
  size_t end = text.len;
  while (end > 0 && str_at(text, end - 1).status == STR_INDEX_OK && str_at(text, end - 1).value == '/') end--;
  return view(text, 0, end);
}

/* parent_chars (50-53): the remaining text, "/" when nothing remains. */
Str host_parent_chars(Str text) { return text.len == 0 ? str_lit("/") : text; }

/* dirname_rev (55-59): drop the last component; "." when the text holds no slash. */
Str host_dirname_rev(Str text) {
  size_t end = text.len;
  while (end > 0 && !(str_at(text, end - 1).status == STR_INDEX_OK && str_at(text, end - 1).value == '/')) end--;
  if (end == 0) return str_lit(".");
  return host_parent_chars(host_trim_slashes(view(text, 0, end - 1)));
}

/* dirname_trimmed (61-64) */
Str host_dirname_trimmed(Str trimmed, Str original) {
  if (trimmed.len == 0) return original.len == 0 ? str_lit(".") : str_lit("/");
  return host_dirname_rev(trimmed);
}

/* dirname (66-67) */
Str host_dirname(Str path) { return host_dirname_trimmed(host_trim_slashes(path), path); }

/* root (69-72) */
Str host_root(Heap *heap) { return host_dirname(host_dirname(host_dirname(host_executable(heap)))); }

/* join (74-75) */
Str host_join(Heap *heap, Str root, Str relative) {
  if (root.len == 0) return relative;
  return heap_str_cat(heap, heap_str_cat(heap, root, str_lit("/")), relative);
}

/* missing_at (77-83) */
HostText host_missing_at(Heap *heap, HostMaybeRunner r, Str root) {
  if (!r.some) return text_none();
  bool found = io_exists(heap, host_join(heap, root, r.value.relative));
  if (found) return text_none();
  Str head = heap_str_cat(heap, str_lit("kanon: run: cannot find "), r.value.relative);
  return text_some(heap_str_cat(heap, head, str_lit(" beside the executable")));
}

/* missing_runner (85-86) */
HostText host_missing_runner(Heap *heap, Str root, HostKind h) { return host_missing_at(heap, host_runner(h), root); }

/* out_path (88-89) */
Str host_out_path(Heap *heap, Str wasm) { return heap_str_cat(heap, wasm, str_lit(".out")); }

/* err_path (91-92) */
Str host_err_path(Heap *heap, Str wasm) { return heap_str_cat(heap, wasm, str_lit(".err")); }

/* captures (94-95): [out_path, err_path] as a Vec of boxed Str. */
Vec host_captures(Heap *heap, Str wasm) {
  Vec paths = vec_new(heap->arena);
  Str *out = heap_alloc(heap, sizeof(Str));
  Str *err = heap_alloc(heap, sizeof(Str));
  if (out == NULL || err == NULL) {
    heap->oom = true;
    return paths;
  }
  *out = host_out_path(heap, wasm);
  *err = host_err_path(heap, wasm);
  if (!vec_push(&paths, out) || !vec_push(&paths, err)) heap->oom = true;
  return paths;
}

/* text_result (97-100) */
Str host_text_result(IoResult r) {
  switch (r.status) {
  case IO_DONE: return r.value;
  case IO_FAIL: return str_empty();
  }
  return str_empty();
}

/* read_text (102-105) */
Str host_read_text(Heap *heap, Str path) { return host_text_result(io_read_file(heap, path)); }

static bool is_space(char c) { return c == ' ' || (c >= 9 && c <= 13); }

/* String.trim: drop Char.is_space (32 and 9..13) at both ends. */
static Str trim(Str s) {
  size_t start = 0;
  size_t end = s.len;
  while (start < end && str_at(s, start).status == STR_INDEX_OK && is_space(str_at(s, start).value)) start++;
  while (end > start && str_at(s, end - 1).status == STR_INDEX_OK && is_space(str_at(s, end - 1).value)) end--;
  return view(s, start, end);
}

/* first_nonblank (107-112) over String.split(text, '\n'): the first piece that is not blank after
 * trim, trimmed; "no message" when every piece is blank. */
Str host_first_nonblank(Str text) {
  size_t start = 0;
  while (start <= text.len) {
    size_t end = start;
    while (end < text.len && !(str_at(text, end).status == STR_INDEX_OK && str_at(text, end).value == '\n')) end++;
    Str line = trim(view(text, start, end));
    if (line.len > 0) return line;
    start = end + 1;
  }
  return str_lit("no message");
}

/* first_line (114-117) */
Str host_first_line(Heap *heap, Str path) { return host_first_nonblank(host_read_text(heap, path)); }

/* strip (119-120): the line without PREFIX when it starts with it. */
Str host_strip(Str prefix, Str line) {
  if (!str_starts_with(line, prefix)) return line;
  return view(line, prefix.len, line.len);
}

/* digit (122-126): the value of a hex digit, 255 for any other character. */
uint32_t host_digit(uint32_t u) {
  if (u >= 48 && u <= 57) return u - 48;
  if (u >= 97 && u <= 102) return u - 97 + 10;
  if (u >= 65 && u <= 70) return u - 65 + 10;
  return 255;
}

/* number_digit (128-131) */
MaybeBig host_number_digit(Heap *heap, bool valid, uint32_t d, uint32_t base, Big n) {
  if (!valid) return big_none();
  return big_some(big_add(heap, big_mul(heap, n, big_of_u32(heap, base)), big_of_u32(heap, d)));
}

/* number_rest (133-140): '_' separators are skipped; any other byte must be a digit below base. */
MaybeBig host_number_rest(Heap *heap, Str s, uint32_t base, MaybeBig n) {
  MaybeBig acc = n;
  size_t i = 0;
  while (acc.some && i < s.len) {
    StrAt at = str_at(s, i);
    uint32_t c = at.status == STR_INDEX_OK ? (uint32_t)(unsigned char)at.value : 255;
    uint32_t d = host_digit(c);
    acc = c == '_' ? acc : host_number_digit(heap, d < base, d, base, acc.value);
    i++;
  }
  return acc;
}

/* number_first (142-147) */
MaybeBig host_number_first(Heap *heap, Str s, uint32_t base) {
  StrAt at = str_at(s, 0);
  if (s.len == 0 || !at.status == STR_INDEX_OK) return big_none();
  uint32_t d = host_digit((uint32_t)(unsigned char)at.value);
  return host_number_rest(heap, view(s, 1, s.len), base, host_number_digit(heap, d < base, d, base, big_zero()));
}

/* Big.Big{False{}, [a, b, c, d, e]} */
static Big limbs5(Heap *heap, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
  uint32_t *digits = heap_alloc(heap, 5 * sizeof(uint32_t));
  if (digits == NULL) {
    heap->oom = true;
    return big_zero();
  }
  digits[0] = a;
  digits[1] = b;
  digits[2] = c;
  digits[3] = d;
  digits[4] = e;
  Limbs value = {digits, 5};
  return big_make(heap, false, value);
}

/* int_limit (149-150) */
Big host_int_limit(Heap *heap) { return limbs5(heap, 0, 0, 0, 0, 4); }

/* int_modulus (152-153) */
Big host_int_modulus(Heap *heap) { return limbs5(heap, 0, 0, 0, 0, 8); }

/* int_max (155-156) */
Big host_int_max(Heap *heap) { return limbs5(heap, 32767, 32767, 32767, 32767, 3); }

/* unsigned_max (158-159) */
Big host_unsigned_max(Heap *heap) { return limbs5(heap, 32767, 32767, 32767, 32767, 7); }

/* signed_wrap (161-165) */
Big host_signed_wrap(Heap *heap, bool negative, Big n) {
  Big value = negative ? big_neg(n) : n;
  if (negative) return big_le(n, host_int_limit(heap)) ? value : big_add(heap, value, host_int_modulus(heap));
  return big_le(n, host_int_max(heap)) ? value : big_add(heap, value, big_neg(host_int_modulus(heap)));
}

/* number_bound (167-172) */
MaybeBig host_number_bound(Heap *heap, MaybeBig n, bool negative, bool unsigned_base) {
  if (!n.some) return big_none();
  Big limit = unsigned_base ? host_unsigned_max(heap) : negative ? host_int_limit(heap) : host_int_max(heap);
  return big_le(n.value, limit) ? big_some(host_signed_wrap(heap, negative, n.value)) : big_none();
}

/* number_base (174-184): 0x/0X 16, 0o/0O 8, 0b/0B 2 and 0u/0U 10 are unsigned; plain digits signed. */
MaybeBig host_number_base(Heap *heap, Str s, bool negative) {
  StrAt zero = str_at(s, 0);
  StrAt mark = str_at(s, 1);
  bool prefixed = s.len >= 2 && zero.status == STR_INDEX_OK && mark.status == STR_INDEX_OK && zero.value == '0';
  char m = prefixed ? mark.value : 0;
  uint32_t base = (m == 'x' || m == 'X') ? 16 : (m == 'o' || m == 'O') ? 8 : (m == 'b' || m == 'B') ? 2 : (m == 'u' || m == 'U') ? 10 : 0;
  if (base == 0) return host_number_bound(heap, host_number_first(heap, s, 10), negative, false);
  return host_number_bound(heap, host_number_first(heap, view(s, 2, s.len), base), negative, true);
}

/* parse_number (186-190) */
MaybeBig host_parse_number(Heap *heap, Str s) {
  StrAt sign = str_at(s, 0);
  bool has_sign = s.len > 0 && sign.status == STR_INDEX_OK && (sign.value == '-' || sign.value == '+');
  if (!has_sign) return host_number_base(heap, s, false);
  return host_number_base(heap, view(s, 1, s.len), sign.value == '-');
}

/* printed (192-195) */
HostOutcome host_printed(Heap *heap, MaybeBig n, Str text) {
  if (!n.some) return outcome_invalid(heap_str_cat(heap, str_lit("runner printed "), text));
  return outcome_value(n.value);
}

/* of_exit (197-211): 0 = the trimmed stdout number, 1 = a trap line, any other code = invalid. */
HostOutcome host_of_exit(Heap *heap, uint32_t code, Str wasm) {
  if (code == 0) {
    Str text = trim(host_read_text(heap, host_out_path(heap, wasm)));
    return host_printed(heap, host_parse_number(heap, text), text);
  }
  Str line = host_first_line(heap, host_err_path(heap, wasm));
  if (code == 1) return outcome_trap(host_strip(str_lit("trap: "), line));
  return outcome_invalid(host_strip(str_lit("invalid: "), line));
}

/* spawn (213-218) */
HostOutcome host_spawn(Heap *heap, HostRunner r, Str root, Str wasm, Str export_name) {
  uint32_t code = host_process(heap, r.launcher, host_join(heap, root, r.relative), wasm, export_name,
                               host_out_path(heap, wasm), host_err_path(heap, wasm));
  return host_of_exit(heap, code, wasm);
}

/* kernel_small (220-223) */
HostOutcome host_kernel_small(Heap *heap, Big n, MaybeU32 small) {
  if (!small.some) return outcome_trap(heap_str_cat(heap, big_to_string(heap, n), str_lit(" is outside the i31 range")));
  return outcome_value(big_of_u32(heap, small.value));
}

/* kernel_value (225-229) */
HostOutcome host_kernel_value(Heap *heap, Result result) {
  switch (result.kind) {
  case RESULT_FAIL: return outcome_invalid(error_to_string(heap, result.error));
  case RESULT_DONE: {
    const Value *value = (const Value *)result.value;
    if (value->kind != VALUE_LIT) return outcome_invalid(str_lit("the kernel value is not a literal"));
    switch (value->lit.kind) {
    case LITERAL_INT: return host_kernel_small(heap, value->lit.integer, big_to_i31(value->lit.integer));
    case LITERAL_STRING: return outcome_invalid(str_lit("the kernel value is not a literal"));
    }
    return outcome_invalid(str_lit("the kernel value is not a literal"));
  }
  }
  return outcome_invalid(error_to_string(heap, result.error));
}

/* kernel_outcome (231-234): whnf of the global EXPORT. */
HostOutcome host_kernel_outcome(Heap *heap, Globals globals, Str export_name) {
  Result value = eval_eval(heap, globals, vec_new(heap->arena), term_global(heap, export_name));
  switch (value.kind) {
  case RESULT_DONE: return host_kernel_value(heap, eval_whnf(heap, globals, (const Value *)value.value));
  case RESULT_FAIL: return host_kernel_value(heap, value);
  }
  return host_kernel_value(heap, value);
}

/* run_module (236-240) */
HostOutcome host_run_module(Heap *heap, Str root, Globals globals, HostKind h, Str wasm, Str export_name) {
  switch (h) {
  case HOST_NODE: return host_spawn(heap, runner_of(str_lit("node"), str_lit("dev/run-node.mjs")), root, wasm, export_name);
  case HOST_WASMTIME: return host_spawn(heap, runner_of(str_lit("zsh"), str_lit("dev/run-wasmtime.sh")), root, wasm, export_name);
  case HOST_KERNEL: return host_kernel_outcome(heap, globals, export_name);
  }
  return host_kernel_outcome(heap, globals, export_name);
}
