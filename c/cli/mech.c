/* Port of cli/mech.bend: the `mech` command line. arguments(args) dispatches on the first
 * argument: import, diff-parity and map-inventory run the Lean export importer (c/import), rust-out
 * and rust-in run the Rust emitter and importer (rust_out.c, rust_in.c), and every other command goes
 * to Kanon.dispatch (kanon.c). Every function returns the process exit code; IO.die is cli_die. This
 * file needs c/import, c/wasm and c/rust, so the Makefile keeps it out of C_CLI_SOURCES (C_CLI_MECH)
 * and links it only into _tcc/bin/mech and _tcc/bin/main. */
#include <stdio.h>
#include <string.h>

#include "cli.h"
#include "import.h"

/* Export.T or the exit code of IO.die (C only: the IO(Export.T) of read_source and parsed). */
typedef struct {
  bool ok;
  int code;
  Export value;
} MechSource;

/* Report.T or the exit code of IO.die (C only: the IO(Report.T) of imported and prepared). */
typedef struct {
  bool ok;
  int code;
  Report value;
} MechReport;

/* args[index], or the empty string past the end (callers test the length first). */
static Str mech_arg(const Vec *args, size_t index) {
  VecGet got = vec_get(args, index);
  switch (got.status) {
  case VEC_GET_OK: return *(const Str *)got.value;
  case VEC_GET_OUT_OF_RANGE: return str_empty();
  }
  return str_empty();
}

static Str mech_cat(Heap *heap, const Str *parts, size_t count) {
  Str out = str_empty();
  size_t i = 0;
  for (; i < count; i = i + 1) out = heap_str_cat(heap, out, parts[i]);
  return out;
}

static MechSource mech_source_die(int code) {
  MechSource out;
  memset(&out, 0, sizeof out);
  out.ok = false;
  out.code = code;
  return out;
}

static MechReport mech_report_die(int code) {
  MechReport out;
  memset(&out, 0, sizeof out);
  out.ok = false;
  out.code = code;
  return out;
}

/* usage (11-12) */
int mech_usage(void) {
  return cli_die(64, str_lit("usage: mech import FILE [--out DIR] | diff-parity --export FILE\n"
                             "       mech map-inventory --export FILE [--never]\n"
                             "       mech rust-out FILE.mech... OUT_DIR\n"
                             "       mech rust-in CRATE_DIR OUT_DIR\n"
                             "       mech check|axioms|circuit|emit|build|run|spec-count (inherited arguments)"));
}

/* export_error (14-17): "mech: " ++ path ++ ":" ++ U32.show(line) ++ ": " ++ message */
static Str mech_export_error(Heap *heap, ExportError error, Str path) {
  Str parts[5] = {str_lit("mech: "), path, str_lit(":"), str_of_u64(heap->arena, error.line), str_lit(": ")};
  return heap_str_cat(heap, mech_cat(heap, parts, 5), error.message);
}

/* parsed (19-24) */
static MechSource mech_parsed(Heap *heap, ExportResult result, Str path) {
  switch (result.kind) {
  case JSON_DONE: {
    MechSource out;
    out.ok = true;
    out.code = 0;
    out.value = result.value;
    return out;
  }
  case JSON_FAIL: return mech_source_die(cli_die(1, mech_export_error(heap, result.error, path)));
  }
  return mech_source_die(cli_host_failure());
}

/* read_result (26-31) */
static MechSource mech_read_result(Heap *heap, IoResult result, Str path) {
  switch (result.status) {
  case IO_DONE: return mech_parsed(heap, export_read_string(heap, result.value), path);
  case IO_FAIL: {
    Str parts[4] = {str_lit("mech: "), path, str_lit(":0: "), result.value};
    return mech_source_die(cli_die(1, mech_cat(heap, parts, 4)));
  }
  }
  return mech_source_die(cli_host_failure());
}

/* read_source (33-36) */
static MechSource mech_read_source(Heap *heap, Str path) {
  IoResult result = io_read_file(heap, path);
  return mech_read_result(heap, result, path);
}

/* prepared (38-43) */
static MechReport mech_prepared(Heap *heap, ReportResult result, Str path) {
  switch (result.kind) {
  case JSON_DONE: {
    MechReport out;
    out.ok = true;
    out.code = 0;
    out.value = result.value;
    return out;
  }
  case JSON_FAIL: return mech_report_die(cli_die(1, mech_export_error(heap, result.error, path)));
  }
  return mech_report_die(cli_host_failure());
}

/* imported (45-48) */
static MechReport mech_imported(Heap *heap, Str path) {
  MechSource source = mech_read_source(heap, path);
  if (!source.ok) return mech_report_die(source.code);
  /* The Report keeps a pointer to its Export (report->source), so the Export lives on the heap,
   * not in this frame: the report outlives it (hop 229, cli-check export-1087 SIGSEGV). */
  Export *kept = heap_alloc(heap, sizeof(Export));
  if (kept == NULL) {
    heap->oom = true;
    return mech_report_die(cli_host_failure());
  }
  *kept = source.value;
  ReportResult result = report_prepare(heap, kept);
  return mech_prepared(heap, result, path);
}

