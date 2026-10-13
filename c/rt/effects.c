/* Host effects, ported from bend2/effects/fs.c (ops 0..7) and bend2/cli/effects/{process,executable}.c.
 * Library code never exits; every failure is a returned sum value. */
#include "rt.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#if defined(__linux__)
#include <sys/syscall.h>
#endif

extern char **environ;

static FsResult fs_result(FsStatus status, int code, Str value) {
  FsResult out;
  out.status = status;
  out.code = code;
  out.value = value;
  return out;
}

static FsResult fs_fail(int code) { return fs_result(FS_ERRNO, code, str_empty()); }
static FsResult fs_ok(Str value) { return fs_result(FS_OK, 0, value); }
static FsResult fs_bad_path(void) { return fs_result(FS_BAD_PATH, 0, str_empty()); }
static FsResult fs_oom(void) { return fs_result(FS_OOM, 0, str_empty()); }

/* Validate and allocate the entire host path before performing any effect. */
static char *fs_path_join(Arena *arena, const Str *parts, size_t count, FsStatus *status) {
  size_t len = 0;
  *status = FS_OK;
  for (size_t i = 0; i < count; i++) {
    if (parts[i].len > SIZE_MAX - 1 - len) {
      *status = FS_OOM;
      return NULL;
    }
    if (parts[i].len > 0 && (parts[i].data == NULL || memchr(parts[i].data, 0, parts[i].len) != NULL)) {
      *status = FS_BAD_PATH;
      return NULL;
    }
    len += parts[i].len;
  }
  char *path = arena_alloc(arena, len + 1);
  if (path == NULL) {
    *status = FS_OOM;
    return NULL;
  }
  size_t at = 0;
  for (size_t i = 0; i < count; i++) {
    if (parts[i].len > 0) memcpy(path + at, parts[i].data, parts[i].len);
    at += parts[i].len;
  }
  path[len] = 0;
  return path;
}

/* op 0 */
FsResult fs_read(Arena *arena, Str path) {
  Arena scratch = arena_new(4096);
  FsStatus path_status;
  char *cpath = fs_path_join(&scratch, &path, 1, &path_status);
  if (cpath == NULL) {
    arena_free(&scratch);
    return fs_result(path_status, 0, str_empty());
  }
  int fd = open(cpath, O_RDONLY);
  int open_code = errno;
  arena_free(&scratch);
  if (fd < 0) return fs_fail(open_code);
  size_t used = 0;
  size_t capacity = 65536;
  char *data = malloc(capacity);
  if (data == NULL) {
    close(fd);
    return fs_oom();
  }
  int code = 0;
  for (;;) {
    if (used == capacity) {
      if (capacity > SIZE_MAX / 2) {
        code = EFBIG;
        break;
      }
      char *larger = realloc(data, capacity * 2);
      if (larger == NULL) {
        code = ENOMEM;
        break;
      }
      data = larger;
      capacity *= 2;
    }
    ssize_t got = read(fd, data + used, capacity - used);
    if (got < 0 && errno == EINTR) continue;
    if (got < 0) {
      code = errno;
      break;
    }
    if (got == 0) break;
    used += (size_t)got;
  }
  if (close(fd) != 0 && code == 0) code = errno;
  if (code != 0) {
    free(data);
    return code == ENOMEM ? fs_oom() : fs_fail(code);
  }
  char *copy = arena_alloc(arena, used + 1);
  if (copy == NULL) {
    free(data);
    return fs_oom();
  }
  if (used > 0) memcpy(copy, data, used);
  copy[used] = 0;
  free(data);
  return fs_ok(str_n(copy, used));
}

