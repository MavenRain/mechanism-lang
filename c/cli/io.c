/* Port of bend2/io.bend: the file protocol of the compiler (read, write, remove, exists, temporary
 * files, byte output and the atomic publication of a report directory) over host_fs. Definitions
 * keep the Bend names (io_ prefix) and order; the laws cleanup_files and write_files are the
 * declarations in cli.h. */
#include "cli.h"

static IoResult io_done(Str value) {
  IoResult result = {IO_DONE, value};
  return result;
}

static IoResult io_fail(Str message) {
  IoResult result = {IO_FAIL, message};
  return result;
}

static Str io_cat3(Heap *heap, Str a, Str b, Str c) { return heap_str_cat(heap, heap_str_cat(heap, a, b), c); }

/* error_text(error, path) = path ++ ": " ++ message */
Str io_error_text(Heap *heap, uint32_t code, Str message, Str path) {
  (void)code;
  return io_cat3(heap, path, str_lit(": "), message);
}

IoResult io_convert(Heap *heap, HostFsResult result, Str path) {
  switch (result.status) {
  case IO_DONE: return io_done(result.value);
  case IO_FAIL: return io_fail(io_error_text(heap, result.code, result.message, path));
  }
  return io_fail(io_error_text(heap, result.code, result.message, path));
}

IoResult io_fs(Heap *heap, HostFsOp operation, Str path, Str other) {
  return io_convert(heap, mechanism_fs(heap, operation, path, other), path);
}

IoResult io_unit_result(IoResult result) {
  switch (result.status) {
  case IO_FAIL: return result;
  case IO_DONE: return io_done(str_empty());
  }
  return result;
}

IoResult io_fs_unit(Heap *heap, HostFsOp operation, Str path, Str other) {
  return io_unit_result(io_fs(heap, operation, path, other));
}

IoResult io_read_file(Heap *heap, Str path) { return io_fs(heap, HOST_FS_READ, path, str_empty()); }

IoResult io_write_file(Heap *heap, Str path, Str contents) { return io_fs_unit(heap, HOST_FS_WRITE, path, contents); }

IoResult io_remove(Heap *heap, Str path) { return io_fs_unit(heap, HOST_FS_UNLINK, path, str_empty()); }

IoResult io_remove_dir(Heap *heap, Str path) { return io_fs_unit(heap, HOST_FS_RMDIR, path, str_empty()); }

bool io_exists_result(IoResult result) {
  switch (result.status) {
  case IO_FAIL: return false;
  case IO_DONE: return str_eq(result.value, str_lit("1"));
  }
  return false;
}

bool io_exists(Heap *heap, Str path) { return io_exists_result(io_fs(heap, HOST_FS_EXISTS, path, str_empty())); }

IoResult io_temp_file(Heap *heap, Str prefix, Str suffix) { return io_fs(heap, HOST_FS_TEMP, prefix, suffix); }

IoResult io_write_bytes(Heap *heap, Str path, Str bytes) {
  return io_unit_result(io_convert(heap, mechanism_write_bytes(heap, path, bytes), path));
}

Str io_strip_slashes(Str text) {
  size_t len = text.len;
  while (len > 0 && text.data[len - 1] == '/') len--;
  return str_n(text.data, len);
}

Str io_target_name(Str trimmed, Str original) {
  if (trimmed.len > 0) return trimmed;
  if (original.len == 0) return str_lit(".");
  return str_lit("/");
}

Str io_normalize_target(Str directory) { return io_target_name(io_strip_slashes(directory), directory); }

IoResult io_remove_if_exists(Heap *heap, bool yes, Str path) {
  if (yes) return io_remove(heap, path);
  return io_done(str_empty());
}

static IoResult io_cleanup_more(Heap *heap, IoResult result, const Vec *files, size_t index, Str directory) {
  switch (result.status) {
  case IO_FAIL: return result;
  case IO_DONE: return io_cleanup_files(heap, files, index, directory);
  }
  return result;
}