/* show (50-57): IO.print of every line (Str pointers), in order. */
static int mech_show(Heap *heap, const Vec *lines) {
  if (heap->oom) return cli_host_failure();
  size_t i = 0;
  for (; i < lines->len; i = i + 1) cli_print(mech_arg(lines, i));
  return 0;
}

/* output_files (59-64): ReportOutputFile pointers in, IoFile pointers out. */
static Vec mech_output_files(Heap *heap, const Vec *files) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < files->len; i = i + 1) {
    VecGet got = vec_get(files, i);
    switch (got.status) {
    case VEC_GET_OK: {
      const ReportOutputFile *file = got.value;
      IoFile *pair = heap_alloc(heap, sizeof(IoFile));
      if (pair == NULL || !vec_push(&out, pair)) {
        heap->oom = true;
        return out;
      }
      pair->name = file->name;
      pair->contents = file->contents;
      break;
    }
    case VEC_GET_OUT_OF_RANGE: return out;
    }
  }
  return out;
}

/* published (66-71) */
static int mech_published(Heap *heap, IoResult result, const Report *report) {
  switch (result.status) {
  case IO_DONE: {
    Vec summary = report_summary(heap, report);
    return mech_show(heap, &summary);
  }
  case IO_FAIL: return cli_die(1, heap_str_cat(heap, str_lit("mech: "), result.value));
  }
  return cli_host_failure();
}

/* output (73-80): directory NULL = None. */
static int mech_output(Heap *heap, const Str *directory, const Report *report) {
  if (directory == NULL) {
    Vec summary = report_summary(heap, report);
    return mech_show(heap, &summary);
  }
  Vec reported = report_files(heap, report);
  Vec files = mech_output_files(heap, &reported);
  if (heap->oom) return cli_host_failure();
  IoResult result = io_publish(heap, *directory, &files);
  return mech_published(heap, result, report);
}

/* run_import (82-85) */
static int mech_run_import(Heap *heap, Str path, const Str *directory) {
  MechReport report = mech_imported(heap, path);
  if (!report.ok) return report.code;
  if (heap->oom) return cli_host_failure();
  return mech_output(heap, directory, &report.value);
}

/* mapping_text (87-92) */
static Str mech_mapping_text(Heap *heap, bool never, const MappingInventory *inventory) {
  if (never) return mapping_never_tsv(heap, inventory);
  return mapping_map_tsv(heap, inventory);
}

/* mapped (94-99): IO.write is the text on stdout with no newline. */
static int mech_mapped(Heap *heap, MappingResult result, bool never) {
  switch (result.kind) {
  case JSON_DONE: {
    Str text = mech_mapping_text(heap, never, &result.value);
    if (heap->oom) return cli_host_failure();
    if (text.len > 0) fwrite(text.data, 1, text.len, stdout);
    return 0;
  }
  case JSON_FAIL: return cli_die(1, heap_str_cat(heap, str_lit("mech: "), result.message));
  }
  return cli_host_failure();
}

/* map_inventory (101-104) */
static int mech_map_inventory(Heap *heap, Str path, bool never) {
  MechSource source = mech_read_source(heap, path);
  if (!source.ok) return source.code;
  Vec targets = mapping_foundation_targets(heap, &source.value);
  MappingResult result = mapping_inventory(heap, &source.value, &targets);
  return mech_mapped(heap, result, never);
}

/* dispatch (106-135): args = the Str pointers after the command. */
static int mech_dispatch(Heap *heap, Str command, const Vec *args) {
  size_t n = args->len;
  Str a0 = mech_arg(args, 0);
  Str a1 = mech_arg(args, 1);
  Str a2 = mech_arg(args, 2);
  if (str_eq(command, str_lit("import"))) {
    if (n == 1) return mech_run_import(heap, a0, NULL);
    if (n == 3 && str_eq(a1, str_lit("--out"))) return mech_run_import(heap, a0, &a2);
    return mech_usage();
  }
  if (str_eq(command, str_lit("diff-parity"))) {
    if (n == 2 && str_eq(a0, str_lit("--export"))) return mech_run_import(heap, a1, NULL);
    return mech_usage();
  }
  if (str_eq(command, str_lit("map-inventory"))) {
    if (n == 2 && str_eq(a0, str_lit("--export"))) return mech_map_inventory(heap, a1, false);
    if (n == 3 && str_eq(a0, str_lit("--export")) && str_eq(a2, str_lit("--never")))
      return mech_map_inventory(heap, a1, true);
    return mech_usage();
  }
  if (str_eq(command, str_lit("rust-out"))) return rust_out_run(heap, args);
  if (str_eq(command, str_lit("rust-in"))) return rust_in_run(heap, args);
  return kanon_dispatch(heap, command, args, 0);
}

/* arguments (137-142): Nil = usage; Con{command, rest} = dispatch(command, rest). */
int mech_arguments(Heap *heap, const Vec *args) {
  if (args->len == 0) return mech_usage();
  Vec rest = vec_new(heap->arena);
  size_t i = 1;
  for (; i < args->len; i = i + 1) {
    VecGet got = vec_get(args, i);
    switch (got.status) {
    case VEC_GET_OK:
      if (!vec_push(&rest, got.value)) return cli_host_failure();
      break;
    case VEC_GET_OUT_OF_RANGE: return cli_host_failure();
    }
  }
  return mech_dispatch(heap, mech_arg(args, 0), &rest);
}
