#include "dispatch.h"

static int check(const char *self, const char *expected) {
  char path[DISPATCH_PATH_MAX];
  if (dispatch_path(self, "target", path, sizeof path) || strcmp(path, expected) != 0) {
    fprintf(stderr, "FAIL dispatcher path for %s\n", self);
    return 1;
  }
  return 0;
}

int main(int argc, char **argv) {
  (void)argc;
  char executable[DISPATCH_PATH_MAX];
  if (!realpath(argv[0], executable)) return 1;
  char expected[DISPATCH_PATH_MAX];
  char *slash = strrchr(executable, '/');
  int n = snprintf(expected, sizeof expected, "%.*s/target", (int)(slash - executable), executable);
  if (n < 0 || (size_t)n >= sizeof expected) return 1;
  char directory[] = "/tmp/mech-dispatch-path.XXXXXX";
  if (!mkdtemp(directory)) return 1;
  char link[DISPATCH_PATH_MAX];
  n = snprintf(link, sizeof link, "%s/runner", directory);
  if (n < 0 || (size_t)n >= sizeof link) return 1;
  if (symlink(executable, link) != 0 || setenv("PATH", directory, 1) != 0) return 1;
  int failed = check("runner", expected) || check(link, expected);
  if (unlink(link) != 0 || rmdir(directory) != 0 || failed) return 1;
  if (setenv("PATH", "/nonexistent:/bin", 1) != 0) return 1;
  char shell[DISPATCH_PATH_MAX];
  if (!realpath("/bin/sh", shell)) return 1;
  slash = strrchr(shell, '/');
  n = snprintf(expected, sizeof expected, "%.*s/target", (int)(slash - shell), shell);
  if (n < 0 || (size_t)n >= sizeof expected) return 1;
  if (check("sh", expected) || check("/runner", "/target") ||
      check("relative/runner", "relative/target")) return 1;
  if (chdir("/bin") != 0 || setenv("PATH", ":/nonexistent", 1) != 0) return 1;
  if (check("sh", expected)) return 1;
  if (setenv("PATH", "/nonexistent:", 1) != 0 || check("sh", expected)) return 1;
  char small[2];
  if (dispatch_path("sh", "target", small, sizeof small) == 0) return 1;
  return dispatch_print("DISPATCH-PATH-OK");
}
