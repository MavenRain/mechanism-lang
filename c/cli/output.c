/* Port of cli/output.bend: cli_exit ends the program with a code and no output (the effect
 * cli/effects/exit.c calls exit). Library code never exits, so the C port returns the exit
 * status for the program boundary to return from its entry; stdio flushes on that return as it
 * does on exit. cli_print_parts, cli_print, cli_die and cli_host_failure are the IO.print and IO.die
 * effects of the Bend runtime at the same boundary. */
#include "cli.h"

#include <stdio.h>

int cli_exit(uint32_t code) { return (int)(code & 0xffu); }

int cli_print_parts(const Str *parts, size_t count) {
  size_t i = 0;
  for (; i < count; i = i + 1)
    if (parts[i].len > 0) fwrite(parts[i].data, 1, parts[i].len, stdout);
  fputc('\n', stdout);
  return 0;
}

int cli_print(Str text) { return cli_print_parts(&text, 1); }

/* IO.die appends a diagnostic newline, even when its message is empty. */
int cli_die(uint32_t code, Str message) {
  if (message.len > 0) fwrite(message.data, 1, message.len, stderr);
  fputc('\n', stderr);
  return cli_exit(code);
}

int cli_host_failure(void) {
  fputs("mechanism: host out of memory\n", stderr);
  return 70;
}
