/* Port of bend2/tests/relational_native_2.bend: shard 2 of the relational protocols
 * (heterogeneous-whiskering, left-kan-laws, shared-left-kan, shared-nattrans) over the sibling
 * binaries `<dir of argv[0]>/<module>`. */
#include "dispatch.h"

static int select_3(const char *self, int yes, const char *mode, int argc, char **args) {
  (void)mode;
  if (yes) return dispatch_exec(self, "prelude_shared_nattrans", argc, args);
  return dispatch_die(2, "unknown relational shard mode");
}

static int select_2(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_shared_left_kan", argc, args);
  return select_3(self, strcmp(mode, "shared-nattrans") == 0, mode, argc, args);
}

static int select_1(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_left_kan_laws", argc, args);
  return select_2(self, strcmp(mode, "shared-left-kan") == 0, mode, argc, args);
}

static int select_0(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_heterogeneous_whiskering", argc, args);
  return select_1(self, strcmp(mode, "left-kan-laws") == 0, mode, argc, args);
}

static int arguments(const char *self, int argc, char **args) {
  if (argc >= 1) return select_0(self, strcmp(args[0], "heterogeneous-whiskering") == 0, args[0], argc - 1, args + 1);
  return dispatch_die(2, "expected relational shard mode");
}

int main(int argc, char **argv) { return arguments(argv[0], argc - 1, argv + 1); }
