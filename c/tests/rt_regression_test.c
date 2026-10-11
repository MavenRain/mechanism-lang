/* Regressions for allocation bounds and process side effects. */
#include "rt.h"

#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

static int failures;
static int checks;

static void expect(bool ok, const char *name) {
  checks += 1;
  if (!ok) {
    failures += 1;
    printf("FAIL %s\n", name);
  }
}

int main(void) {
  Arena arena = arena_new(4096);
  void *first = arena_alloc(&arena, 16);
  expect(first != NULL && arena_alloc(&arena, SIZE_MAX) == NULL,
         "arena rejects rounding overflow");
  void *second = arena_alloc(&arena, 16);
  expect(second != NULL && second != first, "arena usable after overflow");
  StrSlice empty = str_slice(str_lit(NULL), 0, 0);
  expect(empty.status == STR_INDEX_OK && empty.value.len == 0, "NULL empty slice");

  FsResult temp = fs_temp_dir(&arena, str_lit("mech-rt-regression-"));
  if (temp.status != FS_OK) return 2;
  Str out = str_cat(&arena, temp.value, str_lit("/out"));
  Str err = str_cat(&arena, temp.value, str_lit("/err"));
  expect(fs_write(out, str_lit("keep out")).status == FS_OK &&
         fs_write(err, str_lit("keep err")).status == FS_OK, "seed capture files");
  const char *const invalid[] = {"/bin/sh", NULL};
  ProcessResult bad = host_process_run(invalid, 0, out, err);
  expect(bad.status == PROCESS_INVALID_ARGUMENT, "reject empty argv");
  FsResult out_before = fs_read(&arena, out);
  FsResult err_before = fs_read(&arena, err);
  expect(out_before.status == FS_OK && str_eq(out_before.value, str_lit("keep out")) &&
         err_before.status == FS_OK && str_eq(err_before.value, str_lit("keep err")),
         "invalid argv preserves capture files");

  const char *const command[] = {"/bin/sh", "-c", "printf out; printf err >&2", NULL};
  for (unsigned mask = 0; mask < 8; mask++) {
    pid_t child = fork();
    if (child == 0) {
      for (unsigned fd = 0; fd < 3; fd++) {
        if ((mask & (1u << fd)) != 0) close((int)fd);
      }
      ProcessResult ran = host_process_run(command, 3, out, err);
      _exit(ran.status == PROCESS_RAN && ran.code == 0 ? 0 : 1);
    }
    int status = 0;
    bool ran = child > 0 && waitpid(child, &status, 0) == child &&
               WIFEXITED(status) && WEXITSTATUS(status) == 0;
    FsResult got_out = fs_read(&arena, out);
    FsResult got_err = fs_read(&arena, err);
    char name[64];
    snprintf(name, sizeof name, "closed descriptors mask=%u", mask);
    expect(ran && got_out.status == FS_OK && got_err.status == FS_OK &&
           str_eq(got_out.value, str_lit("out")) && str_eq(got_err.value, str_lit("err")), name);
  }

  expect(fs_unlink(out).status == FS_OK && fs_unlink(err).status == FS_OK &&
         fs_rmdir(temp.value).status == FS_OK, "regression cleanup");
  arena_free(&arena);
  if (failures == 0) printf("RT-REGRESSION-OK checks=%d\n", checks);
  return failures == 0 ? 0 : 1;
}