/* op 6 */
FsResult fs_write(Str path, Str data) {
  Arena scratch = arena_new(4096);
  FsStatus path_status;
  char *cpath = fs_path_join(&scratch, &path, 1, &path_status);
  if (cpath == NULL) {
    arena_free(&scratch);
    return fs_result(path_status, 0, str_empty());
  }
  int fd = open(cpath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  int open_code = errno;
  arena_free(&scratch);
  if (fd < 0) return fs_fail(open_code);
  size_t offset = 0;
  int code = 0;
  while (offset < data.len) {
    ssize_t wrote = write(fd, data.data + offset, data.len - offset);
    if (wrote < 0 && errno == EINTR) continue;
    if (wrote <= 0) {
      code = wrote < 0 ? errno : EIO;
      break;
    }
    offset += (size_t)wrote;
  }
  if (close(fd) != 0 && code == 0) code = errno;
  return code == 0 ? fs_ok(str_empty()) : fs_fail(code);
}

/* op 1: fresh sibling directory path.tmp-XXXXXX */
FsResult fs_stage(Arena *arena, Str path) {
  Str parts[] = {path, str_lit(".tmp-XXXXXX")};
  FsStatus path_status;
  char *buffer = fs_path_join(arena, parts, 2, &path_status);
  if (buffer == NULL) return fs_result(path_status, 0, str_empty());
  if (mkdtemp(buffer) == NULL) return fs_fail(errno);
  return fs_ok(str_lit(buffer));
}

/* op 2: rename that refuses to replace an existing target */
FsResult fs_rename(Str from, Str to) {
  Arena scratch = arena_new(4096);
  FsStatus path_status;
  char *cfrom = fs_path_join(&scratch, &from, 1, &path_status);
  char *cto = cfrom == NULL ? NULL : fs_path_join(&scratch, &to, 1, &path_status);
  if (cfrom == NULL || cto == NULL) {
    arena_free(&scratch);
    return fs_result(path_status, 0, str_empty());
  }
  int status;
#if defined(__APPLE__)
  status = renamex_np(cfrom, cto, RENAME_EXCL);
#elif defined(__linux__) && defined(SYS_renameat2)
  status = (int)syscall(SYS_renameat2, AT_FDCWD, cfrom, AT_FDCWD, cto, 1);
#else
  errno = ENOTSUP;
  status = -1;
#endif
  int code = status != 0 ? errno : 0;
  arena_free(&scratch);
  return code == 0 ? fs_ok(str_empty()) : fs_fail(code);
}

static FsResult fs_path_call(Str path, int (*call)(const char *)) {
  Arena scratch = arena_new(4096);
  FsStatus path_status;
  char *cpath = fs_path_join(&scratch, &path, 1, &path_status);
  if (cpath == NULL) {
    arena_free(&scratch);
    return fs_result(path_status, 0, str_empty());
  }
  int code = call(cpath) != 0 ? errno : 0;
  arena_free(&scratch);
  return code == 0 ? fs_ok(str_empty()) : fs_fail(code);
}

static int fs_mkdir_call(const char *path) { return mkdir(path, 0777); }

/* op 3 */
FsResult fs_unlink(Str path) { return fs_path_call(path, unlink); }
/* op 4 */
FsResult fs_rmdir(Str path) { return fs_path_call(path, rmdir); }
FsResult fs_mkdir(Str path) { return fs_path_call(path, fs_mkdir_call); }

/* op 5 */
bool fs_exists(Str path) {
  Arena scratch = arena_new(4096);
  FsStatus path_status;
  char *cpath = fs_path_join(&scratch, &path, 1, &path_status);
  struct stat statbuf;
  bool exists = cpath != NULL && stat(cpath, &statbuf) == 0;
  arena_free(&scratch);
  return exists;
}

static Str fs_temp_base(void) {
  const char *base = getenv("TMPDIR");
  return (base == NULL || base[0] == 0) ? str_lit("/tmp") : str_lit(base);
}

/* op 7: fresh file $TMPDIR/<prefix>XXXXXX<suffix> */
FsResult fs_temp_file(Arena *arena, Str prefix, Str suffix) {
  if (suffix.len > INT_MAX) return fs_bad_path();
  Str parts[] = {fs_temp_base(), str_lit("/"), prefix, str_lit("XXXXXX"), suffix};
  FsStatus path_status;
  char *buffer = fs_path_join(arena, parts, 5, &path_status);
  if (buffer == NULL) return fs_result(path_status, 0, str_empty());
  int fd = mkstemps(buffer, (int)suffix.len);
  if (fd < 0) return fs_fail(errno);
  if (close(fd) != 0) {
    int code = errno;
    unlink(buffer);
    return fs_fail(code);
  }
  return fs_ok(str_lit(buffer));
}

FsResult fs_temp_dir(Arena *arena, Str prefix) {
  Str parts[] = {fs_temp_base(), str_lit("/"), prefix, str_lit("XXXXXX")};
  FsStatus path_status;
  char *buffer = fs_path_join(arena, parts, 4, &path_status);
  if (buffer == NULL) return fs_result(path_status, 0, str_empty());
  if (mkdtemp(buffer) == NULL) return fs_fail(errno);
  return fs_ok(str_lit(buffer));
}

static ProcessResult process_result(ProcessStatus status, uint32_t code) {
  ProcessResult out;
  out.status = status;
  out.code = code;
  return out;
}

/* Keep capture descriptors separate from the child's standard descriptors. */
static int process_open_capture(const char *path) {
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
  if (fd >= 0 && fd < 3) {
    int moved = fcntl(fd, F_DUPFD_CLOEXEC, 3);
    close(fd);
    return moved;
  }
  return fd;
}

ProcessResult host_process_run(const char *const *argv, size_t argc, Str stdout_path, Str stderr_path) {
  if (argv == NULL || argc == 0 || argc > 4) return process_result(PROCESS_INVALID_ARGUMENT, 127);
  for (size_t i = 0; i < argc; i++) {
    if (argv[i] == NULL) return process_result(PROCESS_INVALID_ARGUMENT, 127);
  }
  Arena scratch = arena_new(4096);
  FsStatus path_status;
  char *out_path = fs_path_join(&scratch, &stdout_path, 1, &path_status);
  char *err_path = out_path == NULL ? NULL : fs_path_join(&scratch, &stderr_path, 1, &path_status);
  if (out_path == NULL || err_path == NULL) {
    arena_free(&scratch);
    return process_result(path_status == FS_BAD_PATH ? PROCESS_INVALID_ARGUMENT : PROCESS_SPAWN_FAILED, 127);
  }
  int out = process_open_capture(out_path);
  if (out < 0) {
    arena_free(&scratch);
    return process_result(PROCESS_SPAWN_FAILED, 127);
  }
  int err = process_open_capture(err_path);
  if (err < 0) {
    close(out);
    arena_free(&scratch);
    return process_result(PROCESS_SPAWN_FAILED, 127);
  }
  ProcessResult result = process_result(PROCESS_SPAWN_FAILED, 127);
  {
    posix_spawn_file_actions_t actions;
    int code = posix_spawn_file_actions_init(&actions);
    if (code == 0) {
      code = posix_spawn_file_actions_adddup2(&actions, out, STDOUT_FILENO);
      if (code == 0) code = posix_spawn_file_actions_adddup2(&actions, err, STDERR_FILENO);
      if (code == 0 && out != STDOUT_FILENO && out != STDERR_FILENO) code = posix_spawn_file_actions_addclose(&actions, out);
      if (code == 0 && err != STDOUT_FILENO && err != STDERR_FILENO) code = posix_spawn_file_actions_addclose(&actions, err);
      pid_t pid = 0;
      char *spawn_argv[5] = {NULL, NULL, NULL, NULL, NULL};
      for (size_t i = 0; i < argc; i++) spawn_argv[i] = (char *)argv[i];
      if (code == 0) code = posix_spawnp(&pid, spawn_argv[0], &actions, NULL, spawn_argv, environ);
      posix_spawn_file_actions_destroy(&actions);
      if (code == 0) {
        int status = 0;
        pid_t waited;
        do {
          waited = waitpid(pid, &status, 0);
        } while (waited < 0 && errno == EINTR);
        bool exited = waited >= 0 && WIFEXITED(status);
        bool signaled = waited >= 0 && WIFSIGNALED(status);
        uint32_t exit_code = exited ? (uint32_t)WEXITSTATUS(status) : signaled ? 128u + (uint32_t)WTERMSIG(status) : 127u;
        if (waited < 0) code = errno;
        else result = process_result(PROCESS_RAN, exit_code);
      }
    }
    /* bend2/cli/effects/process.c: a failed spawn or wait keeps exit 127 and writes one stderr line. */
    if (code != 0) dprintf(err, "invalid: %s: %s\n", argv[0], strerror(code));
  }
  close(out);
  close(err);
  arena_free(&scratch);
  return result;
}

Str host_executable_path(Arena *arena) {
#if defined(__APPLE__)
  uint32_t len = 1024;
  char *path = arena_alloc(arena, len);
  if (path == NULL) return str_empty();
  if (_NSGetExecutablePath(path, &len) != 0) {
    path = arena_alloc(arena, len);
    if (path == NULL || _NSGetExecutablePath(path, &len) != 0) return str_empty();
  }
  return str_lit(path);
#else
  size_t cap = 1024;
  for (;;) {
    char *path = arena_alloc(arena, cap);
    if (path == NULL) return str_empty();
    ssize_t len = readlink("/proc/self/exe", path, cap - 1);
    if (len < 0) return str_empty();
    if ((size_t)len < cap - 1) {
      path[len] = 0;
      return str_lit(path);
    }
    cap *= 2;
  }
#endif
}
