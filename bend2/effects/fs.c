#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif

typedef struct {
  uint32_t op;
  char *path;
  char *other;
  uint64_t other_len;
  char *result;
  size_t result_len;
} MechFs;

static void mech_fs_read(IoWork *w, MechFs *s) {
  int fd = open(s->path, O_RDONLY);
  if (fd < 0) { w->code = errno; return; }
  size_t used = 0, capacity = 65536;
  char *data = malloc(capacity);
  if (!data) { w->code = ENOMEM; close(fd); return; }
  for (;;) {
    if (used == capacity) {
      if (capacity > SIZE_MAX / 2) { w->code = EFBIG; break; }
      size_t next = capacity * 2;
      char *larger = realloc(data, next);
      if (!larger) { w->code = ENOMEM; break; }
      data = larger; capacity = next;
    }
    ssize_t got = read(fd, data + used, capacity - used);
    if (got < 0) {
      if (errno == EINTR) continue;
      w->code = errno; break;
    }
    if (got == 0) break;
    used += (size_t)got;
  }
  if (close(fd) != 0 && !w->code) w->code = errno;
  s->result = data; s->result_len = used;
}

static void mech_fs_write(IoWork *w, MechFs *s) {
  int fd = open(s->path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd < 0) { w->code = errno; return; }
  size_t offset = 0;
  while (offset < s->other_len) {
    ssize_t wrote = write(fd, s->other + offset, (size_t)s->other_len - offset);
    if (wrote < 0 && errno == EINTR) continue;
    if (wrote <= 0) { w->code = wrote < 0 ? errno : EIO; break; }
    offset += (size_t)wrote;
  }
  if (close(fd) != 0 && !w->code) w->code = errno;
}

static void mech_fs_stage(IoWork *w, MechFs *s) {
  size_t size = strlen(s->path) + sizeof(".tmp-XXXXXX");
  s->result = malloc(size);
  if (!s->result) { w->code = ENOMEM; return; }
  snprintf(s->result, size, "%s.tmp-XXXXXX", s->path);
  if (!mkdtemp(s->result)) { w->code = errno; return; }
  s->result_len = strlen(s->result);
}

static void mech_fs_rename(IoWork *w, MechFs *s) {
  int status;
#if defined(__APPLE__)
  status = renamex_np(s->path, s->other, RENAME_EXCL);
#elif defined(__linux__) && defined(SYS_renameat2)
  status = (int)syscall(SYS_renameat2, AT_FDCWD, s->path, AT_FDCWD, s->other, 1);
#else
  errno = ENOTSUP;
  status = -1;
#endif
  if (status != 0) w->code = errno;
}

static void mech_fs_temp(IoWork *w, MechFs *s) {
  const char *base = getenv("TMPDIR");
  if (!base || !base[0]) base = "/tmp";
  size_t size = strlen(base) + strlen(s->path) + strlen(s->other) + 10;
  s->result = malloc(size);
  if (!s->result) { w->code = ENOMEM; return; }
  snprintf(s->result, size, "%s/%sXXXXXX%s", base, s->path, s->other);
  int fd = mkstemps(s->result, (int)strlen(s->other));
  if (fd < 0) { w->code = errno; return; }
  if (close(fd) != 0) {
    w->code = errno;
    unlink(s->result);
    return;
  }
  s->result_len = strlen(s->result);
}

static void mech_fs_call(IoWork *w) {
  MechFs *s = (MechFs *)w->data;
  switch (s->op) {
    case 0: mech_fs_read(w, s); return;
    case 1: mech_fs_stage(w, s); return;
    case 2: mech_fs_rename(w, s); return;
    case 3: if (unlink(s->path) != 0) w->code = errno; return;
    case 4: if (rmdir(s->path) != 0) w->code = errno; return;
    case 5: {
      struct stat statbuf;
      const char *value = stat(s->path, &statbuf) == 0 ? "1" : "0";
      s->result = strdup(value);
      if (!s->result) { w->code = ENOMEM; return; }
      s->result_len = 1;
      return;
    }
    case 6: mech_fs_write(w, s); return;
    case 7: mech_fs_temp(w, s); return;
    default: w->code = EINVAL; return;
  }
}

static Term mech_fs_pack(Env e, IoWork *w) {
  MechFs *s = (MechFs *)w->data;
  Term result = w->code ? io_fail(e, w->code, NULL)
    : io_done(e, io_str(e, s->result ? s->result : "", s->result_len));
  free(s->path); free(s->other); free(s->result); free(s);
  return result;
}

#ifdef CID_MECHANISM_FS
Term mechanism_fs_run(Env e, Term *f, IoWork *w) {
  w->code = 0;
  uint64_t path_len = 0;
  MechFs *s = io_mem(calloc(1, sizeof(*s)));
  s->op = (uint32_t)f[0];
  s->path = io_cstr(e, f[1], &path_len);
  s->other = io_cstr(e, f[2], &s->other_len);
  w->data = (char *)s;
  if (io_nul(s->path, path_len) ||
      (s->op != 6 && io_nul(s->other, s->other_len))) {
    w->code = EILSEQ;
    return mech_fs_pack(e, w);
  }
  return io_work(w, mech_fs_call, mech_fs_pack);
}

static void __attribute__((constructor)) mechanism_fs_use(void) {
  io_eff(CID_MECHANISM_FS, mechanism_fs_run, 0);
}
#endif

#ifdef CID_MECHANISM_WRITE_BYTES
Term mechanism_write_bytes_run(Env e, Term *f, IoWork *w) {
  w->code = 0;
  uint64_t path_len = 0;
  size_t capacity = 64;
  MechFs *s = io_mem(calloc(1, sizeof(*s)));
  s->op = 6;
  s->path = io_cstr(e, f[0], &path_len);
  s->other = io_mem(malloc(capacity));
  w->data = (char *)s;
  if (io_nul(s->path, path_len)) w->code = EILSEQ;
  Term xs = f[1];
  while (term_aux(xs) == CID_CON) {
    Term fields[2];
    spare_free(e, cls_fit(2), ctr_take(e, xs, 2, fields));
    if (s->other_len == capacity) {
      if (capacity > SIZE_MAX / 2) {
        w->code = EFBIG;
      } else {
        capacity *= 2;
        s->other = io_mem(realloc(s->other, capacity));
      }
    }
    if (fields[0] > 255) w->code = EINVAL;
    if (!w->code) s->other[s->other_len++] = (char)fields[0];
    xs = fields[1];
  }
  return w->code ? mech_fs_pack(e, w) : io_work(w, mech_fs_call, mech_fs_pack);
}

static void __attribute__((constructor)) mechanism_write_bytes_use(void) {
  io_eff(CID_MECHANISM_WRITE_BYTES, mechanism_write_bytes_run, 0);
}
#endif
