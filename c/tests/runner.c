/* Port of bend2/tests/runner.bend: the test runner that dispatches a mode word to a driver module.
 * `unit NAME` and `fixture NAME` call the module main() (the six Bool mains through boolean(...));
 * the other modes pass the remaining arguments to the module arguments(args) or run(args). Each
 * module is the sibling binary `<dir of argv[0]>/<module>` (see dispatch.h). */
#include "dispatch.h"

typedef enum { UNIT_MAIN, UNIT_BOOLEAN } UnitKind;

typedef struct {
  const char *mode;
  const char *target;
  UnitKind kind;
} UnitEntry;

/* boolean(result, name): PASS <name> on True, die 1 FAIL <name> on False. */
static int boolean(const char *self, const char *target, const char *name) {
  return dispatch_boolean(self, target, name);
}

static const UnitEntry UNITS[] = {
  {"kernel-numbers", "kernel_numbers", UNIT_MAIN},
  {"kernel-eval", "kernel_eval", UNIT_MAIN},
  {"kernel-quantity", "kernel_quantity", UNIT_MAIN},
  {"kernel-check", "kernel_check", UNIT_MAIN},
  {"kernel-declarations", "kernel_declarations", UNIT_MAIN},
  {"kernel-conversion", "kernel_conversion", UNIT_MAIN},
  {"kernel-recursive", "kernel_recursive", UNIT_MAIN},
  {"kernel-budget-dag", "kernel_budget_dag", UNIT_MAIN},
  {"kernel-budget-callbacks", "kernel_budget_callbacks", UNIT_MAIN},
  {"kernel-budget-callback-check", "kernel_budget_callback_check", UNIT_MAIN},
  {"kernel-spec-count", "kernel_spec_count", UNIT_MAIN},
  {"levels-exact", "levels_exact", UNIT_MAIN},
  {"levels-budget", "levels_budget", UNIT_MAIN},
  {"levels-checker", "levels_checker", UNIT_MAIN},
  {"levels-regression", "levels_regression", UNIT_MAIN},
  {"surface-lexer", "surface_lexer", UNIT_BOOLEAN},
  {"surface-diagnostics", "surface_diagnostics", UNIT_BOOLEAN},
  {"surface-shape-metadata", "surface_shape_metadata", UNIT_MAIN},
  {"surface-parser", "surface_parser", UNIT_BOOLEAN},
  {"surface-transform", "surface_transform", UNIT_BOOLEAN},
  {"surface-poly", "surface_poly", UNIT_BOOLEAN},
  {"surface-family", "surface_family", UNIT_BOOLEAN},
  {"surface-elab", "surface_elab", UNIT_MAIN},
  {"surface-catalog", "surface_catalog", UNIT_MAIN},
  {"surface-program", "surface_program", UNIT_MAIN},
  {"surface-prelude", "surface_prelude", UNIT_MAIN},
  {"surface-crypto", "surface_crypto", UNIT_MAIN},
  {"surface-trust", "surface_trust", UNIT_MAIN},
  {"surface-boundary", "surface_boundary", UNIT_MAIN},
  {"surface-universes", "surface_universes", UNIT_MAIN},
  {"wasm-circuit", "wasm_circuit", UNIT_MAIN},
  {"wasm-order", "wasm_order", UNIT_MAIN},
  {"wasm-link", "wasm_link", UNIT_MAIN},
  {"wasm-host", "wasm_host", UNIT_MAIN},
};

static int unit(const char *self, const char *name) {
  size_t count = sizeof UNITS / sizeof UNITS[0];
  for (size_t i = 0; i < count; i++) {
    if (strcmp(UNITS[i].mode, name) != 0) continue;
    switch (UNITS[i].kind) {
      case UNIT_MAIN: return dispatch_exec(self, UNITS[i].target, 0, NULL);
      case UNIT_BOOLEAN: return boolean(self, UNITS[i].target, UNITS[i].target);
    }
  }
  return dispatch_die_with(64, "unknown unit: ", name);
}

static const DispatchEntry FIXTURES[] = {
  {"encode", "wasm_encode"},
  {"emit", "wasm_emit"},
  {"nat", "wasm_nat_runtime"},
  {"reactor", "wasm_reactor"},
  {"layout", "wasm_layout"},
  {"oracle", "wasm_oracle"},
};

static int fixture(const char *self, const char *name) {
  const DispatchEntry *entry = dispatch_find(FIXTURES, sizeof FIXTURES / sizeof FIXTURES[0], name);
  if (entry) return dispatch_exec(self, entry->target, 0, NULL);
  return dispatch_die_with(64, "unknown fixture: ", name);
}

/* named(is_fixture, args): exactly one name, else die 64. */
static int named(const char *self, int is_fixture, int argc, char **args) {
  if (argc != 1) return dispatch_die(64, "expected one fixture or unit name");
  if (is_fixture) return fixture(self, args[0]);
  return unit(self, args[0]);
}

static const DispatchEntry MODES[] = {
  {"import-json", "import_json"},
  {"import-export", "import_export"},
  {"import-translate", "import_translate"},
  {"import-pipeline", "import_pipeline"},
  {"surface-parser", "surface_driver"},
  {"surface-check", "surface_check_driver"},
  {"erase", "kernel_erase_driver"},
  {"io", "io_driver"},
  {"prelude-runtime", "prelude_runtime"},
  {"wasm-protocol", "wasm_protocol"},
  {"prelude-protocol", "prelude_protocol"},
  {"mapping-protocol", "mapping_protocol"},
};

static int dispatch(const char *self, const char *mode, int argc, char **args) {
  if (strcmp(mode, "unit") == 0) return named(self, 0, argc, args);
  if (strcmp(mode, "fixture") == 0) return named(self, 1, argc, args);
  const DispatchEntry *entry = dispatch_find(MODES, sizeof MODES / sizeof MODES[0], mode);
  if (entry) return dispatch_exec(self, entry->target, argc, args);
  return dispatch_die_with(64, "unknown test mode: ", mode);
}

static int arguments(const char *self, int argc, char **args) {
  if (argc >= 1) return dispatch(self, args[0], argc - 1, args + 1);
  return dispatch_die(64, "expected a test mode");
}

int main(int argc, char **argv) { return arguments(argv[0], argc - 1, argv + 1); }
