/* dispatch_probe: host test support for the S7.7 dispatchers, with no Bend counterpart.
 * c-test-dispatch links every target name of the runner and surface_foundation_runner tables to this
 * binary inside _tcc/dispatch-probe/, next to copies of the two dispatchers. A run prints one line,
 * `probe <name of argv[0]> [args...]`, and exits 0, so each mode shows the target it routes to and the
 * arguments it passes on (a boolean unit prints the PASS line of the dispatcher instead). */
#include <stdio.h>
#include <string.h>

static const char *base_name(const char *path) {
  const char *slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

static void print_args(int argc, char **argv, int index) {
  if (index >= argc) return;
  printf(" %s", argv[index]);
  print_args(argc, argv, index + 1);
}

int main(int argc, char **argv) {
  if (argc < 1) return 2;
  printf("probe %s", base_name(argv[0]));
  print_args(argc, argv, 1);
  putchar('\n');
  return 0;
}
