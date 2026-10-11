/* Shared boundary of the C test drivers: the IO.print, IO.die and IO.now effects of the Bend
 * drivers (print appends a newline on stdout, die writes the message and a newline on stderr
 * and exits with the code) and the host failure exit after an out-of-memory heap. */
#ifndef MECH_DRIVER_H
#define MECH_DRIVER_H

#include "kernel.h"

#include <stdio.h>
#include <time.h>

static inline int driver_print(Str text) {
  if (text.len > 0) fwrite(text.data, 1, text.len, stdout);
  fputc('\n', stdout);
  return 0;
}

static inline int driver_die(int code, const char *message) {
  fputs(message, stderr);
  fputc('\n', stderr);
  return code;
}

static inline int driver_host_failure(void) {
  fputs("mechanism: host out of memory\n", stderr);
  return 70;
}

/* IO.now: monotonic milliseconds. */
static inline uint64_t driver_now_ms(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
  return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

#endif
