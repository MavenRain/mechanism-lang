/* Port of host_fs.bend: mechanism_fs and mechanism_write_bytes, the POSIX file effect of
 * effects/fs.c, over the fs_* calls of c/rt/effects.c (which follow fs.c call for call).
 * A failure carries the errno and strerror(errno), the text the Bend effect gives a NULL message. */
#include "cli.h"

#include <errno.h>
#include <string.h>

static HostFsResult host_fs_done(Str value) {
  HostFsResult result = {IO_DONE, 0, str_empty(), value};
  return result;
}

static HostFsResult host_fs_fail(Heap *heap, int code) {
  HostFsResult result = {IO_FAIL, (uint32_t)code, str_copy(heap->arena, str_lit(strerror(code))), str_empty()};
  return result;
}

static int host_fs_errno(FsResult result) {
  switch (result.status) {
  case FS_OK: return 0;
  case FS_ERRNO: return result.code;
  case FS_OOM: return ENOMEM;
  case FS_BAD_PATH: return EILSEQ;
  }
  return EIO;
}

static HostFsResult host_fs_from(Heap *heap, FsResult result) {
  int code = host_fs_errno(result);
  if (code != 0) return host_fs_fail(heap, code);
  return host_fs_done(result.value);
}

static bool host_fs_has_nul(Str path) { return path.len > 0 && memchr(path.data, 0, path.len) != NULL; }

/* op 5: "1" when stat succeeds, else "0"; a NUL byte inside the path fails with EILSEQ first. */
static HostFsResult host_fs_exists(Heap *heap, Str path) {
  if (host_fs_has_nul(path)) return host_fs_fail(heap, EILSEQ);
  return host_fs_done(str_lit(fs_exists(path) ? "1" : "0"));
}

HostFsResult mechanism_fs(Heap *heap, HostFsOp operation, Str path, Str other) {
  if (heap->oom) return host_fs_fail(heap, ENOMEM);
  switch (operation) {
  case HOST_FS_READ: return host_fs_from(heap, fs_read(heap->arena, path));
  case HOST_FS_STAGE: return host_fs_from(heap, fs_stage(heap->arena, path));
  case HOST_FS_RENAME: return host_fs_from(heap, fs_rename(path, other));
  case HOST_FS_UNLINK: return host_fs_from(heap, fs_unlink(path));
  case HOST_FS_RMDIR: return host_fs_from(heap, fs_rmdir(path));
  case HOST_FS_EXISTS: return host_fs_exists(heap, path);
  case HOST_FS_WRITE: return host_fs_from(heap, fs_write(path, other));
  case HOST_FS_TEMP: return host_fs_from(heap, fs_temp_file(heap->arena, path, other));
  }
  return host_fs_fail(heap, EINVAL);
}

/* The Bend effect fails with EILSEQ for a NUL byte inside the path and with EINVAL for an item
 * above 255; a Str holds bytes only, so the second case cannot arise here, and fs_write reports
 * the first (FS_BAD_PATH) before it opens the file. */
HostFsResult mechanism_write_bytes(Heap *heap, Str path, Str bytes) {
  if (heap->oom) return host_fs_fail(heap, ENOMEM);
  return host_fs_from(heap, fs_write(path, bytes));
}
