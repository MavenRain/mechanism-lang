#include "rt.h"

#include <pthread.h>

/* tcc builds segfault when main-thread recursion passes roughly 10k to 50k frames, so every
 * binary runs its program on a thread with a 1 GiB stack. Exit code 70 marks a host failure. */
typedef struct {
  int argc;
  char **argv;
  int (*program)(int argc, char **argv);
  int code;
} RtMainJob;

static void *rt_main_thread(void *data) {
  RtMainJob *job = data;
  job->code = job->program(job->argc, job->argv);
  return NULL;
}

int rt_run_main(int argc, char **argv, int (*program)(int argc, char **argv)) {
  RtMainJob job;
  job.argc = argc;
  job.argv = argv;
  job.program = program;
  job.code = 70;
  pthread_attr_t attr;
  if (pthread_attr_init(&attr) != 0) return 70;
  if (pthread_attr_setstacksize(&attr, (size_t)1 << 30) != 0) {
    pthread_attr_destroy(&attr);
    return 70;
  }
  pthread_t thread;
  int created = pthread_create(&thread, &attr, rt_main_thread, &job);
  pthread_attr_destroy(&attr);
  if (created != 0) return 70;
  if (pthread_join(thread, NULL) != 0) return 70;
  return job.code;
}
