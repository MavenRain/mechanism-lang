/* Shared boundary of the C dispatcher drivers (tests/runner.bend, surface_foundation_runner.bend,
 * relational_native*.bend and relational_protocols.bend). A Bend dispatcher imports every driver
 * module and calls its main(), arguments(rest) or run(rest); the C port execs the sibling binary
 * `<dir of argv[0]>/<target>` with the same remaining arguments, so stdout, stderr and the exit code
 * are the target's. The boolean(...) wrapper of runner.bend forks the target with both streams on
 * /dev/null and reports PASS or FAIL itself. IO.print appends a newline on stdout; IO.die writes the
 * message and a newline on stderr and exits with the code. */
#ifndef MECH_DISPATCH_H
#define MECH_DISPATCH_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define DISPATCH_PATH_MAX 4096

/* A mode word of a dispatcher and the C driver it reaches. */
typedef struct {
  const char *mode;
  const char *target;
} DispatchEntry;

static inline int dispatch_print(const char *text) {
  fputs(text, stdout);
  fputc('\n', stdout);
  return 0;
}

static inline int dispatch_die(int code, const char *message) {
  fputs(message, stderr);
  fputc('\n', stderr);
  return code;
}

/* IO.die with a prefix and the offending word (the `"unknown unit: " ++ name` texts). */
static inline int dispatch_die_with(int code, const char *prefix, const char *word) {
  fputs(prefix, stderr);
  fputs(word, stderr);
  fputc('\n', stderr);
  return code;
}

/* The entry whose mode equals `mode`, or NULL. */
static inline const DispatchEntry *dispatch_find(const DispatchEntry *table, size_t count, const char *mode) {
  for (size_t i = 0; i < count; i++) {
    if (strcmp(table[i].mode, mode) == 0) return table + i;
  }
  return NULL;
}

/* `<dir of self>/<target>`; resolve a bare executable name through PATH as execvp does.
 * Empty PATH components denote the working directory. Returns 1 when the path does not fit. */
static inline int dispatch_path(const char *self, const char *target, char *out, size_t cap) {
  char resolved[DISPATCH_PATH_MAX];
  const char *slash = strrchr(self, '/');
  const char *base = slash ? self : ".";
  int dir = slash ? (int)(slash - self) : 1;
  if (!slash) {
    const char *entry = getenv("PATH");
    while (entry) {
      const char *end = strchr(entry, ':');
      size_t len = end ? (size_t)(end - entry) : strlen(entry);
      char candidate[DISPATCH_PATH_MAX];
      int n = snprintf(candidate, sizeof candidate, "%.*s/%s", (int)(len ? len : 1),
                       len ? entry : ".", self);
      struct stat info;
      if (n >= 0 && (size_t)n < sizeof candidate && access(candidate, X_OK) == 0 &&
          stat(candidate, &info) == 0 && S_ISREG(info.st_mode)) {
        base = len ? entry : ".";
        dir = (int)(len ? len : 1);
        if (realpath(candidate, resolved)) {
          base = resolved;
          dir = (int)(strrchr(resolved, '/') - resolved);
        }
        break;
      }
      entry = end ? end + 1 : NULL;
    }
  } else if (realpath(self, resolved)) {
    base = resolved;
    dir = (int)(strrchr(resolved, '/') - resolved);
  }
  int n = snprintf(out, cap, "%.*s/%s", dir, base, target);
  return (n < 0 || (size_t)n >= cap) ? 1 : 0;
}

/* Module.arguments(rest), Module.run(rest) or Module.main() (argc 0): replace the process by the
 * target. A failed exec has no Bend counterpart: `<path>: <errno text>` on stderr and exit 1. */
static inline int dispatch_exec(const char *self, const char *target, int argc, char **args) {
  char path[DISPATCH_PATH_MAX];
  if (dispatch_path(self, target, path, sizeof path) != 0) return dispatch_die_with(1, target, ": path too long");
  char **vec = (char **)calloc((size_t)argc + 2, sizeof(char *));
  if (!vec) return dispatch_die_with(1, path, ": out of memory");
  vec[0] = path;
  if (argc > 0) memcpy(vec + 1, args, (size_t)argc * sizeof(char *));
  vec[argc + 1] = NULL;
  execv(path, vec);
  int saved = errno;
  free(vec);
  fprintf(stderr, "%s: %s\n", path, strerror(saved));
  return 1;
}

/* runner.bend boolean(result, name): the target's Bool main() is its exit status (0 = True). Both
 * streams of the child go to /dev/null, so only the PASS or FAIL line of the runner is visible. */
static inline int dispatch_boolean(const char *self, const char *target, const char *module) {
  char path[DISPATCH_PATH_MAX];
  if (dispatch_path(self, target, path, sizeof path) != 0) return dispatch_die_with(1, "FAIL ", module);
  fflush(stdout);
  fflush(stderr);
  pid_t pid = fork();
  if (pid < 0) return dispatch_die_with(1, "FAIL ", module);
  if (pid == 0) {
    int null = open("/dev/null", O_WRONLY);
    if (null < 0 || dup2(null, STDOUT_FILENO) < 0 || dup2(null, STDERR_FILENO) < 0) _exit(127);
    char *vec[2] = {path, NULL};
    execv(path, vec);
    _exit(127);
  }
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) return dispatch_die_with(1, "FAIL ", module);
  int passed = WIFEXITED(status) && WEXITSTATUS(status) == 0;
  if (passed) {
    fputs("PASS ", stdout);
    return dispatch_print(module);
  }
  return dispatch_die_with(1, "FAIL ", module);
}

#endif
