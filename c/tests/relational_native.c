/* Port of bend2/tests/relational_native.bend: category, heterogeneous-functor, template-cost and
 * surface-check over the sibling binaries `<dir of argv[0]>/<module>`. */
#include "dispatch.h"

static int surface(const char *self, int yes, int argc, char **args) {
  if (yes) return dispatch_exec(self, "surface_check_driver", argc, args);
  return dispatch_die(2, "expected category, heterogeneous-functor, template-cost or surface-check mode");
}

static int template(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "template_cost", argc, args);
  return surface(self, strcmp(mode, "surface-check") == 0, argc, args);
}

static int heterogeneous(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_heterogeneous_functor", argc, args);
  return template(self, strcmp(mode, "template-cost") == 0, mode, argc, args);
}

static int category(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_category", argc, args);
  return heterogeneous(self, strcmp(mode, "heterogeneous-functor") == 0, mode, argc, args);
}

static int arguments(const char *self, int argc, char **args) {
  if (argc >= 1) return category(self, strcmp(args[0], "category") == 0, args[0], argc - 1, args + 1);
  return dispatch_die(2, "expected protocol mode and arguments");
}

int main(int argc, char **argv) { return arguments(argv[0], argc - 1, argv + 1); }
