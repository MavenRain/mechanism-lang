#include "dispatch.h"

static int fail(const char *what) {
  fprintf(stderr, "FAIL dispatch_path %s\n", what);
  return 1;
}

static int check(const char *self, const char *expected) {
  char path[DISPATCH_PATH_MAX];
  if (dispatch_path(self, "target", path, sizeof path) || strcmp(path, expected) != 0) {
    fprintf(stderr, "FAIL dispatcher path for %s\n", self);
    return 1;
  }
  return 0;
}

/* The PATH directory is made under $TMPDIR (fallback /tmp): a sandbox can deny writes to /tmp. */
static int temp_directory(char *out, size_t cap) {
  const char *env = getenv("TMPDIR");
  const char *root = (env && env[0]) ? env : "/tmp";
  size_t len = strlen(root);
  size_t trim = (len > 1 && root[len - 1] == '/') ? 1 : 0;
  int n = snprintf(out, cap, "%.*s/mech-dispatch-path.XXXXXX", (int)(len - trim), root);
  if (n < 0 || (size_t)n >= cap) return 1;
  return mkdtemp(out) ? 0 : 1;
}

int main(int argc, char **argv) {
  (void)argc;
  char executable[DISPATCH_PATH_MAX];
  if (!realpath(argv[0], executable)) return fail("cannot resolve its own path");
  char expected[DISPATCH_PATH_MAX];
  char *slash = strrchr(executable, '/');
  int n = snprintf(expected, sizeof expected, "%.*s/target", (int)(slash - executable), executable);
  if (n < 0 || (size_t)n >= sizeof expected) return fail("expected path too long");
  char directory[DISPATCH_PATH_MAX];
  if (temp_directory(directory, sizeof directory)) return fail("cannot make the PATH directory");
  char link[DISPATCH_PATH_MAX];
  n = snprintf(link, sizeof link, "%s/runner", directory);
  if (n < 0 || (size_t)n >= sizeof link) return fail("runner link path too long");
  if (symlink(executable, link) != 0) return fail("cannot make the runner link");
  if (setenv("PATH", directory, 1) != 0) return fail("cannot set PATH");
  int failed = check("runner", expected) || check(link, expected);
  if (unlink(link) != 0 || rmdir(directory) != 0) return fail("cannot remove the PATH directory");
  if (failed) return 1;
  if (setenv("PATH", "/nonexistent:/bin", 1) != 0) return fail("cannot set PATH");
  char shell[DISPATCH_PATH_MAX];
  if (!realpath("/bin/sh", shell)) return fail("cannot resolve /bin/sh");
  slash = strrchr(shell, '/');
  n = snprintf(expected, sizeof expected, "%.*s/target", (int)(slash - shell), shell);
  if (n < 0 || (size_t)n >= sizeof expected) return fail("shell expected path too long");
  if (check("sh", expected) || check("/runner", "/target") ||
      check("relative/runner", "relative/target")) return 1;
  if (chdir("/bin") != 0) return fail("cannot enter /bin");
  if (setenv("PATH", ":/nonexistent", 1) != 0) return fail("cannot set PATH");
  if (check("sh", expected)) return 1;
  if (setenv("PATH", "/nonexistent:", 1) != 0) return fail("cannot set PATH");
  if (check("sh", expected)) return 1;
  char small[2];
  if (dispatch_path("sh", "target", small, sizeof small) == 0) return fail("accepted a 2-byte buffer");
  return dispatch_print("DISPATCH-PATH-OK");
}
