/* Port of tests/surface_foundation_runner.bend: the foundation protocol runner. `equality`
 * calls surface_equality main() (the rest is dropped); every other known mode passes the rest to
 * the module arguments(rest) (surface-check to surface_check_driver run(rest)); no mode or an
 * unknown mode dies 2. Each module is the sibling binary `<dir of argv[0]>/<module>`. */
#include "dispatch.h"

static const DispatchEntry MODES[] = {
  {"poly", "surface_prelude_poly"},
  {"dependent", "surface_prelude_dependent"},
  {"equality-ops", "surface_prelude_equality_ops"},
  {"congruence", "surface_prelude_congruence"},
  {"transport", "surface_prelude_transport"},
  {"groups", "surface_family_groups"},
  {"sl", "surface_sl"},
  {"veil", "surface_shape_metadata"},
  {"members", "surface_family_members"},
  {"prenex", "surface_prenex"},
  {"prenex-families", "surface_prenex_families"},
  {"prenex-groups", "surface_prenex_groups"},
  {"prenex-exports", "surface_prenex_exports"},
  {"dependency-exports", "surface_prenex_dependency_exports"},
  {"reuse", "surface_template_reuse"},
  {"symbolic-reuse", "surface_template_symbolic_reuse"},
  {"composition", "surface_template_composition"},
  {"family-poly", "surface_family_poly"},
  {"numbers", "surface_numbers"},
  {"expectations", "surface_expectations"},
  {"surface-check", "surface_check_driver"},
};

static int arguments(const char *self, int argc, char **args) {
  if (argc < 1) return dispatch_die(2, "expected foundation protocol mode");
  if (strcmp(args[0], "equality") == 0) return dispatch_exec(self, "surface_equality", 0, NULL);
  const DispatchEntry *entry = dispatch_find(MODES, sizeof MODES / sizeof MODES[0], args[0]);
  if (entry) return dispatch_exec(self, entry->target, argc - 1, args + 1);
  return dispatch_die(2, "expected foundation protocol mode");
}

int main(int argc, char **argv) { return arguments(argv[0], argc - 1, argv + 1); }
