/* Port of bend2/tests/relational_native_4.bend: shard 4 of the relational protocols
 * (horizontal-associativity) over the sibling binary `<dir of argv[0]>/<module>`. */
#include "dispatch.h"

static int select_0(const char *self, int yes, const char *mode, int argc, char **args) {
  (void)mode;
  if (yes) return dispatch_exec(self, "prelude_horizontal_associativity", argc, args);
  return dispatch_die(2, "unknown relational shard mode");
}

static int arguments(const char *self, int argc, char **args) {
  if (argc >= 1) return select_0(self, strcmp(args[0], "horizontal-associativity") == 0, args[0], argc - 1, args + 1);
  return dispatch_die(2, "expected relational shard mode");
}

int main(int argc, char **argv) { return arguments(argv[0], argc - 1, argv + 1); }