IoResult io_cleanup_files(Heap *heap, const Vec *files, size_t index, Str directory) {
  VecGet got = vec_get(files, index);
  switch (got.status) {
  case VEC_GET_OUT_OF_RANGE: return io_remove_dir(heap, directory);
  case VEC_GET_OK: {
    const IoFile *file = got.value;
    Str path = io_cat3(heap, directory, str_lit("/"), file->name);
    bool present = io_exists(heap, path);
    IoResult result = io_remove_if_exists(heap, present, path);
    return io_cleanup_more(heap, result, files, index + 1, directory);
  }
  }
  return io_remove_dir(heap, directory);
}

IoResult io_cleanup_error(Heap *heap, IoResult result, Str message) {
  switch (result.status) {
  case IO_DONE: return io_fail(message);
  case IO_FAIL: return io_fail(io_cat3(heap, message, str_lit("; temporary output: "), result.value));
  }
  return io_fail(message);
}

IoResult io_finish_publish(Heap *heap, IoResult result, const Vec *files, Str temporary) {
  switch (result.status) {
  case IO_DONE: return io_done(str_empty());
  case IO_FAIL: return io_cleanup_error(heap, io_cleanup_files(heap, files, 0, temporary), result.value);
  }
  return result;
}

static IoResult io_write_more(Heap *heap, IoResult result, const Vec *files, size_t index, Str directory) {
  switch (result.status) {
  case IO_FAIL: return result;
  case IO_DONE: return io_write_files(heap, files, index, directory);
  }
  return result;
}

IoResult io_write_files(Heap *heap, const Vec *files, size_t index, Str directory) {
  VecGet got = vec_get(files, index);
  switch (got.status) {
  case VEC_GET_OUT_OF_RANGE: return io_done(str_empty());
  case VEC_GET_OK: {
    const IoFile *file = got.value;
    IoResult result = io_write_file(heap, io_cat3(heap, directory, str_lit("/"), file->name), file->contents);
    return io_write_more(heap, result, files, index + 1, directory);
  }
  }
  return io_done(str_empty());
}

IoResult io_rename_error(Heap *heap, HostFsResult error, Str original) {
  return io_fail(io_cat3(heap, original, str_lit(": "), error.message));
}

IoResult io_rename_result(Heap *heap, HostFsResult result, Str original) {
  switch (result.status) {
  case IO_DONE: return io_done(str_empty());
  case IO_FAIL: return io_rename_error(heap, result, original);
  }
  return io_rename_error(heap, result, original);
}

IoResult io_publish_rename(Heap *heap, bool present, Str temporary, Str target, Str original) {
  if (present) return io_fail(heap_str_cat(heap, str_lit("output already exists: "), original));
  return io_rename_result(heap, mechanism_fs(heap, HOST_FS_RENAME, temporary, target), original);
}

IoResult io_publish_written(Heap *heap, IoResult result, Str temporary, Str target, Str original) {
  switch (result.status) {
  case IO_FAIL: return result;
  case IO_DONE: return io_publish_rename(heap, io_exists(heap, target), temporary, target, original);
  }
  return result;
}

IoResult io_publish_staged(Heap *heap, IoResult result, const Vec *files, Str target, Str original) {
  switch (result.status) {
  case IO_FAIL: return result;
  case IO_DONE: {
    Str temporary = result.value;
    IoResult written = io_write_files(heap, files, 0, temporary);
    IoResult published = io_publish_written(heap, written, temporary, target, original);
    return io_finish_publish(heap, published, files, temporary);
  }
  }
  return result;
}

IoResult io_publish_exists(Heap *heap, bool present, Str target, Str original, const Vec *files) {
  if (present) return io_fail(heap_str_cat(heap, str_lit("output already exists: "), original));
  return io_publish_staged(heap, io_fs(heap, HOST_FS_STAGE, target, str_empty()), files, target, original);
}

IoResult io_publish(Heap *heap, Str directory, const Vec *files) {
  Str target = io_normalize_target(directory);
  return io_publish_exists(heap, io_exists(heap, target), target, directory, files);
}
