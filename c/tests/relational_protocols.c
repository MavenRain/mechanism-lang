/* Port of tests/relational_protocols.bend: the 18 prelude relational protocols P0..P17 and
 * surface-check over the sibling binaries `<dir of argv[0]>/<module>`; the choose_N / selected_N
 * chain of the Bend file is kept definition for definition. */
#include "dispatch.h"

static int surface_mode(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "surface_check_driver", argc, args);
  return dispatch_die_with(1, "unknown relational protocol: ", mode);
}

static int choose_18(const char *self, const char *mode, int argc, char **args) {
  return surface_mode(self, strcmp(mode, "surface-check") == 0, mode, argc, args);
}

static int selected_17(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_nattrans_units", argc, args);
  return choose_18(self, mode, argc, args);
}

static int choose_17(const char *self, const char *mode, int argc, char **args) {
  return selected_17(self, strcmp(mode, "nattrans-units") == 0, mode, argc, args);
}

static int selected_16(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_horizontal_associativity", argc, args);
  return choose_17(self, mode, argc, args);
}

static int choose_16(const char *self, const char *mode, int argc, char **args) {
  return selected_16(self, strcmp(mode, "horizontal-associativity") == 0, mode, argc, args);
}

static int selected_15(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_iterated_whiskering", argc, args);
  return choose_16(self, mode, argc, args);
}

static int choose_15(const char *self, const char *mode, int argc, char **args) {
  return selected_15(self, strcmp(mode, "iterated-whiskering") == 0, mode, argc, args);
}

static int selected_14(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_horizontal_laws", argc, args);
  return choose_15(self, mode, argc, args);
}

static int choose_14(const char *self, const char *mode, int argc, char **args) {
  return selected_14(self, strcmp(mode, "horizontal-laws") == 0, mode, argc, args);
}

static int selected_13(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_whiskering_laws", argc, args);
  return choose_14(self, mode, argc, args);
}

static int choose_13(const char *self, const char *mode, int argc, char **args) {
  return selected_13(self, strcmp(mode, "whiskering-laws") == 0, mode, argc, args);
}

static int selected_12(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_nattrans_laws", argc, args);
  return choose_13(self, mode, argc, args);
}

static int choose_12(const char *self, const char *mode, int argc, char **args) {
  return selected_12(self, strcmp(mode, "nattrans-laws") == 0, mode, argc, args);
}

static int selected_11(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_shared_nattrans", argc, args);
  return choose_12(self, mode, argc, args);
}

static int choose_11(const char *self, const char *mode, int argc, char **args) {
  return selected_11(self, strcmp(mode, "shared-nattrans") == 0, mode, argc, args);
}

static int selected_10(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_shared_left_kan", argc, args);
  return choose_11(self, mode, argc, args);
}

static int choose_10(const char *self, const char *mode, int argc, char **args) {
  return selected_10(self, strcmp(mode, "shared-left-kan") == 0, mode, argc, args);
}

static int selected_9(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_left_kan_laws", argc, args);
  return choose_10(self, mode, argc, args);
}

static int choose_9(const char *self, const char *mode, int argc, char **args) {
  return selected_9(self, strcmp(mode, "left-kan-laws") == 0, mode, argc, args);
}

static int selected_8(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_heterogeneous_whiskering", argc, args);
  return choose_9(self, mode, argc, args);
}

static int choose_8(const char *self, const char *mode, int argc, char **args) {
  return selected_8(self, strcmp(mode, "heterogeneous-whiskering") == 0, mode, argc, args);
}

static int selected_7(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_heterogeneous_left_kan", argc, args);
  return choose_8(self, mode, argc, args);
}

static int choose_7(const char *self, const char *mode, int argc, char **args) {
  return selected_7(self, strcmp(mode, "heterogeneous-left-kan") == 0, mode, argc, args);
}

static int selected_6(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_heterogeneous_nattrans", argc, args);
  return choose_7(self, mode, argc, args);
}

static int choose_6(const char *self, const char *mode, int argc, char **args) {
  return selected_6(self, strcmp(mode, "heterogeneous-nattrans") == 0, mode, argc, args);
}

static int selected_5(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_composable_functors", argc, args);
  return choose_6(self, mode, argc, args);
}

static int choose_5(const char *self, const char *mode, int argc, char **args) {
  return selected_5(self, strcmp(mode, "composable-functors") == 0, mode, argc, args);
}

static int selected_4(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_heterogeneous_functor", argc, args);
  return choose_5(self, mode, argc, args);
}

static int choose_4(const char *self, const char *mode, int argc, char **args) {
  return selected_4(self, strcmp(mode, "heterogeneous-functor") == 0, mode, argc, args);
}

static int selected_3(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_left_kan", argc, args);
  return choose_4(self, mode, argc, args);
}

static int choose_3(const char *self, const char *mode, int argc, char **args) {
  return selected_3(self, strcmp(mode, "left-kan") == 0, mode, argc, args);
}

static int selected_2(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_nattrans", argc, args);
  return choose_3(self, mode, argc, args);
}

static int choose_2(const char *self, const char *mode, int argc, char **args) {
  return selected_2(self, strcmp(mode, "nattrans") == 0, mode, argc, args);
}

static int selected_1(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_functor", argc, args);
  return choose_2(self, mode, argc, args);
}

static int choose_1(const char *self, const char *mode, int argc, char **args) {
  return selected_1(self, strcmp(mode, "functor") == 0, mode, argc, args);
}

static int selected_0(const char *self, int yes, const char *mode, int argc, char **args) {
  if (yes) return dispatch_exec(self, "prelude_category", argc, args);
  return choose_1(self, mode, argc, args);
}

static int choose_0(const char *self, const char *mode, int argc, char **args) {
  return selected_0(self, strcmp(mode, "category") == 0, mode, argc, args);
}

static int arguments(const char *self, int argc, char **args) {
  if (argc < 1) return dispatch_die(1, "expected relational protocol mode and root");
  return choose_0(self, args[0], argc - 1, args + 1);
}

int main(int argc, char **argv) { return arguments(argv[0], argc - 1, argv + 1); }
